#include "pch.h"

#include "window.h"

#include "printmanager.h"
#include "signalhandler.h"
#include "strings.h"
#include "constants.h"

namespace {

    QStringList ExposureScaleFactorStringList { "1×", "2×", "3×", "4×", "5×" };
    int ExposureScaleFactorValues[] { 1, 2, 3, 4, 5 };

    class TabIndex {
    public:
        enum {
            Select,
            Prepare,
            Print,
            Status,
        };
    };

}

Window::Window(bool fullScreen, bool debuggingPosition, QWidget *parent): QMainWindow(parent) {
    QMargins emptyMargins { };

    move( { 0, debuggingPosition ? 560 : 800 } );
    if ( fullScreen ) {
        showFullScreen( );
    } else {
        setFixedSize( 800, 480 );
    }

    printJob = new PrintJob;

    QObject::connect( g_signalHandler, &SignalHandler::quit, this, &Window::signalHandler_quit );

    //
    // "Select" tab
    //

    selectTab = new SelectTab;
    selectTab->setContentsMargins( emptyMargins );
    selectTab->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    selectTab->setPrintJob( printJob );
    QObject::connect( selectTab, &SelectTab::modelSelected, this, &Window::selectTab_modelSelected );

    //
    // "Prepare" tab
    //

    prepareTab = new PrepareTab;
    prepareTab->setContentsMargins( emptyMargins );
    prepareTab->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    prepareTab->setPrintJob( printJob );
    QObject::connect( prepareTab, &PrepareTab::sliceStarting,  this, &Window::prepareTab_sliceStarting  );
    QObject::connect( prepareTab, &PrepareTab::sliceComplete,  this, &Window::prepareTab_sliceComplete  );
    QObject::connect( prepareTab, &PrepareTab::renderStarting, this, &Window::prepareTab_renderStarting );
    QObject::connect( prepareTab, &PrepareTab::renderComplete, this, &Window::prepareTab_renderComplete );

    //
    // "Print" tab
    //

    exposureTime = new QLineEdit;
    exposureTime->setAlignment( Qt::AlignRight );
    exposureTime->setText( "1.0" );
    exposureTime->setValidator( new QDoubleValidator( 0.0, 1.0E10, 10 ) );
    QObject::connect( exposureTime, &QLineEdit::editingFinished, this, &Window::exposureTime_editingFinished );

    exposureTimeLabel = new QLabel( "Exposure time (seconds):" );
    exposureTimeLabel->setBuddy( exposureTime );

    exposureScaleFactorComboBox = new QComboBox;
    exposureScaleFactorComboBox->setEditable( false );
    exposureScaleFactorComboBox->setMaxVisibleItems( ExposureScaleFactorStringList.count( ) );
    exposureScaleFactorComboBox->addItems( ExposureScaleFactorStringList );
    printJob->exposureTimeScaleFactor = 1;
    QObject::connect( exposureScaleFactorComboBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, &Window::exposureScaleFactorComboBox_currentIndexChanged );

    exposureScaleFactorLabel = new QLabel( "First layers time scale factor:" );
    exposureScaleFactorLabel->setBuddy( exposureScaleFactorComboBox );

    powerLevelSlider = new QSlider( Qt::Orientation::Horizontal );
    powerLevelSlider->setTickPosition( QSlider::TickPosition::TicksBelow );
    powerLevelSlider->setMinimum( 20 );
    powerLevelSlider->setMaximum( 100 );
    powerLevelSlider->setValue( 50 );
    printJob->powerLevel = 127;
    QObject::connect( powerLevelSlider, &QSlider::valueChanged, this, &Window::powerLevelSlider_valueChanged );

    powerLevelLabel = new QLabel( "Projector power level:" );
    powerLevelLabel->setBuddy( powerLevelSlider );

    powerLevelValue = new QLabel( "50%" );
    powerLevelValue->setAlignment( Qt::AlignRight );
    powerLevelValue->setFrameShadow( QFrame::Sunken );
    powerLevelValue->setFrameStyle( QFrame::StyledPanel );

    powerLevelValueLayout = new QHBoxLayout( );
    powerLevelValueLayout->setContentsMargins( emptyMargins );
    powerLevelValueLayout->addWidget( powerLevelLabel );
    powerLevelValueLayout->addStretch( );
    powerLevelValueLayout->addWidget( powerLevelValue );

    powerLevelValueContainer = new QWidget( );
    powerLevelValueContainer->setContentsMargins( emptyMargins );
    powerLevelValueContainer->setLayout( powerLevelValueLayout );

    powerLevelSliderLeftLabel = new QLabel( "20%" );
    powerLevelSliderLeftLabel->setAlignment( Qt::AlignLeft );
    powerLevelSliderRightLabel = new QLabel( "100%" );
    powerLevelSliderRightLabel->setAlignment( Qt::AlignRight );

    powerLevelSliderLabelsLayout = new QHBoxLayout( );
    powerLevelSliderLabelsLayout->addWidget( powerLevelSliderLeftLabel );
    powerLevelSliderLabelsLayout->addStretch( );
    powerLevelSliderLabelsLayout->addWidget( powerLevelSliderRightLabel );

    powerLevelSliderLabelsContainer = new QWidget( );
    powerLevelSliderLabelsContainer->setLayout( powerLevelSliderLabelsLayout );

    optionsLayout = new QVBoxLayout;
    optionsLayout->setContentsMargins( emptyMargins );
    optionsLayout->addWidget( exposureTimeLabel );
    optionsLayout->addWidget( exposureTime );
    optionsLayout->addWidget( exposureScaleFactorLabel );
    optionsLayout->addWidget( exposureScaleFactorComboBox );
    optionsLayout->addWidget( powerLevelValueContainer );
    optionsLayout->addWidget( powerLevelSlider );
    optionsLayout->addWidget( powerLevelSliderLabelsContainer );
    optionsLayout->addStretch( );

    optionsContainer = new QWidget( );
    optionsContainer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    optionsContainer->setLayout( optionsLayout );

    printButton = new QPushButton( "Print" );
    {
        auto font { printButton->font( ) };
        font.setPointSizeF( 22.25 );
        printButton->setFont( font );
    }
    printButton->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::MinimumExpanding );
    printButton->setEnabled( false );
    QObject::connect( printButton, &QPushButton::clicked, this, &Window::printButton_clicked );

    printTabLayout = new QGridLayout;
    printTabLayout->setContentsMargins( emptyMargins );
    printTabLayout->addWidget( optionsContainer,      0, 0, 1, 1 );
    printTabLayout->addWidget( printButton,           1, 0, 1, 1 );
    printTabLayout->setRowStretch( 0, 4 );
    printTabLayout->setRowStretch( 1, 1 );

    printTab = new QWidget;
    printTab->setContentsMargins( emptyMargins );
    printTab->setLayout( printTabLayout );
    printTab->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    //
    // "Status" tab
    //

    printerStateLabel   = new QLabel( "Printer status:" );
    printerStateDisplay = new QLabel( "Offline" );
    printerStateLabel->setBuddy( printerStateDisplay );
    printerStateDisplay->setFrameShadow( QFrame::Sunken );
    printerStateDisplay->setFrameStyle( QFrame::StyledPanel );

    projectorLampStateLabel   = new QLabel( "Projector lamp status:" );
    projectorLampStateDisplay = new QLabel( "Off" );
    projectorLampStateLabel->setBuddy( projectorLampStateDisplay );
    projectorLampStateDisplay->setFrameShadow( QFrame::Sunken );
    projectorLampStateDisplay->setFrameStyle( QFrame::StyledPanel );

    jobStateLabel   = new QLabel( "Job status:" );
    jobStateDisplay = new QLabel( "Not printing" );
    jobStateLabel->setBuddy( jobStateDisplay );
    jobStateDisplay->setFrameShadow( QFrame::Sunken );
    jobStateDisplay->setFrameStyle( QFrame::StyledPanel );

    currentLayerLabel   = new QLabel( "Printer status:" );
    currentLayerDisplay = new QLabel( "Offline" );
    currentLayerLabel->setBuddy( currentLayerDisplay );
    currentLayerDisplay->setFrameShadow( QFrame::Sunken );
    currentLayerDisplay->setFrameStyle( QFrame::StyledPanel );

    progressControlsLayout = new QVBoxLayout;
    progressControlsLayout->setContentsMargins( emptyMargins );
    progressControlsLayout->addWidget( printerStateLabel );
    progressControlsLayout->addWidget( printerStateDisplay );
    progressControlsLayout->addWidget( projectorLampStateLabel );
    progressControlsLayout->addWidget( projectorLampStateDisplay );
    progressControlsLayout->addWidget( jobStateLabel );
    progressControlsLayout->addWidget( jobStateDisplay );
    progressControlsLayout->addWidget( currentLayerLabel );
    progressControlsLayout->addWidget( currentLayerDisplay );
    progressControlsLayout->addStretch( );

    progressControlsContainer = new QWidget;
    progressControlsContainer->setContentsMargins( emptyMargins );
    progressControlsContainer->setLayout( progressControlsLayout );
    progressControlsContainer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    currentLayerImageLabel = new QLabel( "Current layer:" );
    currentLayerImageDisplay = new QLabel;
    currentLayerImageLabel->setBuddy( currentLayerImageDisplay );
    currentLayerImageDisplay->setAlignment( Qt::AlignCenter );
    currentLayerImageDisplay->setMaximumSize( 600, 400 );
    {
        auto pal = currentLayerImageDisplay->palette( );
        pal.setColor( QPalette::Background, Qt::black );
        currentLayerImageDisplay->setPalette( pal );
    }

    currentLayerImageLayout = new QVBoxLayout;
    currentLayerImageLayout->setContentsMargins( emptyMargins );
    currentLayerImageLayout->addWidget( currentLayerImageLabel );
    currentLayerImageLayout->addWidget( currentLayerImageDisplay );
    currentLayerImageLayout->addStretch( );

    currentLayerImageContainer = new QWidget;
    currentLayerImageContainer->setContentsMargins( emptyMargins );
    currentLayerImageContainer->setLayout( currentLayerImageLayout );
    currentLayerImageContainer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    currentLayerImageContainer->setMinimumSize( 600, 400 );

    stopButton = new QPushButton( "STOP" );
    {
        auto font { stopButton->font( ) };
        font.setPointSizeF( 22.25 );
        stopButton->setFont( font );
    }
    stopButton->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::MinimumExpanding );
    stopButton->setEnabled( false );
    QObject::connect( stopButton, &QPushButton::clicked, this, &Window::stopButton_clicked );

    statusTabLayout = new QGridLayout;
    statusTabLayout->setContentsMargins( emptyMargins );
    statusTabLayout->addWidget( progressControlsContainer,  0, 0, 1, 1 );
    statusTabLayout->addWidget( stopButton,                 1, 0, 1, 1 );
    statusTabLayout->addWidget( currentLayerImageContainer, 0, 1, 2, 1 );
    statusTabLayout->setRowStretch( 0, 4 );
    statusTabLayout->setRowStretch( 1, 1 );

    statusTab = new QWidget;
    statusTab->setContentsMargins( emptyMargins );
    statusTab->setLayout( statusTabLayout );
    statusTab->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    //
    // Tab widget
    //

    tabs = new QTabWidget;
    tabs->setContentsMargins( emptyMargins );
    tabs->addTab( selectTab,  "Select"  );
    tabs->addTab( prepareTab, "Prepare" );
    tabs->addTab( printTab,   "Print"   );
    tabs->addTab( statusTab,  "Status"  );
    tabs->setCurrentIndex( TabIndex::Select );

    setCentralWidget( tabs );

    shepherd = new Shepherd( parent );
    QObject::connect( shepherd, &Shepherd::shepherd_Started,              this, &Window::shepherd_Started              );
    QObject::connect( shepherd, &Shepherd::shepherd_Finished,             this, &Window::shepherd_Finished             );
    QObject::connect( shepherd, &Shepherd::shepherd_ProcessError,         this, &Window::shepherd_ProcessError         );
    QObject::connect( shepherd, &Shepherd::printer_Online,                this, &Window::printer_Online                );
    QObject::connect( shepherd, &Shepherd::printer_Offline,               this, &Window::printer_Offline               );
    shepherd->start( );
}

Window::~Window( ) {
    if ( g_signalHandler ) {
        QObject::disconnect( g_signalHandler, &SignalHandler::quit, this, &Window::signalHandler_quit );
    }
}

void Window::closeEvent( QCloseEvent* event ) {
    fprintf( stderr, "+ Window::closeEvent\n" );
    if ( printManager ) {
        printManager->terminate( );
    }
    shepherd->doTerminate( );
    event->accept( );
}

void Window::shepherd_Started( ) {
    fprintf( stderr, "+ Window::shepherd_Started\n" );
}

void Window::shepherd_Finished( int exitCode, QProcess::ExitStatus exitStatus ) {
    fprintf( stderr, "+ Window::shepherd_Finished: exitStatus %d, exitCode %d\n", exitStatus, exitCode );
}

void Window::shepherd_ProcessError( QProcess::ProcessError error ) {
    fprintf( stderr, "+ Window::shepherd_ProcessError: %d\n", error );
}

void Window::printer_Online( ) {
    fprintf( stderr, "+ Window::printer_Online\n" );
    isPrinterOnline = true;
    printerStateDisplay->setText( "Online" );
}

void Window::printer_Offline( ) {
    fprintf( stderr, "+ Window::printer_Offline\n" );
    isPrinterOnline = false;
    printerStateDisplay->setText( "Offline" );
}

void Window::selectTab_modelSelected( bool success, QString const& fileName ) {
    fprintf( stderr, "+ Window::selectTab_modelSelected: success: %s, fileName: '%s'\n", success ? "true" : "false", fileName.toUtf8( ).data( ) );
    if ( success ) {
        prepareTab->setSliceButtonEnabled( true );
        printButton->setEnabled( false );
        printJob->modelFileName = fileName;
        tabs->setCurrentIndex( TabIndex::Prepare );
    } else {
        prepareTab->setSliceButtonEnabled( true );
        printButton->setEnabled( false );
    }
}

void Window::prepareTab_sliceStarting( ) {
    fprintf( stderr, "+ Window::prepareTab_sliceStarting\n" );
}

void Window::prepareTab_sliceComplete( bool success ) {
    fprintf( stderr, "+ Window::prepareTab_sliceComplete: success: %s\n", success ? "true" : "false" );
}

void Window::prepareTab_renderStarting( ) {
    fprintf( stderr, "+ Window::prepareTab_renderStarting\n" );
}

void Window::prepareTab_renderComplete( bool success ) {
    fprintf( stderr, "+ Window::prepareTab_renderComplete: success: %s\n", success ? "true" : "false" );
    if ( !success ) {
        printButton->setEnabled( false );
        return;
    }

    printButton->setEnabled( true );
    tabs->setCurrentIndex( TabIndex::Print );
}

void Window::exposureTime_editingFinished( ) {
    bool valueOk = false;
    double value = exposureTime->validator( )->locale( ).toDouble( exposureTime->text( ), &valueOk );
    if ( valueOk ) {
        fprintf( stderr, "+ Window::exposureTime_editingFinished: new value %f\n", value );
        printJob->exposureTime = value;
    } else {
        fprintf( stderr, "+ Window::exposureTime_editingFinished: bad value\n" );
    }
}

void Window::exposureScaleFactorComboBox_currentIndexChanged( int index ) {
    fprintf( stderr, "+ Window::exposureScaleFactorComboBox_currentIndexChanged: new value: %d×\n", ExposureScaleFactorValues[index] );
    printJob->exposureTimeScaleFactor = ExposureScaleFactorValues[index];
}

void Window::powerLevelSlider_valueChanged( int value ) {
    int scaledValue = ( value / 100.0 * 255.0 ) + 0.5;
    printJob->powerLevel = scaledValue;
    powerLevelValue->setText( QString( "%1%" ).arg( value ) );
}

void Window::printButton_clicked( bool /*checked*/ ) {
    fprintf( stderr, "+ Window::printButton_clicked\n" );
    tabs->setCurrentIndex( TabIndex::Status );

    fprintf( stderr,
        "  + Print job:\n"
        "    + modelFileName:     '%s'\n"
        "    + slicedSvgFileName: '%s'\n"
        "    + pngFilesPath:      '%s'\n"
        "    + layerCount:        %d\n"
        "    + layerThickness:    %d\n"
        "    + exposureTime:      %f\n"
        "    + powerLevel:        %d\n"
        "",
        printJob->modelFileName.toUtf8( ).data( ),
        printJob->slicedSvgFileName.toUtf8( ).data( ),
        printJob->pngFilesPath.toUtf8( ).data( ),
        printJob->layerCount,
        printJob->layerThickness,
        printJob->exposureTime,
        printJob->powerLevel
    );

    PrintJob* newJob = new PrintJob;
    *newJob = *printJob;

    printManager = new PrintManager( shepherd, this );
    printManager->print( printJob );

    printJob = newJob;

    stopButton->setEnabled( true );
}

void Window::stopButton_clicked( bool /*checked*/ ) {
    printManager->abortJob( );
}

void Window::printManager_printStarting( ) {
    jobStateDisplay->setText( "Print started" );
}

void Window::printManager_printingLayer( int const layer ) {
    currentLayerDisplay->setText( QString( "%1" ).arg( layer ) );
    currentLayerImageDisplay->setPixmap( QPixmap( QString( "%1/%2.png" ).arg( printJob->pngFilesPath ).arg( layer, 6, 10, QChar( '0' ) ) ) );
}

void Window::printManager_lampStatusChange( bool const on ) {
    projectorLampStateDisplay->setText( on ? QString( "On" ) : QString( "Off" ) );
}

void Window::printManager_printComplete( bool const success ) {
    jobStateDisplay->setText( success ? "Print complete" : "Print failed" );
}

void Window::signalHandler_quit( int signalNumber ) {
    fprintf( stderr, "+ Window::signalHandler_quit: received signal %d\n", signalNumber );
    close( );
}
