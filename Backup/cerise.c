// Jeu "Cerise sur Gateau" - Placement d'ingrédients sur un gâteau
// Le joueur doit replacer des ingrédients sur un gâteau selon un modèle
// Utilise raylib pour l'affichage et les entrées

#include "cerisesurgateau.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define INGREDIENT_COUNT 5
#define MAX_LEVELS 3
#define MODEL_DISPLAY_TIME 5.0f
#define LEVEL_TIME 30.0f
#define CAKE_PLACEMENT_RADIUS 200.0f
#define MAX_DISTANCE 200.0f
#define PASSING_SCORE 90.0f

// Types d'ingrédients
typedef enum {
    ING_FRAISE = 0,
    ING_BANANE,
    ING_KIWI,
    ING_MANDARINE,
    ING_CHOCOLAT
} IngredientType;

// État du jeu
typedef enum {
    STATE_SHOWING_MODEL,    // Affichage du modèle pendant 5 secondes
    STATE_PLAYING,          // Phase de jeu active
    STATE_LEVEL_COMPLETE,   // Niveau terminé
    STATE_GAME_COMPLETE,    // Tous les niveaux terminés
    STATE_EDITOR            // Mode éditeur (pause)
} GameState;

// Représentation d'un ingrédient
typedef struct {
    IngredientType type;
    Texture2D texture;
    const char* name;
    Vector2 position;           // Position actuelle à l'écran (en pixels)
    Vector2 targetPosition;     // Position cible pour ce niveau (en pixels)
    bool isPlaced;              // Si l'ingrédient a été placé sur le gâteau
    bool isInTray;              // Si l'ingrédient est dans la "barquette"
    Rectangle rect;             // Rectangle de collision pour les clics
} Ingredient;

// Représentation d'un niveau
typedef struct {
    Texture2D modelTexture;     // Image du gâteau modèle (avec ingrédients)
    Texture2D cakeBaseTexture;  // Image du gâteau vide/base
    Ingredient ingredients[INGREDIENT_COUNT];  // Ingrédients à placer
    int ingredientCount;        // Nombre d'ingrédients pour ce niveau
    float score;                // Score de précision pour ce niveau
    bool completed;             // Si le niveau est complété
} Level;

// État global du jeu
static GameState gameState = STATE_SHOWING_MODEL;
static int currentLevel = 0;
static float modelTimer = 0.0f;
static float levelTimer = 0.0f;
static bool canPlace = true;

// Niveaux
static Level levels[MAX_LEVELS];

// Texture de fond
static Texture2D backgroundTexture = {0};

// Ingredient en cours de drag
static Ingredient* draggedIngredient = NULL;
static Vector2 dragOffset = {0, 0};

// "Barquette" (juste une ligne d'ingrédients, visuellement sans fond)
static Vector2 trayStartPos = {0, 0};
static float traySpacing = 80.0f;
static float trayScale = 0.4f;          // Taille des ingrédients en bas
static float trayMarginBottom = 20.0f;  // Marge en bas
static float trayHeight = 120.0f;

// Zone de placement du gâteau (cercle)
static Vector2 cakeCenter = {0, 0};
static float cakeRadius = CAKE_PLACEMENT_RADIUS;

// Scores finaux
static float finalScore = 0.0f;

// Mode éditeur
static bool editorMode = false;
static int editorSelectedLevel = 0;
static int editorSelectedIngredient = -1;
static int editorModeSelection = 0;     // 0=ingrédient cible, 1=cercle, 2=barquette, 3=positions départ
static const char* LEVEL_CONFIG_FILE = "config/cerise_levels.ini";

// Flags pour drag du cercle dans l'éditeur
static bool draggingCircleCenter = false;
static bool draggingCircleRadius = false;

/* ---------- Helpers ---------- */

static const char* getIngredientName(IngredientType type) {
    switch (type) {
        case ING_FRAISE: return "Fraise";
        case ING_BANANE: return "Banane";
        case ING_KIWI: return "Kiwi";
        case ING_MANDARINE: return "Mandarine";
        case ING_CHOCOLAT: return "Chocolat";
        default: return "Inconnu";
    }
}

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

static float distance_vec(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

static bool isPointInCircle(Vector2 point, Vector2 center, float radius) {
    return distance_vec(point, center) <= radius;
}

static float calculatePrecision(Vector2 target, Vector2 placedCenter) {
    float dist = distance_vec(target, placedCenter);
    float precision = 100.0f - (dist / MAX_DISTANCE) * 100.0f;
    if (precision < 0.0f) precision = 0.0f;
    if (precision > 100.0f) precision = 100.0f;
    return precision;
}

/* ---------- Sauvegarde/Chargement des niveaux ---------- */

static void saveLevelConfig(void) {
    FILE *f = fopen(LEVEL_CONFIG_FILE, "w");
    if (!f) return;

    fprintf(f, "# Configuration des niveaux Cerise sur Gateau\n\n");
    fprintf(f, "tray_height=%.2f\n", trayHeight);
    fprintf(f, "cake_center=%.2f,%.2f\n", cakeCenter.x, cakeCenter.y);
    fprintf(f, "cake_radius=%.2f\n", cakeRadius);
    fprintf(f, "tray_scale=%.2f\n", trayScale);
    fprintf(f, "tray_margin_bottom=%.2f\n", trayMarginBottom);
    fprintf(f, "tray_spacing=%.2f\n", traySpacing);

    for (int level = 0; level < MAX_LEVELS; ++level) {
        Level* l = &levels[level];
        for (int i = 0; i < l->ingredientCount; ++i) {
            fprintf(f, "level_%d_ingredient_%d=%.2f,%.2f\n",
                    level + 1, i,
                    l->ingredients[i].targetPosition.x,
                    l->ingredients[i].targetPosition.y);
        }
    }

    fclose(f);
}

static void loadLevelConfig(void) {
    FILE *f = fopen(LEVEL_CONFIG_FILE, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (strncmp(line, "tray_height=", 12) == 0) {
            float h;
            if (sscanf(line + 12, "%f", &h) == 1) {
                trayHeight = h;
                if (trayHeight < 60.0f) trayHeight = 60.0f;
                if (trayHeight > 300.0f) trayHeight = 300.0f;
            }
        } else if (strncmp(line, "cake_center=", 12) == 0) {
            float x, y;
            if (sscanf(line + 12, "%f,%f", &x, &y) == 2) {
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
                if (trayScale < 0.1f) trayScale = 0.1f;
                if (trayScale > 2.0f) trayScale = 2.0f;
            }
        } else if (strncmp(line, "tray_margin_bottom=", 20) == 0) {
            float m;
            if (sscanf(line + 20, "%f", &m) == 1) {
                trayMarginBottom = m;
                if (trayMarginBottom < 0.0f) trayMarginBottom = 0.0f;
                if (trayMarginBottom > 400.0f) trayMarginBottom = 400.0f;
            }
        } else if (strncmp(line, "tray_spacing=", 12) == 0) {
            float s;
            if (sscanf(line + 12, "%f", &s) == 1) {
                traySpacing = s;
                if (traySpacing < 20.0f) traySpacing = 20.0f;
                if (traySpacing > 200.0f) traySpacing = 200.0f;
            }
        } else {
            int level, ing;
            float x, y;
            if (sscanf(line, "level_%d_ingredient_%d=%f,%f",
                       &level, &ing, &x, &y) == 4) {
                if (level >= 1 && level <= MAX_LEVELS &&
                    ing >= 0 && ing < INGREDIENT_COUNT) {
                    levels[level - 1].ingredients[ing].targetPosition =
                        (Vector2){x, y};
                }
            }
        }
    }

    fclose(f);
}

/* ---------- Initialisation d'un niveau ---------- */

static void initLevel(int levelIndex) {
    Level* level = &levels[levelIndex];

    // Image du modèle
    char modelPath[256];
    snprintf(modelPath, sizeof(modelPath),
             "assets/cerisesurgateau/image de gateau lvl/gateau_niveau%d.png",
             levelIndex + 1);
    if (FileExists(modelPath)) {
        Image img = LoadImage(modelPath);
        if (img.data) {
            level->modelTexture = LoadTextureFromImage(img);
            UnloadImage(img);
        }
    }

    // Image de base
    if (FileExists("assets/cerisesurgateau/image de gateau lvl/gateau_base.png")) {
        Image img = LoadImage("assets/cerisesurgateau/image de gateau lvl/gateau_base.png");
        if (img.data) {
            level->cakeBaseTexture = LoadTextureFromImage(img);
            UnloadImage(img);
        }
    } else {
        level->cakeBaseTexture = level->modelTexture;
    }

    level->ingredientCount = INGREDIENT_COUNT;

    // Positions cibles par défaut
    Vector2 defaultPositions[INGREDIENT_COUNT] = {
        {400, 300},  // Fraise
        {450, 320},  // Banane
        {500, 300},  // Kiwi
        {350, 320},  // Mandarine
        {425, 350}   // Chocolat
    };

    for (int i = 0; i < INGREDIENT_COUNT; ++i) {
        Ingredient* ing = &level->ingredients[i];
        ing->type = (IngredientType)i;
        ing->name = getIngredientName(ing->type);
        ing->targetPosition = defaultPositions[i];
        ing->isPlaced = false;
        ing->isInTray = true;

        // Charger et REDIMENSIONNER la texture (PNG trop gros -> on réduit)
        const char* filename = getIngredientFilename(ing->type);
        if (FileExists(filename)) {
            Image img = LoadImage(filename);
            if (img.data) {
                int targetSize = 96;          // taille finale (change à 64 si tu veux encore plus petit)
                ImageResize(&img, targetSize, targetSize);
                ing->texture = LoadTextureFromImage(img);
                UnloadImage(img);
            }
        }

        ing->position = (Vector2){0.0f, 0.0f};
        if (ing->texture.id != 0) {
            ing->rect = (Rectangle){ing->position.x, ing->position.y,
                                    (float)ing->texture.width,
                                    (float)ing->texture.height};
        } else {
            ing->rect = (Rectangle){ing->position.x, ing->position.y, 50, 50};
        }
    }

    level->score = 0.0f;
    level->completed = false;
}

/* ---------- Lifecycle ---------- */

static void mg_init(void) {
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    cakeCenter = (Vector2){(float)screenWidth / 2.0f,
                           (float)screenHeight / 2.0f};
    if (cakeRadius <= 0.0f) cakeRadius = CAKE_PLACEMENT_RADIUS;

    trayStartPos = (Vector2){0.0f, (float)screenHeight - trayHeight};

    // Fond
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

    // Niveaux
    for (int i = 0; i < MAX_LEVELS; ++i) {
        initLevel(i);
    }

    // Charger config (cercle, "barquette", positions cibles)
    loadLevelConfig();

    // Placer tous les ingrédients en ligne en bas au départ
    for (int i = 0; i < MAX_LEVELS; ++i) {
        Level* level = &levels[i];
        float trayCenterX = (float)screenWidth / 2.0f;
        float totalWidth = (INGREDIENT_COUNT - 1) * traySpacing;
        float startX = trayCenterX - totalWidth / 2.0f;
        float trayY = (float)screenHeight - trayHeight + 20.0f;

        for (int j = 0; j < level->ingredientCount; ++j) {
            level->ingredients[j].isPlaced = false;
            level->ingredients[j].isInTray = true;
            level->ingredients[j].position =
                (Vector2){startX + j * traySpacing, trayY};
            level->ingredients[j].rect.x = level->ingredients[j].position.x;
            level->ingredients[j].rect.y = level->ingredients[j].position.y;
        }
    }

    currentLevel = 0;
    gameState = STATE_SHOWING_MODEL;
    modelTimer = 0.0f;
    levelTimer = 0.0f;
    draggedIngredient = NULL;
    finalScore = 0.0f;
    editorMode = false;
    editorSelectedLevel = 0;
    editorSelectedIngredient = -1;
}

/* ---------- Gestion éditeur ---------- */

static void handleEditorMode(void) {
    static bool f2WasPressed = false;
    bool ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool f2Pressed = IsKeyPressed(KEY_F2);

    if (ctrlDown && f2Pressed && !f2WasPressed) {
        f2WasPressed = true;
        editorMode = !editorMode;
        if (editorMode) {
            gameState = STATE_EDITOR;
            editorSelectedIngredient = -1;
        } else {
            if (gameState == STATE_EDITOR)
                gameState = STATE_PLAYING;
            saveLevelConfig();
        }
    }
    if (!f2Pressed) f2WasPressed = false;

    if (!editorMode || gameState != STATE_EDITOR) return;

    Vector2 mousePos = GetMousePosition();

    if (IsKeyPressed(KEY_ONE))  editorSelectedLevel = 0;
    if (IsKeyPressed(KEY_TWO))  editorSelectedLevel = 1;
    if (IsKeyPressed(KEY_THREE))editorSelectedLevel = 2;

    if (IsKeyPressed(KEY_TAB)) {
        editorModeSelection = (editorModeSelection + 1) % 4;
        editorSelectedIngredient = -1;
    }

    Level* level = &levels[editorSelectedLevel];

    // 0 = Position cible des ingrédients
    if (editorModeSelection == 0) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            bool clickedOnTarget = false;
            for (int i = 0; i < level->ingredientCount; ++i) {
                float dist = distance_vec(mousePos,
                                          level->ingredients[i].targetPosition);
                if (dist < 20.0f) {
                    editorSelectedIngredient = i;
                    clickedOnTarget = true;
                    break;
                }
            }
            if (!clickedOnTarget && isPointInCircle(mousePos, cakeCenter, cakeRadius)) {
                if (editorSelectedIngredient >= 0)
                    level->ingredients[editorSelectedIngredient].targetPosition = mousePos;
                else {
                    editorSelectedIngredient = 0;
                    level->ingredients[0].targetPosition = mousePos;
                }
                saveLevelConfig();
            }
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && editorSelectedIngredient >= 0) {
            if (isPointInCircle(mousePos, cakeCenter, cakeRadius))
                level->ingredients[editorSelectedIngredient].targetPosition = mousePos;
        }

        if (IsKeyPressed(KEY_ONE))  editorSelectedIngredient = 0;
        if (IsKeyPressed(KEY_TWO))  editorSelectedIngredient = 1;
        if (IsKeyPressed(KEY_THREE))editorSelectedIngredient = 2;
        if (IsKeyPressed(KEY_FOUR)) editorSelectedIngredient = 3;
        if (IsKeyPressed(KEY_FIVE)) editorSelectedIngredient = 4;
    }
    // 1 = Cercle (centre & rayon à la souris)
    else if (editorModeSelection == 1) {
        float distToCenter = distance_vec(mousePos, cakeCenter);
        bool onCenter = distToCenter < 20.0f;
        bool onEdge   = fabsf(distToCenter - cakeRadius) < 15.0f;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (onCenter) {
                draggingCircleCenter = true;
                draggingCircleRadius = false;
            } else if (onEdge) {
                draggingCircleRadius = true;
                draggingCircleCenter = false;
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (draggingCircleCenter) {
                cakeCenter = mousePos;
            } else if (draggingCircleRadius) {
                cakeRadius = distToCenter;
                if (cakeRadius < 50.0f) cakeRadius = 50.0f;
                if (cakeRadius > 500.0f) cakeRadius = 500.0f;
            }
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            draggingCircleCenter = false;
            draggingCircleRadius = false;
            saveLevelConfig();
        }

        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
            cakeRadius += 10.0f;
            if (cakeRadius > 500.0f) cakeRadius = 500.0f;
            saveLevelConfig();
        }
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
            cakeRadius -= 10.0f;
            if (cakeRadius < 50.0f) cakeRadius = 50.0f;
            saveLevelConfig();
        }
    }
    // 2 = "Barquette" (position/hauteur/espacement/scale) — visuellement sans fond
    else if (editorModeSelection == 2) {
        int trayY = GetScreenHeight() - (int)trayHeight - (int)trayMarginBottom;
        Rectangle trayRect = (Rectangle){0, (float)trayY,
                                         (float)GetScreenWidth(), trayHeight};

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(mousePos, trayRect)) {
            trayMarginBottom =
                (float)GetScreenHeight() - mousePos.y - trayHeight;
            if (trayMarginBottom < 0.0f) trayMarginBottom = 0.0f;
            if (trayMarginBottom > 400.0f) trayMarginBottom = 400.0f;
            saveLevelConfig();
        }

        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
                trayHeight += 10.0f;
                if (trayHeight > 300.0f) trayHeight = 300.0f;
                saveLevelConfig();
            }
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
                trayHeight -= 10.0f;
                if (trayHeight < 60.0f) trayHeight = 60.0f;
                saveLevelConfig();
            }
        }

        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
                traySpacing += 5.0f;
                if (traySpacing > 200.0f) traySpacing = 200.0f;
                saveLevelConfig();
            }
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
                traySpacing -= 5.0f;
                if (traySpacing < 20.0f) traySpacing = 20.0f;
                saveLevelConfig();
            }
        }

        if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
                trayScale += 0.05f;
                if (trayScale > 2.0f) trayScale = 2.0f;
                saveLevelConfig();
            }
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
                trayScale -= 0.05f;
                if (trayScale < 0.1f) trayScale = 0.1f;
                saveLevelConfig();
            }
        }
    }
    else if (editorModeSelection == 3) {
        // mode optionnel, tu pourras rajouter une logique si besoin
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        saveLevelConfig();
    }
}

/* ---------- mg_update ---------- */

static void mg_update(float dt) {
    handleEditorMode();

    if (gameState == STATE_GAME_COMPLETE || gameState == STATE_EDITOR) return;

    if (gameState == STATE_SHOWING_MODEL) {
        modelTimer += dt;
        if (modelTimer >= MODEL_DISPLAY_TIME) {
            gameState = STATE_PLAYING;
            levelTimer = LEVEL_TIME;
            canPlace = true;

            Level* level = &levels[currentLevel];
            for (int i = 0; i < level->ingredientCount; ++i) {
                level->ingredients[i].isPlaced = false;
                level->ingredients[i].isInTray = true;
            }
        }
        return;
    }

    if (gameState == STATE_PLAYING) {
        levelTimer -= dt;
        if (levelTimer <= 0.0f) {
            levelTimer = 0.0f;
            canPlace = false;
            gameState = STATE_LEVEL_COMPLETE;

            Level* level = &levels[currentLevel];
            float totalPrecision = 0.0f;
            int placedCount = 0;

            for (int i = 0; i < level->ingredientCount; ++i) {
                if (level->ingredients[i].isPlaced) {
                    Vector2 centerPlaced = {
                        level->ingredients[i].position.x +
                            (level->ingredients[i].texture.width / 2.0f),
                        level->ingredients[i].position.y +
                            (level->ingredients[i].texture.height / 2.0f)
                    };
                    float prec = calculatePrecision(level->ingredients[i].targetPosition,
                                                   centerPlaced);
                    totalPrecision += prec;
                    placedCount++;
                }
            }

            if (placedCount > 0) {
                level->score = totalPrecision / (float)placedCount;
            } else {
                level->score = 0.0f;
            }

            level->completed = true;
        }

        // Drag & drop
        Vector2 mousePos = GetMousePosition();
        Level* level = &levels[currentLevel];

        // 🔹 Début du drag : maintenant on peut cliquer sur un ingrédient
        // soit en bas (isInTray), soit déjà posé (isPlaced)
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && canPlace) {
            draggedIngredient = NULL;
            for (int i = 0; i < level->ingredientCount; ++i) {
                Ingredient* ing = &level->ingredients[i];
                if ((ing->isInTray || ing->isPlaced) &&
                    CheckCollisionPointRec(mousePos, ing->rect)) {
                    draggedIngredient = ing;
                    dragOffset = (Vector2){
                        ing->rect.width / 2.0f,
                        ing->rect.height / 2.0f
                    };
                    break;
                }
            }
        }

        // Drag en cours
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggedIngredient != NULL) {
            draggedIngredient->position = (Vector2){
                mousePos.x - dragOffset.x,
                mousePos.y - dragOffset.y
            };
            draggedIngredient->rect.x = draggedIngredient->position.x;
            draggedIngredient->rect.y = draggedIngredient->position.y;
        }

        // Fin du drag
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && draggedIngredient != NULL) {
            Vector2 mouseUp = GetMousePosition();
            if (isPointInCircle(mouseUp, cakeCenter, cakeRadius)) {
                // L'ingrédient reste / devient posé sur le gâteau,
                // et on peut le re-déplacer plus tard
                draggedIngredient->isPlaced = true;
                draggedIngredient->isInTray = false;

                if (draggedIngredient->texture.id != 0) {
                    draggedIngredient->position = (Vector2){
                        mouseUp.x - (float)draggedIngredient->texture.width / 2.0f,
                        mouseUp.y - (float)draggedIngredient->texture.height / 2.0f
                    };
                } else {
                    draggedIngredient->position = (Vector2){
                        mouseUp.x - 30.0f,
                        mouseUp.y - 30.0f
                    };
                }
                draggedIngredient->rect.x = draggedIngredient->position.x;
                draggedIngredient->rect.y = draggedIngredient->position.y;
            } else {
                // Si on relâche en dehors du cercle, on le remet en bas
                draggedIngredient->isPlaced = false;
                draggedIngredient->isInTray = true;
            }
            draggedIngredient = NULL;
        }
    }

    if (gameState == STATE_LEVEL_COMPLETE) {
        if (IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            currentLevel++;
            if (currentLevel >= MAX_LEVELS) {
                float totalScore = 0.0f;
                for (int i = 0; i < MAX_LEVELS; ++i)
                    totalScore += levels[i].score;
                finalScore = totalScore / (float)MAX_LEVELS;
                gameState = STATE_GAME_COMPLETE;
            } else {
                gameState = STATE_SHOWING_MODEL;
                modelTimer = 0.0f;
            }
        }
    }
}

/* ---------- mg_draw ---------- */

static void mg_draw(void) {
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    // Fond
    if (backgroundTexture.id != 0) {
        float bgScaleX = (float)screenWidth / (float)backgroundTexture.width;
        float bgScaleY = (float)screenHeight / (float)backgroundTexture.height;
        float bgScale = (bgScaleX > bgScaleY) ? bgScaleX : bgScaleY;
        float bgW = (float)backgroundTexture.width * bgScale;
        float bgH = (float)backgroundTexture.height * bgScale;
        float bgX = (float)screenWidth / 2.0f - bgW / 2.0f;
        float bgY = (float)screenHeight / 2.0f - bgH / 2.0f;
        DrawTextureEx(backgroundTexture, (Vector2){bgX, bgY}, 0.0f, bgScale, WHITE);
    } else {
        ClearBackground((Color){240, 240, 240, 255});
    }

    /* --- Affichage modèle --- */
    if (gameState == STATE_SHOWING_MODEL) {
        Level* level = &levels[currentLevel];
        if (level->modelTexture.id != 0) {
            float progress = modelTimer / MODEL_DISPLAY_TIME;
            float alpha = 1.0f;
            if (progress < 0.2f) alpha = progress / 0.2f;
            else if (progress > 0.8f) alpha = (1.0f - progress) / 0.2f;

            DrawRectangle(0, 0, screenWidth, screenHeight,
                          (Color){0, 0, 0, (unsigned char)(180 * alpha)});

            float scale = 0.6f;
            float x = (float)screenWidth / 2.0f -
                      (float)level->modelTexture.width * scale / 2.0f;
            float y = (float)screenHeight / 2.0f -
                      (float)level->modelTexture.height * scale / 2.0f;
            Color tint = (Color){255, 255, 255, (unsigned char)(255 * alpha)};
            DrawTextureEx(level->modelTexture, (Vector2){x, y}, 0.0f, scale, tint);
        }

        DrawText("Observez le modele...", screenWidth / 2 - 150, 50, 30, WHITE);
        DrawText(TextFormat("%.1f", MODEL_DISPLAY_TIME - modelTimer),
                 screenWidth / 2 - 20, 100, 40, YELLOW);
        return;
    }

    /* --- Jeu en cours --- */
    if (gameState == STATE_PLAYING) {
        Level* level = &levels[currentLevel];

        // Gâteau de base
        if (level->cakeBaseTexture.id != 0) {
            float cakeScale = 0.6f;
            float x = cakeCenter.x -
                      (float)level->cakeBaseTexture.width * cakeScale / 2.0f;
            float y = cakeCenter.y -
                      (float)level->cakeBaseTexture.height * cakeScale / 2.0f;
            DrawTextureEx(level->cakeBaseTexture, (Vector2){x, y}, 0.0f, cakeScale, WHITE);
        }

        // Ingrédients posés
        for (int i = 0; i < level->ingredientCount; ++i) {
            Ingredient* ing = &level->ingredients[i];
            if (ing->isPlaced && ing != draggedIngredient) {
                if (ing->texture.id != 0) {
                    DrawTexture(ing->texture,
                                (int)ing->position.x,
                                (int)ing->position.y,
                                WHITE);
                } else {
                    DrawRectangle((int)ing->position.x,
                                  (int)ing->position.y, 40, 40, RED);
                    DrawText(ing->name,
                             (int)ing->position.x + 5,
                             (int)ing->position.y + 10,
                             10, WHITE);
                }
            }
        }

        // "Barquette" INVISIBLE : on n'affiche pas de rectangle,
        // seulement les ingrédients en ligne en bas.
        int trayY = screenHeight - (int)trayHeight - (int)trayMarginBottom;
        float trayCenterX = (float)screenWidth / 2.0f;
        float totalWidth = (INGREDIENT_COUNT - 1) * traySpacing;
        float startX = trayCenterX - totalWidth / 2.0f;

        for (int i = 0; i < level->ingredientCount; ++i) {
            Ingredient* ing = &level->ingredients[i];
            if (ing->isPlaced || ing == draggedIngredient) continue;

            float ingredientX = startX + i * traySpacing;
            float ingredientY;

            if (ing->texture.id != 0) {
                ingredientX -= (float)ing->texture.width * trayScale / 2.0f;
                ingredientY = (float)trayY + (trayHeight / 2.0f) -
                              (float)ing->texture.height * trayScale / 2.0f;
            } else {
                ingredientX -= 30.0f;
                ingredientY = (float)trayY + (trayHeight / 2.0f) - 30.0f;
            }

            ing->position = (Vector2){ingredientX, ingredientY};
            ing->isInTray = true;

            if (ing->texture.id != 0) {
                ing->rect = (Rectangle){
                    ingredientX, ingredientY,
                    (float)ing->texture.width * trayScale,
                    (float)ing->texture.height * trayScale
                };
                DrawTextureEx(ing->texture,
                              (Vector2){ingredientX, ingredientY},
                              0.0f, trayScale, WHITE);
            } else {
                ing->rect = (Rectangle){ingredientX, ingredientY, 60, 60};
                DrawRectangle((int)ingredientX, (int)ingredientY, 60, 60, RED);
                DrawText(ing->name,
                         (int)ingredientX + 5,
                         (int)ingredientY + 20,
                         12, WHITE);
            }
        }

        // Ingrédient en train d'être déplacé (dessiné par-dessus tout)
        if (draggedIngredient != NULL) {
            if (draggedIngredient->texture.id != 0) {
                DrawTexture(draggedIngredient->texture,
                            (int)draggedIngredient->position.x,
                            (int)draggedIngredient->position.y,
                            WHITE);
            } else {
                DrawRectangle((int)draggedIngredient->position.x,
                              (int)draggedIngredient->position.y,
                              80, 80,
                              (Color){255, 200, 100, 255});
                DrawRectangleLines((int)draggedIngredient->position.x,
                                   (int)draggedIngredient->position.y,
                                   80, 80, RED);
                int textWidth = MeasureText(draggedIngredient->name, 16);
                int textX = (int)draggedIngredient->position.x +
                            (80 - textWidth) / 2;
                DrawText(draggedIngredient->name,
                         textX,
                         (int)draggedIngredient->position.y + 30,
                         16, BLACK);
            }
        }

        DrawText(TextFormat("Temps restant: %.1f", levelTimer),
                 20, 20, 30, BLACK);
        DrawText(TextFormat("Niveau %d/%d", currentLevel + 1, MAX_LEVELS),
                 20, 60, 24, DARKGRAY);
    }

    /* --- Niveau terminé --- */
    if (gameState == STATE_LEVEL_COMPLETE) {
        Level* level = &levels[currentLevel];
        DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0, 0, 0, 180});
        DrawText(TextFormat("Niveau %d termine!", currentLevel + 1),
                 screenWidth / 2 - 150, screenHeight / 2 - 100, 30, WHITE);
        DrawText(TextFormat("Score: %.1f%%", level->score),
                 screenWidth / 2 - 100, screenHeight / 2 - 50, 40, YELLOW);

        if (currentLevel < MAX_LEVELS - 1) {
            DrawText("Appuyez sur ESPACE pour continuer",
                     screenWidth / 2 - 200, screenHeight / 2 + 50, 20, WHITE);
        } else {
            DrawText("Appuyez sur ESPACE pour voir les resultats",
                     screenWidth / 2 - 250, screenHeight / 2 + 50, 20, WHITE);
        }
    }

    /* --- Fin du jeu --- */
    if (gameState == STATE_GAME_COMPLETE) {
        DrawRectangle(0, 0, screenWidth, screenHeight,
                      (Color){0, 0, 0, 220});

        DrawText("JEU TERMINE!", screenWidth / 2 - 150, 100, 40, WHITE);
        int yPos = 200;
        for (int i = 0; i < MAX_LEVELS; ++i) {
            DrawText(TextFormat("Niveau %d: %.1f%%", i + 1, levels[i].score),
                     screenWidth / 2 - 150, yPos, 24, LIGHTGRAY);
            yPos += 40;
        }

        DrawText(TextFormat("Moyenne finale: %.1f%%", finalScore),
                 screenWidth / 2 - 150, yPos + 40, 36, YELLOW);

        if (finalScore >= PASSING_SCORE) {
            DrawText("JEU VALIDE / REUSSI!",
                     screenWidth / 2 - 200, yPos + 100, 30, GREEN);
        } else {
            DrawText("Jeu non valide. Recommencez depuis le niveau 1.",
                     screenWidth / 2 - 300, yPos + 100, 24, RED);
            DrawText("Appuyez sur R pour recommencer",
                     screenWidth / 2 - 200, yPos + 140, 20, WHITE);
        }

        DrawText("Appuyez sur BACKSPACE pour quitter",
                 screenWidth / 2 - 200, screenHeight - 100, 20, LIGHTGRAY);
    }

    /* --- Mode éditeur --- */
    if (gameState == STATE_EDITOR) {
        DrawRectangle(0, 0, screenWidth, screenHeight,
                      (Color){0, 0, 0, 200});

        DrawText("MODE EDITEUR - Ctrl+F2 pour quitter | Tab pour changer de mode",
                 screenWidth / 2 - 300, 50, 24, YELLOW);
        DrawText(TextFormat("Niveau selectionne: %d (Touches 1, 2, 3)",
                            editorSelectedLevel + 1),
                 screenWidth / 2 - 250, 80, 24, WHITE);

        const char* modeNames[] = {
            "Position cible ingredients (1-5 pour selectionner)",
            "Cercle de placement (clic centre/edge, +/- pour rayon)",
            "Barquette invisible (Shift/Ctrl/Alt + +/- pour regler)",
            "Position depart ingredients (optionnel)"
        };
        DrawText(TextFormat("Mode: %s", modeNames[editorModeSelection]),
                 screenWidth / 2 - 300, 110, 20, (Color){0, 255, 255, 255});

        int yPos = 140;
        DrawText(TextFormat("Cercle: Centre(%.0f, %.0f) Rayon: %.0f",
                            cakeCenter.x, cakeCenter.y, cakeRadius),
                 screenWidth / 2 - 250, yPos, 18, LIGHTGRAY);
        yPos += 25;
        DrawText(TextFormat("Barquette: Hauteur %.0f, Marge bas %.0f, Espacement %.0f, Scale %.2f",
                            trayHeight, trayMarginBottom, traySpacing, trayScale),
                 screenWidth / 2 - 300, yPos, 18, LIGHTGRAY);

        Level* level = &levels[editorSelectedLevel];

        // Gâteau
        if (level->cakeBaseTexture.id != 0) {
            float scale = 0.6f;
            float x = cakeCenter.x -
                      (float)level->cakeBaseTexture.width * scale / 2.0f;
            float y = cakeCenter.y -
                      (float)level->cakeBaseTexture.height * scale / 2.0f;
            DrawTextureEx(level->cakeBaseTexture, (Vector2){x, y}, 0.0f, scale, WHITE);
        }

        // Cercle visible
        DrawCircleLines((int)cakeCenter.x, (int)cakeCenter.y,
                        (int)cakeRadius, (Color){200, 200, 200, 255});
        DrawCircle((int)cakeCenter.x, (int)cakeCenter.y, 5, YELLOW);
        DrawCircle((int)(cakeCenter.x + cakeRadius),
                   (int)cakeCenter.y, 5, GREEN);

        // Positions cibles
        for (int i = 0; i < level->ingredientCount; ++i) {
            Vector2 target = level->ingredients[i].targetPosition;
            Color col = (editorSelectedIngredient == i) ? YELLOW : GREEN;
            DrawCircle((int)target.x, (int)target.y, 10, col);
            DrawCircleLines((int)target.x, (int)target.y, 10, WHITE);
            DrawText(level->ingredients[i].name,
                     (int)target.x + 15, (int)target.y - 10, 16, col);
        }

        DrawText("Ingredients:", 50, 200, 24, WHITE);
        for (int i = 0; i < level->ingredientCount; ++i) {
            Color col = (editorSelectedIngredient == i) ? YELLOW : WHITE;
            DrawText(
                TextFormat("%d. %s (%.0f, %.0f)",
                           i + 1,
                           level->ingredients[i].name,
                           level->ingredients[i].targetPosition.x,
                           level->ingredients[i].targetPosition.y),
                50, 240 + i * 30, 20, col);
        }

        // "Barquette" au milieu en éditeur, mais sans fond (juste les ingrédients)
        {
            int trayY = screenHeight/2 - (int)trayHeight/2;

            float trayCenterX = (float)screenWidth / 2.0f;
            float totalWidth = (INGREDIENT_COUNT - 1) * traySpacing;
            float startX = trayCenterX - totalWidth / 2.0f;

            for (int i = 0; i < level->ingredientCount; ++i) {
                Ingredient* ing = &level->ingredients[i];
                float ingredientX = startX + i * traySpacing;
                float ingredientY;

                if (ing->texture.id != 0) {
                    ingredientX -= (float)ing->texture.width * trayScale / 2.0f;
                    ingredientY = (float)trayY + (trayHeight / 2.0f) -
                                  (float)ing->texture.height * trayScale / 2.0f;
                } else {
                    ingredientX -= 30.0f;
                    ingredientY = (float)trayY + (trayHeight / 2.0f) - 30.0f;
                }

                ing->position = (Vector2){ingredientX, ingredientY};

                if (ing->texture.id != 0) {
                    ing->rect = (Rectangle){
                        ingredientX, ingredientY,
                        (float)ing->texture.width * trayScale,
                        (float)ing->texture.height * trayScale
                    };
                    DrawTextureEx(ing->texture,
                                  (Vector2){ingredientX, ingredientY},
                                  0.0f, trayScale, WHITE);
                } else {
                    ing->rect = (Rectangle){ingredientX, ingredientY, 60, 60};
                    DrawRectangle((int)ingredientX, (int)ingredientY, 60, 60, RED);
                    DrawText(ing->name,
                             (int)ingredientX + 5,
                             (int)ingredientY + 20,
                             12, WHITE);
                }
            }

            DrawText("Ligne d'ingredients (barquette invisible, reglable en mode 2)",
                     40, trayY - 24, 18, WHITE);
        }
    }
}

/* ---------- mg_unload & mg_isCompleted ---------- */

static void mg_unload(void) {
    saveLevelConfig();

    for (int i = 0; i < MAX_LEVELS; ++i) {
        Level* level = &levels[i];
        if (level->modelTexture.id != 0)
            UnloadTexture(level->modelTexture);
        if (level->cakeBaseTexture.id != 0 &&
            level->cakeBaseTexture.id != level->modelTexture.id)
            UnloadTexture(level->cakeBaseTexture);
        for (int j = 0; j < level->ingredientCount; ++j) {
            if (level->ingredients[j].texture.id != 0)
                UnloadTexture(level->ingredients[j].texture);
        }
    }

    if (backgroundTexture.id != 0)
        UnloadTexture(backgroundTexture);
}

static bool mg_isCompleted(int *coinsOut) {
    if (gameState == STATE_GAME_COMPLETE) {
        if (coinsOut)
            *coinsOut = (int)(finalScore / 10.0f);

        if (IsKeyPressed(KEY_R) && finalScore < PASSING_SCORE) {
            currentLevel = 0;
            gameState = STATE_SHOWING_MODEL;
            modelTimer = 0.0f;
            levelTimer = 0.0f;
            draggedIngredient = NULL;
            finalScore = 0.0f;

            for (int i = 0; i < MAX_LEVELS; ++i) {
                levels[i].completed = false;
                levels[i].score = 0.0f;
                for (int j = 0; j < levels[i].ingredientCount; ++j) {
                    levels[i].ingredients[j].isPlaced = false;
                    levels[i].ingredients[j].isInTray = true;
                }
            }
            return false;
        }
        return true;
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
