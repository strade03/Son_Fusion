#ifndef AUDIOMERGER_H
#define AUDIOMERGER_H

#include <QObject>
#include <QProcess>
#include <QStringList>

class AudioMerger : public QObject {
    Q_OBJECT
public:
    AudioMerger(QObject* parent = nullptr);
    bool checkFFmpeg();
    void mergeFiles(const QStringList& inputFiles, const QString& outputFile);

signals:
    void started();
    void finished(bool success);
    void error(const QString& message);
    void statusMessage(const QString& message); 

private slots:
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processError(QProcess::ProcessError error);

private:
    QProcess* ffmpegProcess;
    double totalDurationInSeconds;
    
    // NOUVEAUX : Méthodes multiplateforme
    QString getFFmpegPath();
    
    
    double getFileDuration(const QString &file);
    double getWavDuration(const QString &file);
    QString formatTime(double seconds);
};

#endif // AUDIOMERGER_H