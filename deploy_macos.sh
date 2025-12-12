#!/bin/bash

# ============================================================================
# Script de déploiement macOS pour SonFusion Audio Editor
# ============================================================================

# Configuration
APP_NAME="SonFusion"
APP_VERSION="1.0.0"
BUILD_DIR="build-release"
DEPLOY_DIR="deploy_macos"

# Détection automatique de Qt
if command -v brew &> /dev/null && brew --prefix qt &> /dev/null; then
    QT_PATH="$(brew --prefix qt)/bin"
elif [ -d "$HOME/Qt" ]; then
    # Chercher la dernière version de Qt installée
    QT_PATH=$(find "$HOME/Qt" -type d -name "bin" | grep "macos" | sort -V | tail -1)
else
    echo "❌ Qt non trouvé. Veuillez définir QT_PATH manuellement."
    exit 1
fi

# Couleurs pour les messages
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   Déploiement macOS - ${APP_NAME}${NC}"
echo -e "${BLUE}========================================${NC}\n"
echo -e "${YELLOW}Qt Path: ${QT_PATH}${NC}\n"

# Vérification de FFmpeg
if ! command -v ffmpeg &> /dev/null; then
    echo -e "${RED}⚠️  FFmpeg n'est pas installé !${NC}"
    echo -e "Installation via Homebrew : ${YELLOW}brew install ffmpeg${NC}\n"
    read -p "Voulez-vous continuer sans FFmpeg embarqué ? (o/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Oo]$ ]]; then
        exit 1
    fi
    SKIP_FFMPEG=true
else
    echo -e "${GREEN}✓ FFmpeg trouvé : $(which ffmpeg)${NC}\n"
    SKIP_FFMPEG=false
fi

# 1. Compilation en mode Release
echo -e "${GREEN}[1/7] Compilation en mode Release...${NC}"
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# Détection de l'architecture
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    SPEC="macx-clang CONFIG+=arm64"
    echo -e "${YELLOW}Architecture détectée : Apple Silicon (ARM64)${NC}"
else
    SPEC="macx-clang CONFIG+=x86_64"
    echo -e "${YELLOW}Architecture détectée : Intel (x86_64)${NC}"
fi

${QT_PATH}/qmake ../SonFusion.pro -spec $SPEC CONFIG+=release
make clean
make -j$(sysctl -n hw.ncpu)

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Erreur de compilation${NC}"
    exit 1
fi

cd ..

# 2. Création du dossier de déploiement
echo -e "${GREEN}[2/7] Préparation du dossier de déploiement...${NC}"
rm -rf ${DEPLOY_DIR}
mkdir -p ${DEPLOY_DIR}

# Vérifier que le .app existe
if [ ! -d "${BUILD_DIR}/${APP_NAME}.app" ]; then
    echo -e "${RED}❌ ${APP_NAME}.app non trouvé dans ${BUILD_DIR}${NC}"
    exit 1
fi

cp -r ${BUILD_DIR}/${APP_NAME}.app ${DEPLOY_DIR}/

# 3. Copie de FFmpeg dans le bundle
if [ "$SKIP_FFMPEG" = false ]; then
    echo -e "${GREEN}[3/7] Installation de FFmpeg dans le bundle...${NC}"
    mkdir -p ${DEPLOY_DIR}/${APP_NAME}.app/Contents/MacOS
    
    # Copier FFmpeg depuis le système
    FFMPEG_PATH=$(which ffmpeg)
    cp "$FFMPEG_PATH" ${DEPLOY_DIR}/${APP_NAME}.app/Contents/MacOS/
    chmod +x ${DEPLOY_DIR}/${APP_NAME}.app/Contents/MacOS/ffmpeg
    
    echo -e "${GREEN}   ✓ FFmpeg copié dans le bundle${NC}"
else
    echo -e "${YELLOW}[3/7] FFmpeg non inclus dans le bundle${NC}"
fi

# 4. Copie des ressources supplémentaires
echo -e "${GREEN}[4/7] Copie des ressources...${NC}"
mkdir -p ${DEPLOY_DIR}/${APP_NAME}.app/Contents/Resources

# Liste des dossiers de ressources
RESOURCE_DIRS="generic iconengines icones imageformats multimedia networkinformation platforms styles tls translations"

for dir in $RESOURCE_DIRS; do
    if [ -d "$dir" ]; then
        cp -r $dir ${DEPLOY_DIR}/${APP_NAME}.app/Contents/Resources/
        echo -e "   ✓ Copié: $dir"
    else
        echo -e "${YELLOW}   ⚠ Manquant: $dir${NC}"
    fi
done

# 5. Déploiement automatique avec macdeployqt
echo -e "${GREEN}[5/7] Déploiement des frameworks Qt...${NC}"
${QT_PATH}/macdeployqt ${DEPLOY_DIR}/${APP_NAME}.app -verbose=2

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Erreur lors du déploiement Qt${NC}"
    exit 1
fi

# 6. Vérification des dépendances
echo -e "${GREEN}[6/7] Vérification des dépendances...${NC}"
echo -e "${YELLOW}Dépendances de l'exécutable principal :${NC}"
otool -L ${DEPLOY_DIR}/${APP_NAME}.app/Contents/MacOS/${APP_NAME} | grep -v ":" | sed 's/^/   /'

if [ "$SKIP_FFMPEG" = false ]; then
    echo -e "\n${YELLOW}Dépendances de FFmpeg :${NC}"
    otool -L ${DEPLOY_DIR}/${APP_NAME}.app/Contents/MacOS/ffmpeg | grep -v ":" | head -5 | sed 's/^/   /'
fi

# 7. Création du DMG
echo -e "${GREEN}[7/7] Création du fichier DMG...${NC}"
${QT_PATH}/macdeployqt ${DEPLOY_DIR}/${APP_NAME}.app -dmg

if [ $? -eq 0 ] && [ -f "${DEPLOY_DIR}/${APP_NAME}.dmg" ]; then
    # Renommer le DMG avec la version et l'architecture
    DMG_NAME="${APP_NAME}_v${APP_VERSION}_macOS_${ARCH}.dmg"
    mv ${DEPLOY_DIR}/${APP_NAME}.dmg ${DEPLOY_DIR}/${DMG_NAME}
    
    DMG_SIZE=$(du -h "${DEPLOY_DIR}/${DMG_NAME}" | cut -f1)
    
    echo -e "\n${GREEN}========================================${NC}"
    echo -e "${GREEN}   ✅ Déploiement terminé avec succès !${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "📦 Package : ${DEPLOY_DIR}/${DMG_NAME}"
    echo -e "📏 Taille  : ${DMG_SIZE}"
    echo -e "🏗️  Arch.  : ${ARCH}"
    
    if [ "$SKIP_FFMPEG" = true ]; then
        echo -e "\n${YELLOW}⚠️  ATTENTION : FFmpeg n'est PAS inclus dans le bundle${NC}"
        echo -e "${YELLOW}   Les utilisateurs devront installer FFmpeg séparément${NC}"
    fi
    
    echo -e "\n${BLUE}Pour tester :${NC}"
    echo -e "   open ${DEPLOY_DIR}/${APP_NAME}.app"
    
else
    echo -e "${RED}❌ Erreur lors de la création du DMG${NC}"
    exit 1
fi