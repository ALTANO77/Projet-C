# Description détaillée du code principal (main.c)

## Vue d'ensemble

Le fichier `main.c` constitue le cœur du jeu "Gros Nounours 2D". Il gère l'ensemble de la logique principale du jeu, incluant la navigation entre les différents écrans, la gestion des minijeux, le système de progression, et l'affichage de l'interface utilisateur.

---

## 1. INCLUSIONS ET DÉPENDANCES (lignes 11-20)

### Description
Cette section inclut toutes les bibliothèques nécessaires au fonctionnement du jeu.

### Détails
- **raylib.h** : Bibliothèque graphique utilisée pour le rendu, l'audio et la gestion des entrées
- **math.h** : Fonctions mathématiques (utilisées pour les calculs de positionnement)
- **stdio.h** : Entrées/sorties (lecture/écriture de fichiers de configuration)
- **string.h** : Manipulation de chaînes de caractères
- **Headers des minijeux** : Inclusion des interfaces de chaque minijeu (Traffic, Pousse-Pousse, Pendu, Cerise sur gâteau, Boutique)

### Rôle
Permet au compilateur d'accéder aux fonctions et structures nécessaires au jeu.

---

## 2. DÉFINITIONS DES TYPES ET STRUCTURES (lignes 22-154)

### 2.1 Énumération GameState (lignes 28-38)

**Rôle** : Définit tous les états possibles du jeu (machine à états finis).

**États disponibles** :
- `STATE_TITLE` : Écran d'accueil du jeu
- `STATE_HUB` : Menu principal où le joueur choisit une zone
- `STATE_ZONE_JARDIN/CHAMBRE/GRENIER/CUISINE` : Écrans d'information avant chaque minijeu
- `STATE_SHOP` : Boutique pour acheter des tenues
- `STATE_MINIJEU` : Pendant qu'un minijeu est actif
- `STATE_PAUSE` : Écran de pause

**Utilité** : Permet de gérer la navigation entre les différents écrans du jeu de manière structurée.

---

### 2.2 Énumération ZoneId (lignes 41-48)

**Rôle** : Identifie les différentes zones du jeu.

**Zones** :
- `ZONE_NONE` : Aucune zone active
- `ZONE_JARDIN/CHAMBRE/GRENIER/CUISINE` : Les 4 zones principales
- `ZONE_COUNT` : Constante utilisée pour dimensionner les tableaux

**Utilité** : Permet d'associer chaque zone à son minijeu correspondant et de suivre la progression.

---

### 2.3 Structure ZoneProgress (lignes 50-54)

**Rôle** : Stocke la progression d'une zone.

**Champs** :
- `completed` : Booléen indiquant si le minijeu de cette zone a été complété

**Utilité** : Permet de savoir quels minijeux ont été terminés et d'afficher l'état de progression.

---

### 2.4 Structure RectRatios (lignes 56-64)

**Rôle** : Stocke la position et la taille d'un rectangle en pourcentage de l'écran.

**Champs** :
- `left`, `top` : Position en pourcentage (0.0 = bord gauche/haut, 1.0 = bord droit/bas)
- `width`, `height` : Dimensions en pourcentage

**Utilité** : Permet de positionner les éléments de l'interface de manière relative, ce qui assure l'adaptation à différentes résolutions d'écran.

**Exemple** : Un portail à `left=0.1, top=0.2, width=0.15, height=0.3` sera toujours à 10% de la gauche, 20% du haut, avec 15% de largeur et 30% de hauteur, quelle que soit la résolution.

---

### 2.5 Structure BearLayout (lignes 66-71)

**Rôle** : Stocke la position et la taille du nounours.

**Champs** :
- `left`, `top` : Position en pourcentage
- `heightRatio` : Hauteur en pourcentage de l'écran

**Utilité** : Positionne le nounours de manière adaptative. La largeur est calculée automatiquement pour conserver les proportions de l'image.

---

### 2.6 Structure Game (lignes 73-148)

**Rôle** : Structure principale contenant toutes les données du jeu.

**Sections principales** :

#### État et progression
- `state` : État actuel du jeu
- `activeZone` : Zone actuellement active
- `collectibles` : Nombre de pièces collectées
- `progress[]` : Tableau de progression pour chaque zone

#### Minijeu actuel
- `currentMinigame` : Interface du minijeu en cours
- `currentMinigameName` : Nom du minijeu (pour debug)
- `minigameCompleted` : Indique si le minijeu est complété

#### Textures et ressources graphiques
- Textures du menu, de l'écran d'accueil, des tenues, des fonds de zones
- Flags `hasXXX` pour indiquer si chaque texture est chargée

#### Mode debug
- `showDebugOverlay` : Active/désactive l'affichage des rectangles de collision
- Variables de déplacement pour repositionner les éléments en mode debug

#### Positions (layout)
- `portalLayouts[]` : Positions des portails des zones
- `shopPortalLayout` : Position du portail de la boutique
- `titleRectLayout` : Position du rectangle cliquable sur l'écran d'accueil
- `bearLayout` : Position du nounours

#### Audio
- `music` : Musique de fond
- `musicMuted` : État de la musique (mute/unmute)
- Icônes de son

#### Boutique
- Données synchronisées avec le module boutique
- Informations sur les tenues (prix, possession, etc.)

#### Interface
- `hoveredPortal` : Portail actuellement survolé
- `inactivityTimer` : Timer pour afficher un message d'aide après inactivité

**Utilité** : Centralise toutes les données du jeu dans une seule structure, facilitant la gestion et le passage de paramètres.

---

### 2.7 Structure HubPortalInfo (lignes 150-154)

**Rôle** : Associe une zone à son nom d'affichage.

**Champs** :
- `zone` : Identifiant de la zone
- `label` : Nom à afficher à l'utilisateur

**Utilité** : Permet d'afficher des noms lisibles pour chaque zone dans l'interface.

---

## 3. CONSTANTES (lignes 156-198)

### 3.1 HUB_PORTALS (lignes 161-166)

**Rôle** : Tableau constant associant chaque zone à son nom d'affichage.

**Contenu** :
- Jardin → "Jardin"
- Chambre → "Puzzle"
- Grenier → "Bibliothèque"
- Cuisine → "Cuisine"

**Utilité** : Centralise les noms des zones pour faciliter les modifications.

---

### 3.2 Positions par défaut (lignes 170-184)

**Rôle** : Définit les positions par défaut de tous les éléments de l'interface.

**Constantes** :
- `DEFAULT_PORTAL_LAYOUTS[]` : Positions des 4 portails de zones
- `DEFAULT_SHOP_PORTAL` : Position du portail de la boutique
- `DEFAULT_TITLE_RECT` : Position du rectangle cliquable de l'écran d'accueil
- `DEFAULT_BEAR_LAYOUT` : Position du nounours

**Utilité** : Valeurs utilisées si le fichier de configuration n'existe pas ou est corrompu.

---

### 3.3 Autres constantes (lignes 186-198)

- `PORTAL_KEYS[]` : Clés utilisées dans le fichier de configuration
- `LAYOUT_FILE` : Chemin du fichier de configuration
- `ZONE_BG_FILES[]` : Chemins des fichiers de fonds de zones

**Utilité** : Centralise les chemins et noms pour faciliter la maintenance.

---

## 4. FONCTIONS UTILITAIRES (lignes 212-266)

### 4.1 clampf() (lignes 218-222)

**Rôle** : Limite une valeur entre un minimum et un maximum.

**Paramètres** :
- `v` : Valeur à limiter
- `lo` : Minimum
- `hi` : Maximum

**Retour** : La valeur limitée entre `lo` et `hi`.

**Exemple** : `clampf(15, 10, 20)` retourne 15, `clampf(5, 10, 20)` retourne 10.

**Utilité** : Empêche les valeurs de sortir des limites acceptables (par exemple, empêcher un élément de sortir de l'écran).

---

### 4.2 drawCentered() (lignes 229-234)

**Rôle** : Dessine un texte centré horizontalement.

**Paramètres** :
- `txt` : Texte à afficher
- `y` : Position verticale
- `size` : Taille de la police
- `c` : Couleur du texte

**Fonctionnement** :
1. Mesure la largeur du texte
2. Calcule la position X pour centrer le texte
3. Dessine le texte à cette position

**Utilité** : Simplifie l'affichage de textes centrés, utilisé pour les écrans d'information.

---

### 4.3 drawGround() (lignes 237-245)

**Rôle** : Dessine un sol simple (fallback si aucune texture n'est chargée).

**Fonctionnement** :
- Dessine un rectangle vert (herbe) en bas de l'écran
- Dessine un rectangle marron (terre) au-dessus

**Utilité** : Assure qu'il y a toujours quelque chose à afficher, même si les textures ne sont pas chargées.

---

### 4.4 loadTextureIfAvailable() (lignes 249-266)

**Rôle** : Charge une texture depuis un fichier si elle existe.

**Paramètres** :
- `path` : Chemin du fichier image

**Retour** : La texture chargée, ou une texture vide si le fichier n'existe pas.

**Fonctionnement** :
1. Vérifie si le fichier existe
2. Si oui, charge l'image
3. Convertit l'image en texture
4. Libère l'image (on n'a besoin que de la texture)
5. Retourne la texture

**Utilité** : Permet au jeu de fonctionner même si certaines images manquent, sans planter.

---

## 5. GESTION DES MINIJEUX (lignes 268-311)

### 5.1 prepareMinigameSession() (lignes 276-289)

**Rôle** : Prépare le lancement d'un minijeu.

**Paramètres** :
- `g` : Structure du jeu
- `api` : Interface du minijeu à lancer
- `prettyName` : Nom du minijeu (pour debug)

**Fonctionnement** :
1. Enregistre le minijeu dans la structure du jeu
2. Réinitialise le flag de complétion
3. Appelle la fonction d'initialisation du minijeu si elle existe
4. Change l'état du jeu à `STATE_MINIJEU`

**Utilité** : Centralise la logique de démarrage d'un minijeu, assurant une initialisation correcte.

---

### 5.2 finalizeMinigame() (lignes 292-311)

**Rôle** : Finalise un minijeu quand on le quitte.

**Fonctionnement** :
1. Marque la zone comme complétée
2. Appelle la fonction de nettoyage du minijeu si elle existe
3. Réinitialise les données du minijeu
4. Retourne au hub

**Utilité** : Assure un nettoyage propre des ressources et met à jour la progression.

---

## 6. GESTION DES POSITIONS (LAYOUT) (lignes 313-433)

### 6.1 initDefaultLayout() (lignes 318-329)

**Rôle** : Initialise les positions avec les valeurs par défaut.

**Fonctionnement** :
- Copie les constantes de positions par défaut dans la structure du jeu
- Réinitialise les flags de déplacement

**Utilité** : Utilisé au démarrage ou si le fichier de configuration est absent.

---

### 6.2 portalIndexFromName() (lignes 333-340)

**Rôle** : Trouve l'index d'un portail à partir de son nom.

**Paramètres** :
- `name` : Nom du portail (ex: "jardin")

**Retour** : L'index du portail, ou -1 si non trouvé.

**Utilité** : Utilisé lors du chargement du fichier de configuration pour identifier les portails.

---

### 6.3 clampPortalLayout() (lignes 343-351)

**Rôle** : Limite les valeurs d'un rectangle pour qu'il reste dans l'écran.

**Fonctionnement** :
1. Limite la taille entre 2% et 100% de l'écran
2. Limite la position pour que le rectangle ne sorte pas de l'écran

**Utilité** : Empêche les éléments d'être positionnés hors de l'écran, même avec des valeurs erronées.

---

### 6.4 clampBearLayout() (lignes 354-358)

**Rôle** : Limite les valeurs de la position du nounours.

**Fonctionnement** : Similaire à `clampPortalLayout()`, mais adapté à la structure `BearLayout`.

**Utilité** : Assure que le nounours reste visible à l'écran.

---

### 6.5 loadMenuLayout() (lignes 362-407)

**Rôle** : Charge les positions depuis le fichier de configuration.

**Fonctionnement** :
1. Initialise avec les valeurs par défaut
2. Ouvre le fichier de configuration
3. Lit le fichier ligne par ligne
4. Parse chaque ligne selon son format :
   - `portal_jardin=0.065,0.25,0.11,0.30` pour les portails
   - `bear=0.021,0.113,0.85` pour le nounours
5. Applique les valeurs lues
6. Ferme le fichier

**Format du fichier** :
```
portal_jardin=0.065,0.25,0.11,0.30
portal_chambre=0.225,0.25,0.11,0.30
bear=0.021,0.113,0.85
```

**Utilité** : Permet de sauvegarder et restaurer les positions des éléments, notamment après un repositionnement en mode debug.

---

### 6.6 saveMenuLayout() (lignes 410-433)

**Rôle** : Sauvegarde les positions dans le fichier de configuration.

**Fonctionnement** :
1. Ouvre le fichier en écriture
2. Écrit les positions de chaque portail
3. Écrit la position du portail de la boutique
4. Écrit la position du nounours
5. Ferme le fichier

**Utilité** : Permet de persister les modifications faites en mode debug.

---

## 7. CALCUL DES RECTANGLES (lignes 435-538)

### 7.1 computePortalRect() (lignes 441-452)

**Rôle** : Calcule le rectangle d'un portail de zone en pixels.

**Paramètres** :
- `g` : Structure du jeu
- `idx` : Index du portail

**Retour** : Rectangle en pixels (coordonnées absolues).

**Fonctionnement** :
1. Récupère la taille de l'écran
2. Récupère les ratios de position du portail
3. Convertit les pourcentages en pixels
4. Retourne le rectangle

**Utilité** : Convertit les positions relatives (pourcentages) en coordonnées absolues utilisables pour la détection de collision et l'affichage.

---

### 7.2 computeShopPortalRect() (lignes 455-466)

**Rôle** : Calcule le rectangle du portail de la boutique.

**Fonctionnement** : Identique à `computePortalRect()`, mais pour le portail de la boutique.

---

### 7.3 computeTitleRect() (lignes 469-480)

**Rôle** : Calcule le rectangle cliquable de l'écran d'accueil.

**Fonctionnement** : Identique aux fonctions précédentes, mais pour le rectangle de l'écran d'accueil.

---

### 7.4 getBearWidthRatio() (lignes 483-497)

**Rôle** : Calcule le ratio de largeur du nounours.

**Fonctionnement** :
1. Vérifie si une texture est disponible
2. Calcule le ratio largeur/hauteur de l'image
3. Prend en compte le ratio de l'écran
4. Retourne le ratio de largeur

**Utilité** : Permet de calculer la largeur du nounours en conservant les proportions de l'image.

---

### 7.5 computeBearRect() (lignes 500-531)

**Rôle** : Calcule le rectangle du nounours en pixels.

**Fonctionnement** :
1. Choisit la texture à utiliser (tenue actuelle ou par défaut)
2. Calcule la hauteur en pixels
3. Calcule la largeur en conservant les proportions
4. Retourne le rectangle

**Utilité** : Détermine où et à quelle taille afficher le nounours.

---

### 7.6 clampBearToScreen() (lignes 534-538)

**Rôle** : Assure que le nounours reste dans l'écran.

**Fonctionnement** :
1. Calcule le ratio de largeur
2. Limite la position pour que le nounours ne sorte pas de l'écran

**Utilité** : Appelé régulièrement pour maintenir le nounours visible.

---

## 8. AFFICHAGE (lignes 540-760)

### 8.1 drawMenuBackground() (lignes 546-563)

**Rôle** : Dessine le fond du menu principal.

**Fonctionnement** :
1. Choisit la texture de fond par défaut
2. Si un portail est survolé, utilise le fond de cette zone
3. Affiche la texture si elle existe, sinon dessine un sol simple

**Utilité** : Fournit un feedback visuel au survol des portails.

---

### 8.2 drawBearCloseup() (lignes 566-591)

**Rôle** : Dessine le nounours en gros plan.

**Fonctionnement** :
1. Choisit la texture à utiliser (tenue actuelle ou par défaut)
2. Calcule où afficher le nounours
3. Dessine la texture

**Utilité** : Affiche le personnage principal avec la tenue sélectionnée.

---

### 8.3 drawMinigameStatusTable() (lignes 594-626)

**Rôle** : Dessine le tableau montrant l'état des minijeux.

**Fonctionnement** :
1. Dessine un fond semi-transparent
2. Affiche le titre et les en-têtes
3. Pour chaque zone, affiche le nom et le statut (Terminée/Non fait)
4. Utilise des couleurs différentes selon le statut (vert/orange)

**Utilité** : Permet au joueur de voir rapidement sa progression.

---

### 8.4 drawCoinCounter() (lignes 629-648)

**Rôle** : Dessine le compteur de pièces en haut à droite.

**Fonctionnement** :
1. Formate le texte avec le nombre de pièces
2. Calcule la position (en haut à droite)
3. Dessine un fond semi-transparent
4. Affiche le texte en couleur dorée

**Utilité** : Affiche la monnaie du joueur de manière visible.

---

### 8.5 getMusicButtonRect() (lignes 651-672)

**Rôle** : Calcule le rectangle du bouton de musique.

**Fonctionnement** :
1. Calcule une taille proportionnelle à la fenêtre (5% du plus petit côté)
2. Limite la taille entre 32 et 96 pixels
3. Positionne le bouton en bas à droite avec une marge

**Utilité** : Assure que le bouton de musique s'adapte à différentes tailles d'écran.

---

### 8.6 drawMusicButton() (lignes 675-755)

**Rôle** : Dessine le bouton pour activer/désactiver la musique.

**Fonctionnement** :
1. Vérifie si la musique est disponible
2. Calcule la position du bouton
3. Détecte le survol
4. Dessine le fond du bouton avec un effet de survol
5. Affiche l'icône (son activé/désactivé) ou dessine un pictogramme simple

**Utilité** : Permet au joueur de contrôler la musique facilement.

---

## 9. MODE DEBUG (lignes 762-920)

### 9.1 handleDebugDragging() (lignes 767-863)

**Rôle** : Gère le déplacement des éléments en mode debug.

**Fonctionnement** :
1. Si le mode debug est désactivé, réinitialise tout
2. Détecte le début d'un clic sur un élément
3. Pendant le déplacement, met à jour la position de l'élément
4. À la fin du clic, arrête le déplacement

**Éléments déplaçables** :
- Portails de zones
- Portail de la boutique
- Nounours
- Rectangle de l'écran d'accueil

**Utilité** : Permet aux développeurs de repositionner les éléments visuellement sans modifier le code.

---

### 9.2 drawDebugOverlay() (lignes 866-920)

**Rôle** : Affiche les informations de debug.

**Fonctionnement** :
1. Affiche un panneau avec la position de la souris
2. Dessine les rectangles de collision des portails
3. Affiche les coordonnées de chaque rectangle

**Utilité** : Aide au développement et au débogage en visualisant les zones cliquables.

---

## 10. CONVERSION ZONE -> ÉTAT (lignes 922-935)

### 10.1 zoneToState() (lignes 927-935)

**Rôle** : Convertit un identifiant de zone en état de jeu.

**Paramètres** :
- `zone` : Identifiant de la zone

**Retour** : L'état de jeu correspondant.

**Utilité** : Simplifie la navigation en convertissant automatiquement une zone en son état associé.

---

## 11. FONCTION PRINCIPALE (lignes 937-1516)

### 11.1 Initialisation (lignes 941-1037)

**Rôle** : Initialise toutes les ressources du jeu.

**Étapes** :
1. Initialise la structure du jeu à zéro
2. Initialise la fenêtre (1920x1080, redimensionnable)
3. Initialise l'audio
4. Charge l'icône de la fenêtre si elle existe
5. Charge toutes les textures (menu, accueil, tenues, fonds)
6. Initialise la boutique
7. Charge les positions depuis le fichier de configuration
8. Initialise les paramètres (FPS, état initial)
9. Charge la musique et les icônes de son

**Utilité** : Prépare le jeu avant de démarrer la boucle principale.

---

### 11.2 Boucle principale (lignes 1041-1478)

**Rôle** : Boucle de jeu qui s'exécute tant que la fenêtre est ouverte.

**Structure** :

#### Mise à jour (Update)
1. Calcule le temps écoulé depuis la dernière frame (`dt`)
2. Assure que le nounours reste dans l'écran
3. Réinitialise le portail survolé
4. Gère le timer d'inactivité
5. Met à jour la musique
6. Gère les raccourcis clavier (F11 pour plein écran, F2 pour debug)
7. Gère la navigation selon l'état actuel :
   - Écran d'accueil : détection du clic sur le rectangle
   - Pause : gestion de la reprise
   - Hub : détection des clics sur les portails
   - Zones : gestion du lancement des minijeux
   - Boutique : mise à jour de la boutique
   - Minijeu : mise à jour du minijeu actif

#### Affichage (Render)
1. Démarre le rendu
2. Efface l'écran
3. Affiche les éléments selon l'état :
   - Écran d'accueil : fond et texte de bienvenue
   - Pause : texte de pause
   - Hub : fond, nounours, tableau de progression, compteur de pièces, message d'aide
   - Zones : textes d'information
   - Boutique : interface de la boutique
   - Minijeu : rendu du minijeu
4. Affiche le bouton de musique
5. Gère le clic sur le bouton de musique
6. Termine le rendu

**Utilité** : Exécute la logique du jeu et l'affichage à chaque frame (60 fois par seconde).

---

### 11.3 Nettoyage (lignes 1480-1515)

**Rôle** : Libère toutes les ressources avant de fermer le jeu.

**Étapes** :
1. Sauvegarde les positions dans le fichier de configuration
2. Libère toutes les textures
3. Arrête et libère la musique
4. Libère les ressources de la boutique
5. Ferme l'audio et la fenêtre

**Utilité** : Assure une libération propre de la mémoire et évite les fuites mémoire.

---

## 12. ARCHITECTURE GLOBALE

### Machine à états

Le jeu utilise une machine à états finis pour gérer la navigation :
- Chaque état correspond à un écran
- Les transitions entre états sont gérées par des événements (clics, touches)
- Chaque état a sa propre logique de mise à jour et d'affichage

### Système de layout adaptatif

- Les positions sont stockées en pourcentage de l'écran
- Conversion en pixels au moment de l'affichage
- Permet l'adaptation à différentes résolutions
- Sauvegarde/chargement depuis un fichier de configuration

### Gestion des ressources

- Chargement conditionnel : le jeu fonctionne même si certaines ressources manquent
- Flags `hasXXX` pour vérifier la disponibilité des ressources
- Libération systématique à la fermeture

### Interface modulaire

- Chaque minijeu est une interface indépendante (`MinigameAPI`)
- Le code principal ne connaît que l'interface, pas l'implémentation
- Facilite l'ajout de nouveaux minijeux

---

## 13. POINTS CLÉS POUR LE RAPPORT

### Points techniques à mentionner

1. **Architecture modulaire** : Séparation entre le code principal et les minijeux
2. **Adaptabilité** : Système de layout en pourcentage pour différentes résolutions
3. **Robustesse** : Gestion des ressources manquantes sans planter
4. **Persistance** : Sauvegarde des positions dans un fichier de configuration
5. **Mode debug** : Outil de développement intégré pour repositionner les éléments

### Complexité

- **Lignes de code** : ~1517 lignes
- **Fonctions** : ~25 fonctions
- **Structures** : 7 structures principales
- **États** : 9 états différents

### Technologies utilisées

- **Raylib** : Bibliothèque graphique et audio
- **C standard** : Langage de programmation
- **Fichiers INI** : Format de configuration simple

---

## Conclusion

Le fichier `main.c` constitue le cœur du jeu, orchestrant tous les aspects : navigation, affichage, gestion des minijeux, progression, et interface utilisateur. L'architecture modulaire et le système de layout adaptatif permettent une maintenance facile et une bonne expérience utilisateur sur différentes configurations.

