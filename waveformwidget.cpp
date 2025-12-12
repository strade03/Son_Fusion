#include "waveformwidget.h"
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <algorithm>

WaveformWidget::WaveformWidget(QWidget* parent)
    : QWidget(parent)
    , cacheValid(false)
    , totalSamples(0)
    , playheadSample(0)
    , selectionStartSample(-1)
    , selectionEndSample(-1)
    , fixedSelectionEdgeSample(-1)
    , isSelecting(false)
    , isDragging(false)
    , isLoaded(false)
    , samplesPerPixel(1.0) // Sera initialisé dans resetZoom()
    , offsetPixels(0)
    , pen(Qt::blue)
    , background(Qt::white)
    , penText(Qt::black)
{
    setMinimumHeight(150);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    scrollBar = new QScrollBar(Qt::Horizontal, this);
    layout->addStretch();
    layout->addWidget(scrollBar);
    connect(scrollBar, &QScrollBar::valueChanged, this, &WaveformWidget::handleScrollChanged);
}

void WaveformWidget::setColors(const QColor &backgroundColor, const QColor &penColor, const QColor &penTextColor)
{
    background = backgroundColor;
    pen = penColor;
    penText = penTextColor;
    update();
}

void WaveformWidget::setFullWaveform(const QVector<float> &wf)
{
    fullWaveform = wf;
    totalSamples = fullWaveform.size();
    
    // Si la tête de lecture ou la sélection sont au-delà de la nouvelle fin, on les ramène
    if (playheadSample > totalSamples) playheadSample = totalSamples;
    if (selectionStartSample > totalSamples) selectionStartSample = totalSamples;
    if (selectionEndSample > totalSamples) selectionEndSample = totalSamples;

    cacheValid = false;
    resetZoom();
}

void WaveformWidget::resetSelection(const qint64 startIndex)
{
    selectionStartSample = startIndex;
    selectionEndSample = startIndex;
    update();
}

qint64 WaveformWidget::getSelectionStart() const { return selectionStartSample; }
qint64 WaveformWidget::getSelectionEnd() const { return selectionEndSample; }
bool WaveformWidget::hasSelection() const { return selectionStartSample >= 0 && selectionEndSample > selectionStartSample; }

void WaveformWidget::setPlayheadPosition(qint64 sampleIndex)
{
    if (sampleIndex < 0)
        sampleIndex = 0;
    if (sampleIndex > totalSamples)
        sampleIndex = totalSamples;
        
    playheadSample = sampleIndex;
    update();
}

qint64 WaveformWidget::getPlayheadPosition() const { return playheadSample; }

void WaveformWidget::setStartAndEnd(qint64 pt1, qint64 pt2)
{
    // Clamp aux limites
    pt1 = std::clamp(pt1, (qint64)0, totalSamples);
    pt2 = std::clamp(pt2, (qint64)0, totalSamples);

    selectionStartSample = std::min(pt1, pt2);
    selectionEndSample = std::max(pt1, pt2);
    setPlayheadPosition(selectionStartSample);
}

void WaveformWidget::setisLoaded(bool v)
{
    isLoaded = v;
}

//
// RecalcCache() : reconstruit displayWaveform pour la zone visible
// La taille de displayWaveform sera égale à width() (le nombre de pixels horizontaux)
//
void WaveformWidget::recalcCache()
{
    int w = width();
    if (w <= 0) return;

    displayWaveform.resize(w);
    // On efface le cache (remplit de 0) pour éviter les fantômes
    std::fill(displayWaveform.begin(), displayWaveform.end(), 0.0f);

    // On vérifie la taille réelle du vecteur en mémoire
    qint64 realSize = fullWaveform.size();
    if (realSize <= 0) {
        cacheValid = true;
        return;
    }

    for (int x = 0; x < w; ++x) {
        // Calcul des indices
        qint64 startSample = static_cast<qint64>((x + offsetPixels) * samplesPerPixel);
        qint64 endSample = static_cast<qint64>((x + offsetPixels + 1) * samplesPerPixel);
        
        // Bornage basique
        if (startSample >= realSize) break; 
        if (endSample > realSize) endSample = realSize;

        float maxVal = 0.0f;
        
        // --- DÉBUT DE LA BOUCLE SÉCURISÉE ---
        for (qint64 i = startSample; i < endSample; ++i) {
            
            // PROTECTION CRITIQUE CONTRE LE CRASH
            // Même si endSample dit qu'on peut y aller, on vérifie la mémoire réelle.
            if (i >= realSize) break; 
            
            float v = std::abs(fullWaveform[i]);
            if (v > maxVal)
                maxVal = v;
        }
        // --- FIN DE LA BOUCLE SÉCURISÉE ---

        displayWaveform[x] = maxVal;
    }
    cacheValid = true;
}

void WaveformWidget::restoreZoomState(double spp, int offset) 
{
    // 1. Appliquer le zoom
    if (spp < 1.0) spp = 1.0;
    samplesPerPixel = spp;

    // 2. Recalculer la limite max du scroll pour la NOUVELLE taille de fichier
    if (samplesPerPixel > 0) {
        int totalPixels = static_cast<int>(totalSamples / samplesPerPixel);
        int maxOffset = std::max(0, totalPixels - width());

        // 3. Clamper l'offset demandé pour qu'il ne dépasse pas le max
        if (offset > maxOffset) offset = maxOffset;
        if (offset < 0) offset = 0;
    }

    offsetPixels = offset;
    
    // 4. Mettre à jour l'IHM
    updateScrollBar();
    cacheValid = false;
    update();
}

//
// updateScrollBar() : le total d'unités en pixels correspond à totalSamples / samplesPerPixel
// Vérifier que le scroll (offsetPixels) reste valide après une coupe
void WaveformWidget::updateScrollBar()
{
    if (samplesPerPixel <= 0) return; // Sécurité division par zéro
    
    int totalPixels = static_cast<int>(totalSamples / samplesPerPixel);
    int maxOffset = std::max(0, totalPixels - width());
    
    // Si on a coupé la fin, l'offset actuel peut être hors limites. On le ramène.
    if (offsetPixels > maxOffset) {
        offsetPixels = maxOffset;
    }

    scrollBar->setMaximum(maxOffset);
    scrollBar->setPageStep(width());
    
    // On bloque les signaux pour ne pas rappeler handleScrollChanged inutilement
    bool oldState = scrollBar->blockSignals(true);
    scrollBar->setValue(offsetPixels);
    scrollBar->blockSignals(oldState);
}

//
// paintEvent() : utilise displayWaveform (recalculé si besoin)
//
void WaveformWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    
    // 1. Dessiner le fond global (Zone "Vide" par défaut)
    // On utilise un gris clair pour dire "pas de données ici"
    painter.fillRect(rect(), QColor(230, 230, 230)); 

    if (fullWaveform.isEmpty() || totalSamples <= 0) {
        painter.setPen(penText);
        painter.drawText(rect(), Qt::AlignCenter, tr("Aucun fichier audio chargé"));
        return;
    }

    if (!cacheValid) recalcCache();

    int w = width();
    int h = height();
    int midY = h / 2;

    // 2. Calculer où s'arrête le son visuellement
    int audioEndPixel = 0;
    if (samplesPerPixel > 0) {
        audioEndPixel = static_cast<int>(totalSamples / samplesPerPixel) - offsetPixels;
    }

    // 3. Dessiner la zone "Blanche" (Zone Audio active)
    // On ne dessine le blanc que là où il y a du son
    if (audioEndPixel > 0) {
        int drawWidth = std::min(w, audioEndPixel);
        painter.fillRect(0, 0, drawWidth, h, background);
    }

    // 4. Dessiner la forme d'onde (Optimisation par lots)
    painter.setPen(pen);
    int maxX = std::min(w, (int)displayWaveform.size());
    if (audioEndPixel < maxX) maxX = audioEndPixel; 

    // On vide le vecteur sans désallouer la mémoire (très rapide)
    m_lines.clear();
    
    // On s'assure qu'il a assez de capacité (allouera seulement si la fenêtre s'agrandit)
    if (m_lines.capacity() < maxX) {
        m_lines.reserve(maxX);
    }

    for (int x = 0; x < maxX; ++x) {
        float amp = displayWaveform[x];
        int y = static_cast<int>(amp * (h / 2));
        if (y == 0 && amp > 0.001f) y = 1; 
        
        m_lines.append(QLine(x, midY - y, x, midY + y));
    }
    
    painter.drawLines(m_lines);
    
    // (Optionnel) Ajouter une petite ligne verticale grise pour marquer la fin exacte du fichier
    if (audioEndPixel >= 0 && audioEndPixel < w) {
        painter.setPen(QColor(150, 150, 150));
        painter.drawLine(audioEndPixel, 0, audioEndPixel, h);
    }

    // 5. Dessiner la sélection
    if (hasSelection()) {
        double sPixel = selectionStartSample / samplesPerPixel;
        double ePixel = selectionEndSample / samplesPerPixel;
        int x1 = static_cast<int>(sPixel) - offsetPixels;
        int x2 = static_cast<int>(ePixel) - offsetPixels;
        
        if (x2 > 0 && x1 < w) {
             painter.fillRect(QRect(x1, 0, x2 - x1, h), QColor(0, 0, 255, 50));
        }
    }

    // 6. Dessiner la tête de lecture
    QPen playPen(Qt::red);
    playPen.setWidth(2);
    painter.setPen(playPen);
    int px = static_cast<int>(playheadSample / samplesPerPixel) - offsetPixels;
    
    // On dessine la tête même si elle est à la toute fin
    if (px >= 0 && px <= w) {
        painter.drawLine(px, 0, px, h);
    }
}

void WaveformWidget::scrollToPixel(int x) {
    if (samplesPerPixel <= 0) return;
    int totalWidth = static_cast<int>(fullWaveform.size() / samplesPerPixel);
    offsetPixels = std::clamp(x, 0, std::max(0, totalWidth - width()));
    updateScrollBar();
    cacheValid = false; 
    update();
}

int WaveformWidget::sampleToPixel(qint64 sample) const {
    if (samplesPerPixel <= 0) return 0;
    return static_cast<int>(sample / samplesPerPixel);
}


//
// mousePressEvent, mouseMoveEvent, mouseReleaseEvent : 
// Conversion de la position du clic en indice dans le signal complet
//
void WaveformWidget::mousePressEvent(QMouseEvent* event)
{
    if (!isLoaded || totalSamples == 0) return;
        
    int x = event->pos().x();
    pressStartX = x; // On mémorise le début du clic

    qint64 sample = static_cast<qint64>((x + offsetPixels) * samplesPerPixel);
    if (sample < 0) sample = 0;
    if (sample > totalSamples) sample = totalSamples;

    int threshold = static_cast<int>(5 * samplesPerPixel); 
    if (threshold < 5) threshold = 5;

    isDragging = false;
    isSelecting = false;
    
    if (event->button() == Qt::LeftButton) {
        // Logique pour modifier une sélection existante
        if (hasSelection() && std::abs(sample - selectionStartSample) <= threshold) {
            isDragging = true;
            fixedSelectionEdgeSample = selectionEndSample;
            setStartAndEnd(sample, fixedSelectionEdgeSample);
        } else if (hasSelection() && std::abs(sample - selectionEndSample) <= threshold) {
            isDragging = true;
            fixedSelectionEdgeSample = selectionStartSample;
            setStartAndEnd(sample, fixedSelectionEdgeSample);
        } else {
            // Nouvelle sélection
            isSelecting = true;
            fixedSelectionEdgeSample = sample;
            // On commence la sélection visuelle tout de suite pour la réactivité,
            // mais on l'annulera dans Release si c'est trop petit.
            setStartAndEnd(sample, fixedSelectionEdgeSample);
        }
        update();
    }
}

void WaveformWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!isDragging && !isSelecting)
        return;
        
    int x = event->pos().x();
    // On permet de scroller un peu en glissant hors de la fenêtre si on voulait peaufiner, 
    // mais ici on reste simple : on clamp aux pixels visibles
    /* 
       Note: Si la souris sort du widget, x peut être < 0 ou > width. 
       On laisse le calcul se faire mais on clamp le résultat final 'sample'.
    */

    qint64 sample = static_cast<qint64>((x + offsetPixels) * samplesPerPixel);
    
    // Protection
    if (sample < 0) sample = 0;
    if (sample > totalSamples) sample = totalSamples;

    setStartAndEnd(sample, fixedSelectionEdgeSample);
    update();
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (!isDragging && !isSelecting) return;
        
    int x = event->pos().x();
    
    // --- FILTRE ANTI-JITTER ---
    // Calculer la distance en pixels parcourue par la souris
    int distPixels = std::abs(x - pressStartX);
    
    // Si on était en mode "Création de sélection" (isSelecting)
    // et que la souris a bougé de moins de 5 pixels : C'EST UN CLIC SIMPLE
    if (isSelecting && distPixels < 5) {
        
        qint64 sample = static_cast<qint64>((x + offsetPixels) * samplesPerPixel);
        if (sample < 0) sample = 0;
        if (sample > totalSamples) sample = totalSamples;

        // --- CORRECTION ICI ---
        // Au lieu de mettre -1 (qui perd la position), on met 'sample'.
        // Comme start == end, AudioEditor saura qu'il n'y a pas de plage sélectionnée (boutons grisés),
        // MAIS il pourra lire la valeur 'start' pour savoir où lancer la lecture !
        resetSelection(sample); 
        
        setPlayheadPosition(sample);
        
        // On notifie l'éditeur avec la position réelle (start=sample, end=sample)
        emit selectionChanged(sample, sample);
    }
    else {
        // C'était un vrai drag (ou une modification de sélection existante)
        // On confirme la sélection
        qint64 sample = static_cast<qint64>((x + offsetPixels) * samplesPerPixel);
        if (sample < 0) sample = 0;
        if (sample > totalSamples) sample = totalSamples;

        setStartAndEnd(sample, fixedSelectionEdgeSample);
        emit selectionChanged(selectionStartSample, selectionEndSample);
    }
    
    isDragging = false;
    isSelecting = false;
    fixedSelectionEdgeSample = -1;
    update();
}

void WaveformWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // 1. On invalide le cache : la largeur en pixels a changé, 
    // il faut recalculer le nombre de points à afficher.
    cacheValid = false;
    // 2. On met à jour la scrollbar : la taille de la "page" (partie visible) a changé.
    updateScrollBar();
}


//
// Gestion de la roulette de la souris : appel de zoomIn/zoomOut
//
void WaveformWidget::wheelEvent(QWheelEvent* event)
{
    if (event->angleDelta().y() > 0)
        zoomIn();
    else
        zoomOut();
    event->accept();
}

//
// zoomIn() : on diminue samplesPerPixel pour zoomer dans le signal
//
void WaveformWidget::zoomIn()
{
    if (totalSamples == 0) return;
    
    // 1. Sauvegarder la position actuelle de la tête de lecture
    // On veut que ce point reste au centre de l'écran après le zoom
    qint64 anchorSample = playheadSample;

    // 2. Appliquer le zoom
    samplesPerPixel /= 1.25;
    if (samplesPerPixel < 1.0)
        samplesPerPixel = 1.0; 

    // 3. Recalculer l'offset pour centrer la tête de lecture
    if (width() > 0) {
        int centerPixel = width() / 2;
        // Le nouvel offset = (PositionAbsolueDuSample - MoitiéEcran)
        int newOffset = static_cast<int>(anchorSample / samplesPerPixel) - centerPixel;
        
        // On utilise scrollToPixel qui gère les limites (<0 et >max) et invalide le cache
        scrollToPixel(newOffset);
    } else {
        cacheValid = false;
        updateScrollBar();
        update();
    }
    
    // Emission signal pour UI
    double baseZoom = (width() > 0) ? static_cast<double>(totalSamples) / width() : 1.0;
    if (samplesPerPixel <= 0) samplesPerPixel = 1.0;
    
    emit zoomChanged(QString("x%1").arg(baseZoom / samplesPerPixel, 0, 'f', 1));
}

//
// zoomOut() : on augmente samplesPerPixel pour dézoomer (afficher plus du signal)
//
void WaveformWidget::zoomOut()
{
    if (totalSamples == 0 || width() == 0) return;
    
    // 1. Sauvegarder la position
    qint64 anchorSample = playheadSample;
    
    double baseZoom = static_cast<double>(totalSamples) / width();
    
    // 2. Appliquer le zoom
    samplesPerPixel *= 1.25;
    if (samplesPerPixel > baseZoom)
        samplesPerPixel = baseZoom; // ne pas dépasser l'affichage complet

    // 3. Recalculer l'offset pour centrer la tête de lecture
    if (width() > 0) {
        int centerPixel = width() / 2;
        int newOffset = static_cast<int>(anchorSample / samplesPerPixel) - centerPixel;
        scrollToPixel(newOffset);
    } else {
        cacheValid = false;
        updateScrollBar();
        update();
    }

    double factor = baseZoom / samplesPerPixel;
    emit zoomChanged(QString("x%1").arg(factor, 0, 'f', 1));
}

void WaveformWidget::handleScrollChanged(int value)
{
    offsetPixels = value;
    cacheValid = false;
    update();
}

//
// resetZoom() : réinitialise samplesPerPixel pour afficher tout le signal sur la largeur du widget
//
void WaveformWidget::resetZoom()
{
    if (width() > 0 && totalSamples > 0) {
        samplesPerPixel = static_cast<double>(totalSamples) / width();
        offsetPixels = 0;
        cacheValid = false;
        updateScrollBar();
        update();
        emit zoomChanged("x1.0");
    }
}