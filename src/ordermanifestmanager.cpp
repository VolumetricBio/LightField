#include "ordermanifestmanager.h"

const QString ManifestKeys::strings[15] = {
        "size",
        "sort_type",
        "tiling",
        "minExposure",
        "step",
        "space",
        "count",
        "entities",
        "fileName",
        "baseLayer",
        "baseLayerThickness",
        "firstLayerOffset",
        "bodyLayerThickness",
        "exposureTime",
        "estimatedVolume"
};

const QString ManifestSortType::strings[3] = {
        "numeric",
        "alphanumeric",
        "custom"
};

ManifestParseResult OrderManifestManager::parse(QStringList *errors=nullptr, QStringList *warningList = nullptr) {
    debug( "+ OrderManifestManager::parse: parsing manifest file in directory '%s'\n", _dirPath.toUtf8( ).data( ) );
    _fileNameList.clear();
    _isBaseLayer.clear();

    QFile jsonFile( _dirPath % Slash % ManifestFilename );
    if(!jsonFile.exists()) {

        debug( "+ OrderManifestManager::parse: manifest file not exist '%s'\n", _dirPath.toUtf8( ).data( ) );

        return ManifestParseResult::FILE_NOT_EXIST;
    }

    if(!jsonFile.open(QIODevice::ReadOnly)) {

        debug( "+ OrderManifestManager::parse: no access rights to manifest file '%s'\n", _dirPath.toUtf8( ).data( ) );

        if(errors)
            errors->push_back("No access rights to manifest file.");

        return ManifestParseResult::FILE_NOT_EXIST;
    }

    debug( "OrderManifestManager::parse: building document\n" );
    QJsonParseError parseError;
    QJsonDocument jsonDocument = QJsonDocument().fromJson(jsonFile.readAll(), &parseError);

    if(jsonDocument.isNull()) {

        debug( "+ OrderManifestManager::parse: error while parsing manifest file '%s'\n", parseError.errorString().toUtf8( ).data( ) );

        if(errors)
            errors->push_back(parseError.errorString());

        return ManifestParseResult::FILE_CORRUPTED;
    }

    QJsonObject           root = jsonDocument.object();

    _size = root.value(ManifestKeys(ManifestKeys::SIZE).toQString()).toInt();
    _type = ManifestSortType(root.value(ManifestKeys(ManifestKeys::SORT_TYPE).toQString()).toString());
    _firstLayerOffset = root.value(ManifestKeys(ManifestKeys::FIRST_LAYER_OFFSET).toQString()).toInt();
    _baseLayerThickNess = root.value(ManifestKeys(ManifestKeys::BASE_LAYER_THICKNESS).toQString()).toInt();
    _bodyLayerThickNess = root.value(ManifestKeys(ManifestKeys::BODY_LAYER_THICKNESS).toQString()).toInt();

    debug( "+ OrderManifestManager::parse: checking tiling... \n" );
    if( root.contains( ManifestKeys(ManifestKeys::TILING).toQString( ) ) ) {
        QJsonObject tilingNested;
        debug( "+ OrderManifestManager::parse: checking tiled \n" );
        _tiled = true;
        tilingNested = root.value(ManifestKeys(ManifestKeys::TILING).toQString()).toObject();
        _tilingStep = tilingNested.value(ManifestKeys(ManifestKeys::STEP).toQString()).toDouble();
        _tilingMinExposure = tilingNested.value(ManifestKeys(ManifestKeys::MIN_EXPOSURE).toQString()).toDouble();
        _tilingSpace = tilingNested.value(ManifestKeys(ManifestKeys::SPACE).toQString()).toInt();
        _tilingCount = tilingNested.value(ManifestKeys(ManifestKeys::COUNT).toQString()).toInt();
        _estimatedVolume = tilingNested.value(ManifestKeys(ManifestKeys::VOLUME).toQString()).toDouble();
      
        QJsonArray expoTimes = tilingNested.value(ManifestKeys(ManifestKeys::EXPOSURE_TIME).toQString()).toArray();

        for(int i=0; i<expoTimes.count(); ++i) {
            _tilingExpoTime.push_back(expoTimes[i].toDouble());
        }
    }

    QJsonArray entities = root.value(ManifestKeys(ManifestKeys::ENTITIES).toQString()).toArray();

    int i=0;

    try {
        for(; i<entities.count(); ++i) {
            QJsonObject entity = entities[i].toObject();
            QString fileName = entity.value(ManifestKeys(ManifestKeys::FILE_NAME).toQString()).toString();
            bool isBase = entity.value(ManifestKeys(ManifestKeys::FILE_NAME).toQString()).toBool();

            _fileNameList.push_back(fileName);
            _isBaseLayer.push_back(isBase);
        }
    } catch (...) {
        return ManifestParseResult::FILE_CORRUPTED;
    }

    if(i != _size ) {
        if(warningList)
            warningList->push_back( "Declared size is divergent with the number of entries.");

        _size = i;
        return ManifestParseResult::POSITIVE_WITH_WARNINGS;
    }

    _initialized = true;
    return ManifestParseResult::POSITIVE;
}

bool OrderManifestManager::save() {
    debug( "+ OrderManifestManager::save \n" );
    QFile jsonFile( _dirPath % Slash % ManifestFilename );
    QJsonDocument jsonDocument=QJsonDocument( );
    QJsonObject   root;
    root.insert( ManifestKeys(ManifestKeys::SORT_TYPE).toQString(), QJsonValue { _type.toQString() } );
    root.insert( ManifestKeys(ManifestKeys::SIZE).toQString(),      QJsonValue { _size } );

    if(_tiled)
    {
        QJsonObject tiling;

        tiling.insert( ManifestKeys(ManifestKeys::MIN_EXPOSURE).toQString(),    QJsonValue { _tilingMinExposure } );
        tiling.insert( ManifestKeys(ManifestKeys::STEP).toQString(),            QJsonValue { _tilingStep } );
        tiling.insert( ManifestKeys(ManifestKeys::SPACE).toQString(),           QJsonValue { _tilingSpace } );
        tiling.insert( ManifestKeys(ManifestKeys::COUNT).toQString(),           QJsonValue { _tilingCount } );
        tiling.insert( ManifestKeys(ManifestKeys::VOLUME).toQString(),          QJsonValue { _estimatedVolume } );

        QJsonArray expoArray;
        for(int i=0; i<_tilingExpoTime.size(); ++i)
        {
            expoArray.append(_tilingExpoTime[i]);
        }

        tiling.insert( ManifestKeys(ManifestKeys::EXPOSURE_TIME).toQString(), expoArray );

        root.insert( ManifestKeys(ManifestKeys::TILING).toQString(), tiling );
    }

    QJsonArray jsonArray;

    for(int i=0; i<_fileNameList.size(); ++i)
    {
        QJsonObject entity;
        entity.insert( ManifestKeys(ManifestKeys::FILE_NAME).toQString(),      QJsonValue { _fileNameList[i] } );
        entity.insert( ManifestKeys(ManifestKeys::BASE_LAYER).toQString(),     QJsonValue { isBaseLayer(i) } );

        jsonArray.append(entity);
    }

    root.insert( ManifestKeys(ManifestKeys::ENTITIES).toQString(), jsonArray );
    jsonDocument.setObject(root);

    bool result;
    result = jsonFile.open(QFile::WriteOnly);
    result = result && jsonFile.write(jsonDocument.toJson()) > 0;

    return result;
}

double OrderManifestManager::getTimeForElementAt(int position){
    if(!_tiled || _tilingExpoTime.count() < position)
        return 0;
    else
        return _tilingExpoTime[position];
}

bool OrderManifestManager::isBaseLayer(int position) {
    if(this->_isBaseLayer.count() < position)
        return false;
    else
        return _isBaseLayer[position];
}

int OrderManifestManager::baseLayersCount() {
    int sum=0;
    for(int i=0; this->_isBaseLayer.count(); ++i)
    {
        if(_isBaseLayer[i])
            sum++;
    }

    return sum;
}
