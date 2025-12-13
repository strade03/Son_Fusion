#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <QDialog>
#include <QMediaCaptureSession>
#include <QAudioInput>
#include <QMediaRecorder>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTimer>

class AudioRecorder : public QDialog
{
    Q_OBJECT
public:
    explicit AudioRecorder(QWidget *parent = nullptr);
    ~AudioRecorder();
    
    // Retourne le chemin du fichier enregistré
    QString getRecordedFilePath() const;

private slots:
    void toggleRecord();
    void updateDuration();
    void onStateChanged(QMediaRecorder::RecorderState state);
    void updateInputDevice(int index);

private:
    void setupUi();

    QMediaCaptureSession session;
    QAudioInput *audioInput;
    QMediaRecorder *recorder;
    
    QLabel *timeLabel;
    QLabel *statusLabel;
    QPushButton *recordButton;
    QComboBox *deviceCombo;
    
    QTimer *timer;
    qint64 durationSec;
    QString outputFilePath;
};

#endif // AUDIORECORDER_H