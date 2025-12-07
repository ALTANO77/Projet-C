// Jeu "Cerise sur Gateau"
// - 3 niveaux disponibles (un niveau aléatoire par partie)
// - modèle affiché 5 s
// - 30 s de jeu
// - drag & drop d'ingrédients depuis le bas
// - possibilité de re-bouger un ingrédient déjà posé
// - barquette invisible (juste une ligne d'ingrédients en bas)
// - mode éditeur simple (Ctrl+F2) pour déplacer le cercle et les positions cibles
// - le jeu se termine après chaque niveau, rejouer charge un niveau aléatoire différent

#include "cerisesurgateau.h"
#include "minigame_end_screen.h"
#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INGREDIENT_COUNT 5
#define MAX_LEVELS 3
#define MODEL_DISPLAY_TIME 5.0f
#define LEVEL_TIME 30.0f
#define CAKE_PLACEMENT_RADIUS 200.0f
#define MAX_DISTANCE 200.0f
#define PASSING_SCORE 60.0f
#define LEVELCONFIG "config/cerise_levels.ini"

// Liste des types d'ingrédients
typedef enum {
    ING_FRAISE = 0,
    ING_BANANE,
    ING_KIWI,
    ING_MANDARINE,
    ING_CHOCOLAT
} IngredientType;

// Liste des états de jeu
typedef enum {
    STATE_SHOWING_MODEL,
    STATE_PLAYING,
    STATE_GAME_COMPLETE,
    STATE_EDITOR
} GameState;

// Structure pour représenter un ingrédient
typedef struct {
    IngredientType type;
    Texture2D texture;
    const char* name;
    Vector2 position;        // position dessin
    Vector2 targetPosition;  // position idéale sur le gâteau
    bool isPlaced;           // posé sur le gâteau
    bool isInTray;          // en bas (ligne)
    Rectangle rect;          // zone clic
} Ingredient;

// Niveau : un gâteau avec ses ingrédients et sa position cible
typedef struct {
    Texture2D modelTexture; // image du gâteau modèle (avec ingrédients)
    Texture2D cakeBaseTexture; // image du gâteau vide/base
    Ingredient ingredients[INGREDIENT_COUNT];
    int ingredientCount; // nombre d'ingrédients pour ce niveau
    float score; // score de précision pour ce niveau
    bool completed; // si le niveau est complété
} Level;

// Variables globales du jeu

static GameState gameState = STATE_SHOWING_MODEL; // état actuel du jeu
static int currentLevel = 0; // numéro du niveau actuel
static float modelTimer = 0.0f; // timer pour afficher le modèle
static float levelTimer = 0.0f; // timer pour le jeu

static Level levels[MAX_LEVELS]; // liste des niveaux
static Texture2D backgroundTexture = {0}; // texture de fond

static Vector2 cakeCenter = {0}; // centre du gâteau
static float cakeRadius = CAKE_PLACEMENT_RADIUS; // rayon du gâteau

static Ingredient* draggedIngredient = NULL; // ingrédient en cours de drag
static Vector2 dragOffset = {0}; // offset pour le drag

static float finalScore = 0.0f; // score final
static EndScreenState s_endScreen = {0}; // état de l'écran de fin

// "barquette" invisible en bas de l'écran
static float traySpacing = 80.0f; // espacement entre les ingrédients
static float trayScale   = 0.4f; // taille des ingrédients
static float trayHeight  = 120.0f; // hauteur de la barquette
static float trayMarginBottom = 20.0f; // marge en bas

// Éditeur simple
static bool editorMode = false; // mode éditeur
static int editorSelectedLevel = 0; // niveau sélectionné   
static int editorSelectedIngredient = 0; // ingrédient sélectionné


// Fonctions utiles pour le jeu

// Retourne le nom d'un ingrédient en français
static const char* getIngredientName(IngredientType type) { //pointeur sur le nom de l'ingredient
    switch (type) {
        case ING_FRAISE: return "Fraise"; //retourne le nom de l'ingredient
        case ING_BANANE: return "Banane";
        case ING_KIWI: return "Kiwi";
        case ING_MANDARINE: return "Mandarine";
        case ING_CHOCOLAT: return "Chocolat";
        default: return "Inconnu";//evite les erreurs si erreur d'ingredient
    }
}

// Pointe vers l'image de chaque ingredient
static const char* getIngredientFilename(IngredientType type) {
    switch (type) {
        case ING_FRAISE: return "assets/cerisesurgateau/ingredient/Fraise.png";
        case ING_BANANE: return "assets/cerisesurgateau/ingredient/banane.png";
        case ING_KIWI: return "assets/cerisesurgateau/ingredient/kiwi.png";
        case ING_MANDARINE: return "assets/cerisesurgateau/ingredient/mandarine.png";
        case ING_CHOCOLAT: return "assets/cerisesurgateau/ingredient/chocolat.png";
        default: return "";
    }
}
//Pour cette partie, plus de détails dans le rapport

// Calcule la distance entre deux points avec pythagore 
static float dist(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;  
    float dy = a.y - b.y;  
    return sqrtf(dx * dx + dy * dy);  
}

// Vérifie si un point est dans un cercle =si en dehors ca ne marche pas
static bool pointInCircle(Vector2 p, Vector2 center, float radius) {
    return dist(p, center) <= radius;
}

// Convertit une distance en pixels en score de précision (0-100%)
static float calculatePrecision(Vector2 target, Vector2 placedCenter) {
    float d = dist(target, placedCenter);  
    float prec = 100.0f - (d / MAX_DISTANCE) * 100.0f; 
    if (prec < 0.0f) prec = 0.0f;  
    return prec;
}

// Calcule le score final et termine le niveau
static void calculateAndFinalizeScore(Level* level) {
    // On compte combien d'ingrédients sont placés
    int placed = 0;
    for (int i = 0; i < level->ingredientCount; ++i) {
        if (level->ingredients[i].isPlaced) {
            placed++;
        }
    }
    
    // Si tous les ingrédients ne sont pas placés, score = 0%
    if (placed < level->ingredientCount) {
        level->score = 0.0f;
    } else {
        // Tous les ingrédients sont placés, on calcule la précision de chacun
        float total = 0.0f;
        for (int i = 0; i < level->ingredientCount; ++i) {
            Ingredient* ing = &level->ingredients[i];
            // On calcule le centre réel de l'ingrédient (position = coin supérieur gauche)
            Vector2 centerPlaced = {
                ing->position.x + (ing->texture.width  / 2.0f),  // centre X
                ing->position.y + (ing->texture.height / 2.0f)  // centre Y
            };
            // On compare avec la position cible et on ajoute au total
            total += calculatePrecision(ing->targetPosition, centerPlaced);
        }
        // Score final = moyenne de toutes les précisions
        level->score = total / level->ingredientCount;
    }
    
    // On marque le niveau comme terminé et on passe à l'écran de fin
    level->completed = true;
    finalScore = level->score;
    gameState = STATE_GAME_COMPLETE;
    s_endScreen.wantsToExit = false;
    s_endScreen.wantsToReplay = false;
}

//On initialise un niveau

// Initialise un niveau : charge les images, définit les positions cibles, etc.
static void initLevel(int levelIndex) {
    Level* level = &levels[levelIndex];

    // Charge l'image du modèle (le gâteau qu'on montre au début)
    // Chaque niveau a son propre modèle : gateau1.png, gateau2.png, gateau3.png
    char modelPath[256];
    snprintf(modelPath, sizeof(modelPath),
             "assets/cerisesurgateau/image de gateau lvl/gateau%d.png",
             levelIndex + 1);  
    if (FileExists(modelPath)) {
        Image img = LoadImage(modelPath);
        if (img.data) {
            level->modelTexture = LoadTextureFromImage(img);
            UnloadImage(img);  //Supprime l'image
        }
    }

    // On ne charge pas le gâteau de base (on veut juste le fond)
    level->cakeBaseTexture.id = 0;

    level->ingredientCount = INGREDIENT_COUNT;

    // Pour chaque ingrédient, on initialise tout
    for (int i = 0; i < INGREDIENT_COUNT; ++i) {
        Ingredient* ing = &level->ingredients[i];
        ing->type = (IngredientType)i;
        ing->name = getIngredientName(ing->type);
        
        // targetPosition sera chargée depuis le fichier .ini
        ing->targetPosition = (Vector2){0, 0};
        ing->isPlaced = false;  // pas encore posé
        ing->isInTray = true;   // en bas dans la barquette

        // Charge l'image de l'ingrédient et la redimensionne à 96x96
        // Sinon les images sont trop grosses
        const char* filename = getIngredientFilename(ing->type);//pointe vers l'image de l'ingredient l114
        if (FileExists(filename)) {
            Image img = LoadImage(filename);
            if (img.data) {
                int size = 96;  // taille fixe pour tous les ingrédients
                ImageResize(&img, size, size);
                ing->texture = LoadTextureFromImage(img); //charge la texture de la ram vers gpu
                UnloadImage(img);  // on supprime l'image de la ram
            }
        } else {
            ing->texture.id = 0;  // pas de texture si le fichier n'existe pas
        }

        // Position initiale à (0,0), sera mise à jour par updateTrayPositionsForLevel
        ing->position = (Vector2){0, 0};
        // Rectangle de drag pour les clics sur l'ingredient
        if (ing->texture.id != 0) {
            ing->rect = (Rectangle){0, 0,
                                    (float)ing->texture.width,
                                    (float)ing->texture.height};
        } else {
            ing->rect = (Rectangle){0, 0, 50, 50};  // taille par défaut si pas de texture
        }
    }

    level->score = 0.0f;
    level->completed = false;
}

// Placement de la barquette (ingredients en ligne en bas de l'écran)

// Appelée à chaque frame pour repositionner les ingrédients qui ne sont pas encore posés
static void updateTrayPositionsForLevel(Level* level, int screenWidth, int screenHeight) {
    // Calcule la position Y de la barquette (en bas de l'écran) en fonction de la taille ecran
    int trayY = screenHeight - (int)trayHeight - (int)trayMarginBottom;
    
    // Centre de l'écran pour centrer la ligne d'ingrédients
    float centerX = (float)screenWidth / 2.0f;
    
    // Largeur totale de la ligne (5 ingrédients avec 4 espaces entre eux)
    float totalWidth = (INGREDIENT_COUNT - 1) * traySpacing;
    
    // Position X de départ pour centrer la ligne
    float startX = centerX - totalWidth / 2.0f;

    // Pour chaque ingrédient, on calcule sa position dans la barquette
    for (int i = 0; i < level->ingredientCount; ++i) {
        Ingredient* ing = &level->ingredients[i];
        // On skip ceux qui sont déjà posés ou qu'on est en train de bouger
        if (ing->isPlaced || ing == draggedIngredient) continue;

        // Position X : on part du début et on ajoute l'espacement pour chaque ingrédient
        float x = startX + i * traySpacing;
        float y;
        
        // Si l'ingrédient a une texture, on centre par rapport à sa taille
        if (ing->texture.id != 0) {
            x -= (float)ing->texture.width * trayScale / 2.0f;  // centre horizontalement
            y = (float)trayY + (trayHeight / 2.0f) - (float)ing->texture.height * trayScale / 2.0f;  // centre verticalement
        } else {
            // Pas de texture, on utilise une position par défaut
            x -= 30.0f;
            y = (float)trayY + (trayHeight / 2.0f) - 30.0f;
        }

        ing->position = (Vector2){x, y};
        ing->isInTray = true;

        // Met à jour le rectangle de drag pour les clics sur l'ingredient
        if (ing->texture.id != 0) {
            ing->rect = (Rectangle){x, y,
                                    ing->texture.width * trayScale,   // taille réduite
                                    ing->texture.height * trayScale};
        } else {
            ing->rect = (Rectangle){x, y, 60, 60};  // taille par défaut
        }
    }
}

// Save des niveaux dans un fichier .ini (détails dans le rapport)

// Appelée automatiquement quand on quitte le mode éditeur (Ctrl+F2)
static void saveLevelConfig(void) {
    FILE *f = fopen(LEVELCONFIG, "w");  // "w" = write, crée ou écrase le fichier
    if (!f) return;  // Si erreur d'ouverture, on arrête

    // on écrit les paramètres
    fprintf(f, "# Configuration des niveaux Cerise sur Gateau\n\n");
    fprintf(f, "tray_height=%.2f\n", trayHeight);
    fprintf(f, "cake_center=%.2f,%.2f\n", cakeCenter.x, cakeCenter.y);
    fprintf(f, "cake_radius=%.2f\n", cakeRadius);
    fprintf(f, "tray_scale=%.2f\n", trayScale);
    fprintf(f, "tray_margin_bottom=%.2f\n", trayMarginBottom);
    fprintf(f, "tray_spacing=%.2f\n", traySpacing);

    // Pour chaque niveau (3 niveaux) et chaque ingrédient (5 ingrédients)
    // On sauvegarde la position cible en pixels absolus
    for (int level = 0; level < MAX_LEVELS; ++level) {
        Level* l = &levels[level];
        for (int i = 0; i < l->ingredientCount; ++i) {
            // Format : level_1_ingredient_0=893.00,289.09
            fprintf(f, "level_%d_ingredient_%d=%.2f,%.2f\n",
                    level + 1, i,  // level + 1 car les fichiers utilisent 1-3, pas 0-2
                    l->ingredients[i].targetPosition.x,
                    l->ingredients[i].targetPosition.y);
        }
    }

    fclose(f);
}

// Charge les positions depuis le fichier .ini
static void loadLevelConfig(void) {
    FILE *f = fopen(LEVELCONFIG, "r");  // "r" = read, ouvre en lecture seul
    if (!f) return;  // Erreur d'ouverture du fichier

    // On lit le fichier ligne par ligne
    char line[256];  // stockage temporaire de la ligne
    while (fgets(line, sizeof(line), f)) {
        // On ignore les commentaires (lignes qui commencent par #) et les lignes vides
        if (line[0] == '#' || line[0] == '\n') continue;

        // On lit chaque ligne selon son type et donc un else pour chaque ligne possible
        // D'abord les paramètres (tray_height, cake_center, etc. lignes 310-316)
        if (strncmp(line, "tray_height=", 12) == 0) {
            float h;
            if (sscanf(line + 12, "%f", &h) == 1) {  // lit le nombre après "tray_height="
                trayHeight = h;
                // On limite entre 60 et 300 pour éviter des valeurs aberrantes
                if (trayHeight < 60.0f) trayHeight = 60.0f;
                if (trayHeight > 300.0f) trayHeight = 300.0f;
            }
        } else if (strncmp(line, "cake_center=", 12) == 0) {
            float x, y;
            if (sscanf(line + 12, "%f,%f", &x, &y) == 2) {  // lit "x,y"
                cakeCenter = (Vector2){x, y};
            }
        } else if (strncmp(line, "cake_radius=", 12) == 0) {
            float r;
            if (sscanf(line + 12, "%f", &r) == 1) {
                cakeRadius = r;
            }
        } else if (strncmp(line, "tray_scale=", 11) == 0) {
            float s;
            if (sscanf(line + 11, "%f", &s) == 1) {
                trayScale = s;
            }
        } else if (strncmp(line, "tray_margin_bottom=", 19) == 0) {
            float m;
            if (sscanf(line + 19, "%f", &m) == 1) {
                trayMarginBottom = m;
            }
        } else if (strncmp(line, "tray_spacing=", 13) == 0) {
            float sp;
            if (sscanf(line + 13, "%f", &sp) == 1) {
                traySpacing = sp;
            }
        } else {
            // Format : level_1_ingredient_0=893.00,289.09
            int level, ingredient;
            float x, y;
            if (sscanf(line, "level_%d_ingredient_%d=%f,%f", &level, &ingredient, &x, &y) == 4) {
                // On vérifie que les valeurs sont valides avant d'appliquer
                if (level >= 1 && level <= MAX_LEVELS && ingredient >= 0 && ingredient < INGREDIENT_COUNT) {
                    // level - 1 car le fichier utilise 1-3 mais le tableau utilise 0-2
                    levels[level - 1].ingredients[ingredient].targetPosition = (Vector2){x, y};
                }
            }
        }
    }
    fclose(f);
}

// Initialisation du jeu

// Fonction d'initialisation appelée au démarrage du jeu
static void mg_init(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // Le centre du gâteau est au centre de l'écran
    cakeCenter = (Vector2){ sw / 2.0f, sh / 2.0f };
    cakeRadius = CAKE_PLACEMENT_RADIUS;

    // Charge le fond d'écran (on essaie deux chemins possibles)
    if (FileExists("assets/cerisesurgateau/fond du jeu/fond ecran.png")) {
        Image img = LoadImage("assets/cerisesurgateau/fond du jeu/fond ecran.png");
        if (img.data) {
            backgroundTexture = LoadTextureFromImage(img);
            UnloadImage(img);
        }
    } else if (FileExists("assets/imagefond.png")) {
        Image img = LoadImage("assets/imagefond.png");
        if (img.data) {
            backgroundTexture = LoadTextureFromImage(img);
            UnloadImage(img);
        }
    }

    // Initialise les 3 niveaux
    // Pour chaque niveau, on charge les images
    for (int i = 0; i < MAX_LEVELS; ++i) {
        initLevel(i);
        updateTrayPositionsForLevel(&levels[i], sw, sh);
    }

    // Charge la configuration depuis le fichier .ini
    loadLevelConfig();

    // Sélectionne un niveau aléatoire pour cette partie
    srand((unsigned int)time(NULL));  // initialise le générateur aléatoire
    currentLevel = rand() % MAX_LEVELS;  // nombre entre 0 et 2
    
    // Initialise l'état du jeu
    gameState = STATE_SHOWING_MODEL;  // on commence par montrer le modèle
    modelTimer = 0.0f;
    levelTimer = 0.0f;
    draggedIngredient = NULL;
    finalScore = 0.0f;
    editorMode = false;
    editorSelectedLevel = 0;
    editorSelectedIngredient = 0;
    s_endScreen.wantsToExit = false;
    s_endScreen.wantsToReplay = false;
}

// Mode debug avec ctrl+f2

static void handleEditorToggle(void) {
    static bool f2PressedBefore = false;
    bool ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool f2Pressed = IsKeyPressed(KEY_F2);

    if (ctrlDown && f2Pressed && !f2PressedBefore) {
        editorMode = !editorMode;
        if (editorMode) {
            gameState = STATE_EDITOR; //on entre dans le mode debug
        } else {
            // Sauvegarder les paramètres quand on quitte l'éditeur
            saveLevelConfig(); //on sauvegarde les paramètres dans le fichier .ini
            gameState = STATE_PLAYING; //on revient au jeu
        }
    }
    if (!f2Pressed) f2PressedBefore = false;
    if (f2Pressed && !f2PressedBefore) f2PressedBefore = true;
}
//partie pour edition des niveaux, peut etre enlever une fois les niveaux finis
static void updateEditor(void) {
    if (!editorMode || gameState != STATE_EDITOR) return;

    Level* level = &levels[editorSelectedLevel];
    Vector2 mouse = GetMousePosition();

    // changer de niveau avec u/i/o
    if (IsKeyPressed(KEY_U))  editorSelectedLevel = 0;
    if (IsKeyPressed(KEY_I))  editorSelectedLevel = 1;
    if (IsKeyPressed(KEY_O))  editorSelectedLevel = 2;

    // choisir ingrédient (1..5)
    if (IsKeyPressed(KEY_ONE))  editorSelectedIngredient = 0;
    if (IsKeyPressed(KEY_TWO))  editorSelectedIngredient = 1;
    if (IsKeyPressed(KEY_THREE))editorSelectedIngredient = 2;
    if (IsKeyPressed(KEY_FOUR)) editorSelectedIngredient = 3;
    if (IsKeyPressed(KEY_FIVE)) editorSelectedIngredient = 4;

    // déplacer centre du cercle : clic droit + drag
    static bool draggingCenter = false;
    static bool draggingRadius = false;

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) { //lorsque le clic droit est pressé
        float d = dist(mouse, cakeCenter);
        if (d < 20.0f) { //si la distance est inférieure à 20 pixels
            draggingCenter = true; //on commence à drag le centre
        } else if (fabsf(d - cakeRadius) < 15.0f) { //si la distance est inférieure à 15 pixels
            draggingRadius = true; //on commence à drag le rayon
        }
    }
 // lorsque le clic droit est maintenu
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        if (draggingCenter) {
            cakeCenter = mouse;
            // on met a jour le centre
        } else if (draggingRadius) {
            float d = dist(mouse, cakeCenter); //distance entre le centre et la souris
            cakeRadius = d; //on met a jour le rayon
            if (cakeRadius < 50.0f) cakeRadius = 50.0f; //limite le rayon
            if (cakeRadius > 500.0f) cakeRadius = 500.0f;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) { //lorsque le clic droit est relaché
        draggingCenter = false; //on stoppe le drag du centre
        draggingRadius = false; //on stoppe le drag du rayon
    }

    // déplacer la cible de l'ingrédient sélectionné : clic gauche dans le cercle
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { //lorsque le clic gauche est maintenu 
        if (editorSelectedIngredient >= 0 && //si l'ingredient est selectionné
            editorSelectedIngredient < level->ingredientCount && //si l'ingredient est dans le niveau
            pointInCircle(mouse, cakeCenter, cakeRadius)) { //si la souris est dans le cercle
            level->ingredients[editorSelectedIngredient].targetPosition = mouse; //on met a jour la position de l'ingredient
        }
    }
}

// Update du jeu 
// dt = delta time (temps écoulé depuis la dernière frame, en secondes)
static void mg_update(float dt) {
    // Vérifie si on appuie sur Ctrl+F2 pour activer/désactiver l'éditeur
    handleEditorToggle();

    // Si on est en mode éditeur, on gère juste l'éditeur et on sort
    if (gameState == STATE_EDITOR) {
        updateEditor();
        return;
    }

    // Si le jeu est terminé, on gère l'écran de fin
    if (gameState == STATE_GAME_COMPLETE) {
        bool won = finalScore >= PASSING_SCORE;  // gagné si score >= 60%
        UpdateEndScreen(&s_endScreen, won, !won);
        
        // Si le joueur veut rejouer, on charge un nouveau niveau aléatoire
        if (s_endScreen.wantsToReplay) {
            // On choisit un niveau différent du précédent (pour varier)
            int newLevel;
            do {
                newLevel = rand() % MAX_LEVELS;
            } while (newLevel == currentLevel && MAX_LEVELS > 1);  // différent si possible
            currentLevel = newLevel;
            
            // On remet tout à zéro pour le nouveau niveau
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            updateTrayPositionsForLevel(&levels[currentLevel], sw, sh);
            gameState = STATE_SHOWING_MODEL;  // on recommence par montrer le modèle
            modelTimer = 0.0f;
            levelTimer = 0.0f;
            draggedIngredient = NULL;
            finalScore = 0.0f;
            levels[currentLevel].completed = false;
            levels[currentLevel].score = 0.0f;
            s_endScreen.wantsToReplay = false;
            s_endScreen.wantsToExit = false;
        }
        return;
    }

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    Level* level = &levels[currentLevel];

    // État : on montre le modèle (le gâteau qu'il faut reproduire)
    if (gameState == STATE_SHOWING_MODEL) {
        modelTimer += dt;  // on compte le temps
        // Après 5 secondes, on passe au jeu
        if (modelTimer >= MODEL_DISPLAY_TIME) {
            gameState = STATE_PLAYING;
            levelTimer = LEVEL_TIME;  // on démarre le timer de 30 secondes

            // On remet tous les ingrédients en bas (barquette)
            for (int i = 0; i < level->ingredientCount; ++i) {
                level->ingredients[i].isPlaced = false;
                level->ingredients[i].isInTray = true;
            }
            updateTrayPositionsForLevel(level, sw, sh);
        }
        return;
    }

    // État : on joue (le joueur place les ingrédients)
    if (gameState == STATE_PLAYING) {
        levelTimer -= dt;  // le timer descend
        if (levelTimer < 0.0f) levelTimer = 0.0f;  // pas de temps négatif

        // On met à jour les positions des ingrédients qui sont encore en bas
        // (ceux qui sont posés restent où ils sont)
        updateTrayPositionsForLevel(level, sw, sh);

        // Gestion du drag & drop
        Vector2 mouse = GetMousePosition();

        // Début du drag : quand on clique, on vérifie si on a cliqué sur un ingrédient
        // On peut attraper un ingrédient qui est en bas (barquette) OU déjà posé sur le gâteau
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            draggedIngredient = NULL;
            // On parcourt tous les ingrédients pour voir lequel est sous la souris
            for (int i = 0; i < level->ingredientCount; ++i) {
                Ingredient* ing = &level->ingredients[i];
                // On peut attraper si l'ingrédient est dans la barquette OU déjà posé
                // ET si la souris est dans le rectangle de l'ingrédient
                if ((ing->isInTray || ing->isPlaced) &&
                    CheckCollisionPointRec(mouse, ing->rect)) {
                    draggedIngredient = ing;
                    // On calcule l'offset pour que l'ingrédient suive bien la souris
                    // (on veut que le centre de l'ingrédient suive la souris, pas le coin)
                    dragOffset = (Vector2){
                        ing->rect.width  / 2.0f,
                        ing->rect.height / 2.0f
                    };
                    break;  // on a trouvé, on arrête
                }
            }
        }

        // Pendant le drag : on suit la souris
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggedIngredient) {
            // Position de l'ingrédient = position de la souris - offset
            // Comme ça le centre de l'ingrédient suit la souris
            draggedIngredient->position = (Vector2){
                mouse.x - dragOffset.x,
                mouse.y - dragOffset.y
            };
            // On met à jour le rectangle de collision aussi
            draggedIngredient->rect.x = draggedIngredient->position.x;
            draggedIngredient->rect.y = draggedIngredient->position.y;
        }

        // Fin du drag : quand on relâche le clic
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && draggedIngredient) {
            Vector2 mouseUp = GetMousePosition();
            // Si on relâche dans le cercle du gâteau, on pose l'ingrédient
            if (pointInCircle(mouseUp, cakeCenter, cakeRadius)) {
                // L'ingrédient est posé sur le gâteau
                draggedIngredient->isPlaced = true;
                draggedIngredient->isInTray = false;

                // On centre l'ingrédient sur la position de la souris
                if (draggedIngredient->texture.id != 0) {
                    draggedIngredient->position.x =
                        mouseUp.x - draggedIngredient->texture.width / 2.0f;
                    draggedIngredient->position.y =
                        mouseUp.y - draggedIngredient->texture.height / 2.0f;
                } else {
                    // Pas de texture, on utilise une position par défaut
                    draggedIngredient->position.x = mouseUp.x - 30.0f;
                    draggedIngredient->position.y = mouseUp.y - 30.0f;
                }
                draggedIngredient->rect.x = draggedIngredient->position.x;
                draggedIngredient->rect.y = draggedIngredient->position.y;
            } else {
                // Si on relâche en dehors du cercle, on remet l'ingrédient en bas
                draggedIngredient->isPlaced = false;
                draggedIngredient->isInTray = true;
            }

            draggedIngredient = NULL;  // on arrête de bouger cet ingrédient
        }

        // Bouton "Terminer" à droite de l'écran
        // Le joueur peut cliquer dessus pour terminer avant la fin du temps
        int buttonX = sw - 180;
        int buttonY = sh / 2 - 40;
        int buttonWidth = 150;
        int buttonHeight = 80;
        Rectangle finishButton = {buttonX, buttonY, buttonWidth, buttonHeight};
        
        if (CheckCollisionPointRec(mouse, finishButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            // Le joueur a cliqué sur "Terminer", on calcule le score tout de suite
            calculateAndFinalizeScore(level);
        }

        // Si le temps est écoulé, on calcule aussi le score
        if (levelTimer <= 0.0f) {
            calculateAndFinalizeScore(level);
        }
    }
}

// --------- mg_draw ---------

static void mg_draw(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // fond
    if (backgroundTexture.id != 0) {
        float sx = (float)sw / backgroundTexture.width;
        float sy = (float)sh / backgroundTexture.height;
        float s  = (sx > sy) ? sx : sy;
        float w  = backgroundTexture.width * s;
        float h  = backgroundTexture.height * s;
        float x  = sw / 2.0f - w / 2.0f;
        float y  = sh / 2.0f - h / 2.0f;
        DrawTextureEx(backgroundTexture, (Vector2){x, y}, 0.0f, s, WHITE);
    } else {
        ClearBackground((Color){240, 240, 240, 255});
    }

    Level* level = &levels[currentLevel];

    // Affichage du modèle (le gâteau qu'il faut reproduire)
    // On le montre pendant 5 secondes avec un effet de fondu
    if (gameState == STATE_SHOWING_MODEL) {
        if (level->modelTexture.id != 0) {
            float progress = modelTimer / MODEL_DISPLAY_TIME;  // 0.0 à 1.0
            float alpha = 1.0f;
            // Effet de fondu : apparaît au début (0-20%), disparaît à la fin (80-100%)
            if (progress < 0.2f) alpha = progress / 0.2f;  // fade in
            else if (progress > 0.8f) alpha = (1.0f - progress) / 0.2f;  // fade out
            
            // S'assurer que l'alpha est à 0 à la fin du timer
            if (progress >= 1.0f) alpha = 0.0f;

            if (alpha > 0.0f) {
                // Fond noir semi-transparent pour mettre en valeur le modèle
                DrawRectangle(0, 0, sw, sh, (Color){0,0,0,(unsigned char)(180*alpha)});

                // On dessine le modèle au centre, réduit à 60% de sa taille
                float scale = 0.6f;
                float x = sw / 2.0f - level->modelTexture.width * scale / 2.0f;
                float y = sh / 2.0f - level->modelTexture.height * scale / 2.0f;
                DrawTextureEx(level->modelTexture, (Vector2){x,y}, 0, scale,
                              (Color){255,255,255,(unsigned char)(255*alpha)});
            }
        }

        // Affiche le texte "Observez le modele..." et le compte à rebours
        if (modelTimer < MODEL_DISPLAY_TIME) {
            DrawText("Observez le modele...", sw/2 - 150, 50, 30, WHITE);
            DrawText(TextFormat("%.1f", MODEL_DISPLAY_TIME - modelTimer),
                     sw/2 - 20, 100, 40, YELLOW);
        }
        return;
    }

    // Gâteau de base - ne pas afficher (on ne veut que le fond)

    // Ingrédients posés
    for (int i = 0; i < level->ingredientCount; ++i) {
        Ingredient* ing = &level->ingredients[i];
        if (ing->isPlaced && ing != draggedIngredient) {
            if (ing->texture.id != 0) {
                DrawTexture(ing->texture, (int)ing->position.x, (int)ing->position.y, WHITE);
            } else {
                DrawRectangle((int)ing->position.x, (int)ing->position.y, 40, 40, RED);
                DrawText(ing->name, (int)ing->position.x+5, (int)ing->position.y+10, 10, WHITE);
            }
        }
    }

    // Ligne d'ingrédients en bas (barquette invisible)
    int trayY = sh - (int)trayHeight - (int)trayMarginBottom;
    float centerX = (float)sw / 2.0f;
    float totalWidth = (INGREDIENT_COUNT - 1) * traySpacing;
    float startX = centerX - totalWidth / 2.0f;

    for (int i = 0; i < level->ingredientCount; ++i) {
        Ingredient* ing = &level->ingredients[i];
        if (ing->isPlaced || ing == draggedIngredient) continue;

        float x = startX + i * traySpacing;
        float y;
        if (ing->texture.id != 0) {
            x -= (float)ing->texture.width * trayScale / 2.0f;
            y = (float)trayY + (trayHeight / 2.0f) - (float)ing->texture.height * trayScale / 2.0f;
            DrawTextureEx(ing->texture, (Vector2){x, y}, 0.0f, trayScale, WHITE);
        } else {
            x -= 30.0f;
            y = (float)trayY + (trayHeight / 2.0f) - 30.0f;
            DrawRectangle((int)x, (int)y, 60, 60, RED);
            DrawText(ing->name, (int)x+5, (int)y+20, 12, WHITE);
        }
    }

    // Ingrédient que tu bouges en ce moment (dessiné par dessus tout)
    if (draggedIngredient) {
        if (draggedIngredient->texture.id != 0) {
            DrawTexture(draggedIngredient->texture,
                        (int)draggedIngredient->position.x,
                        (int)draggedIngredient->position.y,
                        WHITE);
        } else {
            DrawRectangle((int)draggedIngredient->position.x,
                          (int)draggedIngredient->position.y,
                          80,80,(Color){255,200,100,255});
            DrawRectangleLines((int)draggedIngredient->position.x,
                               (int)draggedIngredient->position.y,
                               80,80,RED);
            int tw = MeasureText(draggedIngredient->name, 16);
            DrawText(draggedIngredient->name,
                     (int)draggedIngredient->position.x + (80-tw)/2,
                     (int)draggedIngredient->position.y + 30,
                     16, BLACK);
        }
    }

    // HUD
    if (gameState == STATE_PLAYING) {
        DrawText(TextFormat("Temps restant: %.1f", levelTimer), 20, 20, 30, BLACK);
        
        // Bouton "Terminer" à droite
        int buttonX = sw - 180;
        int buttonY = sh / 2 - 40;
        int buttonWidth = 150;
        int buttonHeight = 80;
        Rectangle finishButton = {buttonX, buttonY, buttonWidth, buttonHeight};
        Vector2 mouse = GetMousePosition();
        
        Color buttonColor = CheckCollisionPointRec(mouse, finishButton) ? 
                           (Color){100, 200, 100, 255} : (Color){50, 150, 50, 255};
        
        DrawRectangleRounded(finishButton, 0.3f, 8, buttonColor);
        DrawRectangleRoundedLines(finishButton, 0.3f, 8, WHITE);
        
        const char* buttonText = "Terminer";
        int textWidth = MeasureText(buttonText, 24);
        DrawText(buttonText, 
                 buttonX + (buttonWidth - textWidth) / 2,
                 buttonY + (buttonHeight - 24) / 2,
                 24, WHITE);
    }

    // Écran final avec le système standard
    if (gameState == STATE_GAME_COMPLETE) {
        bool won = finalScore >= PASSING_SCORE;
        int coinsWon = won ? 5 : 0; // 5 pièces si gagné, 0 si perdu
        
        int cx = GetScreenWidth()/2;
        int cy = GetScreenHeight()/2;
        
        // Dessiner le popup avec une hauteur plus grande pour le score
        int modalW = 450;
        int modalH = 260; // Augmenté pour avoir de la place pour le score
        Rectangle modal = {cx - modalW/2, cy - modalH/2, modalW, modalH};
        
        // Rectangle modal avec ombre légère
        DrawRectangle(cx - modalW/2 + 3, cy - modalH/2 + 3, modalW, modalH, (Color){0, 0, 0, 50});
        DrawRectangleRec(modal, (Color){250, 250, 250, 255});
        DrawRectangleLinesEx(modal, 4, (Color){80, 80, 80, 255});
        
        int msgY = cy - 80;
        int by = cy + 70;
        
        if (won) {
            char msg[50];
            snprintf(msg, sizeof(msg), "Bravo ! Tu as gagne %d piece%s !", coinsWon, coinsWon > 1 ? "s" : "");
            DrawText("BRAVO !", cx - MeasureText("BRAVO !", 40)/2, msgY, 40, GREEN);
            
            // Afficher le score
            char scoreText[50];
            snprintf(scoreText, sizeof(scoreText), "Score: %.1f%%", finalScore);
            DrawText(scoreText, cx - MeasureText(scoreText, 24)/2, msgY + 45, 24, DARKBLUE);
            
            DrawText(msg, cx - MeasureText(msg, 24)/2, msgY + 75, 24, DARKGRAY);
        } else {
            DrawText("PERDU !", cx - MeasureText("PERDU !", 40)/2, msgY, 40, RED);
            
            // Afficher le score
            char scoreText[50];
            snprintf(scoreText, sizeof(scoreText), "Score: %.1f%%", finalScore);
            DrawText(scoreText, cx - MeasureText(scoreText, 24)/2, msgY + 45, 24, DARKBLUE);
            
            if (coinsWon > 0) {
                char msg[50];
                snprintf(msg, sizeof(msg), "Tu as gagne %d piece%s !", coinsWon, coinsWon > 1 ? "s" : "");
                DrawText(msg, cx - MeasureText(msg, 24)/2, msgY + 75, 24, DARKGRAY);
            }
        }
        
        // Boutons
        Rectangle replayBtn = {cx - 120, by, 100, 40};
        Rectangle backBtn = {cx + 20, by, 100, 40};
        Vector2 m = GetMousePosition();
        
        Color replayColor = CheckCollisionPointRec(m, replayBtn) ? (Color){100, 200, 100, 255} : (Color){50, 150, 50, 255};
        Color backColor = CheckCollisionPointRec(m, backBtn) ? (Color){200, 100, 100, 255} : (Color){150, 50, 50, 255};
        
        DrawRectangleRec(replayBtn, replayColor);
        DrawText("Rejouer", cx - 100, by + 12, 20, WHITE);
        DrawRectangleRec(backBtn, backColor);
        DrawText("Retour", cx + 40, by + 12, 20, WHITE);
    }

    // Éditeur
    if (gameState == STATE_EDITOR) {
        DrawRectangle(0,0,sw,sh,(Color){0,0,0,150});

        DrawText("MODE EDITEUR (Ctrl+F2 pour quitter)",
                 sw/2 - 220, 20, 20, YELLOW);
        DrawText("Clic droit = bouger cercle (centre / rayon)",
                 40, 60, 18, WHITE);
        DrawText("Clic gauche dans le cercle = déplacer la cible de l'ingredient choisi",
                 40, 80, 18, WHITE);
        DrawText("u/i/o = niveau, 1..5 = ingredient cible",
                 40, 100, 18, WHITE);

        DrawText(TextFormat("Niveau: %d", editorSelectedLevel+1), 40, 130, 18, WHITE);
        DrawText(TextFormat("Ingredient cible: %d", editorSelectedIngredient+1),
                 40, 150, 18, WHITE);

        // cercle
        DrawCircleLines((int)cakeCenter.x, (int)cakeCenter.y,
                        (int)cakeRadius, (Color){200,200,200,255});
        DrawCircle((int)cakeCenter.x, (int)cakeCenter.y, 5, YELLOW);
        DrawCircle((int)(cakeCenter.x + cakeRadius),
                   (int)cakeCenter.y, 5, GREEN);

        Level* lev = &levels[editorSelectedLevel];
        for (int i = 0; i < lev->ingredientCount; ++i) {
            Vector2 t = lev->ingredients[i].targetPosition;
            Color c = (i == editorSelectedIngredient) ? YELLOW : GREEN;
            DrawCircle((int)t.x, (int)t.y, 8, c);
            DrawText(lev->ingredients[i].name, (int)t.x+10, (int)t.y-10, 16, c);
        }
    }
}

// --------- mg_unload / mg_isCompleted ---------

static void mg_unload(void) {
    for (int i = 0; i < MAX_LEVELS; ++i) {
        Level* l = &levels[i];
        if (l->modelTexture.id != 0)      UnloadTexture(l->modelTexture);
        if (l->cakeBaseTexture.id != 0 &&
            l->cakeBaseTexture.id != l->modelTexture.id)
            UnloadTexture(l->cakeBaseTexture);

        for (int j = 0; j < l->ingredientCount; ++j) {
            if (l->ingredients[j].texture.id != 0)
                UnloadTexture(l->ingredients[j].texture);
        }
    }
    if (backgroundTexture.id != 0) UnloadTexture(backgroundTexture);
}

static bool mg_isCompleted(int *coinsOut) {
    if (gameState == STATE_GAME_COMPLETE) {
        // 5 pièces si gagné (score >= 60%), 0 si perdu
        bool won = finalScore >= PASSING_SCORE;
        int coinsWon = won ? 5 : 0;

        if (coinsOut) *coinsOut = coinsWon;

        // Ne signaler la complétion que si l'utilisateur veut quitter
        return s_endScreen.wantsToExit;
    }
    return false;
}

MinigameAPI GetMinigameCeriseSurGateau(void) {
    MinigameAPI api = {
        mg_init,
        mg_update,
        mg_draw,
        mg_unload,
        mg_isCompleted
    };
    return api;
}
