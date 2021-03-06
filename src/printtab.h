#ifndef __PRINTTAB_H__
#define __PRINTTAB_H__

#include <QtCore>
#include <QtWidgets>
#include "tabbase.h"
#include "paramslider.h"
#include "constants.h"

enum class BuildPlatformState {
    Lowered,
    Raising,
    Raised,
    Lowering,
};

inline constexpr int operator+( BuildPlatformState const value ) { return static_cast<int>( value ); }

char const* ToString( BuildPlatformState const value );

class PrintTab: public InitialShowEventMixin<PrintTab, TabBase> {

    Q_OBJECT

public:

    PrintTab( QWidget* parent = nullptr );
    virtual ~PrintTab( ) override;

    bool             isPrintButtonEnabled( ) const          { return _printButton->isEnabled( ); }

    virtual TabIndex tabIndex( )             const override { return TabIndex::Print;            }

protected:

    virtual void _connectPrintJob( )                    override;
    virtual void _connectShepherd( )                    override;
    virtual void _initialShowEvent( QShowEvent* event ) override;

private:

    bool               _isPrinterOnline                    { false };
    bool               _isPrinterAvailable                 { true  };
    bool               _isPrinterPrepared                  { false };
    bool               _isModelRendered                    { false };
    BuildPlatformState _buildPlatformState                 { BuildPlatformState::Lowered };

    ParamSlider*       _powerLevelSlider                   { new ParamSlider( "Projector power level",
                                                                              "%",    50, ProjectorMaxPercent, 1, ProjectorMinPercent ) };

    QGroupBox*         _optionsGroup                       { new QGroupBox   };

    QPushButton*       _printButton                        { new QPushButton };

    QPushButton*       _raiseOrLowerButton                 { new QPushButton };
    QPushButton*       _homeButton                         { new QPushButton };

    ParamSlider*        _baseExposureTimeSlider            { new ParamSlider( "Base layers exposure time factor",
                                                                              "x",    2, 5, 1, 1 ) };

    ParamSlider*        _bodyExposureTimeSlider            { new ParamSlider( "Body layers exposure time",
                                                                              "s",    1000, 30000, 250, 250, 1000 ) };
    QLabel*            _expoDisabledTilingWarning          { new QLabel("<font color='red'>Exposure controls disabled by tiling.</font>") };
    QLabel*            _expoDisabledAdvancedWarning        { new QLabel("<font color='red'>Exposure controls disabled by advanced settings.</font>") };
    QGroupBox*         _adjustmentsGroup                   { new QGroupBox   };

    QGridLayout*       _layout                             { new QGridLayout };

    void _updateUiState( );

signals:
    void advancedControlsChanged(bool enabled);
    void printerAvailabilityChanged( bool const available );
    void printRequested( );

    void projectorPowerLevelChanged( int const value );

public slots:

    virtual void tab_uiStateChanged( TabIndex const sender, UiState const state ) override;

    void setModelRendered( bool const value );
    void setPrinterPrepared( bool const value );
    void setPrinterAvailable( bool const value );
    void projectorPowerLevel_changed( int const percentage );
    void changeExpoTimeSliders();

protected slots:

private slots:

    void printer_online( );
    void printer_offline( );

    void raiseBuildPlatform_moveAbsoluteComplete( bool const success );
    void lowerBuildPlatform_moveAbsoluteComplete( bool const success );
    void home_homeComplete( bool const success );

    void powerLevelSlider_valueChanged( );

    void printButton_clicked( bool );
    void raiseOrLowerButton_clicked( bool );
    void homeButton_clicked( bool );

    void exposureTime_update( );

};

#endif // __PRINTTAB_H__
