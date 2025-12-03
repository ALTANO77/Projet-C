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
#define LEVEL_CONFIG_FILE "config/cerise_levels.ini"

// Types d'ingrédients
typedef enum {
    ING_FRAISE = 0,
    ING_BANANE,
    ING_KIWI,
    ING_MANDARINE,
    ING_CHOCOLAT
} IngredientType;

// États de jeu
typedef enum {
    STATE_SHOWING_MODEL,
    STATE_PLAYING,
    STATE_GAME_COMPLETE,
    STATE_EDITOR
} GameState;

// Ingrédient
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

// Niveau
typedef struct {
    Texture2D modelTexture;
    Texture2D cakeBaseTexture;
    Ingredient ingredients[INGREDIENT_COUNT];
    int ingredientCount;
    float score;
    bool completed;
} Level;

// --------- Globals ---------

static GameState gameState = STATE_SHOWING_MODEL;
static int currentLevel = 0;
static float modelTimer = 0.0f;
static float levelTimer = 0.0f;

static Level levels[MAX_LEVELS];
static Texture2D backgroundTexture = {0};

static Vector2 cakeCenter = {0};
static float cakeRadius = CAKE_PLACEMENT_RADIUS;

static Ingredient* draggedIngredient = NULL;
static Vector2 dragOffset = {0};

static float finalScore = 0.0f;
static EndScreenState s_endScreen = {0};

// "barquette" invisible = simple ligne en bas
static float traySpacing = 80.0f;
static float trayScale   = 0.4f;
static float trayHeight  = 120.0f;
static float trayMarginBottom = 20.0f;

// Éditeur simple
static bool editorMode = false;
static int editorSelectedLevel = 0;
static int editorSelectedIngredient = 0;


// --------- Helpers ---------

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

static float dist(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

static bool pointInCircle(Vector2 p, Vector2 center, float radius) {
    return dist(p, center) <= radius;
}

static float calculatePrecision(Vector2 target, Vector2 placedCenter) {
    float d = dist(target, placedCenter);
    float prec = 100.0f - (d / MAX_DISTANCE) * 100.0f;
    if (prec < 0.0f) prec = 0.0f;
    if (prec > 100.0f) prec = 100.0f;
    return prec;
}

// --------- Init niveau ---------

static void initLevel(int levelIndex) {
    Level* level = &levels[levelIndex];

    // Charger modèle
    char modelPath[256];
    snprintf(modelPath, sizeof(modelPath),
             "assets/cerisesurgateau/image de gateau lvl/gateau%d.png",
             levelIndex + 1);
    if (FileExists(modelPath)) {
        Image img = LoadImage(modelPath);
        if (img.data) {
            level->modelTexture = LoadTextureFromImage(img);
            UnloadImage(img);
        }
    }

    // Gâteau de base - ne pas charger (on ne veut pas afficher le gâteau de base)
    level->cakeBaseTexture.id = 0;

    level->ingredientCount = INGREDIENT_COUNT;

    // Positions cibles par défaut (légèrement différentes selon le niveau)
    Vector2 base[INGREDIENT_COUNT] = {
        { -80, -40 }, // fraise
        { -20, -60 }, // banane
        {  40, -40 }, // kiwi
        { -40,  10 }, // mandarine
        {  10,  30 }  // chocolat
    };

    float levelOffsetX = (float)(levelIndex - 1) * 20.0f;
    float levelOffsetY = (float)(levelIndex - 1) * 10.0f;

    for (int i = 0; i < INGREDIENT_COUNT; ++i) {
        Ingredient* ing = &level->ingredients[i];
        ing->type = (IngredientType)i;
        ing->name = getIngredientName(ing->type);
        ing->targetPosition = (Vector2){
            cakeCenter.x + base[i].x + levelOffsetX,
            cakeCenter.y + base[i].y + levelOffsetY
        };
        ing->isPlaced = false;
        ing->isInTray = true;

        // Charger PNG + redimensionner (sinon trop gros)
        const char* filename = getIngredientFilename(ing->type);
        if (FileExists(filename)) {
            Image img = LoadImage(filename);
            if (img.data) {
                int size = 96; // ~100px max
                ImageResize(&img, size, size);
                ing->texture = LoadTextureFromImage(img);
                UnloadImage(img);
            }
        } else {
            ing->texture.id = 0;
        }

        ing->position = (Vector2){0, 0};
        if (ing->texture.id != 0) {
            ing->rect = (Rectangle){0, 0,
                                    (float)ing->texture.width,
                                    (float)ing->texture.height};
        } else {
            ing->rect = (Rectangle){0, 0, 50, 50};
        }
    }

    level->score = 0.0f;
    level->completed = false;
}

// --------- Placement ligne d’ingrédients en bas ---------

static void updateTrayPositionsForLevel(Level* level, int screenWidth, int screenHeight) {
    int trayY = screenHeight - (int)trayHeight - (int)trayMarginBottom;
    float centerX = (float)screenWidth / 2.0f;
    float totalWidth = (INGREDIENT_COUNT - 1) * traySpacing;
    float startX = centerX - totalWidth / 2.0f;

    for (int i = 0; i < level->ingredientCount; ++i) {
        Ingredient* ing = &level->ingredients[i];
        if (ing->isPlaced || ing == draggedIngredient) continue; // ceux posés restent sur le gâteau

        float x = startX + i * traySpacing;
        float y;
        if (ing->texture.id != 0) {
            x -= (float)ing->texture.width * trayScale / 2.0f;
            y = (float)trayY + (trayHeight / 2.0f) - (float)ing->texture.height * trayScale / 2.0f;
        } else {
            x -= 30.0f;
            y = (float)trayY + (trayHeight / 2.0f) - 30.0f;
        }

        ing->position = (Vector2){x, y};
        ing->isInTray = true;

        if (ing->texture.id != 0) {
            ing->rect = (Rectangle){x, y,
                                    ing->texture.width * trayScale,
                                    ing->texture.height * trayScale};
        } else {
            ing->rect = (Rectangle){x, y, 60, 60};
        }
    }
}

// --------- Sauvegarde/Chargement des niveaux ---------

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
            int level, ingredient;
            float x, y;
            if (sscanf(line, "level_%d_ingredient_%d=%f,%f", &level, &ingredient, &x, &y) == 4) {
                if (level >= 1 && level <= MAX_LEVELS && ingredient >= 0 && ingredient < INGREDIENT_COUNT) {
                    levels[level - 1].ingredients[ingredient].targetPosition = (Vector2){x, y};
                }
            }
        }
    }
    fclose(f);
}

// --------- mg_init ---------

static void mg_init(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    cakeCenter = (Vector2){ sw / 2.0f, sh / 2.0f };
    cakeRadius = CAKE_PLACEMENT_RADIUS;

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
        updateTrayPositionsForLevel(&levels[i], sw, sh);
    }

    // Charger la configuration sauvegardée (après initLevel pour écraser les valeurs par défaut)
    loadLevelConfig();

    // Sélectionner un niveau aléatoire
    srand((unsigned int)time(NULL));
    currentLevel = rand() % MAX_LEVELS;
    gameState = STATE_SHOWING_MODEL;
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

// --------- Mode éditeur (simple) ---------

static void handleEditorToggle(void) {
    static bool f2PressedBefore = false;
    bool ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool f2Pressed = IsKeyPressed(KEY_F2);

    if (ctrlDown && f2Pressed && !f2PressedBefore) {
        editorMode = !editorMode;
        if (editorMode) {
            gameState = STATE_EDITOR;
        } else {
            // Sauvegarder les paramètres quand on quitte l'éditeur
            saveLevelConfig();
            gameState = STATE_PLAYING;
        }
    }
    if (!f2Pressed) f2PressedBefore = false;
    if (f2Pressed && !f2PressedBefore) f2PressedBefore = true;
}

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

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        float d = dist(mouse, cakeCenter);
        if (d < 20.0f) {
            draggingCenter = true;
        } else if (fabsf(d - cakeRadius) < 15.0f) {
            draggingRadius = true;
        }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        if (draggingCenter) {
            cakeCenter = mouse;
            // on déplace aussi les cibles pour rester cohérent ??
            // non : on laisse l'utilisateur recaler à la main s'il veut
        } else if (draggingRadius) {
            float d = dist(mouse, cakeCenter);
            cakeRadius = d;
            if (cakeRadius < 50.0f) cakeRadius = 50.0f;
            if (cakeRadius > 500.0f) cakeRadius = 500.0f;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
        draggingCenter = false;
        draggingRadius = false;
    }

    // déplacer la cible de l'ingrédient sélectionné : clic gauche dans le cercle
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (editorSelectedIngredient >= 0 &&
            editorSelectedIngredient < level->ingredientCount &&
            pointInCircle(mouse, cakeCenter, cakeRadius)) {
            level->ingredients[editorSelectedIngredient].targetPosition = mouse;
        }
    }
}

// --------- mg_update ---------

static void mg_update(float dt) {
    handleEditorToggle();

    if (gameState == STATE_EDITOR) {
        updateEditor();
        return;
    }

    if (gameState == STATE_GAME_COMPLETE) {
        bool won = finalScore >= PASSING_SCORE;
        UpdateEndScreen(&s_endScreen, won, !won);
        
        if (s_endScreen.wantsToReplay) {
            // Réinitialiser avec un niveau aléatoire différent
            int newLevel;
            do {
                newLevel = rand() % MAX_LEVELS;
            } while (newLevel == currentLevel && MAX_LEVELS > 1);
            currentLevel = newLevel;
            
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            updateTrayPositionsForLevel(&levels[currentLevel], sw, sh);
            gameState = STATE_SHOWING_MODEL;
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

    if (gameState == STATE_SHOWING_MODEL) {
        modelTimer += dt;
        if (modelTimer >= MODEL_DISPLAY_TIME) {
            gameState = STATE_PLAYING;
            levelTimer = LEVEL_TIME;

            // reset ingrédients du niveau
            for (int i = 0; i < level->ingredientCount; ++i) {
                level->ingredients[i].isPlaced = false;
                level->ingredients[i].isInTray = true;
            }
            updateTrayPositionsForLevel(level, sw, sh);
        }
        return;
    }

    if (gameState == STATE_PLAYING) {
        levelTimer -= dt;
        if (levelTimer < 0.0f) levelTimer = 0.0f;

        // placer les ingrédients en bas (ligne) pour ceux non posés
        updateTrayPositionsForLevel(level, sw, sh);

        // Gestion du drag & drop
        Vector2 mouse = GetMousePosition();

        // début du drag : on peut attraper un ingrédient en bas OU déjà posé
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            draggedIngredient = NULL;
            for (int i = 0; i < level->ingredientCount; ++i) {
                Ingredient* ing = &level->ingredients[i];
                if ((ing->isInTray || ing->isPlaced) &&
                    CheckCollisionPointRec(mouse, ing->rect)) {
                    draggedIngredient = ing;
                    dragOffset = (Vector2){
                        ing->rect.width  / 2.0f,
                        ing->rect.height / 2.0f
                    };
                    break;
                }
            }
        }

        // drag en cours
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggedIngredient) {
            draggedIngredient->position = (Vector2){
                mouse.x - dragOffset.x,
                mouse.y - dragOffset.y
            };
            draggedIngredient->rect.x = draggedIngredient->position.x;
            draggedIngredient->rect.y = draggedIngredient->position.y;
        }

        // fin de drag
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && draggedIngredient) {
            Vector2 mouseUp = GetMousePosition();
            if (pointInCircle(mouseUp, cakeCenter, cakeRadius)) {
                // pose sur le gâteau
                draggedIngredient->isPlaced = true;
                draggedIngredient->isInTray = false;

                if (draggedIngredient->texture.id != 0) {
                    draggedIngredient->position.x =
                        mouseUp.x - draggedIngredient->texture.width / 2.0f;
                    draggedIngredient->position.y =
                        mouseUp.y - draggedIngredient->texture.height / 2.0f;
                } else {
                    draggedIngredient->position.x = mouseUp.x - 30.0f;
                    draggedIngredient->position.y = mouseUp.y - 30.0f;
                }
                draggedIngredient->rect.x = draggedIngredient->position.x;
                draggedIngredient->rect.y = draggedIngredient->position.y;
            } else {
                // remis en bas
                draggedIngredient->isPlaced = false;
                draggedIngredient->isInTray = true;
            }

            draggedIngredient = NULL;
        }

        // Bouton "Terminer" à droite
        int buttonX = sw - 180;
        int buttonY = sh / 2 - 40;
        int buttonWidth = 150;
        int buttonHeight = 80;
        Rectangle finishButton = {buttonX, buttonY, buttonWidth, buttonHeight};
        
        if (CheckCollisionPointRec(mouse, finishButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            // Calculer le score immédiatement
            // Vérifier que tous les ingrédients sont placés
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
                // Tous les ingrédients sont placés, calculer la précision
                float total = 0.0f;
                for (int i = 0; i < level->ingredientCount; ++i) {
                    Ingredient* ing = &level->ingredients[i];
                    Vector2 centerPlaced = {
                        ing->position.x + (ing->texture.width  / 2.0f),
                        ing->position.y + (ing->texture.height / 2.0f)
                    };
                    total += calculatePrecision(ing->targetPosition, centerPlaced);
                }
                level->score = total / level->ingredientCount;
            }

            level->completed = true;
            finalScore = level->score;
            gameState = STATE_GAME_COMPLETE;
            s_endScreen.wantsToExit = false;
            s_endScreen.wantsToReplay = false;
        }

        // fin du temps -> calcul score
        if (levelTimer <= 0.0f) {
            // Vérifier que tous les ingrédients sont placés
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
                // Tous les ingrédients sont placés, calculer la précision
                float total = 0.0f;
                for (int i = 0; i < level->ingredientCount; ++i) {
                    Ingredient* ing = &level->ingredients[i];
                    Vector2 centerPlaced = {
                        ing->position.x + (ing->texture.width  / 2.0f),
                        ing->position.y + (ing->texture.height / 2.0f)
                    };
                    total += calculatePrecision(ing->targetPosition, centerPlaced);
                }
                level->score = total / level->ingredientCount;
            }

            level->completed = true;
            // Le jeu se termine directement après un niveau
            finalScore = level->score;
            gameState = STATE_GAME_COMPLETE;
            s_endScreen.wantsToExit = false;
            s_endScreen.wantsToReplay = false;
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

    // Affichage du modèle
    if (gameState == STATE_SHOWING_MODEL) {
        if (level->modelTexture.id != 0) {
            float progress = modelTimer / MODEL_DISPLAY_TIME;
            float alpha = 1.0f;
            if (progress < 0.2f) alpha = progress / 0.2f;
            else if (progress > 0.8f) alpha = (1.0f - progress) / 0.2f;
            
            // S'assurer que l'alpha est à 0 à la fin du timer
            if (progress >= 1.0f) alpha = 0.0f;

            if (alpha > 0.0f) {
                DrawRectangle(0, 0, sw, sh, (Color){0,0,0,(unsigned char)(180*alpha)});

                float scale = 0.6f;
                float x = sw / 2.0f - level->modelTexture.width * scale / 2.0f;
                float y = sh / 2.0f - level->modelTexture.height * scale / 2.0f;
                DrawTextureEx(level->modelTexture, (Vector2){x,y}, 0, scale,
                              (Color){255,255,255,(unsigned char)(255*alpha)});
            }
        }

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
