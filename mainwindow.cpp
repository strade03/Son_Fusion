#include "mainwindow.h"
#include "audiomerger.h"
#include "customtooltip.h"
#include "audioeditor.h"
#include "audiorecorder.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QIcon>
#include <QStatusBar>
#include <QGroupBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // N° Version défini dans main.cpp
    QString title = QString("%1 v%2").arg(qApp->applicationName(), qApp->applicationVersion());
    setWindowTitle(title);
    //setWindowTitle("Son Fusion");

    setFixedSize(635, 489);

    // Initialiser les attributs
    currentPath = QDir::currentPath();
    audioMerger = new AudioMerger(this);
    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);
    isPlaying = false;

    // Connecter le signal de changement d'état
    connect(mediaPlayer, &QMediaPlayer::playbackStateChanged, this, &MainWindow::onPlaybackStateChanged);

    // Connecter les signaux de l'AudioMerger
    connect(audioMerger, &AudioMerger::started, this, &MainWindow::onMergeStarted);
    connect(audioMerger, &AudioMerger::finished, this, &MainWindow::onMergeFinished);
    connect(audioMerger, &AudioMerger::error, this, &MainWindow::showError);

    connect(audioMerger, &AudioMerger::statusMessage, this, [this](const QString& msg) {
        statusBar()->showMessage(msg);
    });

    // Vérifier si FFmpeg est disponible
    ffmpegAvailable = audioMerger->checkFFmpeg();

    // Initialiser l'interface utilisateur
    createUI();

    // Mettre à jour la liste des fichiers
    updateFileList();

    // Afficher un message si FFmpeg n'est pas disponible
    if (!ffmpegAvailable) {
        QMessageBox::information(this, "Erreur",
                                 "Attention ffmpeg.exe est absent, il ne sera possible de traiter que les fichiers au format wav",
                                 QMessageBox::Ok);
    }
}

MainWindow::~MainWindow() {
    // Nettoyer les ressources si nécessaire
}

void MainWindow::createUI() {

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // Partie supérieure : sélection du dossier et type de fichiers
    QHBoxLayout* topLayout = new QHBoxLayout();

    // Partie gauche : sélection du dossier
    QHBoxLayout* folderLayout = new QHBoxLayout();

    QPushButton* openDirButton = new QPushButton(this);
    openDirButton->setIcon(QIcon(":/icones/open-folder.png"));
    openDirButton->setIconSize(QSize(ICON_SIZE,ICON_SIZE));
    openDirButton->setFixedSize(BUTTON_SIZE,BUTTON_SIZE); // Taille fixe pour le bouton
    connect(openDirButton, &QPushButton::clicked, this, &MainWindow::selectDirectory);
    folderLayout->addWidget(openDirButton);

    pathLabel = new QLabel(currentPath, this);
    folderLayout->addWidget(pathLabel);


    topLayout->addLayout(folderLayout, 3); // Proportion 3/4 pour le chemin

    // Partie droite : type de fichiers dans un cadre
    if (ffmpegAvailable) {
        QGroupBox* fileTypeGroup = new QGroupBox("Type de fichiers", this);
        QVBoxLayout* fileTypeLayout = new QVBoxLayout(fileTypeGroup);

        fileTypeCombo = new QComboBox(this);

        QStringList comboItems;
        comboItems.append("Tous..."); // Ajouter "Tous..." en premier
        comboItems.append(AUDIO_FORMATS); // Ajouter tous les formats ensuite

        // Ajouter les items à la combobox
        fileTypeCombo->addItems(comboItems);
        // Définir "Tous..." comme option par défaut (index 0)
        fileTypeCombo->setCurrentIndex(0);

        connect(fileTypeCombo, &QComboBox::currentTextChanged, this, &MainWindow::onFileTypeChanged);
        fileTypeLayout->addWidget(fileTypeCombo);

        topLayout->addWidget(fileTypeGroup, 1); // Proportion 1/4 pour le type de fichiers
    }

    mainLayout->addLayout(topLayout);

    // Partie centrale : liste des fichiers et boutons de contrôle
    QHBoxLayout* centralLayout = new QHBoxLayout();

    // Liste des fichiers
    fileListWidget = new QListWidget(this);
    fileListWidget->setMinimumSize(500, 300);
    connect(fileListWidget, &QListWidget::itemSelectionChanged, this, &MainWindow::onSelectionChanged);
    connect(fileListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::onItemDoubleClicked);
    centralLayout->addWidget(fileListWidget);

    // Boutons de contrôle
    QVBoxLayout* controlLayout = new QVBoxLayout();
    controlLayout->setAlignment(Qt::AlignTop); // Aligner les contrôles en haut

    // Ligne 1 : Play + Record
    QHBoxLayout* topButtonsLayout = new QHBoxLayout();

    playButton = new QPushButton(this);
    playButton->setIcon(QIcon(":/icones/play.png"));
    playButton->setIconSize(QSize(ICON_SIZE,ICON_SIZE));
    playButton->setFixedSize(BUTTON_SIZE,BUTTON_SIZE);
    playButton->setEnabled(false);
    connect(playButton, &QPushButton::clicked, this, &MainWindow::playSelectedFile);
    topButtonsLayout->addWidget(playButton);

    // Création du bouton Record (C'est ici qu'il doit être créé !)
    recordButton = new QPushButton(this);
    recordButton->setIcon(QIcon(":/icones/record.png")); 
    recordButton->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
    recordButton->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
    connect(recordButton, &QPushButton::clicked, this, &MainWindow::openRecorder);
    topButtonsLayout->addWidget(recordButton);

    controlLayout->addLayout(topButtonsLayout);

    // Ligne 2 : Edition (seul au milieu)
    QHBoxLayout* middleButtonsLayout = new QHBoxLayout();
    editButton = new QPushButton(this);
    editButton->setIcon(QIcon(":/icones/edit.png"));
    editButton->setIconSize(QSize(ICON_SIZE,ICON_SIZE));
    editButton->setFixedSize(BUTTON_SIZE,BUTTON_SIZE);
    editButton->setEnabled(false);
    connect(editButton, &QPushButton::clicked, this, &MainWindow::editSelectedFile);
    middleButtonsLayout->addWidget(editButton);
    controlLayout->addLayout(middleButtonsLayout);

    // Ligne 3 : Dupliquer + Supprimer
    QHBoxLayout* bottomButtonsLayout = new QHBoxLayout();
    
    copieButton = new QPushButton(this);
    copieButton->setIcon(QIcon(":/icones/duplicate.png"));
    copieButton->setIconSize(QSize(ICON_SIZE,ICON_SIZE));
    copieButton->setFixedSize(BUTTON_SIZE,BUTTON_SIZE);
    copieButton->setEnabled(false);
    connect(copieButton, &QPushButton::clicked, this, &MainWindow::duplicateSelectedFile);
    bottomButtonsLayout->addWidget(copieButton);
    
    deleteButton = new QPushButton(this);
    deleteButton->setIcon(QIcon(":/icones/delete.png"));
    deleteButton->setIconSize(QSize(ICON_SIZE,ICON_SIZE));
    deleteButton->setFixedSize(BUTTON_SIZE,BUTTON_SIZE);
    deleteButton->setEnabled(false);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedFile);
    bottomButtonsLayout->addWidget(deleteButton);

    controlLayout->addLayout(bottomButtonsLayout);

    controlLayout->addSpacing(30);

    // Boutons monter et descendre
    upButton = new QPushButton(this);
    upButton->setIcon(QIcon(":/icones/up.png"));
    upButton->setIconSize(QSize(ICON_SIZE,ICON_SIZE));
    upButton->setFixedSize(BUTTON_SIZE,BUTTON_SIZE);
    upButton->setEnabled(false);
    connect(upButton, &QPushButton::clicked, this, &MainWindow::moveSelectedFileUp);
    controlLayout->addWidget(upButton);

    downButton = new QPushButton(this);
    downButton->setIcon(QIcon(":/icones/down.png"));
    downButton->setIconSize(QSize(ICON_SIZE,ICON_SIZE));
    downButton->setFixedSize(BUTTON_SIZE,BUTTON_SIZE);
    downButton->setEnabled(false);
    connect(downButton, &QPushButton::clicked, this, &MainWindow::moveSelectedFileDown);
    controlLayout->addWidget(downButton);

    // Ajouter un stretch pour pousser les boutons vers le haut
    controlLayout->addStretch();

    // Infobulles pour les boutons
    new CustomTooltip(playButton, "Lecture/stop (double-clic sur le nom de fichier lance la lecture)");
    new CustomTooltip(editButton, "Éditer le fichier audio sélectionné (couper des segments)");
    new CustomTooltip(copieButton, "Dupliquer");
    new CustomTooltip(deleteButton, "Supprimer le fichier selectionné de la liste des sons à fusionner.");
    new CustomTooltip(upButton, "Remonter le fichier selectionné d'une place dans la liste des sons à fusionner.");
    new CustomTooltip(downButton, "Descendre le fichier selectionné d'une place dans la liste des sons à fusionner.");
    new CustomTooltip(recordButton, "Enregistrer un nouveau son");
    
    centralLayout->addLayout(controlLayout);

    mainLayout->addLayout(centralLayout);



    // Partie inférieure : fusion des fichiers
    QHBoxLayout* fusionLayout = new QHBoxLayout();

    QLabel* outputLabel = new QLabel("Nom du fichier fusionné : ", this);
    fusionLayout->addWidget(outputLabel);

    outputNameEdit = new QLineEdit("Sons_fusionnes", this);
    fusionLayout->addWidget(outputNameEdit);

    outputFormatCombo = new QComboBox(this);
    if (ffmpegAvailable) {
        //outputFormatCombo->addItems({".mp3", ".wav", ".flac", ".ogg", ".aac", ".m4a", ".opus"});
        outputFormatCombo->addItems(AUDIO_FORMATS);
        // Trouver l'index de .mp3 et le définir comme format par défaut
        int mp3Index = AUDIO_FORMATS.indexOf(".mp3");
        if (mp3Index >= 0) {
            outputFormatCombo->setCurrentIndex(mp3Index);
        }
    } else {
        outputFormatCombo->addItems({".wav"});
    }

    /*if (ffmpegAvailable) {
        outputFormatCombo->addItems({".mp3", ".wav", ".flac", ".ogg"});
    } else {
        outputFormatCombo->addItems({".wav"});
    }*/
    fusionLayout->addWidget(outputFormatCombo);

    QPushButton* fusionButton = new QPushButton(this);
    fusionButton->setIcon(QIcon(":/icones/fusionner.png"));
    fusionButton->setIconSize(QSize(ICON_SIZE,ICON_SIZE));
    fusionButton->setFixedSize(BUTTON_SIZE,BUTTON_SIZE); // Taille fixe
    connect(fusionButton, &QPushButton::clicked, this, &MainWindow::mergeFiles);
    fusionLayout->addWidget(fusionButton);

    // Ajouter les boutons info et quitter
    QPushButton* infoButton = new QPushButton(this);
    infoButton->setIcon(QIcon(":/icones/info.png"));
    infoButton->setIconSize(QSize(ICON_SIZE,ICON_SIZE));
    infoButton->setFixedSize(BUTTON_SIZE,BUTTON_SIZE); // Taille fixe
    connect(infoButton, &QPushButton::clicked, this, &MainWindow::showInfo);
    fusionLayout->addWidget(infoButton);

    QPushButton* quitButton = new QPushButton(this);
    quitButton->setIcon(QIcon(":/icones/exit.png"));
    quitButton->setIconSize(QSize(ICON_SIZE,ICON_SIZE));
    quitButton->setFixedSize(BUTTON_SIZE,BUTTON_SIZE); // Taille fixe
    connect(quitButton, &QPushButton::clicked, this, &MainWindow::close);
    fusionLayout->addWidget(quitButton);

    new CustomTooltip(fusionButton, "Fusionner les fichiers.");
    new CustomTooltip(infoButton, "À propos");
    new CustomTooltip(quitButton, "Quitter");

    mainLayout->addLayout(fusionLayout);

    // Utiliser la barre d'état existante
    statusBar()->showMessage("Prêt");
    statusBar()->setStyleSheet("background-color: " + STATUS_BAR_COLOR + "; color: white;");
}

// Gestion de l'enregistrement Audio
void MainWindow::openRecorder() {
    AudioRecorder recorderDialog(this);
    if (recorderDialog.exec() == QDialog::Accepted) {
        QString recordedFile = recorderDialog.getRecordedFilePath();
        
        if (QFile::exists(recordedFile)) {
            AudioEditor editor(recordedFile, currentPath, this, true);
            
            // On lance l'éditeur
            int result = editor.exec();
            
            // On vérifie simplement si le fichier final existe dans le dossier courant
            QString finalPath = editor.getCurrentFilePath();
            QFileInfo fi(finalPath);
            
            // Condition : L'utilisateur a bien sauvegardé le fichier dans le dossier de travail
            if (fi.exists() && fi.absolutePath() == QDir(currentPath).absolutePath() && finalPath != recordedFile) {
                
                // On crée l'item
                QListWidgetItem* newItem = new QListWidgetItem(fi.fileName());
                // On l'ajoute à la fin
                fileListWidget->addItem(newItem);
                // On le sélectionne pour montrer à l'utilisateur où il est
                fileListWidget->setCurrentItem(newItem);
                // --------------------------------------------
            }
            
            // Nettoyage du fichier brut temporaire (rec_xxx.wav)
            QFile::remove(recordedFile);
        }
    }
}

void MainWindow::showInfo() {
    QDialog* aboutDialog = new QDialog(this);
    aboutDialog->setWindowTitle("À propos");
    aboutDialog->setFixedSize(280, 250);

    QVBoxLayout* layout = new QVBoxLayout(aboutDialog);

    QLabel* aboutLabel = new QLabel(aboutDialog);
    aboutLabel->setTextFormat(Qt::RichText);
    aboutLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    aboutLabel->setOpenExternalLinks(true);
    aboutLabel->setText(QString(
        "<h2 align=\"center\">%1 v%2</h2>"
        "Application de fusion de fichiers audio<br>"
        "©Stéphane Verdier - Freeware - 2025<br><br>"
        "Développée en C++ avec Qt<br>"
        "Ce logiciel utilise FFmpeg (ffmpeg.org)<br>sous licence LGPLv2.1 / GPLv3<br><br>"
        "Crédits icones :<br>"
        "<a href='https://www.flaticon.com/authors/kumakamu'>https://www.flaticon.com/authors/kumakamu</a><br>"
        "<a href='https://www.flaticon.com/fr/auteurs/iconixar'>https://www.flaticon.com/fr/auteurs/iconixar</a>"
                            ).arg(qApp->applicationName(), qApp->applicationVersion()) );

    layout->addWidget(aboutLabel);

    QPushButton* closeButton = new QPushButton("Fermer", aboutDialog);
    connect(closeButton, &QPushButton::clicked, aboutDialog, &QDialog::accept);

    layout->addWidget(closeButton, 0, Qt::AlignCenter);

    aboutDialog->exec();
    delete aboutDialog;
}

void MainWindow::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
    if (state == QMediaPlayer::StoppedState) {
        // La lecture est terminée, remettre l'icône de lecture
        playButton->setIcon(QIcon(":/icones/play.png"));
        isPlaying = false;
    }
}

void MainWindow::updateFileList() {
    fileListWidget->clear();

    QDir dir(currentPath);
    QStringList filters;

    if (ffmpegAvailable && fileTypeCombo) {
        QString selectedType = fileTypeCombo->currentText();
        if (selectedType == "Tous...") {
            for (const QString& format : AUDIO_FORMATS) {
                filters << "*" + format;
            }
            //filters << "*.wav" << "*.mp3" << "*.flac" << "*.ogg" << "*.m4a" << "*.aac" << "*.wma" << "*.aiff" << "*.opus" << "*.ac3";
            //filters << "*.wav" << "*.mp3" << "*.flac" << "*.ogg" << "*.m4a";
        } else {
            filters << "*" + selectedType;
        }
    } else {
        filters << "*.wav";
    }

    dir.setNameFilters(filters);
    QStringList files = dir.entryList(QDir::Files);

    fileListWidget->addItems(files);
}

void MainWindow::selectDirectory() {

    QString dir = QFileDialog::getExistingDirectory(this, "Sélectionner un dossier",
                                                    currentPath,
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        currentPath = dir;
        pathLabel->setText(currentPath);

        // Mettre à jour automatiquement la liste des fichiers après la sélection du dossier
        updateFileList();

        // Afficher un message dans la barre d'état
        statusBar()->showMessage("Dossier sélectionné : " + dir, 3000);
    }
}

void MainWindow::onFileTypeChanged(const QString& /*type*/) { // Warning car type pas utlisé
    updateFileList();
}

void MainWindow::onSelectionChanged() {
    bool hasSelection = !fileListWidget->selectedItems().isEmpty();

    playButton->setEnabled(hasSelection);
    editButton->setEnabled(hasSelection);
    copieButton->setEnabled(hasSelection);
    deleteButton->setEnabled(hasSelection);
    upButton->setEnabled(hasSelection && fileListWidget->currentRow() > 0);
    downButton->setEnabled(hasSelection && fileListWidget->currentRow() < fileListWidget->count() - 1);
}

void MainWindow::onItemDoubleClicked(QListWidgetItem* /*item*/) { // Warning car item pas utlisé
    playSelectedFile();
}

void MainWindow::playSelectedFile() {
    if (fileListWidget->selectedItems().isEmpty()) {
        return;
    }

    if (isPlaying) {
        mediaPlayer->stop();
        playButton->setIcon(QIcon(":/icones/play.png"));
        isPlaying = false;
    } else {
        QString fileName = fileListWidget->currentItem()->text();
        QString filePath = currentPath + "/" + fileName;
       // QMessageBox::information(this, "Fusion", tr("%1").arg(fileName));
        mediaPlayer->setSource(QUrl::fromLocalFile(filePath));
        mediaPlayer->play();
        playButton->setIcon(QIcon(":/icones/stop.png"));
        isPlaying = true;
    }
}

void MainWindow::deleteSelectedFile() {
    if (fileListWidget->selectedItems().isEmpty()) {
        return;
    }

    int row = fileListWidget->currentRow();
//    QMessageBox::information(this, "Fusion", tr("%1").arg(row));
    delete fileListWidget->takeItem(row);

    onSelectionChanged();
}

void MainWindow::duplicateSelectedFile() { // TODO A faire
    if (fileListWidget->selectedItems().isEmpty()) {
        return;
    }

    QListWidgetItem *selectedItem = fileListWidget->currentItem();
    int currentRow = fileListWidget->row(selectedItem);
    QString text = selectedItem->text();
    QListWidgetItem *newItem = new QListWidgetItem(text);
    fileListWidget->insertItem(currentRow + 1, newItem);
    
}

void MainWindow::moveSelectedFileUp() {
    if (fileListWidget->selectedItems().isEmpty()) {
        return;
    }

    int currentRow = fileListWidget->currentRow();
    if (currentRow > 0) {
        QListWidgetItem* item = fileListWidget->takeItem(currentRow);
        fileListWidget->insertItem(currentRow - 1, item);
        fileListWidget->setCurrentItem(item);
    }
}

void MainWindow::moveSelectedFileDown() {
    if (fileListWidget->selectedItems().isEmpty()) {
        return;
    }

    int currentRow = fileListWidget->currentRow();
    if (currentRow < fileListWidget->count() - 1) {
        QListWidgetItem* item = fileListWidget->takeItem(currentRow);
        fileListWidget->insertItem(currentRow + 1, item);
        fileListWidget->setCurrentItem(item);
    }
}

void MainWindow::mergeFiles() {
    if (fileListWidget->count() < 1) {
         QMessageBox::information(this, "Fusion / Conversion", 
                                 "Aucun fichier à traiter.", QMessageBox::Ok);
        return;
    }

    QString outputName = outputNameEdit->text() + outputFormatCombo->currentText();
    QString outputPath = currentPath + "/" + outputName;

    // Vérifier si le fichier existe déjà
    if (QFile::exists(outputPath)) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Fichier existant",
                                                                  "Attention le fichier existe déjà, souhaitez-vous l'écraser ?",
                                                                  QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No) {
            return;
        }
    }

    // Récupérer la liste des fichiers
    QStringList inputFiles;
    for (int i = 0; i < fileListWidget->count(); i++) {
        QString fileName = fileListWidget->item(i)->text();
        if (fileName != outputName) {
            inputFiles << currentPath + "/" + fileName;
        } else {
            QMessageBox::information(this, "Erreur Fusion",
                                     "Le fichier " + outputName + " est en entrée et en sortie, il sera donc ignoré en entrée.",QMessageBox::Ok);
        }
    }

    // Si après filtrage la liste est vide (cas où l'utilisateur a mis le même nom)
    if (inputFiles.isEmpty()) return;
    // Lancer la fusion
    audioMerger->mergeFiles(inputFiles, outputPath);
}

void MainWindow::onMergeStarted() {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    // 2. Désactiver l'interface pour éviter les double-clics    
    centralWidget()->setEnabled(false); 
    statusBar()->showMessage("Fusion en cours...");
}

void MainWindow::onMergeFinished(bool success) {
    QApplication::restoreOverrideCursor();
    // 2. Réactiver l'interface
    centralWidget()->setEnabled(true);
    statusBar()->clearMessage();

    if (success) {
        // 3. Afficher le message "Terminé" (pendant 5 secondes par exemple)
        statusBar()->showMessage("Fusion terminée avec succès !", 5000);
        
        QMessageBox::information(this, "Fusion", "Fusion des fichiers terminée avec succès",QMessageBox::Ok);
        updateFileList();
    } else {
        // En cas d'échec silencieux (si le process renvoie false sans erreur signalée)
        statusBar()->showMessage("Échec de la fusion.", 5000);
    }
}

void MainWindow::showError(const QString& message) {
    // IMPORTANT : Rétablir le curseur aussi en cas d'erreur !
    QApplication::restoreOverrideCursor();
    centralWidget()->setEnabled(true);

    statusBar()->showMessage("Erreur lors de la fusion.");
    QMessageBox::critical(this, "Erreur", message);
}

void MainWindow::editSelectedFile() {
    if (fileListWidget->selectedItems().isEmpty()) {
        return;
    }

    // Récupérer le fichier sélectionné
    QString fileName = fileListWidget->currentItem()->text();
    QString filePath = currentPath + "/" + fileName;

    // Arrêter la lecture si elle est en cours
    if (isPlaying) {
        mediaPlayer->stop();
        playButton->setIcon(QIcon(":/icones/play.png"));
        isPlaying = false;
    }

    // Créer et afficher l'éditeur audio
    AudioEditor editor(filePath, currentPath, this);
    int result = editor.exec();

    // Si l'édition a été effectuée, mettre à jour la liste des fichiers
    if (result == QDialog::Accepted) {
        updateFileList();
    }
}
