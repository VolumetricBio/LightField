#ifndef TILINGMANAGER_H
#define TILINGMANAGER_H

#include "ordermanifestmanager.h"
#include "printjob.h"


class TilingManager {

public:
  TilingManager( PrintJob* printJob );
  ~TilingManager() = default;

  void processImages( int width, int height, double baseExpoTime, double baseStep, double bodyExpoTime, double bodyStep, int space, int count );
  inline QString getPath ( ) { return _path; };

protected:
  void tileImages ( );
  void renderTiles ( QFileInfo info );
  void putImageAt ( QPixmap pixmap, QPainter* painter, int i, int j );
private:
        PrintJob*             _printJob;
        QString               _path;
        int                   _width;
        int                   _height;
        double                _baseExpoTime;
        double                _baseStep;
        double                _bodyExpoTime;
        double                _bodyStep;
        int                   _space;
        int                   _spacePx;
        int                   _count;

        int                   _counter;
        int                   _wCount;
        int                   _hCount;
        QStringList           _fileNameList;
        QList<double>         _expoTimeList;
        std::vector<int>      _tileSlots;
};



#endif // TILINGMANAGER_H
