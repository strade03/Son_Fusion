#pragma once

#include <QMediaPlayer>
#include <QAudioDecoder>
#include <QAudioOutput>
#include <QCoreApplication>
#include "waveformwidget.h"
#include <QLabel>
#include <QWidget>
#include <QCloseEvent>

namespace Ui {
    class AudioEditorWidget;
}

// Pour changer le mode autonome ou boite de dialogue pour son_fusion
// ajouter DEFINES += USE_DIALOG_MODE dans .pro pour passer en mode dialog
#ifdef USE_DIALOG_MODE
#include <QDialog>
#define AUDIOEDITOR_BASE QDialog
#else
#include <QMainWindow>
#define AUDIOEDITOR_BASE QWidget
#endif

class AudioEditor : public AUDIOEDITOR_BASE
{
    Q_OBJECT

public:
    explicit AudioEditor(QWidget *parent = nullptr);
    explicit AudioEditor(const QString &filePath, QWidget *parent);
    ~AudioEditor();

private slots:
    void openFile();
    void updatePositionLabel(qint64 position);
    QString timeToPosition(qint64 x, const QString &format);
    void playPause();
    void resetPosition();
    void stopPlayback();
    void updatePlayhead(qint64 position);
    void handleMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void handleSelectionChanged(qint64 start, qint64 end);
    void handleZoomChanged(const QString &zoomFactor);
    void processBuffer(const QAudioBuffer &buffer);
    void decodingFinished();
    void cutSelection();
    void saveModifiedAudio();
    void normalizeSelection();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    // ========================================================================
    // MÉTHODES MULTIPLATEFORME FFMPEG
    // ========================================================================
    
    /**
     * @brief Retourne le chemin de FFmpeg selon la plateforme
     * @return QString contenant le chemin vers l'exécutable FFmpeg
     */
    QString getFFmpegPath();
    
    /**
     * @brief Vérifie que FFmpeg est disponible et fonctionnel
     * @return true si FFmpeg est trouvé et opérationnel
     */
    bool checkFFmpegAvailability();
    
    // ========================================================================
    // MÉTHODES EXISTANTES
    // ========================================================================
    
    bool writeWavFromFloatBuffer(const QString &filename);
    void init();
    Ui::AudioEditorWidget *ui;
    QMediaPlayer   *player;
    QAudioOutput   *audioOutput;
    QAudioDecoder  *decoder;
    WaveformWidget *waveformWidget;
    QVector<float>  audioSamples;
    qint64          totalSamples;
    QString         currentAudioFile;
    bool            modeAutonome;   
    void extractWaveform(const QString &filePath);
    void extractWaveformWithFFmpeg(const QString &filePath);
    QVector<float> downsampleBuffer(const QVector<float> &buffer, int targetSampleCount);
    void updatePlaybackFromModifiedData();

    std::pair<int, int> getSelectionSampleRange();
    void setButtonsEnabled(bool enabled);
    bool fichierCharge = false;
    bool isModified; 
    void releaseResources();
};