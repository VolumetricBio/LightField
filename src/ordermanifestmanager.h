#ifndef ORDERMANIFESTMANAGER_H
#define ORDERMANIFESTMANAGER_H

#include <QString>
#include <QStringList>
#include <QFile>
#include "constants.h"

enum class ManifestParseResult {
    FILE_NOT_EXIST,
    FILE_CORRUPTED,
    POSITIVE_WITH_WARNINGS,
    POSITIVE
};

class ManifestKeys {
public:
    enum Value : uint8_t
    {
        SIZE,
        SORT_TYPE,
        TILING,
        MIN_EXPOSURE,
        STEP,
        SPACE,
        COUNT,
        ENTITIES,
        FILE_NAME,
        LAYER_THICKNESS,
        EXPOSURE_TIME,
        VOLUME,
        BASE_LAYER_CNT
    };

    static const QString strings[13];

    ManifestKeys() = default;
    constexpr ManifestKeys(Value key) : value(key) { }

    constexpr const QString& toQString() {
        return strings[value];
    }
  private:
    Value value;
};


class ManifestSortType {
public:
    enum Value : uint8_t
    {
        NUMERIC=0,
        ALPHANUMERIC=1,
        CUSTOM=2
    };

    static const QString strings[3];

    ManifestSortType() = default;
    constexpr ManifestSortType(Value key) : value(key) { }

    ManifestSortType(QString key) {
        for(uint8_t i=0; i<strings->length(); ++i) {
            if(strings[i] == key) {
                value = static_cast<Value>(i);
                return;
            }
        }

        value = CUSTOM;
    }

    constexpr const QString& toQString() {
        return strings[value];
    }

    int intVal() { return value; }
private:
    Value value;
};


class OrderManifestManager: public QObject
{
    Q_OBJECT

public:
    class Iterator {
    public:
        Iterator(QStringList list) : _list(list) { _ptr = 0; }

        void     operator++() { _ptr++; }
        QString  operator*()  {
            return _list[_ptr];
        }
        bool     hasNext()    {
            return _ptr + 1 < _list.size();
        }
    private:
            int         _ptr;
            QStringList _list;
    };

    OrderManifestManager()      {
        restart();
    }

    ~OrderManifestManager()     { }

    void setPath ( const QString &path ) {
        _dirPath = path;
        _initialized = false;
    }

    const QString &path() const { return _dirPath; }

    ManifestSortType sortType() { return _type; }

    void setSortType( ManifestSortType sortType)
    {
        this->_type = sortType;
    }

    void addFile(const QString &filename)
    {
        _fileNameList.append(filename);
        _size++;
    }

    void setFileList(const QStringList &list)
    {
        this->_fileNameList.clear();
        this->_fileNameList.append(list);
        this->_size = list.size();
    }

    void setExpoTimeList(const QList<double> &list)
    {
        this->_tilingExpoTime.clear();
        this->_tilingExpoTime.append(list);
    }

    void setLayerThicknessList(const QList<int> &list)
    {
        this->_layerThickNess = list;
    }

    void setTilingMinExpoTime(double minExoTm)
    {
        this->_tilingMinExposure = minExoTm;
    }

    void setTilingStep (double step)
    {
        this->_tilingStep = step;
    }

    void setTilingSpace (int space)
    {
        this->_tilingSpace = space;
    }

    void setTilingCount (int count)
    {
        this->_tilingCount = count;
    }

    void setTiled (bool tiled) {
        this->_tiled = tiled;
    }

    void setVolume (double modelVolume){
        this->_estimatedVolume = modelVolume;
    }

    void setBaseLayerThickness(int thickness) {
        this->_baseLayerThickNess = thickness;
    }

    void setBodyLayerThickness(int thickness) {
        this->_bodyLayerThickNess = thickness;
    }

    void setFirstLayerOffset(int offset) {
        this->_firstLayerOffset = offset;
    }

    void setBodyLayerOffset(int thickness) {
        this->_bodyLayerThickNess = thickness;
    }

    void setBaseLayerCount(int count) {
        this->_baseLayerCount = count;
    }

    void requireAreaCalculation(){
        this->_calculateArea = true;
    }

    inline int baseLayerThickness()    { return _baseLayerThickNess; }
    inline int bodyLayerThickness()    { return _bodyLayerThickNess; }
    inline int baseLayerCount()        { return _baseLayerCount; }
    inline int firstLayerOffset()      { return _firstLayerOffset;  }
    inline bool tiled()                { return _tiled; }
    inline double tilingMinExposure()  { return _tilingMinExposure; }
    inline double tilingStep()         { return _tilingStep; }
    inline int tilingSpace()           { return _tilingSpace; }
    inline int tilingCount()           { return _tilingCount; }
    inline double manifestVolume()     { return _estimatedVolume; }

    inline QString getFirstElement()
    {
        return _fileNameList.size() > 0 ? _fileNameList[0] : nullptr;
    }

    inline QString getElementAt(int position) {
        return _fileNameList[position];
    }

    inline int getSize() { return _size; }

    ManifestParseResult parse(QStringList *errors, QStringList *warningList);

    bool save();

    void removeManifest() {
        QFile jsonFile( _dirPath % Slash % ManifestFilename );
        jsonFile.remove();
    }

    void restart() {
        _tiled = false;
        _size = 0;
        _initialized = true;
        _fileNameList.clear();
        _tilingExpoTime.clear();
        _type = ManifestSortType::NUMERIC;
        _dirPath = "";
        _estimatedVolume = 0;
        _calculateArea = false;
    }

    Iterator iterator() {
        return Iterator(_fileNameList);
    }

    inline bool contains( QString fileName ) { return this->_fileNameList.contains(fileName); }
    inline bool initialized ( ) { return this->_initialized; }

    double getTimeForElementAt(int position);

    int layerThickNessAt(int position);
    bool isBaseLayer(int position);
signals:
    void statusUpdate(const QString &messgae);
    void progressUpdate(int percentage);

private:
    QString             _dirPath;
    ManifestSortType    _type;
    int                 _size;
    bool                _tiled             { false };
    double              _tilingMinExposure { 2L };
    double              _tilingStep        { 2L };
    int                 _tilingSpace       { 1 };
    int                 _tilingCount       { 1 };
    QStringList         _fileNameList      { };
    int                 _baseLayerThickNess{ };
    int                 _firstLayerOffset  { };
    int                 _bodyLayerThickNess{ };
    int                 _baseLayerCount    { };
    QList<int>          _layerThickNess    { };
    QList<double>       _tilingExpoTime    { };
    bool                _initialized       { };
    double              _estimatedVolume   { 0L }; // unit: µL
    bool                _calculateArea     {false};
};

#endif // ORDERMANIFESTMANAGER_H
