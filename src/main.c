// Gros Nounours 2D — squelette 2D (hub, déplacements, saut, interaction)
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "minigames/minigame.h"
#include "minigames/pousse_pousse/pousse_pousse.h"
#include "minigames/traffic/traffic.h"
#include "minigames/gateau/gateau.h"

typedef enum {
    STATE_TITLE = 0,
    STATE_HUB,
    STATE_ZONE_JARDIN,
    STATE_ZONE_CHAMBRE,
    STATE_ZONE_GRENIER,
    STATE_ZONE_CUISINE,
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
    int activeZone;
    ZoneProgress progress[ZONE_COUNT];
    Texture2D menuBackground;
    Texture2D menuBear;
    bool hasMenuBackground;
    bool hasMenuBear;
    bool showDebugOverlay;
    int draggingPortal;
    bool draggingBear;
    Vector2 dragOffset;
    RectRatios portalLayouts[ZONE_COUNT];
    BearLayout bearLayout;
} Game;

typedef struct {
    ZoneId zone;
    const char *label;
} HubPortalInfo;

static const HubPortalInfo HUB_PORTALS[ZONE_COUNT] = {
    { ZONE_JARDIN,  "Jardin" },
    { ZONE_CHAMBRE, "Chambre" },
    { ZONE_GRENIER, "Grenier" },
    { ZONE_CUISINE, "Cuisine" }
};

static const RectRatios DEFAULT_PORTAL_LAYOUTS[ZONE_COUNT] = {
    { 0.065f, 0.25f, 0.11f, 0.30f },
    { 0.225f, 0.25f, 0.11f, 0.30f },
    { 0.395f, 0.25f, 0.11f, 0.30f },
    { 0.565f, 0.25f, 0.11f, 0.30f }
};

static const BearLayout DEFAULT_BEAR_LAYOUT = { 0.021f, 0.113f, 0.85f };
static const char *PORTAL_KEYS[ZONE_COUNT] = { "jardin", "chambre", "grenier", "cuisine" };
static const char *LAYOUT_FILE = "config/menu_layout.ini";

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
    Image img = LoadImage(path);
    if (img.data) {
        tex = LoadTextureFromImage(img);
        UnloadImage(img);
    }
    return tex;
}

static void initDefaultLayout(Game *g) {
    for (int i = 0; i < ZONE_COUNT; ++i) g->portalLayouts[i] = DEFAULT_PORTAL_LAYOUTS[i];
    g->bearLayout = DEFAULT_BEAR_LAYOUT;
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
    fprintf(f, "bear=%.5f,%.5f,%.5f\n", g->bearLayout.left, g->bearLayout.top, g->bearLayout.heightRatio);
    fclose(f);
}

static void drawMenuBackground(const Game *g) {
    if (g->hasMenuBackground) {
        Rectangle src = { 0, 0, (float)g->menuBackground.width, (float)g->menuBackground.height };
        Rectangle dst = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
        DrawTexturePro(g->menuBackground, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        drawGround();
    }
}

static void drawBearCloseup(const Game *g) {
    if (!g->hasMenuBear) return;
    Rectangle dst = computeBearRect(g);
    if (dst.width <= 0 || dst.height <= 0) return;
    Rectangle src = { 0, 0, (float)g->menuBear.width, (float)g->menuBear.height };
    DrawTexturePro(g->menuBear, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
}

static void drawMinigameStatusTable(const Game *g) {
    static const char *zoneLabels[ZONE_COUNT] = { "Jardin", "Chambre", "Grenier", "Cuisine" };
    const float tableX = 60.0f;
    const float tableY = 620.0f;
    const float rowHeight = 42.0f;
    const float tableWidth = 460.0f;
    const float tableHeight = rowHeight * (ZONE_COUNT + 1);

    DrawRectangleRounded((Rectangle){ tableX - 10, tableY - 20, tableWidth + 20, tableHeight + 30 }, 0.08f, 6, (Color){ 0, 0, 0, 160 });
    DrawText("Etat des mini-jeux", (int)tableX, (int)tableY - 10, 26, RAYWHITE);

    DrawLine(tableX, tableY + 8, tableX + tableWidth, tableY + 8, LIGHTGRAY);
    DrawText("Pièce", (int)tableX, (int)tableY + 18, 22, LIGHTGRAY);
    DrawText("Statut", (int)(tableX + tableWidth - 150), (int)tableY + 18, 22, LIGHTGRAY);

    for (int i = 0; i < ZONE_COUNT; ++i) {
        float rowY = tableY + 18 + rowHeight * (i + 1);
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

static Rectangle computeBearRect(const Game *g) {
    if (!g->hasMenuBear) return (Rectangle){ 0 };
    float sh = (float)GetScreenHeight();
    float sw = (float)GetScreenWidth();
    float bearHeight = g->bearLayout.heightRatio * sh;
    float aspect = g->menuBear.height > 0 ? (float)g->menuBear.width / (float)g->menuBear.height : 1.0f;
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
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < ZONE_COUNT; ++i) {
        Rectangle rect = computePortalRect(g, i);
        bool hover = CheckCollisionPointRec(mouse, rect);
        if (hover) {
            DrawRectangleRounded(rect, 0.12f, 6, (Color){ 255, 255, 255, 35 });
            DrawRectangleRoundedLines(rect, 0.12f, 6, (Color){ 255, 215, 0, 200 });
            DrawText(HUB_PORTALS[i].label, (int)(rect.x + rect.width * 0.2f), (int)(rect.y - 32), 28, RAYWHITE);
        }
    }
}

static void handleDebugDragging(Game *g) {
    if (!g->showDebugOverlay) {
        g->draggingPortal = -1;
        g->draggingBear = false;
        return;
    }

    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        g->draggingPortal = -1;
        g->draggingBear = false;
        for (int i = 0; i < ZONE_COUNT; ++i) {
            Rectangle rect = computePortalRect(g, i);
            if (CheckCollisionPointRec(mouse, rect)) {
                g->draggingPortal = i;
                g->dragOffset = (Vector2){ mouse.x - rect.x, mouse.y - rect.y };
                break;
            }
        }
        if (g->draggingPortal == -1 && g->hasMenuBear) {
            Rectangle bearRect = computeBearRect(g);
            if (bearRect.width > 0 && CheckCollisionPointRec(mouse, bearRect)) {
                g->draggingBear = true;
                g->dragOffset = (Vector2){ mouse.x - bearRect.x, mouse.y - bearRect.y };
            }
        }
    }

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();
        if (g->draggingPortal >= 0) {
            RectRatios *layout = &g->portalLayouts[g->draggingPortal];
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
    }
}

static void drawDebugOverlay(const Game *g) {
    if (!g->showDebugOverlay) return;
    Vector2 mouse = GetMousePosition();
    const int panelWidth = 400;
    const int panelHeight = 40 + ZONE_COUNT * 24;
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
    for (int i = 1; i < argc; ++i) if (strcmp(argv[i], "--log") == 0) g.loggingEnabled = true;

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(1920, 1080, "Gros Nounours 2D");
    // Icône de fenêtre (placer votre image sous assets/icon.png)
    {
        Image icon = LoadImage("assets/icon.png");
        if (icon.data) { SetWindowIcon(icon); UnloadImage(icon); }
    }
    g.menuBackground = loadTextureIfAvailable("assets/imagefond.png");
    g.hasMenuBackground = g.menuBackground.id != 0;
    g.menuBear = loadTextureIfAvailable("assets/nounoursmenu.png");
    g.hasMenuBear = g.menuBear.id != 0;
    loadMenuLayout(&g);

    SetTargetFPS(60);
    g.state = STATE_TITLE;
    g.activeZone = ZONE_NONE;
    g.showDebugOverlay = false;
    g.draggingPortal = -1;
    g.draggingBear = false;
    resetPlayer(&g.player);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        clampBearToScreen(&g);

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (IsKeyPressed(KEY_F2)) g.showDebugOverlay = !g.showDebugOverlay;

        if (g.state == STATE_TITLE) {
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
                    for (int i = 0; i < ZONE_COUNT; ++i) {
                        Rectangle rect = computePortalRect(&g, i);
                        if (CheckCollisionPointRec(mouse, rect)) {
                            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                                g.state = zoneToState(HUB_PORTALS[i].zone);
                                g.activeZone = HUB_PORTALS[i].zone;
                            }
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
                    if (g.state == STATE_ZONE_JARDIN) g.currentMinigame = GetMinigamePoussePousse();
                    else if (g.state == STATE_ZONE_CHAMBRE) g.currentMinigame = GetMinigameGateau();
                    else if (g.state == STATE_ZONE_GRENIER) g.currentMinigame = GetMinigameTraffic();
                    else g.currentMinigame = GetMinigamePoussePousse(); // défaut
                    if (g.currentMinigame.init) g.currentMinigame.init();
                    g.state = STATE_MINIJEU;
                }
                break;
            case STATE_MINIJEU:
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    if (g.currentMinigame.unload) g.currentMinigame.unload();
                    g.state = STATE_HUB;
                    g.activeZone = ZONE_NONE;
                }
                if (g.currentMinigame.update) g.currentMinigame.update(dt);
                // Gestion de la fin des mini-jeux et récupération des pièces
                if (g.currentMinigame.isCompleted) {
                    int coins = 0;
                    if (g.currentMinigame.isCompleted(&coins)) {
                        g.collectibles += coins;
                        if (g.activeZone >= 0 && g.activeZone < ZONE_COUNT) g.progress[g.activeZone].completed = true;
                        if (g.currentMinigame.unload) g.currentMinigame.unload();
                        g.state = STATE_HUB;
                        g.activeZone = ZONE_NONE;
                    }
                }
                break;
            default: break;
        }

        BeginDrawing();
        ClearBackground((Color){ 30, 34, 46, 255 });
        switch (g.state) {
            case STATE_TITLE:
                drawCentered("Gros Nounours 2D", 140, 64, RAYWHITE);
                drawCentered("Entrée: Jouer", 240, 26, LIGHTGRAY);
                drawCentered("Flèches: Gauche/Droite — Espace: Saut — E/Entrée: Interagir", 300, 20, GRAY);
                break;
            case STATE_PAUSE:
                drawCentered("Pause", 180, 48, RAYWHITE);
                drawCentered("Échap: Reprendre", 240, 24, LIGHTGRAY);
                break;
            case STATE_HUB: {
                drawMenuBackground(&g);
                drawBearCloseup(&g);
                DrawText("Clique sur une porte | F11: Plein écran | F2: Debug (drag & drop)", 40, 40, 24, WHITE);
                drawPortalHighlights(&g);
                drawMinigameStatusTable(&g);
                drawCoinCounter(&g);
                drawDebugOverlay(&g);
            } break;
            case STATE_ZONE_JARDIN:
                drawCentered("Jardin — Entrée: Mini‑jeu | Retour: Backspace", 160, 26, RAYWHITE);
                break;
            case STATE_ZONE_CHAMBRE:
                drawCentered("Chambre — Entrée: Mini‑jeu | Retour: Backspace", 160, 26, RAYWHITE);
                break;
            case STATE_ZONE_GRENIER:
                drawCentered("Grenier — Entrée: Mini‑jeu | Retour: Backspace", 160, 26, RAYWHITE);
                break;
            case STATE_ZONE_CUISINE:
                drawCentered("Cuisine — Entrée: Mini‑jeu | Retour: Backspace", 160, 26, RAYWHITE);
                break;
            case STATE_MINIJEU:
                if (g.currentMinigame.draw) g.currentMinigame.draw();
                break;
            default: break;
        }
        EndDrawing();
    }
    saveMenuLayout(&g);
    if (g.hasMenuBackground) UnloadTexture(g.menuBackground);
    if (g.hasMenuBear) UnloadTexture(g.menuBear);
    CloseWindow();
    return 0;
}


