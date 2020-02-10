#include "pch.h"

#include "upgradeselector.h"

#include "upgrademanager.h"

namespace {

    QRegularExpression const AsciiBulletLineRegex { "^\\s+\\*", QRegularExpression::MultilineOption };

    QString const DebugBuildSuffix   { " (debug version)" };
    QString const ReleaseBuildSuffix { };

}

UpgradeSelector::UpgradeSelector( UpgradeManager* upgradeManager, QWidget* parent ): InitialShowEventMixin<UpgradeSelector, QMainWindow>( parent ), _upgradeManager( upgradeManager ) {
    setWindowFlags( windowFlags( ) | ( g_settings.frameless ? Qt::FramelessWindowHint : Qt::BypassWindowManagerHint ) );
    setFixedSize( MainWindowSize );
    move( g_settings.mainWindowPosition );

    setFont( ModifyFont( font( ), 22.0 ) );

    _availableKits = _upgradeManager->availableUpgrades( );
    std::sort( _availableKits.begin( ), _availableKits.end( ), [ ] ( UpgradeKitInfo const& a, UpgradeKitInfo const& b ) {
        // sort reverse by version number
        if ( a.version > b.version ) {
            return -1;
        }
        if ( a.version < b.version ) {
            return 1;
        }

        // then sort forward by release train
        if ( a.releaseTrain < b.releaseTrain ) {
            return -1;
        }
        if ( a.releaseTrain > b.releaseTrain ) {
            return 1;
        }

        // then sort forward by build type
        if ( a.buildType < b.buildType ) {
            return -1;
        }
        if ( a.buildType > b.buildType ) {
            return 1;
        }

        return 0;
    } );

    QStringList kitsListStrings;
    for ( auto const& kit : _availableKits ) {
        QString releaseTrain;
        if ( kit.releaseTrain != "base" ) {
            releaseTrain = " [" % kit.releaseTrain % ']';
        }

        QString buildType;
        if ( kit.buildType == BuildType::Debug ) {
            buildType = DebugBuildSuffix;
        } else {
            buildType = ReleaseBuildSuffix;
        }

        kitsListStrings.append( kit.versionString % releaseTrain % buildType );
    }

    auto availableKitsLabel = new QLabel { "Available versions:" };

    auto kitsListView = new GestureListView;
    kitsListView->setFlow( QListView::TopToBottom );
    kitsListView->setLayoutMode( QListView::SinglePass );
    kitsListView->setMovement( QListView::Static );
    kitsListView->setSelectionMode( QListView::SingleSelection );
    kitsListView->setViewMode( QListView::ListMode );
    kitsListView->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    kitsListView->setFixedSize( MainWindowSize.width( ) * 2 / 3 - 14, MainWindowSize.height( ) / 3 );
    kitsListView->setModel( new QStringListModel { kitsListStrings } );
    QObject::connect( kitsListView, &GestureListView::clicked, this, &UpgradeSelector::kitsListView_clicked );

    _description = new QLabel;
    _description->setAlignment( Qt::AlignLeft | Qt::AlignTop );
    _description->setFont( ModifyFont( _description->font( ), 12.0 ) );
    _description->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    _description->setFixedSize( MainWindowSize.width( ) * 2 / 3 - 14, MainWindowSize.height( ) / 3 );
    _description->setWordWrap( true );

    _upgradeButton = new QPushButton { "Upgrade" };
    _upgradeButton->setDefault( false );
    _upgradeButton->setEnabled( false );
    QObject::connect( _upgradeButton, &QPushButton::clicked, [ this ] ( bool ) { emit kitSelected( _availableKits[_currentSelection] );  } );

    _cancelButton = new QPushButton { "Cancel" };
    _cancelButton->setDefault( true );
    QObject::connect( _cancelButton,  &QPushButton::clicked, [ this ] ( bool ) { emit canceled( ); } );

    {
        auto kitsListLayout = WrapWidgetsInVBox(
            availableKitsLabel,
            kitsListView
        );
        kitsListLayout->setAlignment( Qt::AlignHCenter );

        auto scrollArea = new QScrollArea;
        scrollArea->setFixedSize( MainWindowSize.width( ) * 2 / 3, MainWindowSize.height( ) / 3 );
        scrollArea->setFrameStyle( QFrame::Box | QFrame::Sunken );
        scrollArea->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        scrollArea->setWidget( _description );
        scrollArea->setWidgetResizable( true );

        auto centralWidget = new QWidget;
        centralWidget->setLayout( WrapWidgetsInHBox( nullptr, WrapWidgetsInVBox(
            nullptr,
            kitsListLayout,
            scrollArea,
            WrapWidgetsInHBox( nullptr, _upgradeButton, nullptr, _cancelButton, nullptr ),
            nullptr
        ), nullptr ) );
        setCentralWidget( centralWidget );
    }

    {
        auto upgradeInProgressMessage = new QLabel { "Please wait, software update in progress…" };
        upgradeInProgressMessage->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum );

        _upgradeInProgressLayout = WrapWidgetsInHBox( nullptr, WrapWidgetsInVBox(
            nullptr,
            upgradeInProgressMessage,
            nullptr
        ), nullptr );
    }

    {
        auto upgradeFailedMessage = new QLabel { "Software update failed." };
        upgradeFailedMessage->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum );

        _aptOutput = new QLabel;
        _aptOutput->setAlignment( Qt::AlignLeft | Qt::AlignTop );
        _aptOutput->setFont( ModifyFont( _aptOutput->font( ), 12.0 ) );
        _aptOutput->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
        _aptOutput->setFixedSize( MainWindowSize.width( ) * 2 / 3 - 14, MainWindowSize.height( ) / 3 );
        _aptOutput->setWordWrap( true );

        auto scrollArea = new QScrollArea;
        scrollArea->setFixedSize( MainWindowSize.width( ) * 2 / 3, MainWindowSize.height( ) * 2 / 3 );
        scrollArea->setFrameStyle( QFrame::Box | QFrame::Sunken );
        scrollArea->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        scrollArea->setWidget( _aptOutput );
        scrollArea->setWidgetResizable( true );

        auto okButton = new QPushButton { "OK" };
        QObject::connect( okButton, &QPushButton::clicked, [ this ] ( bool ) { emit canceled( ); } );

        _upgradeFailedLayout = WrapWidgetsInHBox( nullptr, WrapWidgetsInVBox(
            nullptr,
            upgradeFailedMessage,
            nullptr,
            scrollArea,
            nullptr,
            okButton,
            nullptr
        ), nullptr );
    }
}

UpgradeSelector::~UpgradeSelector( ) {
    /*empty*/
}

void UpgradeSelector::_initialShowEvent( QShowEvent* event ) {
    move( g_settings.mainWindowPosition );
    setFixedSize( MainWindowSize );

    auto newSize = maxSize( _upgradeButton->size( ), _cancelButton->size( ) ) + ButtonPadding;
    _upgradeButton->setFixedSize( newSize );
    _cancelButton ->setFixedSize( newSize );

    event->accept( );
}

void UpgradeSelector::kitsListView_clicked( QModelIndex const& index ) {
    if ( index.isValid( ) ) {
        auto row = index.row( );
        auto kitInfo = _availableKits[row];
        debug( "+ UpgradeSelector::kitsListView_clicked: row %d selected: version %s, build type %s\n", row, kitInfo.versionString.toUtf8( ).data( ), ToString( kitInfo.buildType ) );

        _currentSelection = row;
        auto description { kitInfo.description };
        description.replace( AsciiBulletLineRegex, Bullet );
        _description->setText( description );
        _upgradeButton->setEnabled( true );
    } else {
        debug( "+ UpgradeSelector::kitsListView_clicked: row %d deselected\n", _currentSelection );

        _currentSelection = -1;
        _description->clear( );
        _upgradeButton->setEnabled( false );
    }
}

void UpgradeSelector::showInProgressMessage( ) {
    debug( "+ UpgradeSelector::showInProgressMessage\n" );

    auto centralWidget { new QWidget };
    centralWidget->setLayout( _upgradeInProgressLayout );
    setCentralWidget( centralWidget );
}

void UpgradeSelector::showFailedMessage( ) {
    debug( "+ UpgradeSelector::showFailedMessage\n" );

    auto stdoutLog { _upgradeManager->stdoutJournal( ) };
    stdoutLog.replace( CarriageReturn, LineFeed );
    if ( stdoutLog.endsWith( LineFeed ) ) {
        stdoutLog.chop( 1 );
    }

    auto stderrLog { _upgradeManager->stderrJournal( ) };
    stderrLog.replace( CarriageReturn, LineFeed );
    if ( stderrLog.endsWith( LineFeed ) ) {
        stderrLog.chop( 1 );
    }

    _aptOutput->setText( stdoutLog % LineFeed % QString { 50, '=' } % LineFeed % stderrLog );

    auto centralWidget { new QWidget };
    centralWidget->setLayout( _upgradeFailedLayout );
    setCentralWidget( centralWidget );
}
