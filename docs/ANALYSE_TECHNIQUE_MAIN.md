# Analyse Technique - src/main.c

## Choix Techniques et Difficultés Résolues

Ce document analyse les choix techniques majeurs et les difficultés rencontrées dans l'implémentation du fichier principal du jeu.

---

## 1. **Système de Layout Relatif (Adaptation Multi-Résolution)**

### Problème résolu
Adapter l'interface à différentes tailles d'écran sans casser le positionnement des éléments.

### Solution implémentée
- **Système de coordonnées en pourcentage** : Utilisation de structures `RectRatios` et `BearLayout` qui stockent les positions en pourcentage (0.0 à 1.0) plutôt qu'en pixels
- **Conversion dynamique** : Les fonctions `computePortalRect()`, `computeShopPortalRect()`, `computeBearRect()` convertissent les pourcentages en pixels à chaque frame
- **Clamping intelligent** : Fonctions `clampPortalLayout()` et `clampBearLayout()` pour éviter que les éléments sortent de l'écran

### Difficultés
- Gestion du ratio d'aspect du nounours : calcul complexe dans `getBearWidthRatio()` pour préserver les proportions
- Synchronisation lors du redimensionnement de fenêtre (FLAG_WINDOW_RESIZABLE)

**Lignes clés** : 56-71, 439-531, 342-358

---

## 2. **Machine à États (State Machine)**

### Problème résolu
Gérer proprement les transitions entre les différents écrans du jeu (accueil, hub, zones, minijeux, boutique, pause).

### Solution implémentée
- **Enum `GameState`** : 9 états distincts pour chaque écran
- **Switch case centralisé** : Une seule boucle principale avec un switch pour gérer tous les états
- **Transitions explicites** : Chaque état définit clairement comment passer à un autre

### Difficultés
- Éviter les états invalides (ex: minijeu sans initialisation)
- Gestion de la pause qui peut être activée depuis plusieurs états
- Synchronisation des données entre états (ex: boutique ↔ hub)

**Lignes clés** : 28-38, 1106-1267, 1275-1461

---

## 3. **Système de Minijeux Modulaire (Pattern Strategy/Plugin)**

### Problème résolu
Intégrer plusieurs minijeux différents sans couplage fort avec le code principal.

### Solution implémentée
- **Interface `MinigameAPI`** : Structure avec pointeurs de fonctions (init, update, draw, unload, isCompleted)
- **Polymorphisme en C** : Chaque minijeu expose la même interface via `GetMinigameXXX()`
- **Gestion du cycle de vie** : `prepareMinigameSession()` et `finalizeMinigame()` gèrent l'initialisation/nettoyage

### Difficultés
- Gestion des pointeurs de fonctions NULL (vérifications systématiques)
- Synchronisation des pièces gagnées : double appel à `isCompleted()` (lignes 1233-1244 et 1247-1262) - code dupliqué
- État du minijeu : distinguer "complété" vs "quitté volontairement"

**Lignes clés** : 84-86, 276-311, 1209-1263

---

## 4. **Système de Configuration Persistante (INI)**

### Problème résolu
Sauvegarder les positions des éléments UI (portails, nounours) pour permettre un mode debug avec repositionnement.

### Solution implémentée
- **Format INI simple** : Fichier texte avec format `portal_jardin=0.065,0.25,0.11,0.30`
- **Parsing manuel** : Utilisation de `sscanf()` pour parser les lignes (pas de bibliothèque externe)
- **Valeurs par défaut** : Si le fichier n'existe pas, utilisation de constantes `DEFAULT_*`

### Difficultés
- Parsing fragile : gestion des erreurs de format limitée
- Pas de validation robuste des valeurs chargées (dépend du clamping)
- Sauvegarde uniquement à la fermeture du jeu (pas de sauvegarde automatique)

**Lignes clés** : 362-407, 409-433, 190

---

## 5. **Mode Debug avec Drag & Drop**

### Problème résolu
Permettre aux développeurs de repositionner visuellement les éléments UI sans modifier le code.

### Solution implémentée
- **Activation F2** : Toggle du mode debug
- **Détection de collision** : Vérification de quel élément est cliqué (portail, boutique, nounours, rectangle titre)
- **Offset de drag** : Calcul de `dragOffset` pour éviter les "sauts" lors du clic
- **Conversion en temps réel** : Conversion pixels → pourcentages pendant le drag

### Difficultés
- Gestion de plusieurs éléments draggables : priorité de sélection (portails → boutique → nounours)
- État du drag : distinguer "pressed", "down", "released"
- Clamping pendant le drag pour éviter de sortir de l'écran

**Lignes clés** : 767-863, 866-920

---

## 6. **Gestion des Ressources Conditionnelles**

### Problème résolu
Le jeu doit fonctionner même si certaines textures sont manquantes (robustesse).

### Solution implémentée
- **Fonction `loadTextureIfAvailable()`** : Vérifie l'existence du fichier avant chargement
- **Flags booléens** : `hasMenuBackground`, `hasMenuBear`, etc. pour savoir si une ressource est disponible
- **Fallback visuel** : `drawGround()` si pas de texture de fond

### Difficultés
- Vérification systématique avant utilisation (beaucoup de `if (hasXXX)`)
- Texture vide : comment détecter une texture invalide (`tex.id != 0`)
- Gestion mémoire : ne pas libérer une texture non chargée

**Lignes clés** : 247-266, 555-562, 1487-1500

---

## 7. **Système de Tenues (Outfits) avec Synchronisation**

### Problème résolu
Synchroniser les tenues du nounours entre le hub et la boutique, avec persistance de l'état "possédé".

### Solution implémentée
- **Double stockage** : Données dans `Game` ET dans `BoutiqueData`
- **Synchronisation bidirectionnelle** : Avant et après chaque appel à `Boutique_Update()` (lignes 1189-1203)
- **Pointeurs partagés** : `boutiqueData.collectibles` et `boutiqueData.currentBearOutfit` pointent vers `Game`

### Difficultés
- Risque de désynchronisation si oubli de synchroniser
- Code dupliqué pour la synchronisation (lignes 1190-1192 et 1201-1203)
- Gestion de l'index -1 pour "tenue de départ"

**Lignes clés** : 100-103, 135-144, 985-1001, 1189-1203

---

## 8. **Gestion de la Musique avec Mute/Unmute**

### Problème résolu
Permettre au joueur de couper/remettre la musique sans la relancer depuis le début.

### Solution implémentée
- **Pause/Resume** : Utilisation de `PauseMusicStream()` / `ResumeMusicStream()` au lieu de Stop/Play
- **Bouton adaptatif** : Taille proportionnelle à la fenêtre avec limites min/max
- **Fallback graphique** : Dessin d'un pictogramme si les icônes ne sont pas chargées

### Difficultés
- Mise à jour de la musique uniquement si non muet (ligne 1056)
- Position du bouton : calcul proportionnel complexe dans `getMusicButtonRect()`

**Lignes clés** : 650-755, 1055-1058, 1463-1475

---

## 9. **Détection de Collision et Interactions Souris**

### Problème résolu
Détecter précisément les clics sur des zones invisibles (portails) et gérer le survol.

### Solution implémentée
- **Rectangles invisibles** : Portails définis par des rectangles, pas de rendu visuel
- **Détection frame par frame** : `CheckCollisionPointRec()` à chaque frame
- **État de survol** : Variable `hoveredPortal` pour changer le fond au survol

### Difficultés
- Performance : Vérification de collision pour chaque portail à chaque frame
- Priorité de détection : Ordre de vérification important (zones → boutique)
- Rectangle de l'écran d'accueil : Zone cliquable invisible

**Lignes clés** : 1125-1150, 1075-1084, 546-552

---

## 10. **Timer d'Inactivité pour Aide Contextuelle**

### Problème résolu
Afficher un message d'aide seulement si le joueur semble perdu (inactif 5 secondes).

### Solution implémentée
- **Timer incrémental** : `inactivityTimer` incrémenté chaque frame si pas de clic
- **Réinitialisation** : Remis à zéro sur tout clic (souris gauche ou droite)
- **Affichage conditionnel** : Message uniquement si `>= 5.0f` secondes

### Difficultés
- Détection de "vraie" inactivité : distinguer "pas de clic" vs "pas d'action"
- Position du message : Calcul dynamique pour centrer (ligne 1337)

**Lignes clés** : 1117-1123, 1319-1347

---

## 11. **Gestion des Proportions du Nounours**

### Problème résolu
Afficher le nounours avec les bonnes proportions quelle que soit la tenue (ratio largeur/hauteur variable).

### Solution implémentée
- **Calcul d'aspect dynamique** : `getBearWidthRatio()` calcule le ratio en fonction de la texture actuelle
- **Prise en compte du ratio d'écran** : Ajustement selon `screenHeight / screenWidth`
- **Clamping du nounours** : `clampBearToScreen()` appelé chaque frame pour éviter la sortie d'écran

### Difficultés
- Calcul complexe : `heightRatio * aspect * screenRatio` (ligne 496)
- Gestion des textures manquantes : valeur par défaut si pas de texture
- Tenues différentes : chaque tenue peut avoir des dimensions différentes

**Lignes clés** : 482-497, 499-531, 533-538

---

## 12. **Double Vérification de Complétion des Minijeux**

### Problème identifié (code dupliqué)
Le code vérifie deux fois si le minijeu est complété (lignes 1233-1244 et 1247-1262).

### Explication
- **Premier bloc** : Ajoute les pièces quand le minijeu est complété
- **Deuxième bloc** : Vérifie si on peut quitter le minijeu (retour automatique au hub)

### Difficulté
- Code dupliqué avec double appel à `isCompleted()`
- Logique complexe : distinguer "complété" (pièces ajoutées) vs "quitté" (retour hub)

**Lignes clés** : 1232-1262

---

## Points d'Attention / Améliorations Possibles

1. **Code dupliqué** : Synchronisation boutique (lignes 1189-1203) et vérification minijeu (1233-1262)
2. **Parsing INI fragile** : Pas de gestion d'erreur robuste
3. **Performance** : Vérifications de collision à chaque frame (pourrait être optimisé)
4. **État global** : Structure `Game` très large (148 lignes) - pourrait être divisée
5. **Magic numbers** : Valeurs hardcodées (5 secondes, 0.6f volume, etc.) - devraient être des constantes nommées

---

## Conclusion

Le code montre une architecture solide avec plusieurs patterns intéressants :
- **State Machine** pour la navigation
- **Strategy Pattern** pour les minijeux
- **Système de layout relatif** pour la résolution adaptative
- **Mode debug intégré** pour le développement

Les principales difficultés résolues concernent la gestion de la résolution variable, la modularité des minijeux, et la robustesse face aux ressources manquantes.

