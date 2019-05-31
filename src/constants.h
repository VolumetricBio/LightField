#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

QSize   extern const SmallMainWindowSize;
QSize   extern const SmallMainButtonSize;
QSize   extern const SmallMaximalRightHandPaneSize;

QSize   extern       MainWindowSize;
QSize   extern       MainButtonSize;
QSize   extern       MaximalRightHandPaneSize;

QSize   extern const ProjectorWindowSize;

QString extern const MediaRootPath;
QString extern const StlModelLibraryPath;
QString extern const JobWorkingDirectoryPath;
QString extern const SlicedSvgFileName;
QString extern const SetpowerCommand;

QChar   extern const Space;
QChar   extern const Slash;
QChar   extern const DigitZero;
QChar   extern const FigureSpace;
QChar   extern const EmDash;
QChar   extern const BlackDiamond;

QChar   extern const FA_Check;
QChar   extern const FA_Times;
QChar   extern const FA_FastBackward;
QChar   extern const FA_Backward;
QChar   extern const FA_Forward;
QChar   extern const FA_FastForward;

double         const PrinterMaximumX          = 64.00;
double         const PrinterMaximumY          = 40.00;
double         const PrinterMaximumZ          = 50.00;
double         const PrinterRaiseToMaxZHeight = 50.00;

double         const AspectRatio5to3          =  5.0 /  3.0;
double         const AspectRatio16to10        = 16.0 / 10.0;

double         const LargeFontSize            = 22.0;
double         const NormalFontSize           = 12.0;

#endif // __CONSTANTS_H__
