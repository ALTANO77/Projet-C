// Gros Nounours 2D — squelette 2D (hub, déplacements, saut, interaction)
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "minigames/minigame.h"
#include "minigames/pousse_pousse/pousse_pousse.h"
#include "minigames/traffic/traffic.h"
#include "minigames/Cerisesurgateau/cerisesurgateau.h"
#include "minigames/pendu/pendu.h"
#include "minigames/boutique/boutique.h"

typedef enum {
    STATE_TITLE = 0,
    STATE_HUB,
    STATE_ZONE_JARDIN,
    STATE_ZONE_CHAMBRE,
    STATE_ZONE_GRENIER,
    STATE_ZONE_CUISINE,
    STATE_SHOP,
    STATE_MINIJEU,
    STATE_PAUSE
} GameState;

typedef enum {
    ZONE_NONE = -1,
    ZONE_JARDIN = 0,
    ZONE_CHAMBRE,
    ZONE_GRENIER,
    ZONE_CUISINE,
    ZONE_COUNT
} ZoneId;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    bool onGround;
} Player;

typedef struct {
    bool completed;
} ZoneProgress;

typedef struct {
    float left;
    float top;
    float width;
    float height;
} RectRatios;

typedef struct {
    float left;
    float top;
    float heightRatio;
} BearLayout;

typedef struct {
    GameState state;
    Player player;
    bool loggingEnabled;
    int collectibles;
    MinigameAPI currentMinigame;
    const char *currentMinigameName;
    bool minigameCompleted;
    bool showCompletionPopup;
    float completionPopupTimer;
    float completionPopupDuration;
    int pendingCoinsReward;
    char completionPopupText[128];
    int activeZone;
    ZoneProgress progress[ZONE_COUNT];
    Texture2D menuBackground;
    Texture2D menuBear;
    Texture2D titleBackground;
    Texture2D titleBackgroundHover;
    bool hasMenuBackground;
    bool hasMenuBear;
    bool hasTitleBackground;
    bool hasTitleBackgroundHover;
    Texture2D bearOutfits[5];  // Images nounours avec différentes tenues
    bool hasBearOutfit[5];
    int currentBearOutfit;  // Index de la tenue actuellement portée (-1 = départ)
    Texture2D zoneBackgrounds[ZONE_COUNT];
    bool hasZoneBackground[ZONE_COUNT];
    bool showDebugOverlay;
    int draggingPortal;
    bool draggingBear;
    Vector2 dragOffset;
    RectRatios portalLayouts[ZONE_COUNT];
    BearLayout bearLayout;
    int hoveredPortal;
    Music music;
    bool hasMusic;
    bool musicMuted;
    Texture2D soundOnIcon;
    Texture2D soundOffIcon;
    bool hasSoundOnIcon;
    bool hasSoundOffIcon;
    RectRatios shopPortalLayout;
    bool draggingShopPortal;
    RectRatios titleRectLayout;  // Rectangle unique pour l'écran d'accueil
    bool draggingTitleRect;     // Flag pour déplacer le rectangle de l'écran d'accueil
    Texture2D outfitPreview;
    bool hasOutfitPreview;
    Texture2D outfits[5];
    bool hasOutfit[5];
    const char *outfitNames[5];
    int selectedOutfit;
    int outfitPrices[5];
    bool outfitOwned[5];
    BoutiqueData boutiqueData;
    float inactivityTimer;  // Timer pour l'inactivité sur l'écran d'accueil
    Vector2 lastMousePos;   // Dernière position de la souris pour détecter l'inactivité
} Game;

typedef struct {
    ZoneId zone;
    const char *label;
} HubPortalInfo;

static const HubPortalInfo HUB_PORTALS[ZONE_COUNT] = {
    { ZONE_JARDIN,  "Jardin" },
    { ZONE_CHAMBRE, "Puzzle" },
    { ZONE_GRENIER, "Bibliothèque" },
    { ZONE_CUISINE, "Cuisine" }
};

static const RectRatios DEFAULT_PORTAL_LAYOUTS[ZONE_COUNT] = {
    { 0.065f, 0.25f, 0.11f, 0.30f },
    { 0.225f, 0.25f, 0.11f, 0.30f },
    { 0.395f, 0.25f, 0.11f, 0.30f },
    { 0.565f, 0.25f, 0.11f, 0.30f }
};
static const RectRatios DEFAULT_SHOP_PORTAL = { 0.78f, 0.20f, 0.12f, 0.32f };
static const RectRatios DEFAULT_TITLE_RECT = { 0.396f, 0.315f, 0.208f, 0.370f };  // Rectangle 400x400 pixels centré (sur écran 1920x1080)

static const BearLayout DEFAULT_BEAR_LAYOUT = { 0.021f, 0.113f, 0.85f };
static const char *PORTAL_KEYS[ZONE_COUNT] = { "jardin", "chambre", "grenier", "cuisine" };
static const char *LAYOUT_FILE = "config/menu_layout.ini";
static const char *ZONE_BG_FILES[ZONE_COUNT] = {
    "assets/bg_jardin.png",
    "assets/bg_puzzle.png",
    "assets/bg_bibliotheque.png",
    "assets/bg_cuisine.png"
};
static const float MINIGAME_POPUP_DURATION = 3.0f;

static void prepareMinigameSession(Game *g, MinigameAPI api, const char *prettyName);
static void finalizeMinigame(Game *g);

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static Rectangle computeBearRect(const Game *g);
static Rectangle computePortalRect(const Game *g, int idx);
static void clampBearToScreen(Game *g);

static void drawCentered(const char *txt, int y, int size, Color c) {
    int w = MeasureText(txt, size);
    DrawText(txt, (GetScreenWidth() - w)/2, y, size, c);
}

static void resetPlayer(Player *p) {
    p->position = (Vector2){ 160, 420 };
    p->velocity = (Vector2){ 0, 0 };
    p->onGround = false;
}

static void drawGround(void) {
    DrawRectangle(0, 480, GetScreenWidth(), GetScreenHeight() - 480, (Color){ 60, 100, 60, 255 });
    DrawRectangle(0, 460, GetScreenWidth(), 20, (Color){ 90, 60, 40, 255 });
}

static Texture2D loadTextureIfAvailable(const char *path) {
    Texture2D tex = {0};
    if (!FileExists(path)) return tex;
    Image img = LoadImage(path);
    if (img.data) {
        tex = LoadTextureFromImage(img);
        UnloadImage(img);
    }
    return tex;
}

static void prepareMinigameSession(Game *g, MinigameAPI api, const char *prettyName) {
    g->currentMinigame = api;
    g->currentMinigameName = prettyName;
    g->minigameCompleted = false;
    g->showCompletionPopup = false;
    g->pendingCoinsReward = 0;
    g->completionPopupTimer = 0.0f;
    g->completionPopupDuration = MINIGAME_POPUP_DURATION;
    g->completionPopupText[0] = '\0';
    if (g->currentMinigame.init) g->currentMinigame.init();
    g->state = STATE_MINIJEU;
}

// Fonction supprimée - les minijeux gèrent maintenant leur propre écran de fin

static void finalizeMinigame(Game *g) {
    g->collectibles += g->pendingCoinsReward;
    if (g->activeZone >= 0 && g->activeZone < ZONE_COUNT) {
        g->progress[g->activeZone].completed = true;
    }
    if (g->currentMinigame.unload) g->currentMinigame.unload();
    g->currentMinigame = (MinigameAPI){0};
    g->currentMinigameName = NULL;
    g->pendingCoinsReward = 0;
    g->minigameCompleted = false;
    g->showCompletionPopup = false;
    g->completionPopupTimer = 0.0f;
    g->completionPopupDuration = MINIGAME_POPUP_DURATION;
    g->completionPopupText[0] = '\0';
    g->state = STATE_HUB;
    g->activeZone = ZONE_NONE;
}

static void initDefaultLayout(Game *g) {
    for (int i = 0; i < ZONE_COUNT; ++i) g->portalLayouts[i] = DEFAULT_PORTAL_LAYOUTS[i];
    g->bearLayout = DEFAULT_BEAR_LAYOUT;
    g->shopPortalLayout = DEFAULT_SHOP_PORTAL;
    g->titleRectLayout = DEFAULT_TITLE_RECT;
    g->draggingTitleRect = false;
}

static int portalIndexFromName(const char *name) {
    for (int i = 0; i < ZONE_COUNT; ++i) {
        if (strcmp(name, PORTAL_KEYS[i]) == 0) return i;
    }
    return -1;
}

static void clampPortalLayout(RectRatios *r) {
    r->width = clampf(r->width, 0.02f, 1.0f);
    r->height = clampf(r->height, 0.02f, 1.0f);
    r->left = clampf(r->left, 0.0f, 1.0f - r->width);
    r->top = clampf(r->top, 0.0f, 1.0f - r->height);
}

static void clampBearLayout(BearLayout *b) {
    b->heightRatio = clampf(b->heightRatio, 0.1f, 1.0f);
    b->left = clampf(b->left, 0.0f, 1.0f);
    b->top = clampf(b->top, 0.0f, 1.0f - b->heightRatio);
}

static void loadMenuLayout(Game *g) {
    initDefaultLayout(g);
    FILE *f = fopen(LAYOUT_FILE, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char key[64];
        float left, top, width, height;
        if (sscanf(line, "portal_%63[^=]=%f,%f,%f,%f", key, &left, &top, &width, &height) == 5) {
            int idx = portalIndexFromName(key);
            if (idx >= 0) {
                g->portalLayouts[idx] = (RectRatios){ left, top, width, height };
                clampPortalLayout(&g->portalLayouts[idx]);
            } else if (strcmp(key, "shop") == 0) {
                g->shopPortalLayout = (RectRatios){ left, top, width, height };
                clampPortalLayout(&g->shopPortalLayout);
            }
        } else if (sscanf(line, "bear=%f,%f,%f", &left, &top, &height) == 3) {
            g->bearLayout = (BearLayout){ left, top, height };
            clampBearLayout(&g->bearLayout);
        }
    }
    fclose(f);
    clampBearToScreen(g);
}

static void saveMenuLayout(const Game *g) {
    FILE *f = fopen(LAYOUT_FILE, "w");
    if (!f) return;
    for (int i = 0; i < ZONE_COUNT; ++i) {
        const RectRatios *r = &g->portalLayouts[i];
        fprintf(f, "portal_%s=%.5f,%.5f,%.5f,%.5f\n", PORTAL_KEYS[i], r->left, r->top, r->width, r->height);
    }
    fprintf(f, "portal_shop=%.5f,%.5f,%.5f,%.5f\n", g->shopPortalLayout.left, g->shopPortalLayout.top, g->shopPortalLayout.width, g->shopPortalLayout.height);
    fprintf(f, "bear=%.5f,%.5f,%.5f\n", g->bearLayout.left, g->bearLayout.top, g->bearLayout.heightRatio);
    fclose(f);
}

static void drawMenuBackground(const Game *g) {
    const Texture2D *tex = &g->menuBackground;
    if (g->hoveredPortal >= 0 && g->hoveredPortal < ZONE_COUNT && g->hasZoneBackground[g->hoveredPortal]) {
        tex = &g->zoneBackgrounds[g->hoveredPortal];
    }

    if (tex->id != 0) {
        Rectangle src = { 0, 0, (float)tex->width, (float)tex->height };
        Rectangle dst = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
        DrawTexturePro(*tex, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        drawGround();
    }
}

static void drawBearCloseup(const Game *g) {
    Texture2D bearToDraw = g->menuBear;
    bool hasBear = g->hasMenuBear;
    
    // Si une tenue est sélectionnée et disponible, utiliser cette image
    if (g->currentBearOutfit >= 0 && g->currentBearOutfit < 5 && g->hasBearOutfit[g->currentBearOutfit]) {
        bearToDraw = g->bearOutfits[g->currentBearOutfit];
        hasBear = true;
    }
    
    if (!hasBear) return;
    Rectangle dst = computeBearRect(g);
    if (dst.width <= 0 || dst.height <= 0) return;
    Rectangle src = { 0, 0, (float)bearToDraw.width, (float)bearToDraw.height };
    DrawTexturePro(bearToDraw, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
}

static void drawMinigameStatusTable(const Game *g) {
    static const char *zoneLabels[ZONE_COUNT] = { "Jardin", "Puzzle", "Bibliothèque", "Cuisine" };
    const float tableX = 60.0f;
    const float tableY = 780.0f;
    const float rowHeight = 42.0f;
    const float tableWidth = 460.0f;
    const float tableHeight = rowHeight * (ZONE_COUNT + 1);

    DrawRectangleRounded((Rectangle){ tableX - 10, tableY - 20, tableWidth + 20, tableHeight + 30 }, 0.08f, 6, (Color){ 0, 0, 0, 160 });
    DrawText("Etat des mini-jeux", (int)tableX, (int)tableY - 10, 26, RAYWHITE);

    DrawLine(tableX, tableY + 18, tableX + tableWidth, tableY + 18, LIGHTGRAY);
    DrawText("Statut", (int)(tableX + tableWidth - 150), (int)tableY + 28, 22, LIGHTGRAY);

    for (int i = 0; i < ZONE_COUNT; ++i) {
        float rowY = tableY + 28 + rowHeight * (i + 1);
        const char *status = g->progress[i].completed ? "Terminée" : "Non fait";
        DrawText(zoneLabels[i], (int)tableX, (int)rowY, 22, RAYWHITE);
        DrawText(status, (int)(tableX + tableWidth - 150), (int)rowY, 22, g->progress[i].completed ? (Color){ 120, 230, 140, 255 } : (Color){ 255, 210, 120, 255 });
    }
}

static void drawCoinCounter(const Game *g) {
    const char *label = TextFormat("Pieces : %d", g->collectibles);
    int fontSize = 30;
    int textWidth = MeasureText(label, fontSize);
    int padding = 18;
    Rectangle box = {
        GetScreenWidth() - textWidth - padding * 2 - 40,
        30,
        (float)textWidth + padding * 2,
        50
    };
    DrawRectangleRounded(box, 0.12f, 6, (Color){ 0, 0, 0, 160 });
    DrawText(label, (int)(box.x + padding), (int)(box.y + 12), fontSize, GOLD);
}

// Fonction supprimée - les minijeux gèrent maintenant leur propre écran de fin

static Rectangle getMusicButtonRect(void) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    // Taille et marge proportionnelles à la taille de la fenêtre
    float base = fminf(sw, sh);
    float size = base * 0.05f;      // ~5% du plus petit côté
    if (size < 32.0f) size = 32.0f; // limite minimale
    if (size > 96.0f) size = 96.0f; // limite maximale
    float margin = size * 0.5f;

    return (Rectangle){
        sw - size - margin,
        sh - size - margin,
        size,
        size
    };
}

static void drawMusicButton(const Game *g) {
    if (!g->hasMusic) return;
    Rectangle r = getMusicButtonRect();
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, r);

    // Fond clair plutôt que noir
    Color bg = (Color){ 245, 245, 245, hover ? 255 : 230 };
    Color border = (Color){ 60, 60, 60, 220 };

    DrawRectangleRounded(r, 0.4f, 6, bg);
    DrawRectangleRoundedLines(r, 0.4f, 6, border);

    // Si des icônes personnalisées sont présentes, on les utilise
    const Texture2D *iconTex = NULL;
    if (!g->musicMuted && g->hasSoundOnIcon) iconTex = &g->soundOnIcon;
    if (g->musicMuted && g->hasSoundOffIcon) iconTex = &g->soundOffIcon;

    if (iconTex && iconTex->id != 0) {
        Rectangle src = { 0, 0, (float)iconTex->width, (float)iconTex->height };
        // On laisse une petite marge à l’intérieur du bouton
        float pad = r.width * 0.15f;
        Rectangle dst = {
            r.x + pad,
            r.y + pad,
            r.width - 2.0f * pad,
            r.height - 2.0f * pad
        };
        DrawTexturePro(*iconTex, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        // Fallback : petit pictogramme simple si pas d’icône personnalisée
        Color iconColor = g->musicMuted ? (Color){ 220, 80, 80, 255 } : (Color){ 120, 220, 140, 255 };
        float cx = r.x + r.width * 0.5f;
        float cy = r.y + r.height * 0.5f;
        float w = r.width * 0.35f;
        float h = r.height * 0.30f;

        DrawTriangle(
            (Vector2){ cx - w * 0.6f, cy - h * 0.6f },
            (Vector2){ cx - w * 0.6f, cy + h * 0.6f },
            (Vector2){ cx,            cy          },
            iconColor
        );
        DrawRectangleV(
            (Vector2){ cx - w, cy - h * 0.4f },
            (Vector2){ w * 0.4f, h * 0.8f },
            iconColor
        );
        if (g->musicMuted) {
            DrawLineEx(
                (Vector2){ cx + w * 0.2f, cy - h * 0.7f },
                (Vector2){ cx + w * 0.8f, cy + h * 0.7f },
                3.0f,
                iconColor
            );
            DrawLineEx(
                (Vector2){ cx + w * 0.8f, cy - h * 0.7f },
                (Vector2){ cx + w * 0.2f, cy + h * 0.7f },
                3.0f,
                iconColor
            );
        } else {
            DrawCircleLines((int)(cx + w * 0.6f), (int)cy, (int)(h * 0.9f), iconColor);
        }
    }
}

static Rectangle computePortalRect(const Game *g, int idx) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    RectRatios layout = g->portalLayouts[idx];
    return (Rectangle){
        layout.left * sw,
        layout.top * sh,
        layout.width * sw,
        layout.height * sh
    };
}

static Rectangle computeShopPortalRect(const Game *g) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    RectRatios layout = g->shopPortalLayout;
    return (Rectangle){
        layout.left * sw,
        layout.top * sh,
        layout.width * sw,
        layout.height * sh
    };
}

static Rectangle computeTitleRect(const Game *g) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    RectRatios layout = g->titleRectLayout;
    return (Rectangle){
        layout.left * sw,
        layout.top * sh,
        layout.width * sw,
        layout.height * sh
    };
}

static Rectangle computeBearRect(const Game *g) {
    Texture2D bearTex = g->menuBear;
    bool hasBear = g->hasMenuBear;
    
    // Utiliser la texture de la tenue sélectionnée si disponible
    if (g->currentBearOutfit >= 0 && g->currentBearOutfit < 5 && g->hasBearOutfit[g->currentBearOutfit]) {
        bearTex = g->bearOutfits[g->currentBearOutfit];
        hasBear = true;
    }
    
    if (!hasBear) return (Rectangle){ 0 };
    float sh = (float)GetScreenHeight();
    float sw = (float)GetScreenWidth();
    float bearHeight = g->bearLayout.heightRatio * sh;
    float aspect = bearTex.height > 0 ? (float)bearTex.width / (float)bearTex.height : 1.0f;
    float bearWidth = bearHeight * aspect;
    return (Rectangle){
        g->bearLayout.left * sw,
        g->bearLayout.top * sh,
        bearWidth,
        bearHeight
    };
}

static float getBearWidthRatio(const Game *g) {
    if (!g->hasMenuBear || g->menuBear.height == 0 || g->menuBear.width == 0) return 0.2f;
    float aspect = (float)g->menuBear.width / (float)g->menuBear.height;
    float screenRatio = (float)GetScreenHeight() / (float)GetScreenWidth();
    return g->bearLayout.heightRatio * aspect * screenRatio;
}

static void clampBearToScreen(Game *g) {
    float widthRatio = clampf(getBearWidthRatio(g), 0.0f, 0.99f);
    g->bearLayout.left = clampf(g->bearLayout.left, 0.0f, 1.0f - widthRatio);
    g->bearLayout.top = clampf(g->bearLayout.top, 0.0f, 1.0f - g->bearLayout.heightRatio);
}

static void drawPortalHighlights(const Game *g) {
    // Plus rien n'est affiché au survol des portails
    (void)g; // Éviter l'avertissement de variable non utilisée
}

static void handleDebugDragging(Game *g) {
    if (!g->showDebugOverlay) {
        g->draggingPortal = -1;
        g->draggingBear = false;
        g->draggingShopPortal = false;
        g->draggingTitleRect = false;
        return;
    }

    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        g->draggingPortal = -1;
        g->draggingBear = false;
        g->draggingShopPortal = false;
        g->draggingTitleRect = false;
        
        // Sur l'écran d'accueil, on ne gère que le rectangle unique
        if (g->state == STATE_TITLE) {
            Rectangle titleRect = computeTitleRect(g);
            if (CheckCollisionPointRec(mouse, titleRect)) {
                g->draggingTitleRect = true;
                g->dragOffset = (Vector2){ mouse.x - titleRect.x, mouse.y - titleRect.y };
            }
        } else {
            // Logique normale pour le hub
            for (int i = 0; i < ZONE_COUNT; ++i) {
                Rectangle rect = computePortalRect(g, i);
                if (CheckCollisionPointRec(mouse, rect)) {
                    g->draggingPortal = i;
                    g->dragOffset = (Vector2){ mouse.x - rect.x, mouse.y - rect.y };
                    break;
                }
            }
            if (g->draggingPortal == -1) {
                Rectangle shopRect = computeShopPortalRect(g);
                if (CheckCollisionPointRec(mouse, shopRect)) {
                    g->draggingShopPortal = true;
                    g->dragOffset = (Vector2){ mouse.x - shopRect.x, mouse.y - shopRect.y };
                }
            }
            if (!g->draggingShopPortal && g->draggingPortal == -1 && g->hasMenuBear) {
                Rectangle bearRect = computeBearRect(g);
                if (bearRect.width > 0 && CheckCollisionPointRec(mouse, bearRect)) {
                    g->draggingBear = true;
                    g->dragOffset = (Vector2){ mouse.x - bearRect.x, mouse.y - bearRect.y };
                }
            }
        }
    }

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();
        if (g->draggingTitleRect) {
            RectRatios *layout = &g->titleRectLayout;
            layout->left = (mouse.x - g->dragOffset.x) / sw;
            layout->top = (mouse.y - g->dragOffset.y) / sh;
            clampPortalLayout(layout);
        } else if (g->draggingPortal >= 0) {
            RectRatios *layout = &g->portalLayouts[g->draggingPortal];
            layout->left = (mouse.x - g->dragOffset.x) / sw;
            layout->top = (mouse.y - g->dragOffset.y) / sh;
            clampPortalLayout(layout);
        } else if (g->draggingShopPortal) {
            RectRatios *layout = &g->shopPortalLayout;
            layout->left = (mouse.x - g->dragOffset.x) / sw;
            layout->top = (mouse.y - g->dragOffset.y) / sh;
            clampPortalLayout(layout);
        } else if (g->draggingBear) {
            float widthRatio = clampf(getBearWidthRatio(g), 0.0f, 0.99f);
            g->bearLayout.left = clampf((mouse.x - g->dragOffset.x) / sw, 0.0f, 1.0f - widthRatio);
            g->bearLayout.top = clampf((mouse.y - g->dragOffset.y) / sh, 0.0f, 1.0f - g->bearLayout.heightRatio);
        }
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        g->draggingPortal = -1;
        g->draggingBear = false;
        g->draggingShopPortal = false;
        g->draggingTitleRect = false;
    }
}

static void drawDebugOverlay(const Game *g) {
    if (!g->showDebugOverlay) return;
    Vector2 mouse = GetMousePosition();
    
    if (g->state == STATE_TITLE) {
        // Sur l'écran d'accueil, afficher uniquement le rectangle unique
        const int panelWidth = 400;
        const int panelHeight = 80;
        DrawRectangle(30, 90, panelWidth, panelHeight, (Color){ 0, 0, 0, 160 });
        DrawRectangleLines(30, 90, panelWidth, panelHeight, (Color){ 255, 255, 255, 80 });
        DrawText(TextFormat("Mouse: %.0f, %.0f", mouse.x, mouse.y), 40, 100, 20, RAYWHITE);
        
        Rectangle titleRect = computeTitleRect(g);
        DrawRectangleLinesEx(titleRect, 2.0f, (Color){ 255, 0, 0, 120 });
        DrawText(TextFormat("Title Rect: x=%.0f y=%.0f w=%.0f h=%.0f",
                            titleRect.x, titleRect.y, titleRect.width, titleRect.height),
                 40, 130, 18, LIGHTGRAY);
    } else {
        // Logique normale pour le hub
        const int panelWidth = 400;
        const int panelHeight = 40 + (ZONE_COUNT + 1) * 24;
        DrawRectangle(30, 90, panelWidth, panelHeight, (Color){ 0, 0, 0, 160 });
        DrawRectangleLines(30, 90, panelWidth, panelHeight, (Color){ 255, 255, 255, 80 });
        DrawText(TextFormat("Mouse: %.0f, %.0f", mouse.x, mouse.y), 40, 100, 20, RAYWHITE);
        for (int i = 0; i < ZONE_COUNT; ++i) {
            Rectangle rect = computePortalRect(g, i);
            DrawRectangleLinesEx(rect, 2.0f, (Color){ 255, 0, 0, 120 });
            DrawText(TextFormat("%s: x=%.0f y=%.0f w=%.0f h=%.0f",
                                HUB_PORTALS[i].label,
                                rect.x, rect.y, rect.width, rect.height),
                     40, 130 + i * 24, 18, LIGHTGRAY);
        }
        Rectangle shopRect = computeShopPortalRect(g);
        DrawRectangleLinesEx(shopRect, 2.0f, (Color){ 0, 180, 255, 150 });
        DrawText(TextFormat("Boutique: x=%.0f y=%.0f w=%.0f h=%.0f",
                            shopRect.x, shopRect.y, shopRect.width, shopRect.height),
                 40, 130 + ZONE_COUNT * 24, 18, LIGHTGRAY);
    }
}

static GameState zoneToState(ZoneId zone) {
    switch (zone) {
        case ZONE_JARDIN: return STATE_ZONE_JARDIN;
        case ZONE_CHAMBRE: return STATE_ZONE_CHAMBRE;
        case ZONE_GRENIER: return STATE_ZONE_GRENIER;
        case ZONE_CUISINE: return STATE_ZONE_CUISINE;
        default: return STATE_HUB;
    }
}

int main(int argc, char **argv) {
    Game g = {0};
    g.completionPopupDuration = MINIGAME_POPUP_DURATION;
    for (int i = 1; i < argc; ++i) if (strcmp(argv[i], "--log") == 0) g.loggingEnabled = true;

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(1920, 1080, "Gros Nounours 2D");
    InitAudioDevice();
    // Icône de fenêtre (placer votre image sous assets/icon.png)
    if (FileExists("assets/icon.png")) {
        Image icon = LoadImage("assets/icon.png");
        if (icon.data) { SetWindowIcon(icon); UnloadImage(icon); }
    }
    g.menuBackground = loadTextureIfAvailable("assets/imagefond.png");
    g.hasMenuBackground = g.menuBackground.id != 0;
    g.menuBear = loadTextureIfAvailable("assets/nounours_depart.png");
    g.hasMenuBear = g.menuBear.id != 0;
    g.titleBackground = loadTextureIfAvailable("assets/ecran acceuil.png");
    g.hasTitleBackground = g.titleBackground.id != 0;
    g.titleBackgroundHover = loadTextureIfAvailable("assets/ecran_accueil_ouvert.png");
    g.hasTitleBackgroundHover = g.titleBackgroundHover.id != 0;
    
    // Charger les nounours avec différentes tenues
    const char *bearOutfitFiles[5] = {
        "assets/nounours_noel.png",
        "assets/nounours_aviateur.png",
        "assets/nounours_plage.png",
        "assets/nounours_vampire.png",
        "assets/nounours_maillot.png"
    };
    for (int i = 0; i < 5; ++i) {
        g.bearOutfits[i] = loadTextureIfAvailable(bearOutfitFiles[i]);
        g.hasBearOutfit[i] = g.bearOutfits[i].id != 0;
    }
    g.currentBearOutfit = -1;  // -1 = tenue de départ
    
    // Initialiser la boutique (charge les textures depuis assets/boutique/)
    g.boutiqueData.collectibles = &g.collectibles;
    g.boutiqueData.currentBearOutfit = &g.currentBearOutfit;
    g.boutiqueData.wantsToExit = false;
    Boutique_Init(&g.boutiqueData);
    
    // Synchroniser les données avec la structure Game (pour compatibilité)
    for (int i = 0; i < 5; ++i) {
        g.outfits[i] = g.boutiqueData.outfits[i];
        g.hasOutfit[i] = g.boutiqueData.hasOutfit[i];
        g.outfitNames[i] = g.boutiqueData.outfitNames[i];
        g.outfitPrices[i] = g.boutiqueData.outfitPrices[i];
        g.outfitOwned[i] = g.boutiqueData.outfitOwned[i];
    }
    g.selectedOutfit = 0;
    // Initialiser outfitPreview avec la première tenue
    g.outfitPreview = g.outfits[0];
    g.hasOutfitPreview = g.hasOutfit[0];
    for (int i = 0; i < ZONE_COUNT; ++i) {
        g.zoneBackgrounds[i] = loadTextureIfAvailable(ZONE_BG_FILES[i]);
        g.hasZoneBackground[i] = g.zoneBackgrounds[i].id != 0;
    }
    loadMenuLayout(&g);

    SetTargetFPS(60);
    g.state = STATE_TITLE;
    g.activeZone = ZONE_NONE;
    g.showDebugOverlay = false;
    g.draggingPortal = -1;
    g.draggingBear = false;
    g.draggingShopPortal = false;
    g.hoveredPortal = -1;
    g.inactivityTimer = 0.0f;
    g.lastMousePos = (Vector2){ 0, 0 };
    g.music = LoadMusicStream("assets/music.ogg");
    g.hasMusic = g.music.frameCount > 0;
    g.musicMuted = false;
    if (g.hasMusic) {
        SetMusicVolume(g.music, 0.6f);
        PlayMusicStream(g.music);
    }
    g.soundOnIcon = loadTextureIfAvailable("assets/sound_on.png");
    g.soundOffIcon = loadTextureIfAvailable("assets/sound_off.png");
    g.hasSoundOnIcon = g.soundOnIcon.id != 0;
    g.hasSoundOffIcon = g.soundOffIcon.id != 0;
    resetPlayer(&g.player);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        clampBearToScreen(&g);
        g.hoveredPortal = -1;
        // Réinitialiser le timer seulement si on quitte l'écran d'accueil
        if (g.state != STATE_HUB) {
            g.inactivityTimer = 0.0f;
        }
        if (g.hasMusic && !g.musicMuted) {
            UpdateMusicStream(g.music);
        }

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (IsKeyPressed(KEY_F2)) g.showDebugOverlay = !g.showDebugOverlay;

        if (g.state == STATE_TITLE) {
            // Mode debug pour déplacer les rectangles (comme dans STATE_HUB)
            handleDebugDragging(&g);
            
            // Détecter le survol et le clic sur le rectangle unique
            Vector2 mouse = GetMousePosition();
            Rectangle titleRect = computeTitleRect(&g);
            bool isHovering = CheckCollisionPointRec(mouse, titleRect);
            g.hoveredPortal = isHovering ? 0 : -1;
            
            // Si on clique sur le rectangle (et qu'on n'est pas en train de le déplacer en mode debug), entrer dans le hub
            if (!g.showDebugOverlay && isHovering && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                g.state = STATE_HUB;
                resetPlayer(&g.player);
            }
            
            if (IsKeyPressed(KEY_ENTER)) { g.state = STATE_HUB; resetPlayer(&g.player); }
        } else if (g.state == STATE_PAUSE) {
            if (IsKeyPressed(KEY_ESCAPE)) g.state = STATE_HUB;
        } else {
            if (IsKeyPressed(KEY_ESCAPE)) g.state = STATE_PAUSE;
        }

        switch (g.state) {
            case STATE_HUB: {
                handleDebugDragging(&g);
                if (!g.showDebugOverlay) {
                    Vector2 mouse = GetMousePosition();
                    
                    // Détecter l'inactivité (seulement les clics réinitialisent le timer)
                    bool anyClick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
                    
                    if (anyClick) {
                        g.inactivityTimer = 0.0f; // Réinitialiser le timer seulement sur clic
                    } else {
                        g.inactivityTimer += dt; // Incrémenter le timer
                    }
                    
                    bool portalHovered = false;
                    bool portalClicked = false;
                    for (int i = 0; i < ZONE_COUNT; ++i) {
                        Rectangle rect = computePortalRect(&g, i);
                        if (CheckCollisionPointRec(mouse, rect)) {
                            portalHovered = true;
                            g.hoveredPortal = i;
                            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                                g.state = zoneToState(HUB_PORTALS[i].zone);
                                g.activeZone = HUB_PORTALS[i].zone;
                                portalClicked = true;
                                break;
                            }
                        }
                    }
                    if (!portalClicked) {
                        Rectangle shopRect = computeShopPortalRect(&g);
                        bool shopHovered = !portalHovered && CheckCollisionPointRec(mouse, shopRect);
                        if (shopHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            g.state = STATE_SHOP;
                            g.activeZone = ZONE_NONE;
                        }
                    }
                }
            } break;
            case STATE_ZONE_JARDIN:
            case STATE_ZONE_CHAMBRE:
            case STATE_ZONE_GRENIER:
            case STATE_ZONE_CUISINE:
                if (IsKeyPressed(KEY_BACKSPACE)) { g.state = STATE_HUB; g.activeZone = ZONE_NONE; }
                if (IsKeyPressed(KEY_ENTER)) {
                    // Choix mini‑jeu par zone
                    // Jardin -> Traffic, Puzzle -> PoussePousse, Bibliothèque -> Pendu, Cuisine -> CeriseSurGateau
                    if (g.state == STATE_ZONE_JARDIN) {
                        prepareMinigameSession(&g, GetMinigameTraffic(), "Traffic");
                    } else if (g.state == STATE_ZONE_CHAMBRE) {
                        prepareMinigameSession(&g, GetMinigamePoussePousse(), "Pousse-Pousse");
                    } else if (g.state == STATE_ZONE_GRENIER) {
                        prepareMinigameSession(&g, GetMinigamePendu(), "Pendu");
                    } else {
                        prepareMinigameSession(&g, GetMinigameCeriseSurGateau(), "Cerise sur gâteau"); // cuisine
                    }
                }
                break;
            case STATE_SHOP:
                if (IsKeyPressed(KEY_BACKSPACE)) { g.state = STATE_HUB; g.activeZone = ZONE_NONE; }
                // Synchroniser les données avant la mise à jour
                for (int i = 0; i < 5; ++i) {
                    g.boutiqueData.outfitOwned[i] = g.outfitOwned[i];
                }
                Boutique_Update(&g.boutiqueData);
                // Vérifier si on veut retourner au hub
                if (g.boutiqueData.wantsToExit) {
                    g.state = STATE_HUB;
                    g.activeZone = ZONE_NONE;
                }
                // Synchroniser les données après la mise à jour
                for (int i = 0; i < 5; ++i) {
                    g.outfitOwned[i] = g.boutiqueData.outfitOwned[i];
                }
                // currentBearOutfit est déjà synchronisé via pointeur
                break;
            case STATE_MINIJEU:
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    if (g.minigameCompleted) {
                        finalizeMinigame(&g);
                    } else {
                        if (g.currentMinigame.unload) g.currentMinigame.unload();
                        g.currentMinigame = (MinigameAPI){0};
                        g.currentMinigameName = NULL;
                        g.state = STATE_HUB;
                        g.activeZone = ZONE_NONE;
                    }
                    break;
                }

                if (g.currentMinigame.update) g.currentMinigame.update(dt);
                // Ajouter les pièces dès qu'on gagne, même si on ne quitte pas encore
                if (g.currentMinigame.isCompleted) {
                    int coins = 0;
                    g.currentMinigame.isCompleted(&coins); // Obtenir les pièces même si on ne quitte pas
                    if (coins > 0 && !g.minigameCompleted) {
                        // Ajouter les pièces immédiatement dès qu'on gagne
                        g.collectibles += coins;
                        g.minigameCompleted = true; // Marquer comme complété pour éviter les ajouts multiples
                    } else if (coins == 0 && g.minigameCompleted) {
                        // Si on rejoue (coins == 0 mais on était complété), réinitialiser le flag
                        g.minigameCompleted = false;
                    }
                }
                // Gestion de la fin des mini-jeux (quand on quitte)
                if (g.currentMinigame.isCompleted) {
                    int coins = 0;
                    if (g.currentMinigame.isCompleted(&coins)) {
                        // L'utilisateur veut quitter, finaliser le minijeu
                        if (g.activeZone >= 0 && g.activeZone < ZONE_COUNT) {
                            g.progress[g.activeZone].completed = true;
                        }
                        if (g.currentMinigame.unload) g.currentMinigame.unload();
                        g.currentMinigame = (MinigameAPI){0};
                        g.currentMinigameName = NULL;
                        g.state = STATE_HUB;
                        g.activeZone = ZONE_NONE;
                    }
                } else {
                    g.completionPopupTimer += dt;
                    bool shouldExit = (g.completionPopupDuration > 0.0f && g.completionPopupTimer >= g.completionPopupDuration)
                                      || IsKeyPressed(KEY_ENTER)
                                      || IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                    if (shouldExit) {
                        finalizeMinigame(&g);
                    }
                }
                break;
            default: break;
        }

        BeginDrawing();
        ClearBackground((Color){ 30, 34, 46, 255 });
        switch (g.state) {
            case STATE_TITLE:
                // Afficher le fond d'accueil (ouvert si survol, normal sinon)
                Texture2D *bgToUse = &g.titleBackground;
                bool hasBg = g.hasTitleBackground && g.titleBackground.id != 0;
                
                // Si on survole le rectangle, utiliser l'image "ouvert"
                if (g.hoveredPortal >= 0 && g.hasTitleBackgroundHover && g.titleBackgroundHover.id != 0) {
                    bgToUse = &g.titleBackgroundHover;
                    hasBg = true;
                }
                
                if (hasBg && bgToUse->id != 0) {
                    Rectangle src = { 0, 0, (float)bgToUse->width, (float)bgToUse->height };
                    Rectangle dst = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
                    DrawTexturePro(*bgToUse, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
                }
                drawCentered("Bienvenue dans l'aventure de Gros Nounours !", 70, 48, RAYWHITE);
                drawCentered("Clique vite sur la porte pour rencontrer Gros Nounours et commencer a jouer", 130, 32, LIGHTGRAY);
                drawDebugOverlay(&g);
                break;
            case STATE_PAUSE:
                drawCentered("Pause", 180, 48, RAYWHITE);
                drawCentered("Échap: Reprendre", 240, 24, LIGHTGRAY);
                break;
            case STATE_HUB: {
                drawMenuBackground(&g);
                drawBearCloseup(&g);
                drawPortalHighlights(&g);
                drawMinigameStatusTable(&g);
                drawCoinCounter(&g);
                drawDebugOverlay(&g);
                
                // Afficher le message d'inactivité après 5 secondes
                if (g.inactivityTimer >= 5.0f) {
                    int sw = GetScreenWidth();
                    int sh = GetScreenHeight();
                    int fontSize = 28;
                    int lineHeight = 35;
                    Color textColor = (Color){ 255, 255, 255, 230 };
                    
                    const char *line1 = "Clique sur une porte, la bibliotheque ou le puzzle pour jouer.";
                    const char *line2 = "Clique sur Gros Nounours pour lui acheter des habits.";
                    
                    int line1Width = MeasureText(line1, fontSize);
                    int line2Width = MeasureText(line2, fontSize);
                    
                    // Fond semi-transparent pour le message
                    int padding = 20;
                    int boxHeight = lineHeight * 2 + padding * 2;
                    int boxWidth = (line1Width > line2Width ? line1Width : line2Width) + padding * 2;
                    int boxX = (sw - boxWidth) / 2 + 100; // Décalé de 100 pixels vers la droite
                    int boxY = sh - boxHeight - 50;
                    
                    DrawRectangle(boxX, boxY, boxWidth, boxHeight, (Color){ 0, 0, 0, 180 });
                    DrawRectangleLinesEx((Rectangle){ boxX, boxY, boxWidth, boxHeight }, 2.0f, (Color){ 255, 255, 255, 100 });
                    
                    DrawText(line1, boxX + padding, boxY + padding, fontSize, textColor);
                    DrawText(line2, boxX + padding, boxY + padding + lineHeight, fontSize, textColor);
                }
            } break;
            case STATE_ZONE_JARDIN: {
                int y = 80;
                int fontSize = 32;
                int lineHeight = 42;
                Color textColor = RAYWHITE;
                
                drawCentered("Bienvenue dans l'aventure de Gros Nounours.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Ton but est d'aider Gros Nounours a atteindre la fin du chemin.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Pour le deplacer, utilise les fleches de ton clavier : vers la droite, la gauche, l'avant ou l'arriere.", y, fontSize - 2, textColor);
                y += lineHeight * 2;
                drawCentered("Attention tout de meme aux troncs d'arbre qui bloquent le passage.", y, fontSize - 2, textColor);
                y += lineHeight;
                drawCentered("Si Gros Nounours en touche un, il perd une vie.", y, fontSize - 2, textColor);
                y += lineHeight;
                drawCentered("Tu as 3 vies pour reussir le parcours.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Es-tu pret a guider Gros Nounours jusqu'au bout de son aventure ?", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Clique sur le bouton Entree pour commencer a jouer.", y, fontSize, (Color){255, 255, 100, 255});
            } break;
            case STATE_ZONE_CHAMBRE: {
                int y = 80;
                int fontSize = 32;
                int lineHeight = 42;
                Color textColor = RAYWHITE;
                
                drawCentered("Bienvenue dans le jeu du puzzle.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Ton objectif est de remettre toutes les pieces a leur bonne place pour reformer l'image.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Pour jouer, c'est tres simple : clique sur une piece du puzzle pour la deplacer.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Pret a reconstruire le puzzle comme un vrai champion ?", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Clique sur le bouton Entree pour commencer a jouer.", y, fontSize, (Color){255, 255, 100, 255});
            } break;
            case STATE_ZONE_GRENIER: {
                int y = 80;
                int fontSize = 32;
                int lineHeight = 42;
                Color textColor = RAYWHITE;
                
                drawCentered("Bienvenue dans le jeu du mot mystere.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Ton defi est de deviner le mot cache.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Pour jouer, c'est tres simple :", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Tu peux taper une lettre sur ton clavier ou cliquer sur une lettre de l'alphabet.", y, fontSize - 2, textColor);
                y += lineHeight * 2;
                drawCentered("Tu as 7 chances pour trouver le mot.", y, fontSize, textColor);
                y += lineHeight;
                drawCentered("Mais attention, a chaque erreur, la fleur perd une petale.", y, fontSize - 2, textColor);
                y += lineHeight;
                drawCentered("Quand il n'y en a plus, la partie est terminee.", y, fontSize - 2, textColor);
                y += lineHeight * 2;
                drawCentered("Si tu fais 5 erreurs, une definition secrete apparait pour t'aider a deviner le mot.", y, fontSize - 2, textColor);
                y += lineHeight;
                drawCentered("Et si tu reussis a trouver le mot, tu pourras aussi lire sa definition pour apprendre de nouvelles choses.", y, fontSize - 2, textColor);
                y += lineHeight * 2;
                drawCentered("Pret pour l'aventure des mots ?", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Appuie sur la touche Entree pour commencer a jouer.", y, fontSize, (Color){255, 255, 100, 255});
            } break;
            case STATE_ZONE_CUISINE: {
                int y = 80;
                int fontSize = 32;
                int lineHeight = 42;
                Color textColor = RAYWHITE;
                
                drawCentered("Bienvenue dans l'atelier des gateaux gourmands.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Tu vas devoir decorer un gateau comme un vrai patissier.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Au debut, tu verras une image pendant quelques secondes. Regarde-la bien et essaie de tout retenir.", y, fontSize - 2, textColor);
                y += lineHeight;
                drawCentered("Ton but est de refaire exactement le meme gateau pour gagner.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Prends les ingredients et pose-les sur le gateau au bon endroit.", y, fontSize, textColor);
                y += lineHeight;
                drawCentered("Si tu as termine avant la fin du chrono, clique sur le bouton Termine.", y, fontSize - 2, textColor);
                y += lineHeight * 2;
                drawCentered("Pret a creer le gateau parfait ?", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Clique sur le bouton Entree pour commencer a jouer.", y, fontSize, (Color){255, 255, 100, 255});
            } break;
            case STATE_SHOP:
                // Synchroniser les données avant l'affichage
                for (int i = 0; i < 5; ++i) {
                    g.boutiqueData.outfitOwned[i] = g.outfitOwned[i];
                }
                Boutique_Draw(&g.boutiqueData);
                break;
            case STATE_MINIJEU:
                if (g.currentMinigame.draw) g.currentMinigame.draw();
                // Les minijeux gèrent maintenant leur propre écran de fin (comme le pendu)
                // Plus besoin du popup générique
                break;
            default: break;
        }

        // Bouton musique (mute/unmute) toujours visible
        drawMusicButton(&g);
        if (g.hasMusic) {
            Rectangle musicBtn = getMusicButtonRect();
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), musicBtn)) {
                g.musicMuted = !g.musicMuted;
                if (g.musicMuted) PauseMusicStream(g.music);
                else ResumeMusicStream(g.music);
            }
        }
        EndDrawing();
    }
    saveMenuLayout(&g);
    if (g.hasMenuBackground) UnloadTexture(g.menuBackground);
    if (g.hasTitleBackground) UnloadTexture(g.titleBackground);
    if (g.hasTitleBackgroundHover) UnloadTexture(g.titleBackgroundHover);
    for (int i = 0; i < ZONE_COUNT; ++i) {
        if (g.hasZoneBackground[i]) UnloadTexture(g.zoneBackgrounds[i]);
    }
    if (g.hasMusic) {
        StopMusicStream(g.music);
        UnloadMusicStream(g.music);
    }
    if (g.hasMenuBear) UnloadTexture(g.menuBear);
    for (int i = 0; i < 5; ++i) {
        if (g.hasBearOutfit[i]) UnloadTexture(g.bearOutfits[i]);
    }
    if (g.hasSoundOnIcon) UnloadTexture(g.soundOnIcon);
    if (g.hasSoundOffIcon) UnloadTexture(g.soundOffIcon);
    if (g.hasOutfitPreview) UnloadTexture(g.outfitPreview);
    // Les textures des tenues sont déchargées par Boutique_Unload
    Boutique_Unload(&g.boutiqueData);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}


