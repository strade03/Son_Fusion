#include "audiomerger.h"
#include <QRegularExpression>
#include <QDebug>
#include <QTime>
#include <QLocale>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>
#include <QTimer>
#include <QEventLoop>
#include <QDataStream>

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
    QFileInfo fi(file);
    if (!fi.exists()) return 0.0;

    // OPTIMISATION : Si c'est un WAV, on lit l'en-tête directement (très rapide)
    if (fi.suffix().compare("wav", Qt::CaseInsensitive) == 0) {
        return getWavDuration(file);
    }

    // POUR LES AUTRES FORMATS (MP3, M4A, ETC.) : On utilise QMediaPlayer
    // Note : QMediaPlayer est asynchrone, nous devons utiliser une boucle d'événement locale
    // pour rendre la fonction synchrone (bloquante) comme l'était votre appel ffprobe.

    QMediaPlayer player;
    QAudioOutput audioOutput;
    player.setAudioOutput(&audioOutput);
    
    QEventLoop loop;
    
    // On quitte la boucle quand les métadonnées sont chargées
    connect(&player, &QMediaPlayer::mediaStatusChanged, [&](QMediaPlayer::MediaStatus status){
        if (status == QMediaPlayer::BufferedMedia || status == QMediaPlayer::LoadedMedia) {
            loop.quit();
        }
    });

    // On quitte aussi en cas d'erreur
    connect(&player, &QMediaPlayer::errorOccurred, [&](QMediaPlayer::Error error, const QString &errorString){
        Q_UNUSED(error);
        Q_UNUSED(errorString);
        loop.quit();
    });

    // Sécurité : Timeout de 2 secondes max (au cas où le fichier est corrompu)
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);

    player.setSource(QUrl::fromLocalFile(file));
    
    // Si le fichier n'est pas instantanément chargé, on attend
    if (player.mediaStatus() != QMediaPlayer::BufferedMedia && 
        player.mediaStatus() != QMediaPlayer::LoadedMedia) {
        loop.exec();
    }

    qint64 durationMs = player.duration();
    
    if (durationMs > 0) {
        return durationMs / 1000.0;
    }

    return 0.0;
}

// ----------------------------------------------------------------------------
// Lecture rapide header WAV
// ----------------------------------------------------------------------------
double AudioMerger::getWavDuration(const QString &file) {
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return 0.0;

    QDataStream in(&f);
    in.setByteOrder(QDataStream::LittleEndian);

    char riff[4];
    in.readRawData(riff, 4); // "RIFF"
    if (strncmp(riff, "RIFF", 4) != 0) return 0.0;

    in.skipRawData(4); // File size

    char wave[4];
    in.readRawData(wave, 4); // "WAVE"
    if (strncmp(wave, "WAVE", 4) != 0) return 0.0;

    // On parcourt les chunks jusqu'à trouver "fmt " et "data"
    quint32 sampleRate = 0;
    quint16 bitsPerSample = 0;
    quint16 channels = 0;
    quint32 dataSize = 0;
    bool fmtFound = false;
    bool dataFound = false;

    while (!in.atEnd() && (!fmtFound || !dataFound)) {
        char chunkId[4];
        if (in.readRawData(chunkId, 4) != 4) break;
        
        quint32 chunkSize;
        in >> chunkSize;

        if (strncmp(chunkId, "fmt ", 4) == 0) {
            quint16 audioFormat;
            in >> audioFormat;
            in >> channels;
            in >> sampleRate;
            in.skipRawData(6); // ByteRate (4) + BlockAlign (2)
            in >> bitsPerSample;
            
            // Si le chunk fmt est plus grand que 16 octets (ex: extension header), on saute le reste
            if (chunkSize > 16) in.skipRawData(chunkSize - 16);
            
            fmtFound = true;
        } 
        else if (strncmp(chunkId, "data", 4) == 0) {
            dataSize = chunkSize;
            dataFound = true;
            // Pas besoin de lire les données, on a la taille
            break; 
        } 
        else {
            // Chunk inconnu (ex: LIST, ID3, JUNK), on saute
            in.skipRawData(chunkSize);
        }
    }

    f.close();

    if (fmtFound && dataFound && sampleRate > 0 && channels > 0 && bitsPerSample > 0) {
        // Formule : TailleData / (TauxEchantillonnage * Canaux * OctetsParEchantillon)
        double bytesPerSample = bitsPerSample / 8.0;
        return (double)dataSize / (sampleRate * channels * bytesPerSample);
    }
    
    // Si échec lecture header, fallback sur méthode lente (rare)
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