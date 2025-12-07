#include "pendu.h"
#include "raylib.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// ============================================================================
// DONNÉES DU JEU
// ============================================================================

// Liste des mots faciles : si le joueur trouve le mot, il gagne 2 pièces
static const char *WORDS_2[] = {
    "NOUNOURS", "COUVERTURE", "OREILLER", "BISOU", "BONBON", "COUETTE", "SOMMEIL",
    "TENDRESSE", "MOUSTACHE", "AVENTURE", "LUMIERE", "ETOILE", "SOURIRE"
};

// Liste des mots difficiles : si le joueur trouve le mot, il gagne 5 pièces
static const char *WORDS_5[] = {
    "MOLLETONNAGE", "EFFILOCHAGE", "REMBOURRAGE", "SCINTILLEMENT", "LUMINESCENCE",
    "SOMNOLENCE", "EMERVEILLEMENT", "APAISEMENT", "BIENVEILLANCE"
};

// Définitions des mots faciles (dans le même ordre que WORDS_2)
// Ces définitions s'affichent après 5 erreurs ou quand le joueur gagne
static const char *DEFS_2[] = {
    "une peluche en forme d'ours, souvent tres douce, que les enfants gardent pour dormir ou pour se rassurer quand ils ont peur.",
    "un grand tissu chaud que l'on met sur soi pour ne pas avoir froid, surtout la nuit dans le lit.",
    "un coussin moelleux sur lequel on pose sa tete pour etre bien installe quand on dort.",
    "un petit geste d'affection que l'on fait avec la bouche pour montrer qu'on aime quelqu'un ou pour reconforter.",
    "une petite gourmandise sucre de differentes formes et couleurs qu'on mange pour se faire plaisir.",
    "une grosse couverture bien remplie et toute douce qui garde bien la chaleur quand on dort.",
    "le moment ou le corps et le cerveau se reposent, ou on ferme les yeux et on dort pour reprendre de l'energie.",
    "un sentiment doux et chaleureux qu'on ressent quand on aime quelqu'un et qu'on veut etre gentil avec lui.",
    "des poils qui poussent au-dessus de la bouche, comme on peut en voir chez certains adultes.",
    "une situation nouvelle, excitante ou surprenante ou il se passe des choses qu'on ne connait pas encore.",
    "ce qui eclaire tout autour de nous, que ce soit le soleil, une lampe ou une flamme, et qui nous permet de voir.",
    "un petit point lumineux que l'on voit dans le ciel la nuit et qui brille tres fort meme si elle est tres loin.",
    "une expression du visage quand on releve les levres pour montrer qu'on est content, heureux ou qu'on veut etre gentil."
};

// Définitions des mots difficiles (dans le même ordre que WORDS_5)
static const char *DEFS_5[] = {
    "une facon de rendre un tissu tres doux, epais et moelleux, comme ceux qu'on trouve dans les doudous.",
    "quand un tissu ou un morceau de fil commence a se defaire et se separe en plein de petits fils.",
    "la matiere douce et legere qu'on met a l'interieur des peluches, des coussins ou des oreillers pour qu'ils soient confortables.",
    "une lumiere qui clignote ou brille par petites etincelles, comme une etoile ou une bougie.",
    "une lumiere speciale qui apparait sans chaleur, comme dans certains jouets fluorescents qui brillent dans le noir.",
    "un etat ou on se sent tres fatigue, ou les yeux se ferment tout seuls et ou on est presque en train de s'endormir.",
    "un sentiment de grande surprise et de joie quand on voit quelque chose de tres beau ou magique.",
    "un moment ou l'on devient calme, tranquille et ou on ne ressent plus de stress ou d'inquietude.",
    "une attitude de gentillesse ou on veut aider les autres, etre respectueux et leur faire du bien."
};

// ============================================================================
// VARIABLES GLOBALES DU JEU
// ============================================================================

static char word[20];           // Le mot à deviner (ex: "NOUNOURS")
static char guessed[20];        // Le mot avec les lettres trouvées (ex: "N__N____")
static int letters[26];         // Tableau pour savoir quelles lettres ont été essayées
                                // letters[0] = A, letters[1] = B, ..., letters[25] = Z
                                // 0 = jamais essayée, 1 = déjà essayée
static int errors = 0;          // Nombre d'erreurs commises (max 7, après c'est perdu)
static int won = 0;             // 1 si le joueur a gagné, 0 sinon
static int lost = 0;            // 1 si le joueur a perdu, 0 sinon
static int init = 0;            // 1 si le jeu est initialisé, 0 sinon
static int isHardWord = 0;      // 1 si c'est un mot difficile (5 pièces), 0 si facile (2 pièces)
static int wantsToExit = 0;     // 1 si le joueur veut quitter le jeu (bouton Retour ou Backspace)
static int currentWordIndex = 0; // Index du mot actuel dans sa liste (0-12 pour WORDS_2, 0-8 pour WORDS_5)
static Texture2D backgroundTex = {0}; // Texture du fond d'écran du jeu
static bool hasBackground = false;   // Vrai si le fond d'écran a été chargé avec succès
static int randomSeed = 0;      // Compteur qui s'incrémente à chaque partie pour améliorer l'aléatoire

// ============================================================================
// FONCTIONS UTILITAIRES
// ============================================================================

// Révèle toutes les occurrences d'une lettre dans le mot
// Paramètre: c = la lettre à révéler (en majuscule, ex: 'N')
// Cette fonction parcourt tout le mot et révèle toutes les positions où cette lettre apparaît
static void revealLetter(char c) {
    int found = 0; // Indique si on a trouvé au moins une lettre à révéler
    
    // Parcourir chaque caractère du mot
    for (int i = 0; word[i]; i++) {
        // Si cette position correspond à la lettre ET qu'elle n'est pas encore révélée
        if (word[i] == c && guessed[i] == '_') {
            guessed[i] = c; // Révéler la lettre à cette position
            found = 1;       // On a trouvé au moins une lettre
        }
    }
    
    // Si on n'a trouvé aucune lettre, c'est une erreur
    if (!found) {
        errors++; // Augmenter le compteur d'erreurs
        // Si on a fait 7 erreurs, le joueur a perdu (toutes les pétales sont tombées)
        if (errors >= 7) lost = 1;
    } 
    // Sinon, vérifier si on a trouvé tout le mot (plus aucun underscore)
    else if (strcmp(word, guessed) == 0) {
        won = 1; // Le joueur a gagné !
    }
}

// Traite une lettre choisie par le joueur (via clavier ou souris)
// Paramètre: c = la lettre choisie (en majuscule, ex: 'A')
// Cette fonction vérifie si la lettre peut être traitée et appelle revealLetter si oui
static void processLetter(char c) {
    // Calculer l'index de la lettre dans l'alphabet (A=0, B=1, ..., Z=25)
    int idx = c - 'A';
    // Vérifier que c'est une lettre valide (entre A et Z)
    if (idx < 0 || idx >= 26) return;
    
    // Vérifier si la lettre est présente dans le mot
    int hasLetter = 0;  // 1 si la lettre est dans le mot, 0 sinon
    int canReveal = 0;   // 1 s'il reste des occurrences non révélées, 0 sinon
    
    // Parcourir le mot pour vérifier
    for (int i = 0; word[i]; i++) {
        if (word[i] == c) {
            hasLetter = 1; // La lettre est dans le mot
            // Si cette occurrence n'est pas encore révélée (il y a encore un underscore)
            if (guessed[i] == '_') {
                canReveal = 1; // On peut encore révéler cette lettre
                break; // Pas besoin de continuer, on a trouvé ce qu'on cherchait
            }
        }
    }
    
    // Traiter la lettre seulement si:
    // - Elle n'a jamais été essayée avant, OU
    // - Elle est dans le mot ET il reste des occurrences à révéler
    // (Cela permet de cliquer plusieurs fois sur une lettre qui apparaît plusieurs fois dans le mot)
    if (!letters[idx] || (hasLetter && canReveal)) {
        // Marquer la lettre comme essayée (pour l'afficher en vert ou rouge dans le clavier)
        if (!letters[idx]) letters[idx] = 1;
        // Révéler toutes les occurrences de cette lettre dans le mot
        revealLetter(c);
    }
}

// ============================================================================
// INITIALISATION DU JEU
// ============================================================================

// Initialise une nouvelle partie du jeu
// Cette fonction est appelée au début de chaque partie
static void init_game(void) {
    // Améliorer l'aléatoire en utilisant le temps + un compteur qui s'incrémente
    // Cela évite d'avoir le même mot plusieurs fois de suite si on rejoue rapidement
    randomSeed++;
    srand((unsigned)(time(NULL) + randomSeed * 1000));
    
    // Choisir aléatoirement entre mot facile (0) et mot difficile (1)
    // rand() % 2 donne 0 ou 1 de façon aléatoire
    if (rand() % 2 == 0) {
        // Mot facile: choisir un mot parmi les 13 mots faciles (indices 0 à 12)
        currentWordIndex = rand() % 13;
        strcpy(word, WORDS_2[currentWordIndex]); // Copier le mot dans la variable word
        isHardWord = 0; // C'est un mot facile (2 pièces)
    } else {
        // Mot difficile: choisir un mot parmi les 9 mots difficiles (indices 0 à 8)
        currentWordIndex = rand() % 9;
        strcpy(word, WORDS_5[currentWordIndex]); // Copier le mot dans la variable word
        isHardWord = 1; // C'est un mot difficile (5 pièces)
    }
    
    // Initialiser le mot deviné avec des underscores
    // Exemple: si word = "NOUNOURS", alors guessed = "________"
    memset(guessed, '_', strlen(word)); // Remplir avec des underscores
    guessed[strlen(word)] = 0; // Terminer la chaîne avec '\0' (caractère de fin de chaîne)
    
    // Révéler automatiquement la première lettre
    guessed[0] = word[0];
    // Révéler aussi toutes les autres occurrences de la première lettre
    // Exemple: si word = "NOUNOURS", alors guessed devient "N__N____"
    for (int i = 1; word[i]; i++) {
        if (word[i] == word[0]) guessed[i] = word[0];
    }
    
    // Réinitialiser toutes les lettres comme non essayées
    // Cela remet à zéro le tableau letters (toutes les valeurs à 0)
    memset(letters, 0, sizeof(letters));
    
    // Réinitialiser l'état du jeu
    errors = won = lost = wantsToExit = 0;
    
    // Charger le fond d'écran si ce n'est pas déjà fait (pour éviter de le charger plusieurs fois)
    if (!hasBackground) {
        Image bgImg = LoadImage("assets/pendu/image_fond_pendu.png");
        if (bgImg.data) {
            // Convertir l'image en texture pour pouvoir l'afficher rapidement
            backgroundTex = LoadTextureFromImage(bgImg);
            UnloadImage(bgImg); // Libérer l'image car on n'a besoin que de la texture
            hasBackground = (backgroundTex.id != 0); // Vérifier que la texture a bien été créée
        }
    }
    
    // Marquer le jeu comme initialisé (pour pouvoir le dessiner)
    init = 1;
}

// ============================================================================
// MISE À JOUR DU JEU (appelée chaque frame, environ 60 fois par seconde)
// ============================================================================

// EXPLICATION DU CONCEPT DE "FRAME" :
// 
// Une "frame" (image) = une image affichée à l'écran
// Le jeu fonctionne comme un film : il affiche des images très rapidement
// (environ 60 images par seconde) pour créer l'illusion du mouvement
//
// BOUCLE PRINCIPALE DU JEU (dans main.c) :
//   while (!WindowShouldClose()) {
//       1. update_game(dt)  ← On met à jour l'état du jeu (ce que fait cette fonction)
//       2. draw_game()      ← On dessine tout à l'écran
//       3. Attendre un peu
//       4. Recommencer depuis le début
//   }
//
// POURQUOI update_game() EST APPELÉE 60 FOIS PAR SECONDE ?
// - Pour détecter les actions du joueur EN TEMPS RÉEL
//   Exemple : si le joueur appuie sur 'A', on doit le détecter immédiatement
//   Si on ne vérifiait qu'une fois par seconde, le jeu serait très lent à réagir
//
// QUE FAIT CETTE FONCTION ?
// - Vérifie si le joueur a appuyé sur une touche du clavier
// - Vérifie si le joueur a cliqué avec la souris
// - Vérifie si le joueur a cliqué sur les boutons (Rejouer/Retour)
// - Met à jour l'état du jeu en fonction de ces actions
//
// Paramètre: dt = temps écoulé depuis la dernière frame (non utilisé ici)
//            (dt pourrait être utilisé pour des animations fluides, mais pas nécessaire ici)
static void update_game(float dt) {
    (void)dt; // Éviter l'avertissement du compilateur (variable non utilisée)
    
    // Si le jeu est terminé (gagné ou perdu) et qu'on appuie sur Backspace, quitter
    if (IsKeyPressed(KEY_BACKSPACE) && (won || lost)) {
        wantsToExit = 1; // Marquer qu'on veut quitter
        return; // Sortir de la fonction
    }
    
    // Si le jeu est terminé, gérer les boutons de fin (Rejouer et Retour)
    if (won || lost) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 m = GetMousePosition(); // Position de la souris
            int cx = GetScreenWidth() / 2;  // Centre de l'écran en X
            int cy = GetScreenHeight() / 2; // Centre de l'écran en Y
            int by = cy + 50; // Position Y des boutons (50 pixels sous le centre)
            
            // Vérifier le clic sur le bouton "Rejouer" (à gauche du centre)
            Rectangle replayBtn = {cx - 120, by, 100, 40}; // Rectangle du bouton Rejouer
            if (CheckCollisionPointRec(m, replayBtn)) {
                init_game(); // Relancer une nouvelle partie
                return; // Sortir de la fonction
            }
            
            // Vérifier le clic sur le bouton "Retour" (à droite du centre)
            Rectangle backBtn = {cx + 20, by, 100, 40}; // Rectangle du bouton Retour
            if (CheckCollisionPointRec(m, backBtn)) {
                wantsToExit = 1; // Marquer qu'on veut quitter le jeu
                return; // Sortir de la fonction
            }
        }
        return; // Ne pas traiter les autres entrées si le jeu est terminé
    }
    
    // Gestion du clavier physique (compatible AZERTY/QWERTY)
    // GetCharPressed() retourne le caractère pressé, peu importe la disposition du clavier
    int charKey = GetCharPressed();
    // Vérifier si une lettre a été pressée (minuscule ou majuscule)
    if ((charKey >= 'a' && charKey <= 'z') || (charKey >= 'A' && charKey <= 'Z')) {
        // Convertir en majuscule si nécessaire
        // Si c'est une minuscule, on la convertit en majuscule
        // Sinon, on garde la majuscule telle quelle
        char c = (charKey >= 'a' && charKey <= 'z') ? ('A' + (charKey - 'a')) : charKey;
        processLetter(c); // Traiter la lettre
    }
    
    // Gestion du clavier visuel (clic souris sur les lettres affichées)
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 m = GetMousePosition(); // Position de la souris
        // Position de départ du clavier visuel (en bas de l'écran, centré)
        int sx = GetScreenWidth() / 2 - 200; // Position X de départ (200 pixels à gauche du centre)
        int sy = GetScreenHeight() - 100;    // Position Y de départ (100 pixels du bas)
        
        // Parcourir les 26 lettres de l'alphabet
        for (int i = 0; i < 26; i++) {
            // Calculer la position de chaque lettre
            // 13 lettres par ligne, 2 lignes au total
            // i % 13 donne la colonne (0-12)
            // i / 13 donne la ligne (0 ou 1)
            int x = sx + (i % 13) * 35; // Colonne: espacement de 35 pixels entre chaque lettre
            int y = sy + (i / 13) * 35;  // Ligne: espacement de 35 pixels entre les lignes
            
            // Vérifier si le clic est sur cette lettre (rectangle de 30x30 pixels)
            if (CheckCollisionPointRec(m, (Rectangle){x, y, 30, 30})) {
                processLetter('A' + i); // Traiter la lettre cliquée (A + i donne la lettre)
                break; // Une seule lettre à la fois, on peut arrêter la boucle
            }
        }
    }
}

// ============================================================================
// FONCTIONS DE DESSIN
// ============================================================================

// Dessine le fond d'écran du jeu
static void drawBackground(void) {
    if (hasBackground) {
        // Rectangle source: toute l'image (de 0,0 à la taille complète de l'image)
        Rectangle src = {0, 0, (float)backgroundTex.width, (float)backgroundTex.height};
        int screenW = GetScreenWidth();  // Largeur de l'écran
        int screenH = GetScreenHeight(); // Hauteur de l'écran
        
        // Calculer les dimensions pour garder le ratio de l'image (éviter qu'elle soit déformée)
        float imgRatio = (float)backgroundTex.width / (float)backgroundTex.height; // Ratio largeur/hauteur de l'image
        float dstW = (float)screenW; // Largeur de destination = largeur de l'écran
        float dstH = dstW / imgRatio * 0.72f; // Hauteur ajustée (réduite de 28% pour l'affichage)
        float dstY = (screenH - dstH) / 2.0f - 30.0f; // Centrer verticalement et remonter de 30 pixels
        
        // Rectangle de destination sur l'écran (où l'image sera affichée)
        Rectangle dst = {0, dstY, dstW, dstH};
        DrawTexturePro(backgroundTex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
        
        // Remplir les zones vides en haut et en bas avec du blanc
        // (car l'image ne couvre peut-être pas tout l'écran)
        if (dstY > 0) {
            // Zone vide en haut
            DrawRectangle(0, 0, screenW, (int)dstY, RAYWHITE);
        }
        if (dstY + dstH < screenH) {
            // Zone vide en bas
            DrawRectangle(0, (int)(dstY + dstH), screenW, screenH - (int)(dstY + dstH), RAYWHITE);
        }
    } else {
        // Si pas de fond, juste un fond blanc
        ClearBackground(RAYWHITE);
    }
}

// Dessine la fleur avec ses pétales (7 pétales au total, une disparaît à chaque erreur)
static void drawFlower(void) {
    int flowerX = GetScreenWidth() / 2 - 400; // Position X de la fleur (400 pixels à gauche du centre)
    int flowerY = 400; // Position Y de la fleur (400 pixels du haut)
    float petalRadius = 30.0f; // Rayon des pétales (taille des cercles roses)
    float centerRadius = 25.0f; // Rayon du centre de la fleur (cercle jaune)
    int remainingPetals = 7 - errors; // Pétales restantes (7 au début, diminue avec les erreurs)
    
    // Texte d'instruction au-dessus de la fleur
    const char *instruction = "Ne perds pas toutes les pétales de la fleur";
    // Centrer le texte horizontalement par rapport à la fleur
    DrawText(instruction, flowerX - MeasureText(instruction, 24) / 2 + 30, flowerY - 200, 24, BLACK);
    
    // Dessiner les pétales restantes
    // Les pétales sont disposées en cercle autour du centre
    for (int i = 0; i < remainingPetals; i++) {
        // Calculer l'angle de chaque pétale (en radians)
        // 360 degrés divisés par 7 pétales, puis convertis en radians
        float angle = (i * 360.0f / 7) * 3.14159f / 180.0f;
        // Position de la pétale (en cercle autour du centre, à 60 pixels du centre)
        float petalX = flowerX + cosf(angle) * 60.0f; // cosinus pour la position X
        float petalY = flowerY + sinf(angle) * 60.0f; // sinus pour la position Y
        // Dessiner la pétale (cercle rose)
        DrawCircle((int)petalX, (int)petalY, petalRadius, (Color){255, 192, 203, 255});
        DrawCircleLines((int)petalX, (int)petalY, petalRadius, BLACK); // Contour noir
    }
    
    // Dessiner le centre de la fleur (cercle jaune)
    DrawCircle(flowerX, flowerY, centerRadius, YELLOW);
    DrawCircleLines(flowerX, flowerY, centerRadius, BLACK);
    
    // Dessiner la tige (ligne verte sous les pétales)
    // Ligne de 8 pixels d'épaisseur, de 90 pixels sous le centre jusqu'à 250 pixels sous le centre
    DrawLineEx((Vector2){flowerX, flowerY + 90}, (Vector2){flowerX, flowerY + 250}, 8.0f, (Color){34, 139, 34, 255});
    
    // Dessiner les feuilles (ellipses vertes)
    // Feuille gauche
    DrawEllipse(flowerX - 30, flowerY + 150, 25, 15, (Color){50, 205, 50, 255});
    DrawEllipseLines(flowerX - 30, flowerY + 150, 25, 15, (Color){34, 139, 34, 255});
    // Feuille droite
    DrawEllipse(flowerX + 30, flowerY + 180, 25, 15, (Color){50, 205, 50, 255});
    DrawEllipseLines(flowerX + 30, flowerY + 180, 25, 15, (Color){34, 139, 34, 255});
}

// Dessine le mot à deviner avec les lettres trouvées
static void drawWord(void) {
    int len = strlen(guessed); // Longueur du mot (ex: "N__N____" = 8)
    int wordY = 280; // Position Y du mot (280 pixels du haut)
    // Position X du début du mot (centré puis décalé de 350 pixels vers la droite)
    int wordStartX = GetScreenWidth() / 2 - len * 15 + 350;
    
    // Texte d'instruction au-dessus du mot
    const char *wordInstruction = "Trouve le mot !";
    // Centrer le texte par rapport au mot
    DrawText(wordInstruction, wordStartX + len * 15 - MeasureText(wordInstruction, 28) / 2 - 50, wordY - 100, 45, BLACK);
    
    // Dessiner chaque lettre du mot (ou underscore si non trouvée)
    for (int i = 0; i < len; i++) {
        char s[2] = {guessed[i], 0}; // Convertir le caractère en chaîne (nécessaire pour DrawText)
        // Espacement de 30 pixels entre chaque lettre
        DrawText(s, wordStartX + i * 30, wordY, 40, BLUE);
    }
    
    // Texte d'aide en dessous du mot
    const char *keyboardHint = "Tu peux utiliser ton clavier";
    // Centrer le texte par rapport au mot
    DrawText(keyboardHint, wordStartX + len * 15 - MeasureText(keyboardHint, 22) / 2, wordY + 50, 22, BLACK);
}

// Dessine le clavier visuel (26 lettres cliquables)
static void drawKeyboard(void) {
    // Parcourir les 26 lettres de l'alphabet
    for (int i = 0; i < 26; i++) {
        // Calculer la position de chaque lettre
        // 13 lettres par ligne, 2 lignes au total
        int x = GetScreenWidth() / 2 - 200 + (i % 13) * 35; // Colonne (0-12), espacement de 35 pixels
        int y = GetScreenHeight() - 100 + (i / 13) * 35;    // Ligne (0 ou 1), espacement de 35 pixels
        
        // Choisir la couleur de la lettre:
        // - GRAY si jamais essayée (lettre grise)
        // - GREEN si essayée ET dans le mot (lettre verte = bonne lettre)
        // - RED si essayée MAIS pas dans le mot (lettre rouge = mauvaise lettre)
        Color c = letters[i] ? (strchr(word, 'A' + i) ? GREEN : RED) : GRAY;
        
        // Dessiner le rectangle de la lettre (30x30 pixels)
        DrawRectangle(x, y, 30, 30, c);
        
        // Dessiner la lettre elle-même (décalée de 8 pixels en X et 5 pixels en Y pour centrer)
        char s[2] = {'A' + i, 0}; // Convertir le caractère en chaîne
        DrawText(s, x + 8, y + 5, 20, BLACK);
    }
}

// Dessine la définition du mot (affichée après 5 erreurs ou si gagné)
static void drawDefinition(void) {
    // Ne rien afficher si moins de 5 erreurs et pas encore gagné
    if (errors < 5 && !won) return;
    
    // Choisir la bonne définition selon le type de mot (facile ou difficile)
    const char *def = isHardWord ? DEFS_5[currentWordIndex] : DEFS_2[currentWordIndex];
    
    // Position et dimensions de la boîte de définition
    int defX = 1220; // Position X (1220 pixels depuis la gauche, donc à droite de l'écran)
    int defY = 450;  // Position Y (450 pixels depuis le haut)
    int defW = 360;  // Largeur de la boîte
    int defH = 280;  // Hauteur de la boîte
    int fontSize = 28; // Taille de la police
    int lineHeight = 34; // Hauteur d'une ligne de texte
    int maxWidth = defW - 20; // Largeur maximale du texte (avec marges de 10 pixels de chaque côté)
    
    // Dessiner le fond de la boîte (jaune clair, semi-transparent)
    DrawRectangle(defX, defY, defW, defH, (Color){255, 255, 200, 230});
    // Dessiner la bordure de la boîte (marron)
    DrawRectangleLines(defX, defY, defW, defH, (Color){200, 150, 50, 255});
    // Titre "Définition:"
    DrawText("Definition:", defX + 10, defY + 10, 30, DARKBLUE);
    
    // Position de départ du texte (avec marge de 10 pixels)
    int x = defX + 10;
    int y = defY + 40;
    int textLen = strlen(def); // Longueur totale du texte de définition
    int currentPos = 0; // Position actuelle dans le texte (on commence au début)
    
    // Découper le texte en plusieurs lignes pour qu'il rentre dans la boîte
    // On continue tant qu'on n'a pas tout affiché ET qu'on n'a pas dépassé la hauteur de la boîte
    while (currentPos < textLen && y < defY + defH - 20) {
        int bestBreak = currentPos; // Meilleure position pour couper la ligne (on commence par la position actuelle)
        int lastSpace = -1; // Position du dernier espace rencontré (pour couper entre les mots, pas au milieu)
        
        // Parcourir le texte caractère par caractère pour trouver où couper
        for (int i = currentPos; i < textLen; i++) {
            // Mémoriser la position des espaces (pour couper entre les mots, pas au milieu d'un mot)
            if (def[i] == ' ') lastSpace = i;
            
            // Créer une chaîne temporaire avec le texte jusqu'à cette position
            char temp[200];
            int len = i - currentPos + 1; // Longueur du texte à mesurer
            if (len > 199) len = 199; // Limiter à 199 caractères pour éviter le débordement
            strncpy(temp, def + currentPos, len); // Copier le texte
            temp[len] = '\0'; // Terminer la chaîne avec '\0'
            
            // Mesurer la largeur du texte avec MeasureText
            // Si on dépasse la largeur maximale, on doit couper ici
            if (MeasureText(temp, fontSize) > maxWidth) {
                // Préférer couper à un espace si possible (pour ne pas couper un mot en deux)
                bestBreak = (lastSpace > currentPos) ? (lastSpace + 1) : i;
                break; // On a trouvé où couper, on peut arrêter la boucle
            }
            
            // Si on arrive à la fin du texte, tout prendre
            if (i == textLen - 1) {
                bestBreak = textLen;
            }
        }
        
        // Afficher la ligne
        int lineLen = bestBreak - currentPos; // Longueur de la ligne à afficher
        if (lineLen > 0) {
            char line[300];
            if (lineLen > 299) lineLen = 299; // Limiter à 299 caractères
            strncpy(line, def + currentPos, lineLen); // Copier la ligne
            line[lineLen] = '\0'; // Terminer la chaîne
            // Enlever l'espace en fin de ligne si présent (pour un affichage plus propre)
            if (lineLen > 0 && line[lineLen - 1] == ' ') line[lineLen - 1] = '\0';
            DrawText(line, x, y, fontSize, BLACK); // Afficher la ligne
            y += lineHeight; // Passer à la ligne suivante (descendre de lineHeight pixels)
        }
        
        // Passer à la position suivante dans le texte
        currentPos = bestBreak;
        // Ignorer les espaces en début de ligne suivante (pour éviter les lignes vides)
        while (currentPos < textLen && def[currentPos] == ' ') currentPos++;
    }
}

// Dessine la fenêtre modale de fin de partie (gagné ou perdu)
static void drawEndModal(void) {
    int cx = GetScreenWidth() / 2;  // Centre de l'écran en X
    int cy = GetScreenHeight() / 2; // Centre de l'écran en Y
    int modalW = 450; // Largeur de la fenêtre modale
    int modalH = 220; // Hauteur de la fenêtre modale
    
    // Dessiner l'ombre de la fenêtre (légèrement décalée de 3 pixels pour l'effet d'ombre)
    DrawRectangle(cx - modalW / 2 + 3, cy - modalH / 2 + 3, modalW, modalH, (Color){0, 0, 0, 50});
    // Dessiner le fond de la fenêtre (blanc)
    DrawRectangle(cx - modalW / 2, cy - modalH / 2, modalW, modalH, (Color){250, 250, 250, 255});
    // Dessiner le contour de la fenêtre (gris foncé, épaisseur de 1 pixel)
    DrawRectangleLines(cx - modalW / 2, cy - modalH / 2, modalW, modalH, (Color){80, 80, 80, 255});
    
    int msgY = cy - 60; // Position Y du message (60 pixels au-dessus du centre)
    int by = cy + 50;   // Position Y des boutons (50 pixels sous le centre)
    
    // Afficher le message selon si on a gagné ou perdu
    if (won) {
        // Message de victoire avec le nombre de pièces gagnées
        char msg[50];
        int coins = isHardWord ? 5 : 2; // 5 pièces pour mot difficile, 2 pour facile
        // Formater le message avec le nombre de pièces (ajouter un 's' si plusieurs pièces)
        snprintf(msg, sizeof(msg), "Bravo ! Tu as gagne %d piece%s !", coins, coins > 1 ? "s" : "");
        // Afficher "BRAVO !" en vert, centré
        DrawText("BRAVO !", cx - MeasureText("BRAVO !", 40) / 2, msgY, 40, GREEN);
        // Afficher le message avec les pièces, centré
        DrawText(msg, cx - MeasureText(msg, 24) / 2, msgY + 45, 24, DARKGRAY);
    } else {
        // Message de défaite avec le mot à trouver
        DrawText("PERDU !", cx - MeasureText("PERDU !", 40) / 2, msgY, 40, RED);
        char wordMsg[50];
        // Formater le message avec le mot à trouver
        snprintf(wordMsg, sizeof(wordMsg), "Le mot etait: %s", word);
        // Afficher le message, centré
        DrawText(wordMsg, cx - MeasureText(wordMsg, 24) / 2, msgY + 45, 24, DARKGRAY);
    }
    
    // Définir les rectangles des boutons
    Rectangle replayBtn = {cx - 120, by, 100, 40}; // Bouton "Rejouer" (à gauche du centre)
    Rectangle backBtn = {cx + 20, by, 100, 40};    // Bouton "Retour" (à droite du centre)
    Vector2 m = GetMousePosition(); // Position de la souris
    
    // Changer la couleur des boutons au survol (plus clair si la souris est dessus)
    Color replayColor = CheckCollisionPointRec(m, replayBtn) ? (Color){100, 200, 100, 255} : (Color){50, 150, 50, 255};
    Color backColor = CheckCollisionPointRec(m, backBtn) ? (Color){200, 100, 100, 255} : (Color){150, 50, 50, 255};
    
    // Dessiner les boutons
    DrawRectangleRec(replayBtn, replayColor);
    DrawText("Rejouer", cx - 100, by + 12, 20, WHITE);
    DrawRectangleRec(backBtn, backColor);
    DrawText("Retour", cx + 40, by + 12, 20, WHITE);
}

// ============================================================================
// FONCTION PRINCIPALE DE DESSIN
// ============================================================================

// Dessine tout le jeu (appelée chaque frame, environ 60 fois par seconde)
static void draw_game(void) {
    // Ne rien dessiner si le jeu n'est pas initialisé
    if (!init) return;
    
    // Dessiner tous les éléments du jeu dans l'ordre (du fond vers l'avant)
    drawBackground();  // Fond d'écran (en premier, donc derrière tout)
    drawFlower();      // Fleur avec pétales
    drawWord();        // Mot à deviner
    drawKeyboard();    // Clavier visuel
    drawDefinition();  // Définition (si 5 erreurs ou gagné)
    
    // Si le jeu est terminé, afficher la fenêtre modale par-dessus tout
    if (won || lost) {
        drawEndModal();
    }
}

// ============================================================================
// NETTOYAGE ET FIN DU JEU
// ============================================================================

// Nettoie les ressources du jeu (appelée quand on quitte le jeu)
static void unload_game(void) {
    // Libérer la texture du fond si elle a été chargée
    // (pour éviter les fuites mémoire)
    if (hasBackground) {
        UnloadTexture(backgroundTex);
        hasBackground = false;
    }
    // Marquer le jeu comme non initialisé
    init = 0;
}

// Vérifie si le jeu est terminé et retourne le nombre de pièces gagnées
// Paramètre: coins = pointeur vers la variable qui recevra le nombre de pièces
// Retourne: true si le joueur veut quitter, false sinon
static bool is_done(int *coins) {
    // Si le jeu est terminé (gagné ou perdu), calculer les pièces
    if (won || lost) {
        if (coins) {
            // 5 pièces si mot difficile et gagné, 2 pièces si mot facile et gagné, 0 si perdu
            *coins = won ? (isHardWord ? 5 : 2) : 0;
        }
    } else {
        // Si le jeu n'est pas terminé, 0 pièce
        if (coins) *coins = 0;
    }
    // Retourner true seulement si le joueur veut quitter (bouton Retour ou Backspace)
    return wantsToExit;
}

// ============================================================================
// FONCTION D'EXPORT (interface avec le système de minijeux)
// ============================================================================

// Retourne l'interface du jeu du pendu
// Cette fonction est appelée par le système principal pour obtenir le jeu
// Elle retourne une structure avec toutes les fonctions nécessaires au jeu
MinigameAPI GetMinigamePendu(void) {
    return (MinigameAPI){
        init_game,    // Fonction d'initialisation (appelée au début)
        update_game,  // Fonction de mise à jour (appelée chaque frame)
        draw_game,    // Fonction de dessin (appelée chaque frame)
        unload_game,  // Fonction de nettoyage (appelée à la fin)
        is_done       // Fonction de vérification de fin (appelée pour savoir si le jeu est terminé)
    };
}
