// Traffic runner version simple : peu de variables et beaucoup de commentaires
#include "traffic.h"
#include <stdbool.h>

typedef struct { float x, y, w, h; } RectF;

// Constantes faciles à lire
#define MAX_OBS   16
#define MAX_COINS 32
#define PLAYER_SPEED_X 250.0f
#define PLAYER_SPEED_Y 180.0f
#define SCROLL_SPEED   200.0f

// Etat du jeu
static RectF player;
static RectF obstacles[MAX_OBS];
static RectF coins[MAX_COINS];
static int obstacleCount;
static int coinCount;
static float roadX, roadW;
static float scrollSpeed;
static int lives;
static float distanceMeters;
static float goalMeters = 400.0f;
static bool levelCompleted;
static float completionTimer;
static int collectedCoins;
static float obstacleTimer;
static float coinTimer;

// Outil de base : collision rectangle/rectangle
static bool intersect(const RectF *a, const RectF *b) {
    return !(a->x + a->w < b->x ||
             b->x + b->w < a->x ||
             a->y + a->h < b->y ||
             b->y + b->h < a->y);
}

// Réinitialisation complète de la partie
static void resetTraffic(void) {
    roadW = GetScreenWidth() * 0.4f;
    roadX = (GetScreenWidth() - roadW) * 0.5f;
    player.w = 70;
    player.h = 80;
    player.x = roadX + (roadW - player.w) * 0.5f;
    player.y = GetScreenHeight() - player.h - 20.0f;
    scrollSpeed = SCROLL_SPEED;
    obstacleCount = 0;
    coinCount = 0;
    obstacleTimer = 0.0f;
    coinTimer = 0.0f;
    lives = 3;
    distanceMeters = 0.0f;
    collectedCoins = 0;
    levelCompleted = false;
    completionTimer = 0.0f;
}

// Génère un obstacle simple positionné aléatoirement sur la route
static void spawnObstacle(void) {
    if (obstacleCount >= MAX_OBS) return;
    RectF o;
    o.w = 60;
    o.h = 80;
    float maxX = roadW - o.w;
    if (maxX < 0) maxX = 0;
    o.x = roadX + (float)GetRandomValue(0, (int)maxX);
    o.y = -o.h;
    obstacles[obstacleCount++] = o;
}

// Génère une pièce simple
static void spawnCoin(void) {
    if (coinCount >= MAX_COINS) return;
    RectF c;
    c.w = c.h = 24;
    float maxX = roadW - c.w;
    if (maxX < 0) maxX = 0;
    c.x = roadX + (float)GetRandomValue(0, (int)maxX);
    c.y = -c.h;
    coins[coinCount++] = c;
}

// Supprime les éléments passés en bas de l'écran
static int removeOffScreen(RectF *items, int count) {
    int write = 0;
    float limit = GetScreenHeight() + 40.0f;
    for (int i = 0; i < count; i++) {
        if (items[i].y < limit) items[write++] = items[i];
    }
    return write;
}

static void mg_init(void) {
    resetTraffic();
}

static void mg_update(float dt) {
    // Relancer rapidement les parties
    if (lives <= 0 && IsKeyPressed(KEY_R)) resetTraffic();
    if (levelCompleted && IsKeyPressed(KEY_R)) resetTraffic();
    if (lives <= 0 || levelCompleted) {
        if (completionTimer > 0.0f) completionTimer -= dt;
        return;
    }

    // Déplacement simple : on limite juste le joueur à la route
    if (IsKeyDown(KEY_LEFT))  player.x -= PLAYER_SPEED_X * dt;
    if (IsKeyDown(KEY_RIGHT)) player.x += PLAYER_SPEED_X * dt;
    if (IsKeyDown(KEY_UP))    player.y -= PLAYER_SPEED_Y * dt;
    if (IsKeyDown(KEY_DOWN))  player.y += PLAYER_SPEED_Y * dt;
    if (player.x < roadX) player.x = roadX;
    if (player.x + player.w > roadX + roadW) player.x = roadX + roadW - player.w;
    float minY = 20.0f;
    float maxY = GetScreenHeight() - player.h - 20.0f;
    if (player.y < minY) player.y = minY;
    if (player.y > maxY) player.y = maxY;

    // Spawns basés sur deux minuteries toutes simples
    obstacleTimer -= dt;
    coinTimer -= dt;
    if (obstacleTimer <= 0.0f) {
        obstacleTimer = 1.0f;
        spawnObstacle();
    }
    if (coinTimer <= 0.0f) {
        coinTimer = 1.4f;
        spawnCoin();
    }

    // Descente des éléments avec la vitesse de scroll
    for (int i = 0; i < obstacleCount; i++) obstacles[i].y += scrollSpeed * dt;
    for (int i = 0; i < coinCount; i++) coins[i].y += scrollSpeed * dt;

    // Collision obstacles -> on retire une vie et on pousse l'obstacle en dehors
    for (int i = 0; i < obstacleCount; i++) {
        if (intersect(&player, &obstacles[i])) {
            lives -= 1;
            obstacles[i].y = GetScreenHeight() + 100.0f;
        }
    }

    // Collision pièces -> simple compteur
    for (int i = 0; i < coinCount; i++) {
        if (intersect(&player, &coins[i])) {
            collectedCoins += 1;
            coins[i].y = GetScreenHeight() + 100.0f;
        }
    }

    obstacleCount = removeOffScreen(obstacles, obstacleCount);
    coinCount = removeOffScreen(coins, coinCount);

    // Distance parcourue (on convertit la vitesse en mètres arbitraires)
    distanceMeters += (scrollSpeed * dt) / 50.0f;
    if (distanceMeters >= goalMeters) {
        levelCompleted = true;
        completionTimer = 2.5f;
    }
}

static void mg_draw(void) {
    // Route grise et lignes centrales jaunes
    DrawRectangle((int)roadX, 0, (int)roadW, GetScreenHeight(), (Color){ 40, 40, 40, 255 });
    for (int y = -40; y < GetScreenHeight(); y += 60) {
        DrawRectangle((int)(roadX + roadW * 0.5f - 4), y, 8, 30, (Color){ 255, 220, 0, 200 });
    }

    // Joueur représenté par un rectangle orange
    DrawRectangle((int)player.x, (int)player.y, (int)player.w, (int)player.h, (Color){ 255, 180, 80, 255 });

    // Obstacles rouges
    for (int i = 0; i < obstacleCount; i++) {
        DrawRectangle((int)obstacles[i].x, (int)obstacles[i].y, (int)obstacles[i].w, (int)obstacles[i].h, (Color){ 190, 60, 60, 255 });
    }

    // Pièces jaunes
    for (int i = 0; i < coinCount; i++) {
        DrawCircle((int)(coins[i].x + coins[i].w * 0.5f),
                   (int)(coins[i].y + coins[i].h * 0.5f),
                   coins[i].w * 0.5f,
                   (Color){ 255, 220, 40, 255 });
    }

    // Informations HUD simples
    DrawText(TextFormat("Vies : %d", lives), 20, 20, 22, LIGHTGRAY);
    DrawText(TextFormat("Distance : %.0f m", distanceMeters), 20, 46, 22, LIGHTGRAY);
    DrawText(TextFormat("Pieces : %d", collectedCoins), 20, 72, 22, LIGHTGRAY);
    DrawText("R pour recommencer", 20, 98, 20, LIGHTGRAY);

    if (lives <= 0) {
        DrawText("Perdu ! R pour rejouer.", 20, 140, 24, (Color){ 255, 100, 100, 255 });
    } else if (levelCompleted) {
        DrawText("Bravo ! Objectif atteint.", 20, 140, 24, (Color){ 120, 250, 120, 255 });
    }
}

// Aucune ressource à libérer dans la version simple, mais on garde la fonction
static void mg_unload(void) { }

static bool mg_isCompleted(int *coinsOut) {
    if (coinsOut) *coinsOut = collectedCoins;
    return levelCompleted && completionTimer <= 0.0f;
}

MinigameAPI GetMinigameTraffic(void) {
    MinigameAPI api = { mg_init, mg_update, mg_draw, mg_unload, mg_isCompleted };
    return api;
}


