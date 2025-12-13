# Son Fusion

[![License: GPL](https://img.shields.io/badge/License-GPL-blue.svg)](LICENSE)
![Platform](https://img.shields.io/badge/Platform-Windows_|_macOS_|_Linux-lightgrey.svg)
[![Qt](https://img.shields.io/badge/Built%20with-Qt6-green.svg)](https://www.qt.io/)

**Son Fusion** est une application gratuite et open-source permettant de fusionner et d'éditer simplement vos fichiers audio. Idéal pour les projets scolaires, les webradios, les podcasts ou les interviews.
 
    
| Fenêtre Principale | Éditeur Audio |
|:------------------:|:-------------:|
| ![Capture 1](https://estrade03.forge.apps.education.fr/son_fusion/img/capture1.jpg) | <img src="https://estrade03.forge.apps.education.fr/son_fusion/img/capture2.jpg" width="70%"> <img src="https://estrade03.forge.apps.education.fr/son_fusion/img/capture3.jpg" width="30%"> |

## 📥 Téléchargements

Choisissez la version correspondant à votre système d'exploitation :

### 🪟 Windows
👉 **[Télécharger Son Fusion pour Windows](https://forge.apps.education.fr/estrade03/son_fusion/-/raw/main/dist/SonFusion_Setup_2.2.exe)**

> ⚠️ **Important :** L'application n'étant pas signée numériquement (certificat payant), Windows peut afficher un écran bleu "Windows a protégé votre ordinateur" (SmartScreen).
> *   Cliquez sur **"Informations complémentaires"**.
> *   Puis cliquez sur le bouton **"Exécuter quand même"**.

### 🍎 macOS
👉 **[Télécharger Son Fusion pour macOS](https://forge.apps.education.fr/estrade03/son_fusion/-/raw/main/dist/SonFusion_macOS_2.2.dmg?ref_type=heads)**

> ⚠️ **Important :** Au premier lancement, Apple bloquera l'application car le développeur n'est pas identifié.
> *   Faites un **Clic-Droit** (ou `Ctrl` + Clic) sur l'icône de l'application.
> *   Sélectionnez **Ouvrir** dans le menu contextuel.
> *   Cliquez à nouveau sur **Ouvrir** dans la fenêtre de dialogue qui apparaît.

### 🐧 Linux (Ubuntu/Debian)
👉 **[Télécharger Son Fusion pour Linux](https://forge.apps.education.fr/estrade03/son_fusion/-/raw/main/dist/SonFusion_Linux_2.2?ref_type=heads)**

> ⚠️ **Prérequis :** Vous devez installer **FFmpeg** pour que l'application fonctionne correctement (traitement audio).
> Ouvrez un terminal et lancez la commande suivante :
> ```bash
> sudo apt install ffmpeg
> ```
> *Note : Pensez à rendre le fichier téléchargé exécutable (Clic-droit > Propriétés > Permissions > Autoriser l'exécution ou `chmod +x SonFusion_Linux_2.1`).*

## 🚀 Fonctionnalités

Cette application a été conçue pour simplifier le traitement audio par lots :

*   **Fusion de fichiers :** Combinez plusieurs fichiers audio (WAV, MP3, FLAC, OGG, M4A, etc.) en un seul fichier unique.
*   **Éditeur Audio Intégré :**
    *   Visualisation de la forme d'onde (Waveform).
    *   **Couper** : Supprimez facilement les parties indésirables.
    *   **Normaliser** : Harmonisez le volume d'une sélection ou du fichier entier.
    *   Zoom avant/arrière pour une édition précise.
*   **Gestion de liste :** Organisez l'ordre de vos pistes avant la fusion (Monter/Descendre).
*   **Compatibilité :** Utilise la puissance de **FFmpeg** pour traiter une grande variété de formats audio.

## 🎯 Cas d'usage

*   **En classe :** Pour oraliser un livre en fusionnant les enregistrements de plusieurs élèves.
*   **Journalisme / Webradio :** Assembler un reportage ou une interview scindée en plusieurs fichiers.
*   **Musique :** Créer des compilations ou des mix simples.

## 📖 Guide d'utilisation

1.  **Sélectionner le dossier** contenant vos fichiers audio.
2.  **Organiser la liste** : Utilisez les flèches pour définir l'ordre de lecture.
    *   *Conseil :* Nommez vos fichiers en les numérotant (ex: `01.Intro.mp3`, `02.Corps.mp3`) pour un tri automatique.
3.  **Éditer (Optionnel)** : Double-cliquez ou sélectionnez "Éditer" pour couper des silences ou normaliser le volume.
4.  **Fusionner** : Choisissez le format de sortie (MP3, WAV, etc.) et cliquez sur le bouton de fusion.

## 📄 Licence

Ce projet est distribué sous la licence **GNU General Public License (GPL)**. Voir le fichier [LICENSE](LICENSE) pour plus de détails.

## 👤 Auteur et Crédits

*   **Développeur :** Stéphane Verdier
*   **Technologie :** C++ / Framework Qt
*   **Moteur Audio :** FFmpeg
*   **Icônes :** [Flaticon](https://www.flaticon.com/) (Auteurs : Kumakamu, Iconixar)

---
*© 2025 Stéphane Verdier - Freeware*
