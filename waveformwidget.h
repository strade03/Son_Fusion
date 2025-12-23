#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>
#include <QScrollBar>

class WaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget *parent = nullptr);

    // Configure les couleurs
    void setColors(const QColor &backgroundColor, const QColor &penColor, const QColor &penTextColor);

    // Passe le signal complet (brut) à afficher
    void setFullWaveform(const QVector<float> &fullWaveform);

    void resetSelection(const qint64 startIndex);
    qint64 getSelectionStart() const;
    qint64 getSelectionEnd() const;
    bool hasSelection() const;
    void setPlayheadPosition(qint64 sampleIndex);
    qint64 getPlayheadPosition() const;
    void setStartAndEnd(qint64 pt1, qint64 pt2);
    void setisLoaded(bool v);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    // Expose l’état courant de zoom (samplesPerPixel) et d’offset (scroll)
    double getSamplesPerPixel() const { return samplesPerPixel; }
    int    getScrollOffset()    const { return offsetPixels; }
    int sampleToPixel(qint64 sample) const;
    void scrollToPixel(int x);
    void restoreZoomState(double spp, int offset);
    void setLoading(bool loading); 

signals:
    void selectionChanged(qint64 start, qint64 end);
    void playbackFinished();
    void zoomChanged(const QString &zoomFactor);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    
private slots:
    void handleScrollChanged(int value);

private:
    // Recalcule la représentation downsamplée pour la zone visible
    void recalcCache();
    // Met à jour la barre de défilement
    void updateScrollBar();

    // Signal complet (tous les échantillons)
    QVector<float> fullWaveform;
    // Représentation downsamplée calculée pour la largeur (cache)
    QVector<float> displayWaveform;
    bool cacheValid; // vrai si displayWaveform est à jour

    // Nombre total d'échantillons (du signal complet)
    qint64 totalSamples;
    qint64 playheadSample;
    qint64 selectionStartSample;
    qint64 selectionEndSample;
    qint64 fixedSelectionEdgeSample; // pour glisser la sélection

    bool isSelecting;
    bool isDragging;
    bool isLoaded;
    // Pour gérer le filtre de clic ---
    int pressStartX; 
    // Facteur de zoom : nombre d'échantillons du signal complet par pixel dans la vue courante
    double samplesPerPixel;
    int offsetPixels; // décalage horizontal (scroll en pixels)

    QColor pen;
    QColor background;
    QColor penText;

    QVector<QLine> m_lines;
    QScrollBar *scrollBar;

    bool isLoading;
};
