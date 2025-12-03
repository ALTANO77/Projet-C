// Traffic runner avancé (textures, pièces, distance, complétion)
#include "traffic.h"
#include <stdbool.h>
#include <math.h>

typedef struct { float x, y, w, h; } RectF;

static RectF player;
static float roadX, roadW;
static float speedScroll;
static float laneWidth;
static int lives;
// Score/distance & speed model
static float distancePixels;
static float pixelsPerMeter = 48.0f; // approx. 48 px = 1 m (tunable)
static float speedAccelPx = 18.0f;   // px/s^2 acceleration for increasing difficulty
static float maxSpeedPx   = 1200.0f; // > 15 m/s (1200/48 = 25 m/s)
static float goalMeters   = 1000.0f;
static bool levelCompleted;
static float completionMessageTimer;

// Textures
static Texture2D texPlayer; // fallback single frame
static Texture2D texPlayerFrames[4];
static int playerFrameCount;
static int playerFrameIndex;
static float playerFrameTimer; // accumulates time for frame switching
static float playerFrameDuration = 0.12f; // ~8 FPS
static Texture2D texObstacle;
static Texture2D texRoad;
static Texture2D texBorderLeft;
static Texture2D texBorderRight;
static Texture2D texCoin;
static bool texturesReady;
static float roadScroll;

#define MAX_OBS 32
static RectF obs[MAX_OBS];
static int obsCount;
static float spawnTimer;

#define MAX_COINS 64
static RectF coins[MAX_COINS];
static int coinCount;
static float coinSpawnTimer;
static int collectedCoins;

static Texture2D loadTextureIfExists(const char *path) {
    if (!FileExists(path)) return (Texture2D){0};
    Image img = LoadImage(path);
    if (!img.data) return (Texture2D){0};
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

static bool consumeTimer(float *timer, float dt, float interval) {
    *timer -= dt;
    if (*timer > 0.0f) return false;
    *timer = interval;
    return true;
}

static void scrollRects(RectF *items, int count, float offsetY) {
    for (int i = 0; i < count; i++) items[i].y += offsetY;
}

static int compactVisible(RectF *items, int count, float limitY) {
    int write = 0;
    for (int i = 0; i < count; i++) {
        if (items[i].y < limitY) items[write++] = items[i];
    }
    return write;
}

static bool drawBorderTexture(Texture2D tex, float x, float width) {
    if (!tex.id || width <= 1.0f) return false;
    const float srcW = (float)tex.width;
    const float srcH = (float)tex.height;
    const float scale = width / srcW;
    const float tileH = srcH * scale;
    float startY = fmodf(-roadScroll, tileH);
    if (startY > 0) startY -= tileH;
    for (float y = startY; y < GetScreenHeight(); y += tileH) {
        Rectangle src = { 0, 0, srcW, srcH };
        Rectangle dst = { x, y, width, tileH };
        DrawTexturePro(tex, src, dst, (Vector2){0,0}, 0.0f, WHITE);
    }
    return true;
}

static void resetTraffic(void) {
    roadW = GetScreenWidth() * 0.45f;
    roadX = (GetScreenWidth() - roadW) * 0.5f;
    laneWidth = roadW / 3.0f;
    // Nounours plus grand et un peu plus large, centré sur la route
    player.w = 80; player.h = 84;
    player.x = roadX + (roadW - player.w) * 0.5f;
    player.y = GetScreenHeight() - 120.0f;
    speedScroll = 220.0f;
    obsCount = 0; spawnTimer = 0.0f;
    lives = 3;
    distancePixels = 0.0f;
    coinCount = 0; coinSpawnTimer = 0.0f; collectedCoins = 0;
    completionMessageTimer = 0.0f;
}

static bool intersect(const RectF *a, const RectF *b) {
    return !(a->x + a->w < b->x || b->x + b->w < a->x || a->y + a->h < b->y || b->y + b->h < a->y);
}

static RectF shrinkRect(RectF r, float padX, float padY) {
    RectF s;
    s.x = r.x + padX;
    s.y = r.y + padY;
    s.w = r.w - 2*padX;
    s.h = r.h - 2*padY;
    if (s.w < 0) s.w = 0; if (s.h < 0) s.h = 0;
    return s;
}

static void spawnObstacle(void) {
    if (obsCount >= MAX_OBS) return;
    RectF r;
    // Obstacles plus gros et position aléatoire sur toute la largeur
    r.w = 64; r.h = 84;
    int maxOffset = (int)(roadW - r.w);
    if (maxOffset < 0) maxOffset = 0;
    r.x = roadX + (float)GetRandomValue(0, maxOffset);
    r.y = -r.h - 10.0f;
    obs[obsCount++] = r;
}

static void spawnCoin(void) {
    if (coinCount >= MAX_COINS) return;
    RectF c;
    c.w = 24; c.h = 24;
    int maxOffset = (int)(roadW - c.w);
    if (maxOffset < 0) maxOffset = 0;
    c.x = roadX + (float)GetRandomValue(0, maxOffset);
    c.y = -c.h - 10.0f;
    coins[coinCount++] = c;
}

static void mg_init(void) {
    resetTraffic();
    texturesReady = false;
    roadScroll = 0.0f;
    levelCompleted = false;

    texPlayer = (Texture2D){0};
    texObstacle = (Texture2D){0};
    texRoad = (Texture2D){0};
    texCoin = (Texture2D){0};
    texBorderLeft = (Texture2D){0};
    texBorderRight = (Texture2D){0};
    for (int i=0;i<4;i++) texPlayerFrames[i] = (Texture2D){0};
    playerFrameCount = 0;
    playerFrameIndex = 0;
    playerFrameTimer = 0.0f;

    // Chargement optionnel des textures (fallback sur rectangles si absent)
    Image img = LoadImage("assets/traffic/player1.png");
    if (img.data) { texPlayerFrames[playerFrameCount++] = LoadTextureFromImage(img); UnloadImage(img); }
    img = LoadImage("assets/traffic/player2.png");
    if (img.data) { texPlayerFrames[playerFrameCount++] = LoadTextureFromImage(img); UnloadImage(img); }
    // Fallback single-frame sprite if no numbered frames present
    if (playerFrameCount == 0) {
        img = LoadImage("assets/traffic/player.png");
        if (img.data) { texPlayer = LoadTextureFromImage(img); UnloadImage(img); }
    }

    texObstacle = loadTextureIfExists("assets/traffic/obstacle1.png");
    texRoad = loadTextureIfExists("assets/traffic/road.png");
    texBorderLeft = loadTextureIfExists("assets/traffic/borduregauche.png");
    texBorderRight = loadTextureIfExists("assets/traffic/borduredroite.png");
    texCoin = loadTextureIfExists("assets/traffic/coin.png");

    texturesReady = ((playerFrameCount > 0) || (texPlayer.id != 0)) && (texObstacle.id != 0);
}

static void mg_update(float dt) {
    if (lives <= 0) {
        if (IsKeyPressed(KEY_R)) resetTraffic();
        return;
    }

    if (levelCompleted && completionMessageTimer > 0.0f) {
        completionMessageTimer -= dt;
        if (completionMessageTimer < 0.0f) completionMessageTimer = 0.0f;
        return;
    }

    // Déplacement continu sur deux axes (X et Y)
    {
        const float moveSpeedX = 260.0f;
        const float moveSpeedY = 200.0f;
        float maxX = roadX + roadW - player.w;
        float minY = 10.0f;
        float maxY = GetScreenHeight() - player.h - 10.0f;
        if (IsKeyDown(KEY_LEFT))  player.x -= moveSpeedX * dt;
        if (IsKeyDown(KEY_RIGHT)) player.x += moveSpeedX * dt;
        if (IsKeyDown(KEY_UP))    player.y -= moveSpeedY * dt;
        if (IsKeyDown(KEY_DOWN))  player.y += moveSpeedY * dt;
        if (player.x < roadX) player.x = roadX;
        if (player.x > maxX)  player.x = maxX;
        if (player.y < minY)  player.y = minY;
        if (player.y > maxY)  player.y = maxY;
    }

    // Spawns
    if (consumeTimer(&spawnTimer, dt, 0.8f)) spawnObstacle();
    if (consumeTimer(&coinSpawnTimer, dt, 1.2f)) spawnCoin(); // coins slightly less frequent

    // Update obstacles
    scrollRects(obs, obsCount, speedScroll * dt);
    scrollRects(coins, coinCount, speedScroll * dt);
    // Player animation (loops while running)
    if (playerFrameCount > 1) {
        playerFrameTimer += dt;
        while (playerFrameTimer >= playerFrameDuration) {
            playerFrameTimer -= playerFrameDuration;
            playerFrameIndex = (playerFrameIndex + 1) % playerFrameCount;
        }
    }
    // Road visual scroll (purement visuel)
    roadScroll -= speedScroll * dt;
    // Score/distance and dynamic speed increase
    distancePixels += speedScroll * dt;
    speedScroll += speedAccelPx * dt;
    if (speedScroll > maxSpeedPx) speedScroll = maxSpeedPx;
    // Remove off-screen
    obsCount = compactVisible(obs, obsCount, GetScreenHeight()+20);
    coinCount = compactVisible(coins, coinCount, GetScreenHeight()+20);

    // Collisions
    RectF pbox = shrinkRect(player, 8, 8);
    for (int i=0;i<obsCount;i++) {
        RectF obox = shrinkRect(obs[i], 10, 12);
        if (intersect(&pbox, &obox)) {
            lives -= 1;
            // knockback
            player.y += 12;
            obs[i].y = GetScreenHeight()+100; // discard
        }
    }
    for (int i=0;i<coinCount;i++) {
        RectF cbox = shrinkRect(coins[i], 4, 4);
        if (intersect(&pbox, &cbox)) {
            collectedCoins += 1;
            coins[i].y = GetScreenHeight()+100; // discard
        }
    }

    // Check level completion
    if (!levelCompleted) {
        float meters = distancePixels / pixelsPerMeter;
        if (meters >= goalMeters) levelCompleted = true;
        if (levelCompleted) completionMessageTimer = 3.0f;
    }
}

static void mg_draw(void) {
    // Route (texture répétée si disponible, sinon rectangles)
    if (texRoad.id) {
        float scale = roadW / (float)texRoad.width;
        float tileH = texRoad.height * scale;
        // point de départ pour bouclage
        float startY = fmodf(-roadScroll, tileH);
        if (startY > 0) startY -= tileH; // s'assurer de commencer au‑dessus
        for (float y = startY; y < GetScreenHeight(); y += tileH) {
            Rectangle src = { 0, 0, (float)texRoad.width, (float)texRoad.height };
            Rectangle dst = { roadX, y, roadW, tileH };
            DrawTexturePro(texRoad, src, dst, (Vector2){0,0}, 0.0f, WHITE);
        }
    } else {
        DrawRectangle((int)roadX, 0, (int)roadW, GetScreenHeight(), (Color){ 40, 40, 40, 255 });
        for (int y=-40;y<GetScreenHeight();y+=60) DrawRectangle((int)(roadX+roadW/2-4), y, 8, 30, (Color){220,220,220,180});
    }

    // Bordures latérales (textures optionnelles)
    bool drewBorder = false;
    drewBorder |= drawBorderTexture(texBorderLeft, 0.0f, roadX);
    drewBorder |= drawBorderTexture(texBorderRight, roadX + roadW, GetScreenWidth() - (roadX + roadW));
    if (!drewBorder) {
        Color borderColor = (Color){ 30, 20, 20, 255 };
        DrawRectangle((int)0, 0, (int)roadX, GetScreenHeight(), borderColor);
        DrawRectangle((int)(roadX + roadW), 0, GetScreenWidth() - (int)(roadX + roadW), GetScreenHeight(), borderColor);
    }

    // Joueur
    if (playerFrameCount > 0 && texPlayerFrames[playerFrameIndex].id) {
        Texture2D t = texPlayerFrames[playerFrameIndex];
        Rectangle src = { 0, 0, (float)t.width, (float)t.height };
        Rectangle dst = { player.x, player.y, player.w, player.h };
        DrawTexturePro(t, src, dst, (Vector2){0,0}, 0.0f, WHITE);
    } else if (texturesReady && texPlayer.id) {
        Rectangle src = { 0, 0, (float)texPlayer.width, (float)texPlayer.height };
        Rectangle dst = { player.x, player.y, player.w, player.h };
        DrawTexturePro(texPlayer, src, dst, (Vector2){0,0}, 0.0f, WHITE);
    } else {
        DrawRectangle((int)player.x, (int)player.y, (int)player.w, (int)player.h, (Color){255, 190, 80, 255});
    }

    // Obstacles
    for (int i=0;i<obsCount;i++) {
        if (texturesReady && texObstacle.id) {
            Rectangle src = { 0, 0, (float)texObstacle.width, (float)texObstacle.height };
            Rectangle dst = { obs[i].x, obs[i].y, obs[i].w, obs[i].h };
            DrawTexturePro(texObstacle, src, dst, (Vector2){0,0}, 0.0f, WHITE);
        } else {
            DrawRectangle((int)obs[i].x, (int)obs[i].y, (int)obs[i].w, (int)obs[i].h, (Color){200,80,80,255});
        }
    }

    // Coins
    for (int i=0;i<coinCount;i++) {
        if (texCoin.id) {
            Rectangle src = { 0, 0, (float)texCoin.width, (float)texCoin.height };
            Rectangle dst = { coins[i].x, coins[i].y, coins[i].w, coins[i].h };
            DrawTexturePro(texCoin, src, dst, (Vector2){0,0}, 0.0f, WHITE);
        } else {
            DrawCircle((int)(coins[i].x + coins[i].w*0.5f), (int)(coins[i].y + coins[i].h*0.5f), coins[i].w*0.5f, (Color){255, 216, 0, 255});
        }
    }

    // HUD
    DrawText(TextFormat("Vies: %d  |  Gauche/Droite pour bouger  |  R pour recommencer", lives), 20, 20, 18, LIGHTGRAY);
    // Retro-style top-right speed & distance & coins
    {
        float meters = distancePixels / pixelsPerMeter;
        float speedMs = speedScroll / pixelsPerMeter;
        const int fontSize = 20;
        const int margin = 20;
        const char *hud = TextFormat("%0.1f m  |  %0.1f m/s  |  %d", meters, speedMs, collectedCoins);
        int w = MeasureText(hud, fontSize);
        int x = GetScreenWidth() - margin - w;
        int y = 16;
        DrawText(hud, x+1, y+1, fontSize, (Color){20,20,20,180});
        DrawText(hud, x,   y,   fontSize, (Color){255, 240, 160, 255});
    }
    if (lives <= 0) DrawText("Oups! Tu as perdu. Appuie sur R pour rejouer.", 20, 60, 24, (Color){255,230,120,255});
    if (levelCompleted && completionMessageTimer > 0.0f) {
        const char *msg = "Felicitation vous avez reussi ce mini jeu";
        int fontSize = 36;
        int textWidth = MeasureText(msg, fontSize);
        int x = (GetScreenWidth() - textWidth) / 2;
        int y = GetScreenHeight() / 2 - fontSize;
        DrawText(msg, x+2, y+2, fontSize, (Color){20,20,20,200});
        DrawText(msg, x, y, fontSize, (Color){255,255,200,255});
    }
}

static void mg_unload(void) {
    if (texPlayer.id) UnloadTexture(texPlayer);
    for (int i=0;i<playerFrameCount;i++) if (texPlayerFrames[i].id) UnloadTexture(texPlayerFrames[i]);
    if (texObstacle.id) UnloadTexture(texObstacle);
    if (texRoad.id) UnloadTexture(texRoad);
    if (texCoin.id) UnloadTexture(texCoin);
    if (texBorderLeft.id) UnloadTexture(texBorderLeft);
    if (texBorderRight.id) UnloadTexture(texBorderRight);
    texturesReady = false;
}

static bool mg_isCompleted(int *coinsOut) {
    if (coinsOut) *coinsOut = collectedCoins;
    // Ne signaler la fin qu'une fois l'écran de félicitations écoulé
    return levelCompleted && completionMessageTimer <= 0.0f;
}

MinigameAPI GetMinigameTraffic(void) {
    MinigameAPI api = { mg_init, mg_update, mg_draw, mg_unload, mg_isCompleted };
    return api;
}


