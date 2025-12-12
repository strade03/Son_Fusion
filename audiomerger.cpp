#include "audiomerger.h"
#include <QRegularExpression>
#include <QDebug>
#include <QTime>
#include <QLocale>
#include <QCoreApplication>
#include <QFile>

// ============================================================================
// CONFIGURATION MULTIPLATEFORME FFMPEG
// ============================================================================

/**
 * @brief Retourne le chemin de l'exécutable FFmpeg selon la plateforme
 */
QString AudioMerger::getFFmpegPath()
{
    QString ffmpegPath;
    
#ifdef Q_OS_WIN
    ffmpegPath = QCoreApplication::applicationDirPath() + "/ffmpeg.exe";
    if (!QFile::exists(ffmpegPath)) {
        ffmpegPath = "ffmpeg.exe";
    }
#elif defined(Q_OS_MAC)
    ffmpegPath = QCoreApplication::applicationDirPath() + "/ffmpeg";
    if (!QFile::exists(ffmpegPath)) {
        ffmpegPath = "ffmpeg";
    }
#elif defined(Q_OS_LINUX)
    ffmpegPath = "ffmpeg";
#else
    ffmpegPath = "ffmpeg";
#endif

    return ffmpegPath;
}

/**
 * @brief Retourne le chemin de l'exécutable FFprobe selon la plateforme
 */
QString AudioMerger::getFFprobePath()
{
    QString ffprobePath;
    
#ifdef Q_OS_WIN
    ffprobePath = QCoreApplication::applicationDirPath() + "/ffprobe.exe";
    if (!QFile::exists(ffprobePath)) {
        ffprobePath = "ffprobe.exe";
    }
#elif defined(Q_OS_MAC)
    ffprobePath = QCoreApplication::applicationDirPath() + "/ffprobe";
    if (!QFile::exists(ffprobePath)) {
        ffprobePath = "ffprobe";
    }
#elif defined(Q_OS_LINUX)
    ffprobePath = "ffprobe";
#else
    ffprobePath = "ffprobe";
#endif

    return ffprobePath;
}

// ============================================================================
// CODE EXISTANT AVEC MODIFICATIONS
// ============================================================================

AudioMerger::AudioMerger(QObject* parent) : QObject(parent), totalDurationInSeconds(0) {
    ffmpegProcess = new QProcess(this);
    
    ffmpegProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(ffmpegProcess, &QProcess::finished, this, &AudioMerger::processFinished);
    connect(ffmpegProcess, &QProcess::errorOccurred, this, &AudioMerger::processError);

    connect(ffmpegProcess, &QProcess::readyReadStandardOutput, [this]() {
        QByteArray data = ffmpegProcess->readAllStandardOutput();
        QString output = QString::fromLatin1(data);

        static QRegularExpression timeRegex("time=(\\d{2}:\\d{2}:\\d{2})\\.\\d+");
        QRegularExpressionMatch match = timeRegex.match(output);

        if (match.hasMatch()) {
            QString currentTimeStr = match.captured(1); 

            if (totalDurationInSeconds > 0.1) {
                QString totalTimeStr = formatTime(totalDurationInSeconds);
                emit statusMessage(QString("Traitement : %1 / %2").arg(currentTimeStr).arg(totalTimeStr));
            } else {
                emit statusMessage(QString("Traitement : %1").arg(currentTimeStr));
            }
        }
    });
}

QString AudioMerger::formatTime(double seconds) {
    int totalSecs = static_cast<int>(seconds);
    int h = totalSecs / 3600;
    int m = (totalSecs % 3600) / 60;
    int s = totalSecs % 60;
    
    return QString("%1:%2:%3")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
}

double AudioMerger::getFileDuration(const QString &file) {
    QProcess probe;
    
    // MODIFICATION : Utiliser getFFprobePath()
    QString ffprobePath = getFFprobePath();
    
    QStringList args;
    args << "-v" << "error" << "-show_entries" << "format=duration" 
         << "-of" << "csv=p=0" << file;
    
    probe.start(ffprobePath, args);
    if (!probe.waitForFinished(3000)) return 0.0;
    
    QString result = probe.readAllStandardOutput().trimmed();
    
    bool ok;
    double val = QLocale::c().toDouble(result, &ok);
    if (!ok) {
        val = QLocale::system().toDouble(result, &ok);
    }
    
    if (ok) return val;
    return 0.0;
}

bool AudioMerger::checkFFmpeg() {
    QProcess process;
    
    // MODIFICATION : Utiliser getFFmpegPath()
    QString ffmpegPath = getFFmpegPath();
    
    process.start(ffmpegPath, QStringList() << "-version");
    process.waitForFinished();
    return (process.exitCode() == 0);
}

void AudioMerger::mergeFiles(const QStringList& inputFiles, const QString& outputFile) {
    if (inputFiles.isEmpty()) {
        emit error("Aucun fichier en entrée.");
        return;
    }

    // Calcul de la durée totale
    totalDurationInSeconds = 0;
    for (const QString& file : inputFiles) {
        totalDurationInSeconds += getFileDuration(file);
    }

    // MODIFICATION : Utiliser getFFmpegPath()
    QString ffmpegPath = getFFmpegPath();
    
    QStringList arguments;

    if (inputFiles.size() == 1) {
        arguments << "-i" << inputFiles.first();
        arguments << "-y" << outputFile;
    } else {
        for (const QString& file : inputFiles) arguments << "-i" << file;
        QString filterComplex = "";
        for (int i = 0; i < inputFiles.size(); i++) filterComplex += "[" + QString::number(i) + ":a]";
        filterComplex += "concat=n=" + QString::number(inputFiles.size()) + ":v=0:a=1[out]";
        arguments << "-filter_complex" << filterComplex;
        arguments << "-map" << "[out]";
        arguments << "-y" << outputFile;
    }

    ffmpegProcess->start(ffmpegPath, arguments);
    ffmpegProcess->closeWriteChannel();

    emit started();
}

void AudioMerger::processFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    emit statusMessage("Finalisation..."); 
    emit finished(exitCode == 0 && exitStatus == QProcess::NormalExit);
}

void AudioMerger::processError(QProcess::ProcessError error) {
    emit this->error("Erreur FFmpeg code: " + QString::number(error));
}