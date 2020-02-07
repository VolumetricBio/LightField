#include "pch.h"

#include <sys/utsname.h>

#include "upgrademanager.h"

#include "gpgsignaturechecker.h"
#include "hasher.h"
#include "processrunner.h"
#include "stdiologger.h"
#include "timinglogger.h"
#include "upgradekitunpacker.h"
#include "version.h"

namespace {

    QStringList const UpgradeKitFileGlobs {
        "lightfield-*.kit",
    };

    QStringList const UpgradeKitDirGlobs {
        "lightfield-*",
    };

    QMap<BuildType, QString> const BuildTypeToString {
        { BuildType::Release, "release" },
        { BuildType::Debug,   "debug"   },
    };

    QMap<QString, BuildType> const StringToBuildType {
        { "release", BuildType::Release },
        { "debug",   BuildType::Debug   },
    };

    QRegularExpression const MetadataFieldMatcherRegex { "^([-0-9A-Za-z]+):\\s*"                                     };
    QRegularExpression const WhitespaceRegex           { "\\s+"                                                      };
    QRegularExpression const CommentMatcherRegex       { "\\s+#.*$"                                                  };
    QRegularExpression const KitFileMatcherRegex       { "^lightfield(?:-(.*?))?-(debug|release)_(.*?)_(.*?)\\.kit$" };
    QRegularExpression const KitDirMatcherRegex        { "^lightfield(?:-(.*?))?-(debug|release)_(.*?)_(.*?)$"       };

    QStringList const ExpectedMetadataV1Fields {
        "Version",
        "Build-Type",
        "Release-Date",
        "Description",
        "Checksums-SHA256"
    };

    QStringList const ExpectedMetadataV2Fields {
        "Architecture",
        "Release-Train"
    };

    QMap<QString, QString> const KernelArchToDebianArch {
        { "x86_64", "amd64" },
        { "armv7l", "armhf" },
    };

    QStringList _EnsureMapContainsKeys( QMap<QString, QString> map, QStringList keyList ) {
        QStringList result;
        for ( auto const& key : keyList ) {
            if ( !map.contains( key ) ) {
                result += key;
            }
        }
        return result;
    }

}

UpgradeManager::UpgradeManager( QObject* parent ): QObject( parent ) {
    _stderrLogger  = new StdioLogger   { "apt-get/stderr", this };
    _stdoutLogger  = new StdioLogger   { "apt-get/stdout", this };
    _processRunner = new ProcessRunner {                   this };

    {
        utsname u;
        uname( &u );

        _architecture = u.machine;
        if ( KernelArchToDebianArch.contains( _architecture ) ) {
            _architecture = KernelArchToDebianArch[_architecture];
        }
        debug(
            "UpgradeManager::`ctor:\n"
            "  + Linux kernel architecture: %s\n"
            "  + Debian architecture:       %s\n"
            "",
            u.machine,
            _architecture.toUtf8( ).data( )
        );
    }

    QObject::connect( _processRunner, &ProcessRunner::readyReadStandardError,  _stderrLogger, &StdioLogger::read );
    QObject::connect( _processRunner, &ProcessRunner::readyReadStandardOutput, _stdoutLogger, &StdioLogger::read );

    QObject::connect( _processRunner, &ProcessRunner::readyReadStandardError,  [ this ] ( QString const& data ) { _stderrJournal += data; } );
    QObject::connect( _processRunner, &ProcessRunner::readyReadStandardOutput, [ this ] ( QString const& data ) { _stdoutJournal += data; } );

    QObject::connect( this, &UpgradeManager::upgradeCheckComplete, [ this ] ( bool ) {
        _isBusy.clear( );
        TimingLogger::stopTiming( TimingId::UpgradeCheck );
    } );

    QObject::connect( this, &UpgradeManager::upgradeFailed, [ this ] ( ) {
        _isBusy.clear( );
        TimingLogger::stopTiming( TimingId::UpgradeInstallation );
    } );
}

UpgradeManager::~UpgradeManager( ) {
    if ( _stderrLogger ) {
        QObject::disconnect( _stderrLogger );
        _stderrLogger->deleteLater( );
        _stderrLogger = nullptr;
    }
    if ( _stdoutLogger ) {
        QObject::disconnect( _stdoutLogger );
        _stdoutLogger->deleteLater( );
        _stdoutLogger = nullptr;
    }
    if ( _processRunner ) {
        QObject::disconnect( _processRunner );
        _processRunner->deleteLater( );
        _processRunner = nullptr;
    }
}

void UpgradeManager::_flushLoggers( ) {
    _stderrLogger->flush( );
    _stdoutLogger->flush( );
}

void UpgradeManager::_clearJournals( ) {
    _stderrJournal.clear( );
    _stdoutJournal.clear( );
}

void UpgradeManager::_checkForUpgrades( QString const& upgradesPath ) {
    debug( "+ UpgradeManager::_checkForUpgrades: looking for unpacked upgrade kits in '%s'\n", UpdatesRootPath.toUtf8( ).data( ) );
    _unprocessedUpgradeKits.clear( );
    _processedUpgradeKits  .clear( );
    _goodUpgradeKits       .clear( );

    for ( auto kitDirInfo : QDir { UpdatesRootPath }.entryInfoList( UpgradeKitDirGlobs, QDir::Dirs | QDir::Readable | QDir::Executable, QDir::Name ) ) {
        QString kitPath { kitDirInfo.absoluteFilePath( ) };
        QDir    kitDir  { kitPath };

        auto match { KitDirMatcherRegex.match( kitDirInfo.fileName( ) ) };
        if ( !match.hasMatch( ) ) {
            debug( "  + ignoring unpacked kit '%s': folder name doesn't match schema\n", kitPath.toUtf8( ).data( ) );
            continue;
        }
        if ( match.captured( 4 ) != _architecture ) {
            debug( "  + ignoring unpacked kit '%s': wrong architecture\n", kitPath.toUtf8( ).data( ) );
            continue;
        }

        if ( !kitDir.exists( "version.inf" ) ) {
            debug( "  + deleting bad unpacked kit '%s': version.inf file is missing\n", kitPath.toUtf8( ).data( ) );
            kitDir.removeRecursively( );
            continue;
        }
        if ( !kitDir.exists( "version.inf.sig" ) ) {
            debug( "  + deleting bad unpacked kit '%s': version.inf.sig file is missing\n", kitPath.toUtf8( ).data( ) );
            kitDir.removeRecursively( );
            continue;
        }

        debug(
            "+ UpgradeManager::_checkForUpgrades: found unpacked kit in '%s'\n"
            "  + Version:       %s\n"
            "  + Build type:    %s\n"
            "  + Release train: %s\n"
            "  + Architecture:  %s\n"
            "",
            kitPath.toUtf8( ).data( ),
            match.captured( 3 ).toUtf8( ).data( ),
            match.captured( 2 ).toUtf8( ).data( ),
            match.captured( 1 ).toUtf8( ).data( ),
            match.captured( 4 ).toUtf8( ).data( )
        );

        _unprocessedUpgradeKits.append( UpgradeKitInfo { kitPath } );
    }

    if ( !upgradesPath.isEmpty( ) ) {
        debug( "+ UpgradeManager::_checkForUpgrades: looking for upgrade kits in path '%s'\n", upgradesPath.toUtf8( ).data( ) );
        for ( auto kitFile : QDir { upgradesPath }.entryInfoList( UpgradeKitFileGlobs, QDir::Files | QDir::Readable, QDir::Name ) ) {
            auto kitFilePath { kitFile.absoluteFilePath( ) };

            auto match { KitFileMatcherRegex.match( kitFile.fileName( ) ) };
            if ( !match.hasMatch( ) ) {
                debug( "  + ignoring kit '%s': file name doesn't match schema\n", kitFilePath.toUtf8( ).data( ) );
                continue;
            }
            if ( match.captured( 4 ) != _architecture ) {
                debug( "  + ignoring kit '%s': wrong architecture\n", kitFilePath.toUtf8( ).data( ) );
                continue;
            }

            QFileInfo sigFile { kitFilePath % ".sig" };
            if ( !sigFile.exists( ) ) {
                debug( "  + ignoring kit '%s': signature file is missing\n", kitFilePath.toUtf8( ).data( ) );
                continue;
            }

            debug(
                "+ UpgradeManager::_checkForUpgrades: found kit file '%s'\n"
                "  + Version:       '%s'\n"
                "  + Build type:    '%s'\n"
                "  + Release train: '%s'\n"
                "  + Architecture:  '%s'\n"
                "",
                kitFilePath.toUtf8( ).data( ),
                match.captured( 3 ).toUtf8( ).data( ),
                match.captured( 2 ).toUtf8( ).data( ),
                match.captured( 1 ).toUtf8( ).data( ),
                match.captured( 4 ).toUtf8( ).data( )
            );

            _unprocessedUpgradeKits.append( UpgradeKitInfo { kitFile, sigFile } );
        }
    }

    if ( _unprocessedUpgradeKits.isEmpty( ) ) {
        debug( "+ UpgradeManager::_checkForUpgrades: no upgrade kits found\n" );
        emit upgradeCheckComplete( false );
        return;
    }

    debug( "+ UpgradeManager::_checkForUpgrades: found %d upgrade kits\n", _unprocessedUpgradeKits.count( ) );
    _checkNextKitSignature( );
}

void UpgradeManager::_checkNextKitSignature( ) {
    debug( "+ UpgradeManager::_checkNextKitSignature\n" );

top:
    if ( _unprocessedUpgradeKits.isEmpty( ) ) {
        debug( "  + finished checking kit signatures\n" );

        _unprocessedUpgradeKits = std::move( _processedUpgradeKits );
        if ( _unprocessedUpgradeKits.isEmpty( ) ) {
            debug( "  + unprocessed upgrade kits list is empty; emitting upgradeCheckComplete(false).\n" );
            emit upgradeCheckComplete( false );
        } else {
            debug( "  + starting unpacking %d kits\n", _unprocessedUpgradeKits.count( ) );
            _unpackNextKit( );
        }
        return;
    }

    auto& currentUpgradeKit = _unprocessedUpgradeKits.front( );

    if ( currentUpgradeKit.isAlreadyUnpacked ) {
        debug( "  + skipping unpacked upgrade kit '%s'\n", currentUpgradeKit.directory.absolutePath( ).toUtf8( ).data( ) );
        _processedUpgradeKits.append( currentUpgradeKit );
        _unprocessedUpgradeKits.removeFirst( );
        goto top;
    }

    auto kitFilePath = currentUpgradeKit.kitFileInfo.absoluteFilePath( );
    auto sigFilePath = currentUpgradeKit.sigFileInfo.absoluteFilePath( );
    debug(
        "  + checking signature for upgrade kit\n"
        "    + upgrade kit file name: '%s'\n"
        "    + signature file name:   '%s'\n"
        "",
        kitFilePath.toUtf8( ).data( ),
        sigFilePath.toUtf8( ).data( )
    );

    _gpgSignatureChecker = new GpgSignatureChecker( this );
    QObject::connect( _gpgSignatureChecker, &GpgSignatureChecker::signatureCheckComplete, this, &UpgradeManager::gpgSignatureChecker_kit_complete );
    _gpgSignatureChecker->startCheckDetachedSignature( kitFilePath, sigFilePath );
}

void UpgradeManager::gpgSignatureChecker_kit_complete( bool const result ) {
    _gpgSignatureChecker->deleteLater( );
    _gpgSignatureChecker = nullptr;

    debug( "+ UpgradeManager::gpgSignatureChecker_kit_complete: signature is %s\n", result ? "good" : "bad" );
    if ( result ) {
        _processedUpgradeKits.append( _unprocessedUpgradeKits.front( ) );
    }

    _unprocessedUpgradeKits.removeFirst( );
    _checkNextKitSignature( );
}

void UpgradeManager::_unpackNextKit( ) {
    debug( "+ UpgradeManager::_unpackNextKit\n" );

top:
    if ( _unprocessedUpgradeKits.isEmpty( ) ) {
        debug( "  + finished unpacking kits\n" );

        _unprocessedUpgradeKits = std::move( _processedUpgradeKits );
        if ( _unprocessedUpgradeKits.isEmpty( ) ) {
            debug( "  + unprocessed upgrade kits list is empty; emitting upgradeCheckComplete(false).\n" );
            emit upgradeCheckComplete( false );
        } else {
            debug( "  + starting checking version.inf signatures for %d kits\n", _unprocessedUpgradeKits.count( ) );
            _checkNextVersionInfSignature( );
        }
        return;
    }

    auto& currentUpgradeKit = _unprocessedUpgradeKits.front( );

    if ( currentUpgradeKit.isAlreadyUnpacked ) {
        debug( "  + skipping unpacked upgrade kit '%s'\n", currentUpgradeKit.directory.absolutePath( ).toUtf8( ).data( ) );
        _processedUpgradeKits.append( currentUpgradeKit );
        _unprocessedUpgradeKits.removeFirst( );
        goto top;
    }

    QString kitFilePath { currentUpgradeKit.kitFileInfo.absoluteFilePath( ) };
    debug( "  + kit file name: '%s'\n", kitFilePath.toUtf8( ).data( ) );

    QString dirName { UpdatesRootPath + Slash + currentUpgradeKit.kitFileInfo.fileName( ) };
    if ( dirName.endsWith( ".kit" ) ) {
        dirName = dirName.left( dirName.length( ) - 4 );
    }

    debug( "  + Unpacking kit '%s' into directory '%s'\n", kitFilePath.toUtf8( ).data( ), dirName.toUtf8( ).data( ) );
    currentUpgradeKit.directory = QDir { dirName };
    if ( currentUpgradeKit.directory.exists( ) ) {
        debug( "  + Directory already exists, deleting\n" );
        currentUpgradeKit.directory.removeRecursively( );

        auto const pred = [ &dirName ] ( UpgradeKitInfo const& kitInfo ) {
            return kitInfo.isAlreadyUnpacked && ( kitInfo.directory.absolutePath( ) == dirName );
        };

        if ( auto iter = std::find_if( _unprocessedUpgradeKits.begin( ), _unprocessedUpgradeKits.end( ), pred ); iter != _unprocessedUpgradeKits.end( ) ) {
            debug( "  + Forgetting about unprocessed upgrade kit already unpacked into same directory\n" );
            _unprocessedUpgradeKits.erase( iter );
        }
        if ( auto iter = std::find_if( _processedUpgradeKits.begin( ), _processedUpgradeKits.end( ), pred ); iter != _processedUpgradeKits.end( ) ) {
            debug( "  + Forgetting about processed upgrade kit already unpacked into same directory\n" );
            _processedUpgradeKits.erase( iter );
        }
    }

    currentUpgradeKit.directory.mkdir( dirName );

    _upgradeKitUnpacker = new UpgradeKitUnpacker( this );
    QObject::connect( _upgradeKitUnpacker, &UpgradeKitUnpacker::unpackComplete, this, &UpgradeManager::upgradeKitUnpacker_complete );
    _upgradeKitUnpacker->startUnpacking( kitFilePath, dirName );
}

#if defined _DEBUG
void UpgradeManager::upgradeKitUnpacker_complete( bool const result, QString const& tarOutput, QString const& tarError ) {
#else
void UpgradeManager::upgradeKitUnpacker_complete( bool const result ) {
#endif // defined _DEBUG
    _upgradeKitUnpacker->deleteLater( );
    _upgradeKitUnpacker = nullptr;

    debug( "+ UpgradeManager::upgradeKitUnpacker_complete: result is %s\n", ToString( result ) );

    if ( result ) {
        _unprocessedUpgradeKits[0].isAlreadyUnpacked = true;
        _processedUpgradeKits.append( _unprocessedUpgradeKits.front( ) );
#if defined _DEBUG
    } else {
        if ( !tarOutput.isEmpty( ) ) {
            debug( "+ UpgradeManager::upgradeKitUnpacker_complete: tar's stdout:\n%s", tarOutput.toUtf8( ).data( ) );
        }
        if ( !tarError.isEmpty( ) ) {
            debug( "+ UpgradeManager::upgradeKitUnpacker_complete: tar's stderr:\n%s", tarError.toUtf8( ).data( ) );
        }
#endif // defined _DEBUG
    }

    _unprocessedUpgradeKits.removeFirst( );
    _unpackNextKit( );
}

void UpgradeManager::_checkNextVersionInfSignature( ) {
    debug( "+ UpgradeManager::_checkNextVersionInfSignature\n" );

    if ( _unprocessedUpgradeKits.isEmpty( ) ) {
        debug( "  + finished checking version.inf signatures, moving on to examining %d kits\n", _processedUpgradeKits.count( ) );

        _unprocessedUpgradeKits = std::move( _processedUpgradeKits );
        if ( _unprocessedUpgradeKits.isEmpty( ) ) {
            emit upgradeCheckComplete( false );
        } else {
            _examineUnpackedKits( );
        }
        return;
    }

    auto versionInfFilePath = _unprocessedUpgradeKits[0].directory.absoluteFilePath( "version.inf" );
    debug( "  + checking signature for version.inf file '%s'\n", versionInfFilePath.toUtf8( ).data( ) );

    _gpgSignatureChecker = new GpgSignatureChecker( this );
    QObject::connect( _gpgSignatureChecker, &GpgSignatureChecker::signatureCheckComplete, this, &UpgradeManager::gpgSignatureChecker_versionInf_complete );
    _gpgSignatureChecker->startCheckDetachedSignature( versionInfFilePath, versionInfFilePath + QString { ".sig" } );
}

void UpgradeManager::gpgSignatureChecker_versionInf_complete( bool const result ) {
    _gpgSignatureChecker->deleteLater( );
    _gpgSignatureChecker = nullptr;

    {
        auto& kit = _unprocessedUpgradeKits.front( );
        if ( result ) {
            debug( "+ UpgradeManager::gpgSignatureChecker_versionInf_complete: signature is good\n" );
            _processedUpgradeKits.append( kit );
        } else {
            debug( "+ UpgradeManager::gpgSignatureChecker_versionInf_complete: deleting bad unpacked kit: signature is bad\n" );
            kit.directory.removeRecursively( );
        }
    }

    _unprocessedUpgradeKits.removeFirst( );
    _checkNextVersionInfSignature( );
}

bool UpgradeManager::_parseVersionInfo( QString const& versionInfoFileName, UpgradeKitInfo& update ) {
    debug( "+ UpgradeManager::_parseVersionInfo: file name is '%s'\n", versionInfoFileName.toUtf8( ).data( ) );
    auto versionInfo = ReadWholeFile( versionInfoFileName ).replace( EndsWithWhitespaceRegex, "" ).split( NewLineRegex );

    QMap<QString, QString> fields;
    QString currentKey;
    int index = 0;

    while ( index < versionInfo.count( ) ) {
        auto line = versionInfo[index];
        line.replace( EndsWithWhitespaceRegex, "" ).replace( CommentMatcherRegex, "" );
        debug( "  + line #%d: \"%s\"\n", index, line.toUtf8( ).data( ) );

        if ( auto result = MetadataFieldMatcherRegex.match( line ); result.hasMatch( ) ) {
            currentKey = result.captured( 1 );
            line.remove( result.capturedStart( 0 ), result.capturedLength( 0 ) );
            if ( fields.contains( currentKey ) ) {
                debug( "    + metadata for this kit contains duplicate key \"%s\"\n", currentKey.toUtf8( ).data( ) );
                return false;
            }
            fields.insert( currentKey, line );
        } else if ( ( line.length( ) > 1 ) && ( line[0] == Space ) ) {
            if ( currentKey.isEmpty( ) ) {
                debug( "    + continuation line in metadata file without anything to continue\n" );
                return false;
            }

            line.remove( 0, 1 );
            auto& currentField = fields[currentKey];
            if ( !currentField.isEmpty( ) ) {
                currentField.append( LineFeed );
            }
            if ( line != "." ) {
                currentField.append( line );
            }
        } else {
            debug( "    + unintelligible gibberish in metadata file\n" );
            return false;
        }

        ++index;
    }

    {
        update.metadataVersionString = fields.value( "Metadata-Version" );

        bool ok = false;
        update.metadataVersion = update.metadataVersionString.isEmpty( ) ? 0 : update.metadataVersionString.toInt( &ok );
        if ( !ok ) {
            debug( "  + bad metadata version \"%s\"\n", update.metadataVersionString.toUtf8( ).data( ) );
            return false;
        }

        if ( ( update.metadataVersion < 1 ) || ( update.metadataVersion > 2 ) ) {
            debug( "  + unknown metadata version \"%s\"\n", update.metadataVersionString.toUtf8( ).data( ) );
            return false;
        }
    }

    QStringList missingMetadataFields;
    if ( update.metadataVersion >= 1 ) {
        if ( auto missingFields = _EnsureMapContainsKeys( fields, ExpectedMetadataV1Fields ); !missingFields.isEmpty( ) ) {
            missingMetadataFields += missingFields;
        }
    }
    if ( update.metadataVersion >= 2 ) {
        if ( auto missingFields = _EnsureMapContainsKeys( fields, ExpectedMetadataV2Fields ); !missingFields.isEmpty( ) ) {
            missingMetadataFields += missingFields;
        }
    }
    if ( !missingMetadataFields.isEmpty( ) ) {
        debug( "  + metadata is missing field%s %s\n", missingMetadataFields.count( ) != 1 ? "s" : "", missingMetadataFields.join( ", " ) );
        return false;
    }

    {
        update.versionString = fields["Version"];

        auto versionParts = update.versionString.split( "." );
        if ( versionParts.count( ) < 3 || versionParts.count( ) > 4 ) {
            debug( "  + bad software version \"%s\" (1)\n", update.versionString.toUtf8( ).data( ) );
            return false;
        }

        bool ok0 = false, ok1 = false, ok2 = false, ok3 = false;
        update.version = MakeVersionCode( versionParts[0].toUInt( &ok0 ), versionParts[1].toUInt( &ok1 ), versionParts[2].toUInt( &ok2 ), ( ( versionParts.count( ) == 4 ) ? versionParts[3] : QString { "0" } ).toUInt( &ok3 ) );
        if ( !ok0 || !ok1 || !ok2 || !ok3 ) {
            debug( "  + bad software version \"%s\" (2)\n", update.versionString.toUtf8( ).data( ) );
            return false;
        }
    }

    {
        auto buildType = fields["Build-Type"];
        auto buildTypeValue = buildType.toLower( );
        if ( !StringToBuildType.contains( buildTypeValue ) ) {
            debug( "  + unknown build type \"%s\"\n", buildType.toUtf8( ).data( ) );
            return false;
        }
        update.buildType = StringToBuildType[buildTypeValue];
    }

    {
        auto releaseDate = fields["Release-Date"];
        auto releaseDateParts = releaseDate.split( "-" );
        if ( releaseDateParts.count( ) != 3 ) {
            debug( "  + bad release date format \"%s\"\n", releaseDate.toUtf8( ).data( ) );
            return false;
        }
        auto year  = releaseDateParts[0].toInt( );
        auto month = releaseDateParts[1].toInt( );
        auto day   = releaseDateParts[2].toInt( );
        update.releaseDate = QDate( year, month, day );
        if ( !update.releaseDate.isValid( ) ) {
            debug( "  + bad release date \"%s\"\n", releaseDate.toUtf8( ).data( ) );
            return false;
        }
    }

    update.description = fields["Description"];
    update.description.replace( EndsWithWhitespaceRegex, "" );

    update.checksums.clear( );
    auto path = update.directory.absolutePath( ) + Slash;
    for ( auto item : fields["Checksums-SHA256"].replace( EndsWithWhitespaceRegex, "" ).split( NewLineRegex ) ) {
        auto pieces = item.split( WhitespaceRegex, QString::SkipEmptyParts );
        if ( ( pieces.count( ) != 2 ) || ( pieces[0].length( ) != 64 ) ) {
            debug( "  + bad checksum entry \"%s\"\n", item.toUtf8( ).data( ) );
            return false;
        }

        update.checksums[path + pieces[1]] = pieces[0];
        debug( "    + file '%s': checksum %s\n", ( path + pieces[1] ).toUtf8( ).data( ), pieces[0].toUtf8( ).data( ) );
    }

    if ( update.metadataVersion > 1 ) {
        update.architecture = fields["Architecture"];
        if ( _architecture != update.architecture ) {
            debug( "  + wrong architecture '%s'\n", update.architecture.toUtf8( ).data( ) );
            return false;
        }
        update.releaseTrain = fields["Release-Train"];
    }

    return true;
}

void UpgradeManager::_examineUnpackedKits( ) {
    debug( "+ UpgradeManager::_examineUnpackedKits: %d kits to examine\n", _unprocessedUpgradeKits.count( ) );

    for ( auto& update : _unprocessedUpgradeKits ) {
        if ( _parseVersionInfo( update.directory.absoluteFilePath( "version.inf" ), update ) ) {
            _processedUpgradeKits.append( update );
        } else {
            debug( "+ UpgradeManager::_examineUnpackedKits: deleting bad unpacked kit '%s': bad metadata\n", update.directory.absolutePath( ).toUtf8( ).data( ) );
        }
    }

    _unprocessedUpgradeKits = std::move( _processedUpgradeKits );
    _checkNextKitsHashes( );
}

void UpgradeManager::_checkNextKitsHashes( ) {
    debug( "+ UpgradeManager::_checkNextKitsHashes\n" );

    if ( _unprocessedUpgradeKits.isEmpty( ) ) {
        debug( "  + finished checking checksums, %d good kits\n", _goodUpgradeKits.count( ) );

        _unprocessedUpgradeKits.clear( );
        _processedUpgradeKits.clear( );

        emit upgradeCheckComplete( _goodUpgradeKits.count( ) > 0 );
        return;
    }

    if ( !_hashChecker ) {
        _hashChecker = new Hasher;
        QObject::connect( _hashChecker, &Hasher::hashCheckResult, this, &UpgradeManager::hasher_hashCheckResult, Qt::QueuedConnection );
    }

    _hashChecker->checkHashes( _unprocessedUpgradeKits[0].checksums, QCryptographicHash::Sha256 );
}

void UpgradeManager::hasher_hashCheckResult( bool const result ) {
    debug( "+ UpgradeManager::hasher_hashCheckResult: result is %s\n", ToString( result ) );

    if ( result ) {
        _goodUpgradeKits.append( _unprocessedUpgradeKits[0] );
    }
    _unprocessedUpgradeKits.removeFirst( );
    _checkNextKitsHashes( );
}

void UpgradeManager::checkForUpgrades( QString const& upgradesPath ) {
    if ( _isBusy.test_and_set( ) ) {
        debug( "+ UpgradeManager::checkForUpgrades: already busy performing an upgrade check or upgrade install\n" );
        return;
    }

    TimingLogger::startTiming( TimingId::UpgradeCheck );
    debug( "+ UpgradeManager::checkForUpgrades: starting check\n" );
    emit upgradeCheckStarting( );

    _checkForUpgrades( upgradesPath );
}

void UpgradeManager::installUpgradeKit( UpgradeKitInfo const& kit ) {
    if ( _isBusy.test_and_set( ) ) {
        debug( "+ UpgradeManager::installUpgradeKit: already busy performing an upgrade check or upgrade install\n" );
        emit upgradeFailed( );
        return;
    }

    TimingLogger::startTiming( TimingId::UpgradeInstallation, kit.versionString );
    debug( "+ UpgradeManager::installUpgradeKit: installing version %s build type %s\n", kit.versionString.toUtf8( ).data( ), ToString( kit.buildType ) );
    _kitToInstall = new UpgradeKitInfo { kit };

    QString kitPath { _kitToInstall->directory.absolutePath( ) };

    QFile aptSourcesFile { AptSourcesFilePath };
    if ( !aptSourcesFile.exists( ) ) {
        debug( "+ UpgradeManager::installUpgradeKit: upgrade has failed: our apt sources list '%s' doesn't exist and can't be created\n", AptSourcesFilePath.toUtf8( ).data( ) );
        emit upgradeFailed( );
        return;
    }

    if ( !aptSourcesFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
        debug( "+ UpgradeManager::installUpgradeKit: upgrade has failed: can't open our apt sources list '%s' for writing\n", AptSourcesFilePath.toUtf8( ).data( ) );
        emit upgradeFailed( );
        return;
    }
    aptSourcesFile.write( QString { "deb file:" % kitPath % " ./\n" }.toUtf8( ).data( ) );
    aptSourcesFile.close( );

    debug( "+ UpgradeManager::installUpgradeKit: running `apt-get update`\n" );
    _clearJournals( );

    QObject::disconnect( _processRunner, &ProcessRunner::succeeded, this, nullptr );
    QObject::disconnect( _processRunner, &ProcessRunner::failed,    this, nullptr );
    QObject::connect   ( _processRunner, &ProcessRunner::succeeded, this, &UpgradeManager::aptGetUpdate_succeeded );
    QObject::connect   ( _processRunner, &ProcessRunner::failed,    this, &UpgradeManager::aptGetUpdate_failed    );

    _processRunner->start( { "sudo" }, { "apt-get", "-y", "update", } );
}

void UpgradeManager::aptGetUpdate_succeeded( ) {
    _flushLoggers( );
    debug( "+ UpgradeManager::aptGetUpdate_succeeded: running `apt-get install`\n" );

    QStringList processArgs { "apt-get", "-y", "install", };
    QString versionNumberSuffix;

    if ( _kitToInstall->version < LIGHTFIELD_VERSION_CODE ) {
        versionNumberSuffix = '=' % _kitToInstall->versionString;
        processArgs.append( "--allow-downgrades" );
    } else if ( _kitToInstall->version == LIGHTFIELD_VERSION_CODE ) {
        processArgs.append( "--reinstall" );
    }

    if ( _kitToInstall->version <= LIGHTFIELD_VERSION_CODE ) {
        processArgs.append( "lightfield-common" % versionNumberSuffix );
    }

    processArgs.append( QString { "lightfield-" % BuildTypeToString[_kitToInstall->buildType] % versionNumberSuffix } );

    _clearJournals( );

    QObject::disconnect( _processRunner, &ProcessRunner::succeeded, this, nullptr );
    QObject::disconnect( _processRunner, &ProcessRunner::failed,    this, nullptr );
    QObject::connect   ( _processRunner, &ProcessRunner::succeeded, this, &UpgradeManager::aptGetInstall_succeeded );
    QObject::connect   ( _processRunner, &ProcessRunner::failed,    this, &UpgradeManager::aptGetInstall_failed    );

    _processRunner->start( { "sudo" }, processArgs );
}

void UpgradeManager::aptGetUpdate_failed( int const exitCode, QProcess::ProcessError const error ) {
    _flushLoggers( );
    debug( "+ UpgradeManager::aptGetUpdate_failed: exit code: %d, error %s [%d]\n", exitCode, ToString( error ), error );

    QObject::disconnect( _processRunner, &ProcessRunner::succeeded, this, nullptr );
    QObject::disconnect( _processRunner, &ProcessRunner::failed,    this, nullptr );

    emit upgradeFailed( );
}

void UpgradeManager::aptGetInstall_succeeded( ) {
    _flushLoggers( );
    debug( "+ UpgradeManager::aptGetInstall_succeeded: running `apt-get dist-upgrade`\n" );

    _clearJournals( );

    QObject::disconnect( _processRunner, &ProcessRunner::succeeded, this, nullptr );
    QObject::disconnect( _processRunner, &ProcessRunner::failed,    this, nullptr );
    QObject::connect   ( _processRunner, &ProcessRunner::succeeded, this, &UpgradeManager::aptGetDistUpgrade_succeeded );
    QObject::connect   ( _processRunner, &ProcessRunner::failed,    this, &UpgradeManager::aptGetDistUpgrade_failed    );

    _processRunner->start( { "sudo" }, { "apt-get", "-y", "dist-upgrade", } );
}

void UpgradeManager::aptGetInstall_failed( int const exitCode, QProcess::ProcessError const error ) {
    _flushLoggers( );
    debug( "+ UpgradeManager::aptGetInstall_failed: exit code: %d, error %s [%d]\n", exitCode, ToString( error ), error );

    QObject::disconnect( _processRunner, &ProcessRunner::succeeded, this, nullptr );
    QObject::disconnect( _processRunner, &ProcessRunner::failed,    this, nullptr );

    emit upgradeFailed( );
}

void UpgradeManager::aptGetDistUpgrade_succeeded( ) {
    TimingLogger::stopTiming( TimingId::UpgradeInstallation );

    _flushLoggers( );
    debug( "+ UpgradeManager::aptGetDistUpgrade_succeeded: rebooting\n" );

    QObject::disconnect( _processRunner, &ProcessRunner::succeeded, this, nullptr );
    QObject::disconnect( _processRunner, &ProcessRunner::failed,    this, nullptr );

    RebootPrinter( );
}

void UpgradeManager::aptGetDistUpgrade_failed( int const exitCode, QProcess::ProcessError const error ) {
    _flushLoggers( );
    debug( "+ UpgradeManager::aptGetDistUpgrade_failed: exit code: %d, error %s [%d]\n", exitCode, ToString( error ), error );

    QObject::disconnect( _processRunner, &ProcessRunner::succeeded, this, nullptr );
    QObject::disconnect( _processRunner, &ProcessRunner::failed,    this, nullptr );

    emit upgradeFailed( );
}
