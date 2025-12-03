// Jeu "Cerise sur Gateau" - version simplifiée
// - 3 niveaux
// - modèle affiché 5 s
// - 30 s de jeu par niveau
// - drag & drop d'ingrédients depuis le bas
// - possibilité de re-bouger un ingrédient déjà posé
// - barquette invisible (juste une ligne d'ingrédients en bas)
// - mode éditeur simple (Ctrl+F2) pour déplacer le cercle et les positions cibles

#include "cerisesurgateau.h"
#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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
    STATE_LEVEL_COMPLETE,
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

// "barquette" invisible = simple ligne en bas
static float traySpacing = 80.0f;
static float trayScale   = 0.4f;
static float trayHeight  = 120.0f;
static float trayMarginBottom = 20.0f;

// Éditeur simple
static bool editorMode = false;
static int editorSelectedLevel = 0;
static int editorSelectedIngredient = 0;

// Flag pour savoir si l'utilisateur a cliqué sur le bouton de retour
static bool returnToSalonClicked = false;

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

    currentLevel = 0;
    gameState = STATE_SHOWING_MODEL;
    modelTimer = 0.0f;
    levelTimer = 0.0f;
    draggedIngredient = NULL;
    finalScore = 0.0f;
    editorMode = false;
    editorSelectedLevel = 0;
    editorSelectedIngredient = 0;
    returnToSalonClicked = false;
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
        // Gestion du clic sur le bouton "Retourner dans le salon"
        Vector2 mouse = GetMousePosition();
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        
        // Position du bouton
        int buttonX = sw / 2 - 150;
        int buttonY = sh - 120;
        int buttonWidth = 300;
        int buttonHeight = 50;
        Rectangle buttonRect = {buttonX, buttonY, buttonWidth, buttonHeight};
        
        // Vérifier le clic sur le bouton
        if (CheckCollisionPointRec(mouse, buttonRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            returnToSalonClicked = true;
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

        // fin du temps -> calcul score
        if (levelTimer <= 0.0f) {
            float total = 0.0f;
            int placed = 0;
            for (int i = 0; i < level->ingredientCount; ++i) {
                Ingredient* ing = &level->ingredients[i];
                if (ing->isPlaced) {
                    Vector2 centerPlaced = {
                        ing->position.x + (ing->texture.width  / 2.0f),
                        ing->position.y + (ing->texture.height / 2.0f)
                    };
                    total += calculatePrecision(ing->targetPosition, centerPlaced);
                    placed++;
                }
            }
            if (placed > 0) level->score = total / placed;
            else level->score = 0.0f;

            level->completed = true;
            gameState = STATE_LEVEL_COMPLETE;
        }
    }

    if (gameState == STATE_LEVEL_COMPLETE) {
        if (IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            currentLevel++;
            if (currentLevel >= MAX_LEVELS) {
                // calcul moyenne finale
                float sum = 0.0f;
                for (int i = 0; i < MAX_LEVELS; ++i) sum += levels[i].score;
                finalScore = sum / MAX_LEVELS;
                gameState = STATE_GAME_COMPLETE;
                returnToSalonClicked = false;
            } else {
                gameState = STATE_SHOWING_MODEL;
                modelTimer = 0.0f;
            }
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
        DrawText(TextFormat("Niveau %d/%d", currentLevel+1, MAX_LEVELS),
                 20, 60, 24, DARKGRAY);
    }

    // Écran niveau terminé
    if (gameState == STATE_LEVEL_COMPLETE) {
        DrawRectangle(0,0,sw,sh,(Color){0,0,0,180});
        DrawText(TextFormat("Niveau %d termine!", currentLevel+1),
                 sw/2 - 150, sh/2 - 100, 30, WHITE);
        DrawText(TextFormat("Score: %.1f%%", level->score),
                 sw/2 - 100, sh/2 - 50, 40, YELLOW);
        if (currentLevel < MAX_LEVELS-1) {
            DrawText("Appuyez sur ESPACE pour continuer",
                     sw/2 - 200, sh/2 + 50, 20, WHITE);
        } else {
            DrawText("Appuyez sur ESPACE pour voir les resultats",
                     sw/2 - 250, sh/2 + 50, 20, WHITE);
        }
    }

    // Écran final
    if (gameState == STATE_GAME_COMPLETE) {
        DrawRectangle(0,0,sw,sh,(Color){0,0,0,220});
        DrawText("JEU TERMINE!", sw/2 - 150, 80, 40, WHITE);

        int y = 160;
        for (int i = 0; i < MAX_LEVELS; ++i) {
            DrawText(TextFormat("Niveau %d: %.1f%%", i+1, levels[i].score),
                     sw/2 - 150, y, 24, LIGHTGRAY);
            y += 40;
        }

        DrawText(TextFormat("Moyenne finale: %.1f%%", finalScore),
                 sw/2 - 150, y+40, 36, YELLOW);

        // Calcul des pièces gagnées
        int coinsWon = 0;
        if (finalScore >= 100.0f) coinsWon = 5;
        else if (finalScore >= 90.0f) coinsWon = 4;
        else if (finalScore >= 80.0f) coinsWon = 3;
        else if (finalScore >= 70.0f) coinsWon = 2;
        else if (finalScore >= 60.0f) coinsWon = 1;

        if (finalScore >= PASSING_SCORE) {
            DrawText("JEU VALIDE / REUSSI!", sw/2 - 200, y+100, 30, GREEN);
            if (coinsWon > 0) {
                DrawText(TextFormat("Pieces gagnees: %d", coinsWon),
                         sw/2 - 120, y+140, 24, YELLOW);
            }
        } else {
            DrawText("Jeu non valide. Recommencez depuis le niveau 1.",
                     sw/2 - 300, y+100, 24, RED);
            DrawText("Appuyez sur R pour recommencer",
                     sw/2 - 200, y+140, 20, WHITE);
        }

        // Bouton "Retourner dans le salon"
        int buttonX = sw / 2 - 150;
        int buttonY = sh - 120;
        int buttonWidth = 300;
        int buttonHeight = 50;
        Vector2 mouse = GetMousePosition();
        Rectangle buttonRect = {buttonX, buttonY, buttonWidth, buttonHeight};
        
        Color buttonColor = CheckCollisionPointRec(mouse, buttonRect) ? 
                           (Color){100, 150, 200, 255} : (Color){80, 120, 160, 255};
        
        DrawRectangleRounded(buttonRect, 0.3f, 8, buttonColor);
        DrawRectangleRoundedLines(buttonRect, 0.3f, 8, WHITE);
        
        int textWidth = MeasureText("Retourner dans le salon", 24);
        DrawText("Retourner dans le salon", 
                 buttonX + (buttonWidth - textWidth) / 2,
                 buttonY + (buttonHeight - 24) / 2,
                 24, WHITE);
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
        // Calcul des pièces selon le système : 60%=1, 70%=2, 80%=3, 90%=4, 100%=5
        int coinsWon = 0;
        if (finalScore >= 100.0f) coinsWon = 5;
        else if (finalScore >= 90.0f) coinsWon = 4;
        else if (finalScore >= 80.0f) coinsWon = 3;
        else if (finalScore >= 70.0f) coinsWon = 2;
        else if (finalScore >= 60.0f) coinsWon = 1;

        if (coinsOut) *coinsOut = coinsWon;

        // R pour recommencer si raté
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
            }
            returnToSalonClicked = false;
            return false;
        }

        // On ne signale la complétion au hub que si l'utilisateur a cliqué sur le bouton
        if (returnToSalonClicked) {
            return true;
        }
        return false;
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
