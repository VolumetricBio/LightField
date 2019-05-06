#ifndef __PROCESSRUNNER_H__
#define __PROCESSRUNNER_H__

class ProcessRunner: public QObject {

    Q_OBJECT

public:

    ProcessRunner( QObject* parent = nullptr );
    virtual ~ProcessRunner( ) override;

    void start( QString const& program, QStringList const& arguments, QProcess::OpenMode const mode = QProcess::ReadWrite );

    int instanceId( ) const {
        return _instanceId;
    }

    QProcess::ProcessState state( ) const {
        return _process.state( );
    }

protected:

private:

    QProcess _process;
    int _instanceId;

signals:

    void succeeded( );
    void failed( int const exitCode, QProcess::ProcessError const error );
    void readyReadStandardOutput( QString const& data );
    void readyReadStandardError( QString const& data );

public slots:

    void kill( ) {
        _process.kill( );
    }

    void terminate( ) {
        _process.terminate( );
    }

    void setProcessChannelMode( QProcess::ProcessChannelMode const mode ) {
        _process.setProcessChannelMode( mode );
    }

protected slots:

private slots:

    void process_errorOccurred( QProcess::ProcessError error );
    void process_finished( int exitCode, QProcess::ExitStatus exitStatus );
    void process_readyReadStandardOutput( );
    void process_readyReadStandardError( );

};

#endif // __PROCESSRUNNER_H__
