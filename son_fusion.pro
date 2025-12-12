# ============================================================================
# SonFusion - Fichier de projet Qt multiplateforme
# Windows, macOS, Linux
# ============================================================================

QT += core gui widgets multimedia
CONFIG += c++17

TARGET = son_fusion
TEMPLATE = app

# Mode Dialog pour AudioEditor (utilisé par mainwindow)
DEFINES += USE_DIALOG_MODE

# ============================================================================
# SOURCES ET HEADERS
# ============================================================================

SOURCES += \
    audioeditor.cpp \
    audiomerger.cpp \
    customtooltip.cpp \
    main.cpp \
    mainwindow.cpp \
    waveformwidget.cpp

HEADERS += \
    audioeditor.h \
    audiomerger.h \
    customtooltip.h \
    mainwindow.h \
    waveformwidget.h

FORMS += \
    audioeditor.ui

# ============================================================================
# CONFIGURATION MACOS
# ============================================================================

macx {
    # Icône de l'application
    ICON = icones/icon.icns
    
    # Informations du bundle
    QMAKE_INFO_PLIST = Info.plist
    
    # Version minimale de macOS
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
    
    # ========================================================================
    # DEPLOIEMENT DES RESSOURCES DANS LE BUNDLE
    # ========================================================================
    
    # FFmpeg et FFprobe (optionnel, sinon utilise le système)
    exists($$PWD/bin/macos/ffmpeg) {
        ffmpeg.files = $$PWD/bin/macos/ffmpeg
        ffmpeg.path = Contents/MacOS
        QMAKE_BUNDLE_DATA += ffmpeg
        message("FFmpeg sera inclus dans le bundle depuis bin/macos/")
    }
    
    exists($$PWD/bin/macos/ffprobe) {
        ffprobe.files = $$PWD/bin/macos/ffprobe
        ffprobe.path = Contents/MacOS
        QMAKE_BUNDLE_DATA += ffprobe
        message("FFprobe sera inclus dans le bundle depuis bin/macos/")
    }
    
    # Dossier icones
    icones.files = $$PWD/icones
    icones.path = Contents/Resources
    QMAKE_BUNDLE_DATA += icones
    
    message("Configuration macOS activée")
}

# ============================================================================
# CONFIGURATION WINDOWS
# ============================================================================

win32 {
    # Icône de l'application Windows
    RC_ICONS = icones/fusionner.ico
    
    # Copier FFmpeg.exe et FFprobe.exe dans le dossier de sortie
    CONFIG(debug, debug|release) {
        DEST_DIR = $$OUT_PWD/debug
    } else {
        DEST_DIR = $$OUT_PWD/release
    }
    
    # Copier FFmpeg
    exists($$PWD/bin/windows/ffmpeg.exe) {
        QMAKE_POST_LINK += $$QMAKE_COPY $$shell_quote($$PWD/bin/windows/ffmpeg.exe) $$shell_quote($$DEST_DIR) $$escape_expand(\n\t)
        message("FFmpeg.exe sera copié dans le dossier de build")
    } else {
        warning("FFmpeg.exe non trouvé dans bin/windows/")
    }
    
    # Copier FFprobe
    exists($$PWD/bin/windows/ffprobe.exe) {
        QMAKE_POST_LINK += $$QMAKE_COPY $$shell_quote($$PWD/bin/windows/ffprobe.exe) $$shell_quote($$DEST_DIR) $$escape_expand(\n\t)
        message("FFprobe.exe sera copié dans le dossier de build")
    } else {
        warning("FFprobe.exe non trouvé dans bin/windows/")
    }
    
    # Copier le dossier icones
    exists($$PWD/icones) {
        QMAKE_POST_LINK += $$QMAKE_COPY_DIR $$shell_quote($$PWD/icones) $$shell_quote($$DEST_DIR/icones) $$escape_expand(\n\t)
    }
    
    message("Configuration Windows activée")
}

# ============================================================================
# CONFIGURATION LINUX
# ============================================================================

unix:!macx {
    # Installation dans /usr/local/bin
    target.path = /usr/local/bin
    INSTALLS += target
    
    # Installation des icones
    icones.path = /usr/share/son_fusion/icones
    icones.files = icones/*
    INSTALLS += icones
    
    # Fichier .desktop pour l'intégration au menu
    desktop.path = /usr/share/applications
    desktop.files = son_fusion.desktop
    # INSTALLS += desktop  # Décommenter si vous créez le fichier .desktop
    
    # Icône pour le système
    icon.path = /usr/share/icons/hicolor/256x256/apps
    icon.files = icones/fusionner.png
    # INSTALLS += icon  # Décommenter si vous avez l'icône PNG
    
    message("Configuration Linux activée")
    message("FFmpeg et FFprobe seront utilisés depuis le PATH système")
}

# ============================================================================
# MESSAGES DE CONFIGURATION
# ============================================================================

message("========================================")
message("Configuration du projet SonFusion")
message("========================================")
message("Target: $$TARGET")
message("Qt Version: $$QT_VERSION")
message("Build dir: $$OUT_PWD")
message("========================================")

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# ============================================================================
# VÉRIFICATIONS
# ============================================================================

!exists($$PWD/audioeditor.cpp) {
    error("audioeditor.cpp introuvable !")
}

!exists($$PWD/mainwindow.cpp) {
    error("mainwindow.cpp introuvable !")
}