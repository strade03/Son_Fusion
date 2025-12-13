#include "audiorecorder.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
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
    setFixedSize(350, 300);
    

    // Initialisation des objets multimédia
    audioInput = new QAudioInput(this);

    // audioInput->setVolume(1.0f);

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

    layout->addSpacing(10);

     QHBoxLayout *volumeLayout = new QHBoxLayout();

    // 1. Le titre à gauche
    QLabel *volLabel = new QLabel(tr("Volume :"), this);
    volumeLayout->addWidget(volLabel);

    // 2. Le slider au milieu
    volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);

    // Gestion de la session (static)
    static int sessionVolume = 100; 
    
    // Application des valeurs mémorisées
    volumeSlider->setValue(sessionVolume); 
    audioInput->setVolume(sessionVolume / 100.0f);

    volumeLayout->addWidget(volumeSlider);
    
    // 3. Le pourcentage à droite (initialisé avec la valeur de session)
    QLabel *volValueLabel = new QLabel(QString::number(sessionVolume) + "%", this);
    
    // Optionnel : fixer une petite largeur pour éviter que ça bouge quand on passe de 99 à 100
    volValueLabel->setFixedWidth(35); 
    volValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    volumeLayout->addWidget(volValueLabel);

    // On ajoute la ligne complète au layout principal
    layout->addLayout(volumeLayout);

    // Connexion
    connect(volumeSlider, &QSlider::valueChanged, this, [this, volValueLabel](int value){
        // Mise à jour audio (linéaire simple)
        audioInput->setVolume(value / 100.0f); 
        
        // Mise à jour visuelle
        volValueLabel->setText(QString::number(value) + "%");
        
        // Mise à jour de la mémoire de session
        sessionVolume = value;
    });

    layout->addSpacing(5);

    // 3. Timer
    timeLabel = new QLabel("00:00", this);
    timeLabel->setAlignment(Qt::AlignCenter);
    QFont f = timeLabel->font();
    f.setPointSize(32);
    f.setBold(true);
    timeLabel->setFont(f);
    layout->addWidget(timeLabel);

    layout->addSpacing(5);

    // Bouton Record
    recordButton = new QPushButton(this);
    
    // Configuration de l'icone initiale
    recordButton->setIcon(QIcon(":/icones/record.png"));
    recordButton->setIconSize(QSize(48, 48)); // Une bonne taille pour l'action principale
    
    // On enlève le texte pour ne laisser que l'image
    recordButton->setText(""); 
    recordButton->setToolTip(tr("Démarrer l'enregistrement"));
    
    // On le rend carré et un peu plus grand pour être facile à cliquer
    recordButton->setFixedSize(64, 64);
    
    connect(recordButton, &QPushButton::clicked, this, &AudioRecorder::toggleRecord);
    
    // On centre le bouton dans le layout pour faire joli
    layout->addWidget(recordButton, 0, Qt::AlignCenter);

    layout->addSpacing(20);

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
        // État : En train d'enregistrer -> On affiche le bouton STOP
        recordButton->setIcon(QIcon(":/icones/stop.png"));
        recordButton->setToolTip(tr("Arrêter l'enregistrement"));
        
        // Optionnel : un fond rouge léger pour bien montrer que ça tourne
        recordButton->setStyleSheet("background-color: #ffcccc; border-radius: 5px; border: 1px solid gray;");
        
        statusLabel->setText(tr("Enregistrement en cours..."));
    } else {
        // État : Arrêté -> On affiche le bouton RECORD
        recordButton->setIcon(QIcon(":/icones/record.png"));
        recordButton->setToolTip(tr("Démarrer l'enregistrement"));
        
        // On remet le style par défaut
        recordButton->setStyleSheet("");
        
        statusLabel->setText(tr("Arrêté"));
    }
}

QString AudioRecorder::getRecordedFilePath() const
{
    return outputFilePath;
}
