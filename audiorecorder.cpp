#include "audiorecorder.h"
#include <QVBoxLayout>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QMediaFormat>

AudioRecorder::AudioRecorder(QWidget *parent)
    : QDialog(parent)
    , durationSec(0)
{
    setWindowTitle(tr("Enregistrement Audio"));
    setFixedSize(350, 250);

    // Initialisation des objets multimédia
    audioInput = new QAudioInput(this);
    audioInput->setVolume(1.0f);
    recorder = new QMediaRecorder(this);
    session.setAudioInput(audioInput);
    session.setRecorder(recorder);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &AudioRecorder::updateDuration);

    setupUi();

    // Remplir la liste des périphériques
    const auto devices = QMediaDevices::audioInputs();
    for (const QAudioDevice &device : devices) {
        deviceCombo->addItem(device.description(), QVariant::fromValue(device));
    }
    
    // Sélectionner le défaut
    if (!devices.isEmpty()) {
        updateInputDevice(deviceCombo->currentIndex());
    }

    connect(deviceCombo, &QComboBox::currentIndexChanged, this, &AudioRecorder::updateInputDevice);
    connect(recorder, &QMediaRecorder::recorderStateChanged, this, &AudioRecorder::onStateChanged);
    connect(recorder, &QMediaRecorder::errorOccurred, this, [this](){
        QMessageBox::critical(this, tr("Erreur"), recorder->errorString());
    });
}

AudioRecorder::~AudioRecorder() {}

void AudioRecorder::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    // Choix du micro
    layout->addWidget(new QLabel(tr("Microphone :"), this));
    deviceCombo = new QComboBox(this);
    layout->addWidget(deviceCombo);

    layout->addSpacing(20);

    // Timer
    timeLabel = new QLabel("00:00", this);
    timeLabel->setAlignment(Qt::AlignCenter);
    QFont f = timeLabel->font();
    f.setPointSize(32);
    f.setBold(true);
    timeLabel->setFont(f);
    layout->addWidget(timeLabel);

    layout->addSpacing(20);

    // Bouton Record
    recordButton = new QPushButton(tr("Enregistrer"), this);
    recordButton->setMinimumHeight(50);
    // Vous pouvez mettre une icône ici si vous en avez une (ex: un point rouge)
    connect(recordButton, &QPushButton::clicked, this, &AudioRecorder::toggleRecord);
    layout->addWidget(recordButton);

    statusLabel = new QLabel(tr("Prêt"), this);
    statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(statusLabel);
}

void AudioRecorder::updateInputDevice(int index)
{
    if (index >= 0) {
        auto device = deviceCombo->itemData(index).value<QAudioDevice>();
        audioInput->setDevice(device);
    }
}

void AudioRecorder::toggleRecord()
{
    if (recorder->recorderState() == QMediaRecorder::StoppedState) {
        // Définir le fichier de sortie temporaire
        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QString fileName = QString("rec_%1.wav").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        outputFilePath = tempDir + "/" + fileName;
        
        recorder->setOutputLocation(QUrl::fromLocalFile(outputFilePath));
        QMediaFormat format;
        format.setFileFormat(QMediaFormat::Wave);
        
        // CORRECTION ICI : Utilisez Unspecified au lieu de Uncompressed
        format.setAudioCodec(QMediaFormat::AudioCodec::Unspecified); 
        
        recorder->setMediaFormat(format);
        
        recorder->record();

        durationSec = 0;
        timeLabel->setText("00:00");
        timer->start(1000);
        
        // Désactiver la combo box pendant l'enregistrement
        deviceCombo->setEnabled(false);
    } else {
        recorder->stop();
        timer->stop();
        deviceCombo->setEnabled(true);
        accept(); // Ferme la fenêtre et renvoie QDialog::Accepted
    }
}

void AudioRecorder::updateDuration()
{
    durationSec++;
    int m = durationSec / 60;
    int s = durationSec % 60;
    timeLabel->setText(QString("%1:%2")
                       .arg(m, 2, 10, QChar('0'))
                       .arg(s, 2, 10, QChar('0')));
}

void AudioRecorder::onStateChanged(QMediaRecorder::RecorderState state)
{
    if (state == QMediaRecorder::RecordingState) {
        recordButton->setText(tr("Arrêter"));
        recordButton->setStyleSheet("background-color: #ffcccc; color: red; font-weight: bold;");
        statusLabel->setText(tr("Enregistrement en cours..."));
    } else {
        recordButton->setText(tr("Enregistrer"));
        recordButton->setStyleSheet("");
        statusLabel->setText(tr("Arrêté"));
    }
}

QString AudioRecorder::getRecordedFilePath() const
{
    return outputFilePath;
}