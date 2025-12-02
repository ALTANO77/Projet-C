# Cerise sur Gâteau - Mini-jeu

## Description
Jeu de placement d'ingrédients sur un gâteau selon un modèle. Le joueur doit replacer des ingrédients aux bons endroits en utilisant le drag-and-drop.

## Structure des assets

Le jeu nécessite les fichiers suivants dans le dossier `assets/` :

### assets/ingredients/
- `fraise.png` - Image de la fraise
- `banane.png` - Image de la banane
- `kiwi.png` - Image du kiwi
- `mandarine.png` - Image de la mandarine
- `chocolat.png` - Image du chocolat

### assets/cakes/
- `gateau_niveau1.png` - Image du gâteau modèle pour le niveau 1
- `gateau_niveau2.png` - Image du gâteau modèle pour le niveau 2
- `gateau_niveau3.png` - Image du gâteau modèle pour le niveau 3
- `gateau_base.png` - Image du gâteau vide/base (optionnel, sinon utilise le modèle)

### assets/backgrounds/
- `fond.png` - Image de fond du jeu (optionnel)

### assets/cursor/
- `main_ouverte.png` - Curseur normal (main ouverte)
- `main_fermee.png` - Curseur quand on tient un ingrédient (main fermée)

## Fonctionnement

1. **Affichage du modèle** : Pendant 5 secondes, le joueur voit le gâteau modèle avec les ingrédients placés
2. **Phase de jeu** : Le joueur a 30 secondes pour replacer les ingrédients sur le gâteau
3. **Calcul du score** : Basé sur la précision du placement (distance par rapport à la position cible)
4. **3 niveaux** : Le jeu comporte 3 niveaux qui s'enchaînent
5. **Validation** : Le jeu est validé si la moyenne finale est >= 90%

## Contrôles

- **Clic gauche** : Prendre/déposer un ingrédient
- **Drag & Drop** : Déplacer un ingrédient de la barquette vers le gâteau
- **ESPACE** : Passer au niveau suivant (quand le niveau est terminé)
- **R** : Recommencer depuis le niveau 1 (si le jeu n'est pas validé)
- **BACKSPACE** : Quitter le mini-jeu

## Notes techniques

- Les positions cibles des ingrédients sont définies dans le code pour chaque niveau
- La zone de placement est un cercle de 200 pixels de rayon au centre de l'écran
- Le score de précision est calculé avec une distance maximale de 200 pixels

