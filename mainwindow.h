#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QMediaPlayer>
#include <QAudioOutput>

class AudioMerger;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    virtual ~MainWindow();

private slots:
    void selectDirectory();
    void onFileTypeChanged(const QString& type);
    void onSelectionChanged();
    void onItemDoubleClicked(QListWidgetItem* item);
    void playSelectedFile();
    void deleteSelectedFile();
    void duplicateSelectedFile();
    void editSelectedFile();
    void moveSelectedFileUp();
    void moveSelectedFileDown();
    void mergeFiles();
    void onMergeStarted();
    void onMergeFinished(bool success);
    void showError(const QString& message);
    void showInfo();
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void openRecorder();

private:
    void createUI();
    void updateFileList();

    int iconSize;
    int buttonSize;
    const QStringList AUDIO_FORMATS = {".wav", ".mp3", ".m4a", ".aac",".ac3", ".aiff", ".flac", ".ogg", ".opus", ".wma"};

    const int ICON_SIZE = 32;
    const int BUTTON_SIZE = 48;
    const QString STATUS_BAR_COLOR = "#12689E";

    QString currentPath;
    bool ffmpegAvailable;
    bool isPlaying;

    QLabel* pathLabel;
    QListWidget* fileListWidget;
    QComboBox* fileTypeCombo;
    QComboBox* outputFormatCombo;
    QLineEdit* outputNameEdit;

    QPushButton* playButton;
    QPushButton* deleteButton;
    QPushButton* editButton;
    QPushButton* copieButton;
    QPushButton* upButton;
    QPushButton* downButton;

    AudioMerger* audioMerger;
    QMediaPlayer* mediaPlayer;
    QAudioOutput* audioOutput;
    QPushButton* recordButton;
};

#endif // MAINWINDOW_H
