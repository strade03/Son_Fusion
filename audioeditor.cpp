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

AudioEditor::AudioEditor(const QString &filePath, QWidget *parent)
    : AUDIOEDITOR_BASE(parent)
    , modeAutonome(false)
    , currentAudioFile(filePath)
{
    init();
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

    player->setSource(QUrl::fromLocalFile(currentAudioFile));
    audioSamples.clear(); totalSamples = 0;
    if (currentAudioFile.endsWith(".wav", Qt::CaseInsensitive))
        extractWaveform(currentAudioFile);
    else
        extractWaveformWithFFmpeg(currentAudioFile);
    
    setWindowTitle(tr("Éditeur Audio - ") + QFileInfo(currentAudioFile).fileName());
    
    setButtonsEnabled(true);
    waveformWidget->resetSelection(-1);
    waveformWidget->setPlayheadPosition(0);
    waveformWidget->setisLoaded(true);
}

void AudioEditor::extractWaveform(const QString &path)
{
    decoder->setSource(QUrl::fromLocalFile(path));
    decoder->start();
}

void AudioEditor::extractWaveformWithFFmpeg(const QString &path)
{
    // Vérifier la disponibilité de FFmpeg
    if (!checkFFmpegAvailability()) {
        QApplication::restoreOverrideCursor();
        return;
    }
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/temp.raw";
    
    // MODIFICATION : Utiliser getFFmpegPath() au lieu de "ffmpeg" en dur
    QString ffmpegPath = getFFmpegPath();
    
    QStringList args{"-i", path, "-ac","1","-ar","44100","-f","f32le", tmp};
    QProcess ff;
    ff.start(ffmpegPath, args);
    
    if (!ff.waitForFinished(15000) || ff.exitStatus() != QProcess::NormalExit) {
        QMessageBox::warning(this, tr("Erreur FFmpeg"),
                             tr("La commande FFmpeg a échoué.\n%1").arg(QString(ff.readAllStandardError())));
        QApplication::restoreOverrideCursor();
        return;
    }
    QFile f(tmp);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Erreur"), tr("Impossible d'ouvrir le fichier raw généré."));
        QApplication::restoreOverrideCursor();
        return;
    }
    QByteArray d = f.readAll(); f.close(); QFile::remove(tmp);
    int count = static_cast<int>(d.size() / sizeof(float));
    if (count <= 0) {
        QMessageBox::warning(this, tr("Erreur"), tr("Aucun échantillon n'a été extrait."));
        QApplication::restoreOverrideCursor();
        return;
    }
    audioSamples.resize(count);
    std::memcpy(audioSamples.data(), d.constData(), count * sizeof(float));
    totalSamples = count;
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

void AudioEditor::saveModifiedAudio()
{
    if (audioSamples.isEmpty()) {
        QMessageBox::warning(this, tr("Erreur"), tr("Aucun signal audio à sauvegarder."));
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
    if (reply != QMessageBox::Yes)
        return;

    QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/temp_save.raw";
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Erreur"), tr("Impossible d'ouvrir le fichier temporaire."));
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    f.write(reinterpret_cast<const char*>(audioSamples.constData()), audioSamples.size() * sizeof(float));
    f.flush();
    f.close();

    // MODIFICATION : Utiliser getFFmpegPath() au lieu de "ffmpeg" en dur
    QString ffmpegPath = getFFmpegPath();
    
    QStringList args;
    if (currentAudioFile.endsWith(".wav", Qt::CaseInsensitive)) {
        args << "-y" << "-f" << "f32le" << "-ar" << "44100" << "-ac" << "1"
             << "-i" << tmp << "-c:a" << "pcm_s16le" << currentAudioFile;
    } else {
        args << "-y" << "-f" << "f32le" << "-ar" << "44100" << "-ac" << "1"
             << "-i" << tmp << currentAudioFile;
    }
    QProcess ff;
    ff.start(ffmpegPath, args);
    
    if (!ff.waitForFinished(15000) || ff.exitStatus() != QProcess::NormalExit) {
        QMessageBox::warning(this, tr("Erreur FFmpeg"), tr("Échec de l'encodage."));
        QApplication::restoreOverrideCursor();
        return;
    }
    QFile::remove(tmp);
    isModified = false;

    player->setSource(QUrl::fromLocalFile(currentAudioFile));
    QApplication::restoreOverrideCursor();
    
    QMessageBox::information(this, tr("Succès"), tr("Fichier sauvegardé avec succès."));
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
            int s = waveformWidget->getSelectionStart();
            
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