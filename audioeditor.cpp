#include "audioeditor.h"
#include "ui_audioeditor.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QTime>
#include <QCoreApplication>
#include <cmath>
#include <cstring>
//#include <QInputDialog>

#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QThread> 

  
// ============================================================================
// CONFIGURATION MULTIPLATEFORME FFMPEG
// ============================================================================

/**
 * @brief Retourne le chemin de l'exécutable FFmpeg selon la plateforme
 * @return QString contenant le chemin de FFmpeg
 */
QString AudioEditor::getFFmpegPath()
{
    QString ffmpegPath;
    
#ifdef Q_OS_WIN
    // Windows : chercher d'abord dans le dossier de l'application
    ffmpegPath = QCoreApplication::applicationDirPath() + "/ffmpeg.exe";
    
    // Si pas trouvé localement, utiliser le PATH système
    if (!QFile::exists(ffmpegPath)) {
        ffmpegPath = "ffmpeg.exe";
    }
    
#elif defined(Q_OS_MAC)
    // macOS : chercher dans le bundle d'abord
    ffmpegPath = QCoreApplication::applicationDirPath() + "/ffmpeg";
    
    // Si pas trouvé dans le bundle, utiliser le PATH système (Homebrew)
    if (!QFile::exists(ffmpegPath)) {
        ffmpegPath = "ffmpeg";
    }
    
#elif defined(Q_OS_LINUX)
    // Linux : utiliser le PATH système (généralement /usr/bin/ffmpeg)
    ffmpegPath = "ffmpeg";
    
#else
    // Plateforme non supportée
    ffmpegPath = "ffmpeg";
#endif

    return ffmpegPath;
}

/**
 * @brief Vérifie si FFmpeg est disponible et fonctionnel
 * @return true si FFmpeg est trouvé et opérationnel
 */
bool AudioEditor::checkFFmpegAvailability()
{
    QProcess testProcess;
    QString ffmpegPath = getFFmpegPath();
    
    testProcess.start(ffmpegPath, QStringList() << "-version");
    testProcess.waitForFinished(3000);
    
    if (testProcess.exitCode() != 0 || testProcess.exitStatus() != QProcess::NormalExit) {
        QMessageBox::warning(this, tr("FFmpeg non trouvé"),
            tr("FFmpeg n'est pas installé ou introuvable.\n\n"
#ifdef Q_OS_WIN
               "Veuillez placer ffmpeg.exe dans le dossier de l'application,\n"
               "ou l'installer sur votre système."
#elif defined(Q_OS_MAC)
               "Veuillez installer FFmpeg via Homebrew :\n"
               "brew install ffmpeg"
#elif defined(Q_OS_LINUX)
               "Veuillez installer FFmpeg via votre gestionnaire de paquets :\n"
               "Ubuntu/Debian: sudo apt install ffmpeg\n"
               "Fedora: sudo dnf install ffmpeg"
#endif
            ));
        return false;
    }
    
    return true;
}

// ============================================================================
// CODE EXISTANT AVEC MODIFICATIONS
// ============================================================================

AudioEditor::AudioEditor(QWidget *parent)
    : AUDIOEDITOR_BASE(parent)
    , modeAutonome(true)
{
    init();
}

AudioEditor::AudioEditor(const QString &filePath, const QString &workingDir, QWidget *parent, bool isTempRecording)
    : AUDIOEDITOR_BASE(parent)
    , modeAutonome(false)
    , currentAudioFile(filePath)
    , isTempRecording(isTempRecording)
    , workingDirectory(workingDir) // <--- On mémorise le dossier
{
    init();
    
    if (isTempRecording) {
        isModified = true;
    }
}

AudioEditor::~AudioEditor()
{
    releaseResources();
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir dir(tempDir);
    dir.setNameFilters({"temp_playback_*", "temp_modified*", "temp.raw", "temp_save.raw"});
    for (const QString &f : dir.entryList(QDir::Files))
        dir.remove(f);
    delete ui;
}

void AudioEditor::showEvent(QShowEvent *event)
{
    AUDIOEDITOR_BASE::showEvent(event);
    if (!modeAutonome && !fichierCharge) {
        fichierCharge = true;
        openFile();
    }
}

void AudioEditor::init()
{
    ui = new Ui::AudioEditorWidget;
    ui->setupUi(this);
    isModified = false;

    player = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    decoder = new QAudioDecoder(this);
    totalSamples = 0;

    waveformWidget = ui->waveformWidget;
    waveformWidget->setisLoaded(false);

    setButtonsEnabled(false);
    audioOutput->setVolume(0.5);
    player->setAudioOutput(audioOutput);

    connect(ui->btnOpen,      &QPushButton::clicked, this, &AudioEditor::openFile);
    connect(ui->btnPlay,      &QPushButton::clicked, this, &AudioEditor::playPause);
    connect(ui->btnReset,     &QPushButton::clicked, this, &AudioEditor::resetPosition);
    connect(ui->btnStop,      &QPushButton::clicked, this, &AudioEditor::stopPlayback);
    connect(ui->btnCut,       &QPushButton::clicked, this, &AudioEditor::cutSelection);
    connect(ui->btnSave,      &QPushButton::clicked, this, &AudioEditor::saveModifiedAudio);
    connect(ui->btnNormalizeAll, &QPushButton::clicked, this, &AudioEditor::normalizeSelection);
    connect(ui->btnNormalize,    &QPushButton::clicked, this, &AudioEditor::normalizeSelection);
    connect(ui->btnZoomIn,       &QPushButton::clicked, waveformWidget, &WaveformWidget::zoomIn);
    connect(ui->btnZoomOut,      &QPushButton::clicked, waveformWidget, &WaveformWidget::zoomOut);

    connect(player, &QMediaPlayer::positionChanged,    this, &AudioEditor::updatePlayhead);
    connect(player, &QMediaPlayer::mediaStatusChanged, this, &AudioEditor::handleMediaStatusChanged);
    connect(player, &QMediaPlayer::durationChanged,    this, [this](qint64 d){
        ui->lengthLabel->setText(QTime::fromMSecsSinceStartOfDay(d).toString("hh:mm:ss"));
    });

    connect(decoder, &QAudioDecoder::bufferReady, this, [this]() {
        QAudioBuffer buf = decoder->read();
        processBuffer(buf);
    });
    connect(decoder, &QAudioDecoder::finished, this, &AudioEditor::decodingFinished);

    connect(waveformWidget, &WaveformWidget::zoomChanged,      this, &AudioEditor::handleZoomChanged);
    connect(waveformWidget, &WaveformWidget::selectionChanged, this, &AudioEditor::handleSelectionChanged);
    connect(waveformWidget, &WaveformWidget::playbackFinished, this, &AudioEditor::stopPlayback);

    if (!modeAutonome)
        ui->btnOpen->setVisible(false);
}

void AudioEditor::setButtonsEnabled(bool e)
{
    ui->btnPlay->setEnabled(e);
    ui->btnReset->setEnabled(e);
    ui->btnNormalizeAll->setEnabled(e);
    ui->btnStop->setEnabled(false);
    ui->btnCut->setEnabled(false);
    ui->btnNormalize->setEnabled(false);
    ui->btnSave->setEnabled(e);
    ui->btnZoomIn->setEnabled(e);
    ui->btnZoomOut->setEnabled(e);
    ui->selectionLabel->clear();
}

void AudioEditor::updatePositionLabel(qint64 pos)
{
    ui->positionLabel->setText(QTime(0,0,0).addMSecs(pos).toString("hh:mm:ss"));
}

QString AudioEditor::timeToPosition(qint64 sampleIdx, const QString &fmt)
{
    qint64 dur = player->duration();
    if (dur <= 0 || totalSamples <= 0) return "00:00:00";
    qint64 ms = static_cast<qint64>((double)sampleIdx / totalSamples * dur);
    return QTime(0,0,0).addMSecs(ms).toString(fmt);
}

void AudioEditor::openFile()
{
    if (decoder) {
        decoder->stop();
        delete decoder;
        decoder = new QAudioDecoder(this);
        connect(decoder, &QAudioDecoder::bufferReady, this, [this]() {
            QAudioBuffer buf = decoder->read(); processBuffer(buf);
        });
        connect(decoder, &QAudioDecoder::finished, this, &AudioEditor::decodingFinished);
    }
    if (modeAutonome) {
        currentAudioFile = QFileDialog::getOpenFileName(this,
            tr("Ouvrir un fichier audio"), {}, tr("Audio (*.wav *.mp3 *.flac)"));
    }
    if (currentAudioFile.isEmpty()) return;

    // player->setSource(QUrl::fromLocalFile(currentAudioFile));
    audioSamples.clear(); 
    totalSamples = 0;
    waveformWidget->setLoading(true);
    QCoreApplication::processEvents();
    if (currentAudioFile.endsWith(".wav", Qt::CaseInsensitive))
        extractWaveform(currentAudioFile);
    else
        extractWaveformWithFFmpeg(currentAudioFile);
    
    setWindowTitle(tr("Éditeur Audio - ") + QFileInfo(currentAudioFile).fileName());
    
    setButtonsEnabled(true);
    waveformWidget->resetSelection(-1);
    waveformWidget->setPlayheadPosition(0);
    waveformWidget->setisLoaded(true);
    player->setSource(QUrl::fromLocalFile(currentAudioFile));
}

void AudioEditor::extractWaveform(const QString &path)
{
    decoder->setSource(QUrl::fromLocalFile(path));
    decoder->start();
}

// Ancienne méthode qui ne gere pas le mono/stéréo
// void AudioEditor::extractWaveformWithFFmpeg(const QString &path)
// {
//     // Vérifier la disponibilité de FFmpeg
//     if (!checkFFmpegAvailability()) {
//         QApplication::restoreOverrideCursor();
//         return;
//     }
    
//     QApplication::setOverrideCursor(Qt::WaitCursor);
//     QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/temp.raw";
    
//     // MODIFICATION : Utiliser getFFmpegPath() au lieu de "ffmpeg" en dur
//     QString ffmpegPath = getFFmpegPath();
    
//     QStringList args{"-i", path, "-ac","1","-ar","44100","-f","f32le", tmp};
//     QProcess ff;
//     ff.start(ffmpegPath, args);
    
//     if (!ff.waitForFinished(15000) || ff.exitStatus() != QProcess::NormalExit) {
//         QMessageBox::warning(this, tr("Erreur FFmpeg"),
//                              tr("La commande FFmpeg a échoué.\n%1").arg(QString(ff.readAllStandardError())));
//         QApplication::restoreOverrideCursor();
//         return;
//     }
//     QFile f(tmp);
//     if (!f.open(QIODevice::ReadOnly)) {
//         QMessageBox::warning(this, tr("Erreur"), tr("Impossible d'ouvrir le fichier raw généré."));
//         QApplication::restoreOverrideCursor();
//         return;
//     }
//     QByteArray d = f.readAll(); f.close(); QFile::remove(tmp);
//     int count = static_cast<int>(d.size() / sizeof(float));
//     if (count <= 0) {
//         QMessageBox::warning(this, tr("Erreur"), tr("Aucun échantillon n'a été extrait."));
//         QApplication::restoreOverrideCursor();
//         return;
//     }
//     audioSamples.resize(count);
//     std::memcpy(audioSamples.data(), d.constData(), count * sizeof(float));
//     totalSamples = count;
//     waveformWidget->setFullWaveform(audioSamples);
//     QApplication::restoreOverrideCursor();
// }


int AudioEditor::getChannelCount(const QString &filePath)
{
    QProcess process;
    QString ffmpegPath = getFFmpegPath();
    
    // On lance juste "ffmpeg -i fichier"
    // FFmpeg va afficher les infos et s'arrêter car il manque le fichier de sortie
    QStringList args;
    args << "-i" << filePath; 

    process.start(ffmpegPath, args);
    process.waitForFinished(3000); 

    // FFmpeg écrit les infos techniques dans le canal d'Erreur (StandardError), pas Output !
    QString output = process.readAllStandardError(); 

    // On analyse le texte pour trouver "stereo" ou "mono"
    // Ex de sortie : "Stream #0:0: Audio: mp3, 44100 Hz, stereo, fltp, 128 kb/s"
    
    if (output.contains(" stereo,", Qt::CaseInsensitive)) {
        return 2;
    }
    
    if (output.contains(" mono,", Qt::CaseInsensitive)) {
        return 1;
    }

    // Sécurité pour les formats bizarres ("1 channels")
    if (output.contains("1 channels", Qt::CaseInsensitive)) return 1;
    if (output.contains("2 channels", Qt::CaseInsensitive)) return 2;

    // Par défaut, si on ne sait pas, on dit 2 (Stéréo) 
    // pour déclencher la formule de mixage sécurisée (0.5+0.5) et éviter la saturation.
    return 2; 
}

// Extract ok mais pb gestion de la memoire avecc gros fichier
// void AudioEditor::extractWaveformWithFFmpeg(const QString &path)
// {
//     if (!checkFFmpegAvailability()) {
//         QApplication::restoreOverrideCursor();
//         return;
//     }
    
//     // =========================================================
//     // CORRECTION ICI : ON LANCE L'AFFICHAGE "CHARGEMENT" MAINTENANT
//     // =========================================================
//     waveformWidget->setLoading(true);
//     QApplication::setOverrideCursor(Qt::WaitCursor);
    
//     // TRÈS IMPORTANT : Force Qt à dessiner le widget TOUT DE SUITE
//     // Sinon, le programme gèle sur l'ancien affichage pendant le waitForFinished
//     QCoreApplication::processEvents(); 
//     // =========================================================

//     QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/temp.raw";
//     QString ffmpegPath = getFFmpegPath();
    
//     // getChannelCount prend un peu de temps, c'est bien de l'avoir après le setLoading
//     int channels = getChannelCount(path);
    
//     QStringList args;
//     args << "-i" << path;
//     if (channels > 1) {
//         // Stéréo -> Moyenne pour éviter saturation
//         args << "-af" << "pan=mono|c0=0.5*c0+0.5*c1";
//     } else {
//         // Mono -> Copie
//         args << "-ac" << "1"; 
//     }
    
//     args << "-ar" << "44100"
//          << "-f" << "f32le"
//          << tmp;

//     QProcess ff;
//     ff.start(ffmpegPath, args);
    
//     // Le programme va attendre ici (interface bloquée), mais comme on a fait
//     // processEvents() juste avant, le texte "Chargement..." restera affiché.
//     if (!ff.waitForFinished(15000) || ff.exitStatus() != QProcess::NormalExit) {
//         // En cas d'erreur, on enlève le message de chargement
//         waveformWidget->setLoading(false); 
//         QMessageBox::warning(this, tr("Erreur FFmpeg"),
//                              tr("La commande FFmpeg a échoué.\n%1").arg(QString(ff.readAllStandardError())));
//         QApplication::restoreOverrideCursor();
//         return;
//     }
    
//     QFile f(tmp);
//     if (!f.open(QIODevice::ReadOnly)) {
//         waveformWidget->setLoading(false); // Pensez à désactiver le loading en cas d'erreur
//         QMessageBox::warning(this, tr("Erreur"), tr("Impossible d'ouvrir le fichier raw généré."));
//         QApplication::restoreOverrideCursor();
//         return;
//     }
    
//     QByteArray d = f.readAll(); 
//     f.close(); 
//     QFile::remove(tmp);
    
//     int count = static_cast<int>(d.size() / sizeof(float));
//     if (count <= 0) {
//         waveformWidget->setLoading(false); // Idem
//         QMessageBox::warning(this, tr("Erreur"), tr("Aucun échantillon n'a été extrait."));
//         QApplication::restoreOverrideCursor();
//         return;
//     }
    
//     audioSamples.resize(count);
//     std::memcpy(audioSamples.data(), d.constData(), count * sizeof(float));
//     totalSamples = count;
    
//     // On désactive le loading, setFullWaveform va repeindre l'onde
//     waveformWidget->setLoading(false); 
//     waveformWidget->setFullWaveform(audioSamples);
    
//     QApplication::restoreOverrideCursor();
// }

void AudioEditor::extractWaveformWithFFmpeg(const QString &path)
{
    if (!checkFFmpegAvailability()) {
        QApplication::restoreOverrideCursor();
        return;
    }
    
    // 1. Activation de l'interface de chargement
    waveformWidget->setLoading(true);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QCoreApplication::processEvents(); 

    QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/temp.raw";
    QString ffmpegPath = getFFmpegPath();
    
    int channels = getChannelCount(path);
    
    QStringList args;
    args << "-y" << "-i" << path; // Ajout de -y pour forcer l'écrasement si le temp existe
    if (channels > 1) {
        args << "-af" << "pan=mono|c0=0.5*c0+0.5*c1";
    } else {
        args << "-ac" << "1"; 
    }
    
    args << "-ar" << "44100"
         << "-f" << "f32le"
         << tmp;

    QProcess ff;
    ff.start(ffmpegPath, args);
    
    // 2. CORRECTION TIMEOUT : 15s est trop court pour 264Mo. 
    // On passe à 300000ms (5 minutes) ou -1 (infini) pour être sûr.
    if (!ff.waitForFinished(300000) || ff.exitStatus() != QProcess::NormalExit) {
        waveformWidget->setLoading(false); 
        QMessageBox::warning(this, tr("Erreur FFmpeg"),
                             tr("La commande FFmpeg a échoué ou a pris trop de temps.\n%1").arg(QString(ff.readAllStandardError())));
        QApplication::restoreOverrideCursor();
        return;
    }
    
    QFile f(tmp);
    if (!f.open(QIODevice::ReadOnly)) {
        waveformWidget->setLoading(false);
        QMessageBox::warning(this, tr("Erreur"), tr("Impossible d'ouvrir le fichier raw généré."));
        QApplication::restoreOverrideCursor();
        return;
    }
    
    // 3. CORRECTION MÉMOIRE (La partie critique)
    qint64 fileSize = f.size();
    qint64 samplesCount = fileSize / sizeof(float);

    if (samplesCount <= 0) {
        waveformWidget->setLoading(false);
        QMessageBox::warning(this, tr("Erreur"), tr("Aucun échantillon n'a été extrait."));
        f.close();
        QApplication::restoreOverrideCursor();
        return;
    }
    
    // Au lieu de tout lire dans un QByteArray (double RAM),
    // on redimensionne d'abord le vecteur cible...
    try {
        audioSamples.resize(samplesCount);
    } catch (const std::bad_alloc&) {
        waveformWidget->setLoading(false);
        f.close();
        QFile::remove(tmp);
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, tr("Erreur Mémoire"), 
            tr("Fichier trop volumineux pour la mémoire disponible (RAM insuffisante)."));
        return;
    }

    // ... et on lit directement du disque vers le vecteur
    // On lit par blocs pour ne pas geler l'OS si le disque est lent, 
    // bien que read() direct soit possible.
    char* ptr = reinterpret_cast<char*>(audioSamples.data());
    qint64 bytesToRead = fileSize;
    while (bytesToRead > 0) {
        qint64 chunk = qMin(bytesToRead, qint64(1024 * 1024 * 64)); // Blocs de 64Mo
        qint64 read = f.read(ptr, chunk);
        if (read <= 0) break;
        ptr += read;
        bytesToRead -= read;
    }

    f.close(); 
    QFile::remove(tmp);
    
    totalSamples = audioSamples.size();
    
    waveformWidget->setLoading(false); 
    waveformWidget->setFullWaveform(audioSamples);
    
    QApplication::restoreOverrideCursor();
}

void AudioEditor::processBuffer(const QAudioBuffer &buf)
{
    int frames = buf.frameCount(); if (frames <= 0) return;
    auto fmt = buf.format(); int ch = fmt.channelCount(), bps = fmt.bytesPerSample();
    const char *raw = buf.constData<char>();
    for (int i = 0; i < frames; ++i) {
        double s = 0;
        if (bps == 2) {
            auto *d = reinterpret_cast<const qint16*>(raw);
            for (int c = 0; c < ch; ++c)
                s += d[i*ch + c] / 32768.0;
        } else {
            auto *d = reinterpret_cast<const qint32*>(raw);
            for (int c = 0; c < ch; ++c)
                s += d[i*ch + c] / 2147483648.0;
        }
        audioSamples.append(s / ch);
    }
    totalSamples += frames;
}

void AudioEditor::decodingFinished()
{
    if (audioSamples.isEmpty()) {
        QMessageBox::warning(this, tr("Erreur"), tr("Aucun échantillon lu."));
        return;
    }
    totalSamples = audioSamples.size();
    waveformWidget->setLoading(false);
    waveformWidget->setFullWaveform(audioSamples);
}

std::pair<int,int> AudioEditor::getSelectionSampleRange()
{
    int s = waveformWidget->getSelectionStart();
    int e = waveformWidget->getSelectionEnd();
    return { qBound(0, s, totalSamples), qBound(0, e, totalSamples) };
}

void AudioEditor::cutSelection()
{
    if (!waveformWidget->hasSelection()) {
        QMessageBox::warning(this, tr("Erreur"), tr("Sélection invalide."));
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    double oldZoom = waveformWidget->getSamplesPerPixel();
    int oldScroll = waveformWidget->getScrollOffset();

    int safeSize = audioSamples.size();
    auto range = getSelectionSampleRange();
    
    int s = std::clamp(range.first, 0, safeSize);
    int e = std::clamp(range.second, 0, safeSize);

    if (s >= e) return;

    player->stop();

    int countToRemove = e - s;
    
    if (s + countToRemove > safeSize) {
        countToRemove = safeSize - s;
    }

    if (countToRemove > 0) {
        audioSamples.remove(s, countToRemove);
        isModified = true;
    }
    
    totalSamples = audioSamples.size();

    waveformWidget->setFullWaveform(audioSamples);
    waveformWidget->restoreZoomState(oldZoom, oldScroll);
    
    int newPos = std::clamp(s, 0, (int)audioSamples.size());
    waveformWidget->resetSelection(newPos);
    waveformWidget->setPlayheadPosition(newPos);

    ui->btnCut->setEnabled(false);
    ui->btnNormalize->setEnabled(false);
    
    updatePlaybackFromModifiedData();
    QApplication::restoreOverrideCursor();    
}

void AudioEditor::normalizeSelection()
{
    int startIndex,endIndex;
    if (!waveformWidget->hasSelection()) {
        startIndex = 0;
        endIndex = audioSamples.size();
    }
    else
    {
        std::tie(startIndex, endIndex) = getSelectionSampleRange();
    }
    if (startIndex >= endIndex) return;
    double oldZoom   = waveformWidget->getSamplesPerPixel();
    int    oldScroll = waveformWidget->getScrollOffset();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    player->stop();
    float mv = 0.0f;
    for (int i = startIndex; i < endIndex; ++i)
        mv = qMax(mv, std::fabs(audioSamples[i]));
    if (mv <= 0.0f) return;
    float scale = 1.0f / mv;
    for (int i = startIndex; i < endIndex; ++i)
        audioSamples[i] *= scale;
    isModified = true;

    waveformWidget->setFullWaveform(audioSamples);
    waveformWidget->restoreZoomState(oldZoom, oldScroll);

    waveformWidget->setStartAndEnd(startIndex, endIndex);
    waveformWidget->setPlayheadPosition(startIndex);
    
    updatePlaybackFromModifiedData();
    QApplication::restoreOverrideCursor();
}

void AudioEditor::updatePlaybackFromModifiedData()
{
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    static int playbackCounter = 0;
    QString tempPlaybackFile = tempDir + QString("/temp_playback_%1.wav").arg(++playbackCounter);

    if (!writeWavFromFloatBuffer(tempPlaybackFile)) {
        QMessageBox::warning(this, tr("Erreur"), tr("Impossible de générer le fichier WAV temporaire."));
        return;
    }

    player->stop();
    player->setSource(QUrl::fromLocalFile(tempPlaybackFile));
}

// void AudioEditor::saveModifiedAudio()
// {
//     if (audioSamples.isEmpty()) {
//         QMessageBox::warning(this, tr("Erreur"), tr("Aucun signal audio à sauvegarder."));
//         return;
//     }
//     QString targetFile;

//     if (isTempRecording) {
//         // 1. Générer un nom par défaut
//         QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
//         QString defaultName = QString("Enregistrement_%1").arg(timeStamp);
        
//         QDialog dialog(this);
//         dialog.setWindowTitle(tr("Sauvegarder l'enregistrement"));
        
//         QVBoxLayout *layout = new QVBoxLayout(&dialog);
        
//         QLabel *label = new QLabel(tr("Nom du fichier :"), &dialog);
//         layout->addWidget(label);
        
//         QLineEdit *lineEdit = new QLineEdit(&dialog);
//         lineEdit->setText(defaultName);
//         layout->addWidget(lineEdit);
        
//         QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
//         layout->addWidget(buttons);
        
//         connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
//         connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        
//         // ICI, le redimensionnement fonctionnera parfaitement car c'est votre layout
//         dialog.resize(280, dialog.height()); 
        
//         bool ok = (dialog.exec() == QDialog::Accepted);
//         QString fileName = lineEdit->text();
        
//         if (!ok || fileName.isEmpty()) return; // Annulation
        
//         // 2. S'assurer de l'extension .wav
//         if (!fileName.endsWith(".wav", Qt::CaseInsensitive)) {
//             fileName += ".wav";
//         }

//         // 3. Construire le chemin complet dans le dossier de travail
//         targetFile = QDir(workingDirectory).filePath(fileName);

//         // 4. Vérifier l'écrasement
//         if (QFile::exists(targetFile)) {
//             QMessageBox::StandardButton reply = QMessageBox::question(this, 
//                 tr("Fichier existant"),
//                 tr("Le fichier '%1' existe déjà.\nVoulez-vous l'écraser ?").arg(fileName),
//                 QMessageBox::Yes | QMessageBox::No);
            
//             if (reply != QMessageBox::Yes) return;
//         }
        
//     } else {
//         QMessageBox msgBox;
//         msgBox.setWindowTitle(tr("Confirmer la sauvegarde"));
//         msgBox.setText(tr("Voulez-vous vraiment enregistrer les modifications sur ce fichier audio ? "
//                         "Assurez-vous d'avoir une sauvegarde de l'original"));
//         msgBox.setIcon(QMessageBox::Question);
//         msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
//         msgBox.setDefaultButton(QMessageBox::No);
        
//         msgBox.button(QMessageBox::Yes)->setText(tr("Oui"));
//         msgBox.button(QMessageBox::No)->setText(tr("Non"));
        
//         int reply = msgBox.exec();
//         if (reply != QMessageBox::Yes)
//             return;
//     }
//     QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/temp_save.raw";
//     QFile f(tmp);
//     if (!f.open(QIODevice::WriteOnly)) {
//         QMessageBox::warning(this, tr("Erreur"), tr("Impossible d'ouvrir le fichier temporaire."));
//         return;
//     }
//     QApplication::setOverrideCursor(Qt::WaitCursor);
//     f.write(reinterpret_cast<const char*>(audioSamples.constData()), audioSamples.size() * sizeof(float));
//     f.flush();
//     f.close();

//     // MODIFICATION : Utiliser getFFmpegPath() au lieu de "ffmpeg" en dur
//     QString ffmpegPath = getFFmpegPath();
    
//     QStringList args;
//     args << "-y" << "-f" << "f32le" << "-ar" << "44100" << "-ac" << "1"
//          << "-i" << tmp
//          << "-c:a" << "pcm_s16le"
//          << targetFile;

//     // if (currentAudioFile.endsWith(".wav", Qt::CaseInsensitive)) {
//     //     args << "-y" << "-f" << "f32le" << "-ar" << "44100" << "-ac" << "1"
//     //          << "-i" << tmp << "-c:a" << "pcm_s16le" << currentAudioFile;
//     // } else {
//     //     args << "-y" << "-f" << "f32le" << "-ar" << "44100" << "-ac" << "1"
//     //          << "-i" << tmp << currentAudioFile;
//     // }

//     QProcess ff;
//     ff.start(ffmpegPath, args);
    
//     if (!ff.waitForFinished(15000) || ff.exitStatus() != QProcess::NormalExit) {
//         QMessageBox::warning(this, tr("Erreur FFmpeg"), tr("Échec de l'encodage."));
//         QApplication::restoreOverrideCursor();
//         return;
//     }
//     QFile::remove(tmp);
//     isModified = false;

//     // Mise à jour de l'état
//     if (isTempRecording) {
//         isTempRecording = false; // Ce n'est plus un temp, c'est un vrai fichier maintenant
//         currentAudioFile = targetFile;
//         setWindowTitle(tr("Éditeur Audio - ") + QFileInfo(currentAudioFile).fileName());
//     }

//     player->setSource(QUrl::fromLocalFile(currentAudioFile));
//     QApplication::restoreOverrideCursor();
    
//     QMessageBox::information(this, tr("Succès"), tr("Fichier sauvegardé avec succès."));
// }


void AudioEditor::saveModifiedAudio()
{
    if (audioSamples.isEmpty()) {
        QMessageBox::warning(this, tr("Erreur"), tr("Aucun signal audio à sauvegarder."));
        return;
    }

    // ========================================================================
    // CORRECTION 1 : LIBÉRATION TOTALE DU FICHIER
    // ========================================================================
    player->stop();
    player->setSource(QUrl()); // Détache le fichier du lecteur
    if (decoder) decoder->stop(); // Par sécurité

    // ========================================================================
    // CORRECTION 2 : ANTI-SATURATION (AUTO-LIMITER)
    // ========================================================================
    // On vérifie si le signal dépasse le maximum autorisé (1.0)
    // float maxVal = 0.0f;
    // for (float s : audioSamples) {
    //     if (std::abs(s) > maxVal) maxVal = std::abs(s);
    // }

    // // Si on dépasse 1.0 (0dB), on réduit tout pour éviter le "crachotement"
    // if (maxVal > 1.0f) {
    //     float scaleFactor = 0.99f / maxVal; // On vise 0.99 pour garder une marge de sécu
    //     for (int i = 0; i < audioSamples.size(); ++i) {
    //         audioSamples[i] *= scaleFactor;
    //     }
    //     // On met à jour l'affichage pour montrer que le son a été "rentré" dans les clous
    //     waveformWidget->setFullWaveform(audioSamples); 
    // }

    // --- Suite logique de sauvegarde (Nom du fichier) ---
    QString targetFile;

    if (isTempRecording) {
        QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        QString defaultName = QString("Enregistrement_%1").arg(timeStamp);
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Sauvegarder l'enregistrement"));
        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        QLabel *label = new QLabel(tr("Nom du fichier :"), &dialog);
        layout->addWidget(label);
        QLineEdit *lineEdit = new QLineEdit(&dialog);
        lineEdit->setText(defaultName);
        layout->addWidget(lineEdit);
        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialog.resize(280, dialog.height());
        bool ok = (dialog.exec() == QDialog::Accepted);
        QString fileName = lineEdit->text();
        if (!ok || fileName.isEmpty()) {
            // Si annulation, on recharge le son pour ne pas planter
             player->setSource(QUrl::fromLocalFile(currentAudioFile));
             return;
        }
        if (!fileName.endsWith(".wav", Qt::CaseInsensitive)) fileName += ".wav";
        targetFile = QDir(workingDirectory).filePath(fileName);
        if (QFile::exists(targetFile)) {
             if (QMessageBox::question(this, tr("Existe"), tr("Écraser ?"), QMessageBox::Yes|QMessageBox::No) != QMessageBox::Yes) {
                 player->setSource(QUrl::fromLocalFile(currentAudioFile));
                 return;
             }
        }
    } else {
        targetFile = currentAudioFile;
        if (targetFile.isEmpty()) {
            QMessageBox::warning(this, tr("Erreur"), tr("Aucun fichier cible défini."));
            return;
        }
        
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("Confirmer la sauvegarde"));
        msgBox.setText(tr("Voulez-vous vraiment enregistrer les modifications sur ce fichier audio ? "
                        "Assurez-vous d'avoir une sauvegarde de l'original"));
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        
        msgBox.button(QMessageBox::Yes)->setText(tr("Oui"));
        msgBox.button(QMessageBox::No)->setText(tr("Non"));
        
        int reply = msgBox.exec();
        if (reply != QMessageBox::Yes) return;
    }
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString tempWav = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/temp_save.wav";

    // Écriture du fichier temporaire
    if (!writeWavFromFloatBuffer(tempWav)) {
        QMessageBox::warning(this, tr("Erreur"), tr("Impossible d'écrire le WAV temporaire."));
        QApplication::restoreOverrideCursor();
        player->setSource(QUrl::fromLocalFile(currentAudioFile));
        return;
    }

    bool success = false;

    // ========================================================================
    // CORRECTION 3 : BOUCLE DE RÉESSAI POUR LE FICHIER VERROUILLÉ (WAV)
    // ========================================================================
    if (targetFile.endsWith(".wav", Qt::CaseInsensitive)) {
        
        // Si le fichier existe, on essaie de le supprimer avec insistance
        if (QFile::exists(targetFile)) {
            bool removed = false;
            // On essaie 10 fois avec 100ms de pause (1 seconde max)
            for (int i = 0; i < 10; ++i) {
                if (QFile::remove(targetFile)) {
                    removed = true;
                    break;
                }
                QThread::msleep(100); // Pause pour laisser Windows libérer le fichier
            }

            if (!removed) {
                QMessageBox::warning(this, tr("Erreur"), 
                    tr("Impossible d'écraser le fichier.\nIl est peut-être utilisé par une autre application ou l'antivirus."));
                QFile::remove(tempWav);
                QApplication::restoreOverrideCursor();
                player->setSource(QUrl::fromLocalFile(currentAudioFile)); // On rétablit
                return;
            }
        }
        
        // Copie du nouveau fichier
        if (QFile::copy(tempWav, targetFile)) {
            success = true;
        } else {
            QMessageBox::warning(this, tr("Erreur"), tr("Impossible de copier le fichier final."));
        }
        QFile::remove(tempWav);
    } 
    else {
        // ====================================================================
        // GESTION MP3 / AUTRES (Conversion FFmpeg)
        // ====================================================================
        // Pour le MP3, comme on a déjà appliqué l'Auto-Limiter au début,
        // le son envoyé à FFmpeg est propre et ne saturera pas.
        
        QString ffmpegPath = getFFmpegPath();
        QStringList args;
        args << "-y" << "-i" << tempWav;
        
        if (targetFile.endsWith(".mp3", Qt::CaseInsensitive)) {
            args << "-codec:a" << "libmp3lame" << "-q:a" << "2"; 
        } else if (targetFile.endsWith(".m4a", Qt::CaseInsensitive)) {
            args << "-codec:a" << "aac" << "-b:a" << "192k";
        }
        
        args << targetFile;
        QProcess ff;
        ff.start(ffmpegPath, args);
        
        if (ff.waitForFinished(30000) && ff.exitStatus() == QProcess::NormalExit) {
            success = true;
        } else {
            QString err = ff.readAllStandardError();
            QMessageBox::warning(this, tr("Erreur FFmpeg"), tr("Échec encodage: %1").arg(err));
        }
        QFile::remove(tempWav);
    }

    QApplication::restoreOverrideCursor();

    if (success) {
        isModified = false;
        if (isTempRecording) isTempRecording = false;
        currentAudioFile = targetFile;
        setWindowTitle(tr("Éditeur Audio - ") + QFileInfo(currentAudioFile).fileName());
        QMessageBox::information(this, tr("Succès"), tr("Fichier sauvegardé avec succès."));
    }
    
    // IMPORTANT : On recharge le fichier frais
    player->setSource(QUrl::fromLocalFile(currentAudioFile));
}

void AudioEditor::updatePlayhead(qint64 ms)
{
    qint64 dur = player->duration();
    if (dur <= 0) return;
    qint64 idx = static_cast<qint64>((double)ms / dur * totalSamples);
    if (waveformWidget->hasSelection() && idx >= waveformWidget->getSelectionEnd()) {
        stopPlayback();
        return;
    }

    waveformWidget->setPlayheadPosition(idx);
    updatePositionLabel(ms);

    int visibleStart = waveformWidget->getScrollOffset();
    int visibleEnd   = visibleStart + waveformWidget->width();
    int playheadPixel = waveformWidget->sampleToPixel(idx);
    if (playheadPixel < visibleStart || playheadPixel > visibleEnd - 30) {
        waveformWidget->scrollToPixel(playheadPixel - waveformWidget->width() / 3);
    }
}

bool AudioEditor::writeWavFromFloatBuffer(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) return false;

    const int sampleRate = 44100;
    const int channels = 1;
    const int bitsPerSample = 16;
    const int byteRate = sampleRate * channels * bitsPerSample / 8;
    const int blockAlign = channels * bitsPerSample / 8;
    int dataSize = static_cast<int>(audioSamples.size() * sizeof(qint16));
    const int fileSize = 44 + dataSize - 8;

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    out.writeRawData("RIFF", 4);
    out << quint32(fileSize);
    out.writeRawData("WAVE", 4);

    out.writeRawData("fmt ", 4);
    out << quint32(16);
    out << quint16(1);
    out << quint16(channels);
    out << quint32(sampleRate);
    out << quint32(byteRate);
    out << quint16(blockAlign);
    out << quint16(bitsPerSample);

    out.writeRawData("data", 4);
    out << quint32(dataSize);

    for (const float &sample : audioSamples) { 
        qint16 val = qBound(static_cast<qint16>(-32768),
                            static_cast<qint16>(sample * 32767),
                            static_cast<qint16>(32767));
        out << val;
    }

    file.close();
    return true;
}

void AudioEditor::playPause()
{
    if (player->playbackState() == QMediaPlayer::PlayingState) {
        ui->btnPlay->setIcon(QIcon(":/icones/play.png"));
        player->pause();
    } else {
        ui->btnPlay->setIcon(QIcon(":/icones/pause.png"));

        if (player->playbackState() == QMediaPlayer::StoppedState) {
            qint64 s = waveformWidget->getSelectionStart();
            if (s < 0) {
                s = waveformWidget->getPlayheadPosition();
            }            
            if (s >= 0 && totalSamples > 0 && player->duration() > 0) {
                qint64 posMs = static_cast<qint64>((double)s / totalSamples * player->duration());
                player->setPosition(posMs);
            }
        }
        
        player->play();
    }

    ui->btnStop->setEnabled(true);
    ui->btnNormalizeAll->setEnabled(false);
}

void AudioEditor::stopPlayback()
{
    player->blockSignals(true);
    player->stop();
    player->blockSignals(false);

    ui->btnStop->setEnabled(false);
    if (!waveformWidget->hasSelection()) ui->btnNormalizeAll->setEnabled(true);
    ui->btnPlay->setIcon(QIcon(":/icones/play.png"));

    const qint64 startposition = waveformWidget->getSelectionStart();
    qint64 targetPos = (startposition > 0) ? startposition : 0;
    
    waveformWidget->setPlayheadPosition(targetPos);
    ui->positionLabel->setText(timeToPosition(targetPos, "hh:mm:ss"));
}

void AudioEditor::resetPosition()
{
    waveformWidget->resetSelection(-1);
    waveformWidget->setPlayheadPosition(0);
    ui->selectionLabel->clear();
    updatePositionLabel(0);
    stopPlayback();
}

void AudioEditor::handleZoomChanged(const QString &zf)
{
    ui->infoZoom->setText(zf);
}

void AudioEditor::handleSelectionChanged(qint64 s, qint64 e)
{
    if (s >= 0 && e > s) {
        ui->selectionLabel->setText(tr("Sélection: %1 - %2 (Durée: %3)")
            .arg(timeToPosition(s, "hh:mm:ss.zzz"))
            .arg(timeToPosition(e, "hh:mm:ss.zzz"))
            .arg(timeToPosition(e - s, "hh:mm:ss.zzz")));
        ui->btnCut->setEnabled(true);
        ui->btnNormalize->setEnabled(true);
        ui->btnNormalizeAll->setEnabled(false);
    } else {
        ui->selectionLabel->clear();
        ui->btnCut->setEnabled(false);
        ui->btnNormalize->setEnabled(false);
        ui->btnNormalizeAll->setEnabled(true);
    }
    ui->positionLabel->setText(timeToPosition(s,"hh:mm:ss"));
}

void AudioEditor::handleMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        qint64 pos = waveformWidget->hasSelection() ? waveformWidget->getSelectionStart() : 0;
        waveformWidget->setPlayheadPosition(pos);
        ui->btnPlay->setIcon(QIcon(":/icones/play.png"));
        ui->btnStop->setEnabled(false);
    }
}

void AudioEditor::releaseResources()
{
    if (player) {
        player->stop();
        player->setSource(QUrl()); 
    }
    if (decoder) {
        decoder->stop();
    }
}

void AudioEditor::closeEvent(QCloseEvent *event)
{
    if (isModified) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Modifications non enregistrées"));
        msgBox.setText(tr("Vous avez des modifications non sauvegardées.\nVoulez-vous les enregistrer avant de quitter ?"));
        msgBox.setIcon(QMessageBox::Question);
        
        QPushButton *btnOui = msgBox.addButton(tr("Oui"), QMessageBox::YesRole);
        QPushButton *btnNon = msgBox.addButton(tr("Non"), QMessageBox::NoRole); 
        QPushButton *btnAnnuler = msgBox.addButton(tr("Annuler"), QMessageBox::RejectRole);
        
        msgBox.setDefaultButton(btnOui);
        msgBox.exec();

        if (msgBox.clickedButton() == btnOui) {
            saveModifiedAudio();
            if (isModified) {
                event->ignore();
                return;
            }
        } else if (msgBox.clickedButton() == btnAnnuler) {
            event->ignore();
            return;
        }
    }

    releaseResources();
    AUDIOEDITOR_BASE::closeEvent(event);
}
