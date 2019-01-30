#ifndef WINDOW_H
#define WINDOW_H

#include <QCollator>
//#include <QFileSystemWatcher>
#include <QProcess>
#include <QMainWindow>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QPushButton>

class Canvas;
class Loader;
class Shepherd;

class Window : public QMainWindow
{
    Q_OBJECT

public:
    explicit Window(QWidget* parent=0);

    bool load_stl(const QString& filename, bool is_reload=false);

protected:
    void closeEvent(QCloseEvent *event);

private:
    void sorted_insert(QStringList& list, const QCollator& collator, const QString& value);
    void build_folder_file_list();
    void setFullScreen(bool const fullScreen = true);

    QString current_file;
    QString lookup_folder;
    QStringList lookup_folder_files;

    //QFileSystemWatcher* watcher;
    Canvas* canvas;
    Shepherd* shepherd;

    Loader* loader = nullptr;

    QPushButton* move_up_button;
    QPushButton* move_down_button;
    QWidget* containerWidget;
    QVBoxLayout* containerVBox;
    QHBoxLayout* buttonHBox;
    QGroupBox* buttonGroupBox;

signals:

public slots:

private slots:

    void shepherd_Started( );
    void shepherd_Finished( );
    void shepherd_ProcessError( );

    void printer_Online( );
    void printer_Offline( );
    void printer_Position( float position );
    void printer_Temperature( char const* temperatureInfo );

    void printProcess_ShowImage( char const* fileName, char const* brightness, char const* index, char const* total );
    void printProcess_HideImage( );
    void printProcess_StartedPrinting( );
    void printProcess_FinishedPrinting( );

    void on_bad_stl();
    void on_empty_mesh();
    void on_confusing_stl();
    void on_missing_file();

    void on_loaded(const QString& filename);
    void on_move_up();
    void on_move_down();
};

#endif // WINDOW_H
