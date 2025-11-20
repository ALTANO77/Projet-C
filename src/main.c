// Gros Nounours 2D — squelette 2D (hub, déplacements, saut, interaction)
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "minigames/minigame.h"
#include "minigames/pousse_pousse/pousse_pousse.h"
#include "minigames/traffic/traffic.h"
#include "minigames/gateau/gateau.h"
#include "minigames/youre_cooked/youre_cooked.h"

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

typedef struct {
    Vector2 position;
    Vector2 velocity;
    bool onGround;
} Player;

typedef struct {
    GameState state;
    Player player;
    bool loggingEnabled;
    int collectibles;
    MinigameAPI currentMinigame;
} Game;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void drawCentered(const char *txt, int y, int size, Color c) {
    int w = MeasureText(txt, size);
    DrawText(txt, (GetScreenWidth() - w)/2, y, size, c);
}

static void resetPlayer(Player *p) {
    p->position = (Vector2){ 160, 420 };
    p->velocity = (Vector2){ 0, 0 };
    p->onGround = false;
}

static void updatePlayer(Player *p, float dt) {
    const float moveSpeed = 220.0f;
    const float jumpVel = -420.0f;
    const float gravity = 980.0f;

    float ax = 0.0f;
    if (IsKeyDown(KEY_LEFT)) ax -= moveSpeed;
    if (IsKeyDown(KEY_RIGHT)) ax += moveSpeed;

    p->velocity.x = ax;
    if (p->onGround && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_A))) {
        p->velocity.y = jumpVel;
        p->onGround = false;
    }

    p->velocity.y += gravity * dt;
    p->position.x += p->velocity.x * dt;
    p->position.y += p->velocity.y * dt;

    // Simple ground at y=460
    if (p->position.y >= 460) { p->position.y = 460; p->velocity.y = 0; p->onGround = true; }

    // Screen bounds
    p->position.x = clampf(p->position.x, 16, GetScreenWidth() - 16);
}

static void drawGround(void) {
    DrawRectangle(0, 480, GetScreenWidth(), GetScreenHeight() - 480, (Color){ 60, 100, 60, 255 });
    DrawRectangle(0, 460, GetScreenWidth(), 20, (Color){ 90, 60, 40, 255 });
}

static void drawPlayer(const Player *p) {
    DrawCircleV((Vector2){ p->position.x, p->position.y - 16 }, 16, (Color){ 240, 200, 120, 255 }); // tête
    DrawRectangle((int)(p->position.x - 14), (int)(p->position.y - 14), 28, 28, (Color){ 255, 170, 60, 255 }); // corps
}

int main(int argc, char **argv) {
    Game g = {0};
    for (int i = 1; i < argc; ++i) if (strcmp(argv[i], "--log") == 0) g.loggingEnabled = true;

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1920, 1080, "Gros Nounours 2D");
    // Icône de fenêtre (placer votre image sous assets/icon.png)
    {
        Image icon = LoadImage("assets/icon.png");
        if (icon.data) { SetWindowIcon(icon); UnloadImage(icon); }
    }
    SetTargetFPS(60);
    g.state = STATE_TITLE;
    resetPlayer(&g.player);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (g.state == STATE_TITLE) {
            if (IsKeyPressed(KEY_ENTER)) { g.state = STATE_HUB; resetPlayer(&g.player); }
        } else if (g.state == STATE_PAUSE) {
            if (IsKeyPressed(KEY_ESCAPE)) g.state = STATE_HUB;
        } else {
            if (IsKeyPressed(KEY_ESCAPE)) g.state = STATE_PAUSE;
        }

        switch (g.state) {
            case STATE_HUB: {
                updatePlayer(&g.player, dt);
                // Interactions zones (simples portails)
                // Jardin
                if (fabsf(g.player.position.x - 200) < 30 && g.player.onGround && (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER))) g.state = STATE_ZONE_JARDIN;
                // Chambre
                if (fabsf(g.player.position.x - 500) < 30 && g.player.onGround && (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER))) g.state = STATE_ZONE_CHAMBRE;
                // Grenier
                if (fabsf(g.player.position.x - 800) < 30 && g.player.onGround && (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER))) g.state = STATE_ZONE_GRENIER;
                // Cuisine
                if (fabsf(g.player.position.x - 1100) < 30 && g.player.onGround && (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER))) g.state = STATE_ZONE_CUISINE;
            } break;
            case STATE_ZONE_JARDIN:
            case STATE_ZONE_CHAMBRE:
            case STATE_ZONE_GRENIER:
            case STATE_ZONE_CUISINE:
                if (IsKeyPressed(KEY_BACKSPACE)) g.state = STATE_HUB;
                if (IsKeyPressed(KEY_ENTER)) {
                    // Choix mini‑jeu par zone
                    if (g.state == STATE_ZONE_JARDIN) g.currentMinigame = GetMinigamePoussePousse();
                    else if (g.state == STATE_ZONE_CHAMBRE) g.currentMinigame = GetMinigameGateau();
                    else if (g.state == STATE_ZONE_GRENIER) g.currentMinigame = GetMinigameTraffic();
                    else g.currentMinigame = GetMinigameYoureCooked(); // cuisine
                    if (g.currentMinigame.init) g.currentMinigame.init();
                    g.state = STATE_MINIJEU;
                }
                break;
            case STATE_MINIJEU:
                if (IsKeyPressed(KEY_BACKSPACE)) { if (g.currentMinigame.unload) g.currentMinigame.unload(); g.state = STATE_HUB; }
                if (g.currentMinigame.update) g.currentMinigame.update(dt);
                // Gestion de la fin des mini-jeux et récupération des pièces
                if (g.currentMinigame.isCompleted) {
                    int coins = 0;
                    if (g.currentMinigame.isCompleted(&coins)) {
                        g.collectibles += coins;
                        if (g.currentMinigame.unload) g.currentMinigame.unload();
                        g.state = STATE_HUB;
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
                // Décor simple + portails
                drawGround();
                // Portails
                DrawRectangle(180, 380, 40, 80, (Color){ 90, 180, 90, 255 }); DrawText("Jardin", 170, 360, 18, LIGHTGRAY);
                DrawRectangle(480, 380, 40, 80, (Color){ 90, 140, 200, 255 }); DrawText("Chambre", 458, 360, 18, LIGHTGRAY);
                DrawRectangle(780, 380, 40, 80, (Color){ 180, 140, 90, 255 }); DrawText("Grenier", 764, 360, 18, LIGHTGRAY);
                DrawRectangle(1080, 380, 40, 80, (Color){ 200, 120, 120, 255 }); DrawText("Cuisine", 1068, 360, 18, LIGHTGRAY);
                // Indication interaction
                DrawText("Approche un portail et appuie sur E/Entrée", 28, 28, 20, LIGHTGRAY);
                drawPlayer(&g.player);
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
    CloseWindow();
    return 0;
}


