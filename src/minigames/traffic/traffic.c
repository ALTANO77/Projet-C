#include "traffic.h"
#include "minigame_end_screen.h"
#include <stdbool.h>
#include <math.h>

// 1- Partie définitions

// Structure rectangle flottant pour gérer positions / tailles
typedef struct { float x, y, w, h; } RectF;

// Variables globales du minijeu
static RectF player;          // Joueur
static float roadX, roadW;    // Position et largeur de la route
static float speedScroll;     // Vitesse verticale (défilement)
static float laneWidth;       // Largeur de voie (non utilisé ici)
static int lives;             // Nombre de vies

// Gestion distance et vitesse
static float distancePixels;      // Distance parcourue en pixels
static float pixelsPerMeter = 48.0f; // Conversion px → m
static float speedAccelPx = 18.0f;   // Accélération progressive px/s²
static float maxSpeedPx   = 1200.0f; // Vitesse maximale
static float goalMeters   = 1000.0f; // Distance à parcourir pour gagner

static bool levelCompleted;
static float completionMessageTimer;
static bool gameLost;

// Écran de fin (victoire/défaite)
static EndScreenState s_endScreen = {0};


// 2- Textures et animation du joueur
static Texture2D texPlayer;         
static Texture2D texPlayerFrames[4]; // Frames d’animation
static int playerFrameCount;
static int playerFrameIndex;
static float playerFrameTimer;       
static float playerFrameDuration = 0.12f; // 8 FPS

static Texture2D texObstacle;
static Texture2D texRoad;
static Texture2D texBorderLeft;
static Texture2D texBorderRight;
static Texture2D texCoin;

static bool texturesReady;
static float roadScroll;     // défilement visuel de la route

// 3- Obstacles
#define MAX_OBS 32
static RectF obs[MAX_OBS];
static int obsCount;
static float spawnTimer;

// 4- Pièces, comme les obstacles
#define MAX_COINS 64
static RectF coins[MAX_COINS];
static int coinCount;
static float coinSpawnTimer;
static int collectedCoins;

// Chargement d'un texture si elle existe (pour eviter erreurs)
static Texture2D loadTextureIfExists(const char *path) {
    if (!FileExists(path)) return (Texture2D){0}; // Fichier manquant donne une texture vide
    Image img = LoadImage(path);
    if (!img.data) return (Texture2D){0};
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

// Timer cyclique : true quand le timer est écoulé
static bool consumeTimer(float *timer, float dt, float interval) {
    *timer -= dt;
    if (*timer > 0.0f) return false;
    *timer = interval;
    return true;
}


// Défilement Y d’un tableau de Rectangles (pour les obstacles et les pièces)
static void scrollRects(RectF *items, int count, float offsetY) {
    for (int i = 0; i < count; i++)
        items[i].y += offsetY;
}

// Garde uniquement les éléments visibles de l'écran (pour les performances !)
static int compactVisible(RectF *items, int count, float limitY) {
    int write = 0;
    for (int i = 0; i < count; i++)
        if (items[i].y < limitY)
            items[write++] = items[i];
    return write;
}

// Bordures : répéter une texture verticalement pour les bordures de la route
static bool drawBorderTexture(Texture2D tex, float x, float width) {
    if (!tex.id || width <= 1.0f) return false;

    float srcW = tex.width;
    float srcH = tex.height;
    float scale = width / srcW;
    float tileH = srcH * scale;

    float startY = fmodf(-roadScroll, tileH);
    if (startY > 0) startY -= tileH;

    for (float y = startY; y < GetScreenHeight(); y += tileH) {
        DrawTexturePro(
            tex,
            (Rectangle){0,0,srcW,srcH},
            (Rectangle){x,y,width,tileH},
            (Vector2){0,0},
            0.0f,
            WHITE
        );
    }
    return true;
}

// Reset complet du jeu (pour le rejouer)
static void resetTraffic(void) {
    roadW = GetScreenWidth() * 0.45f;     // Route = 45% de la largeur écran
    roadX = (GetScreenWidth() - roadW) * 0.5f;
    laneWidth = roadW / 3.0f;

    // Joueur centré en bas
    player.w = 80; player.h = 84;
    player.x = roadX + (roadW - player.w) * 0.5f;
    player.y = GetScreenHeight() - 120.0f;

    speedScroll = 220.0f;

    obsCount = 0; spawnTimer = 0.0f;
    coinCount = 0; coinSpawnTimer = 0.0f;

    lives = 3;
    collectedCoins = 0;
    distancePixels = 0.0f;

    completionMessageTimer = 0.0f;
    gameLost = false;
    levelCompleted = false;

    // Reset écran de fin
    s_endScreen.wantsToExit = false;
    s_endScreen.wantsToReplay = false;
}

// Test collision entre deux objets
static bool intersect(const RectF *a, const RectF *b) {
    return !(
        a->x + a->w < b->x ||
        b->x + b->w < a->x ||
        a->y + a->h < b->y ||
        b->y + b->h < a->y
    );
}

// Code pour réuidre facilement la taille d'un rectangle (pour les hitboxes pendant le debug)
static RectF shrinkRect(RectF r, float padX, float padY) {
    r.x += padX;
    r.y += padY;
    r.w -= 2 * padX;
    r.h -= 2 * padY;
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}


// Spawn obstacle aléatoire
static void spawnObstacle(void) {
    if (obsCount >= MAX_OBS) return;

    RectF r;
    r.w = 64; r.h = 84;
    int maxOffset = (int)(roadW - r.w);
    if (maxOffset < 0) maxOffset = 0;

    r.x = roadX + GetRandomValue(0, maxOffset);
    r.y = -r.h - 10.0f;

    obs[obsCount++] = r;
}

// Spawn pièce
static void spawnCoin(void) {
    if (coinCount >= MAX_COINS) return;

    RectF c;
    c.w = 24; c.h = 24;
    int maxOffset = (int)(roadW - c.w);
    if (maxOffset < 0) maxOffset = 0;

    c.x = roadX + GetRandomValue(0, maxOffset);
    c.y = -c.h - 10.0f;

    coins[coinCount++] = c;
}

////////////////////////
////////////////////////

// INIT : chargement chargement du minijeu
static void mg_init(void) {
    resetTraffic(); //reset qu'on a vu avant

    texturesReady = false;
    roadScroll = 0.0f;

    // Reset des textures
    texPlayer = (Texture2D){0};
    texObstacle = (Texture2D){0};
    texRoad = (Texture2D){0};
    texCoin = (Texture2D){0};
    texBorderLeft = (Texture2D){0};
    texBorderRight = (Texture2D){0};
    for (int i=0;i<4;i++) texPlayerFrames[i]=(Texture2D){0};

    // Reset des frames du joueur
    playerFrameCount = 0;
    playerFrameIndex = 0;
    playerFrameTimer = 0.0f;

    // Chargement frames du joueur (anim)
    Image img = LoadImage("assets/traffic/player1.png");
    if (img.data) { texPlayerFrames[playerFrameCount++] = LoadTextureFromImage(img); UnloadImage(img); }

    img = LoadImage("assets/traffic/player2.png");
    if (img.data) { texPlayerFrames[playerFrameCount++] = LoadTextureFromImage(img); UnloadImage(img); }

    // Fallback frame unique (pour quand on avait qu'une texture de nounours)
    if (playerFrameCount == 0) {
        img = LoadImage("assets/traffic/player.png");
        if (img.data) { texPlayer = LoadTextureFromImage(img); UnloadImage(img); }
    }

    // Chargement des textures des obstacles, de la route, des bordures et des pièces
    texObstacle = loadTextureIfExists("assets/traffic/obstacle1.png");
    texRoad     = loadTextureIfExists("assets/traffic/road.png");
    texBorderLeft  = loadTextureIfExists("assets/traffic/borduregauche.png");
    texBorderRight = loadTextureIfExists("assets/traffic/borduredroite.png");
    texCoin        = loadTextureIfExists("assets/traffic/coin.png");

    // Si le joueur + obstacle existants → textures OK
    texturesReady = ((playerFrameCount > 0) || texPlayer.id) && texObstacle.id;
}

// UPDATE : logique du jeu (update de toutes les variables et des collisions)
// pour chaque frame, on va tout surveiller
static void mg_update(float dt) {

    // --- Gestion fin de partie ---
    if (lives <= 0 && !gameLost)
        gameLost = true;

    if (gameLost || levelCompleted) {
        bool won = levelCompleted;
        UpdateEndScreen(&s_endScreen, won, !won);

        // Si joueur veut rejouer -> reset du jeu
        if (s_endScreen.wantsToReplay) {
            resetTraffic();
            s_endScreen.wantsToReplay = false;
            s_endScreen.wantsToExit = false;
        } else {
            return;
        }
    }

 
    // Gestion déplacement joueur

    // Vitesse de déplacement
    const float moveSpeedX = 360.0f;
    const float moveSpeedY = 300.0f;

    float maxX = roadX + roadW - player.w;
    float minY = 10.0f;
    float maxY = GetScreenHeight() - player.h - 10.0f;

    if (IsKeyDown(KEY_LEFT))  player.x -= moveSpeedX * dt;
    if (IsKeyDown(KEY_RIGHT)) player.x += moveSpeedX * dt;
    if (IsKeyDown(KEY_UP))    player.y -= moveSpeedY * dt;
    if (IsKeyDown(KEY_DOWN))  player.y += moveSpeedY * dt;

    // Limites
    if (player.x < roadX) player.x = roadX;
    if (player.x > maxX)  player.x = maxX;
    if (player.y < minY)  player.y = minY;
    if (player.y > maxY)  player.y = maxY;

    // Spawns périodiques (des obstacles et des pièces)
    if (consumeTimer(&spawnTimer, dt, 0.8f)) spawnObstacle();
    if (consumeTimer(&coinSpawnTimer, dt, 1.2f)) spawnCoin();

    // Défilement des éléments
    scrollRects(obs, obsCount, speedScroll * dt);
    scrollRects(coins, coinCount, speedScroll * dt);

    // Animation du joueur
    if (playerFrameCount > 1) {
        playerFrameTimer += dt;
        while (playerFrameTimer >= playerFrameDuration) {
            playerFrameTimer -= playerFrameDuration;
            playerFrameIndex = (playerFrameIndex + 1) % playerFrameCount;
        }
    }

    // Scroll visuel route
    roadScroll -= speedScroll * dt;

    // Distance + accélération
    distancePixels += speedScroll * dt;
    speedScroll += speedAccelPx * dt;
    if (speedScroll > maxSpeedPx) speedScroll = maxSpeedPx;

    // Suppression off-screen
    obsCount = compactVisible(obs, obsCount, GetScreenHeight()+20);
    coinCount = compactVisible(coins, coinCount, GetScreenHeight()+20);

    // CollisionS
    RectF pbox = shrinkRect(player, 8, 8);

    // Collisions avec les obstacles
    for (int i=0;i<obsCount;i++) {
        RectF obox = shrinkRect(obs[i], 10, 12);
        if (intersect(&pbox, &obox)) {
            lives -= 1;
            player.y += 12;                  // petit recul
            obs[i].y = GetScreenHeight()+100; // supprimer obstacle
        }
    }

    // Collisions avec les pièces
    for (int i=0;i<coinCount;i++) {
        RectF cbox = shrinkRect(coins[i], 4, 4);
        if (intersect(&pbox, &cbox)) {
            collectedCoins += 1;
            coins[i].y = GetScreenHeight()+100;
        }
    }

    // Vérification victoire
    float meters = distancePixels / pixelsPerMeter;
    if (!levelCompleted && meters >= goalMeters)
        levelCompleted = true;
}

// DRAW : rendu graphique
static void mg_draw(void) {

    // Dessin route
    if (texRoad.id) {
        float scale = roadW / texRoad.width;
        float tileH = texRoad.height * scale;

        float startY = fmodf(-roadScroll, tileH);
        if (startY > 0) startY -= tileH;

        for (float y=startY; y < GetScreenHeight(); y+=tileH)
            DrawTexturePro(texRoad,
                (Rectangle){0,0,texRoad.width,texRoad.height},
                (Rectangle){roadX,y,roadW,tileH},
                (Vector2){0,0},0,WHITE);
    }
    else {
        DrawRectangle(roadX,0,roadW,GetScreenHeight(),(Color){40,40,40,255});
    }

    // Bordures route
    bool drewBorder = false;
    drewBorder |= drawBorderTexture(texBorderLeft, 0.0f, roadX);
    drewBorder |= drawBorderTexture(texBorderRight, roadX + roadW, GetScreenWidth() - (roadX + roadW));

    // Si pas de textures -> couleur unie
    if (!drewBorder) {
        Color bc = (Color){30,20,20,255};
        DrawRectangle(0,0,roadX,GetScreenHeight(),bc);
        DrawRectangle(roadX+roadW,0,GetScreenWidth()-(roadX+roadW),GetScreenHeight(),bc);
    }

    // Dessin du joueur
    if (playerFrameCount > 0) {
        Texture2D t = texPlayerFrames[playerFrameIndex];
        DrawTexturePro(
            t,
            (Rectangle){0,0,t.width,t.height},
            (Rectangle){player.x,player.y,player.w,player.h},
            (Vector2){0,0},0,WHITE
        );
    }
    else if (texPlayer.id) {
        DrawTexturePro(
            texPlayer,
            (Rectangle){0,0,texPlayer.width,texPlayer.height},
            (Rectangle){player.x,player.y,player.w,player.h},
            (Vector2){0,0},0,WHITE
        );
    }
    else {
        DrawRectangle(player.x,player.y,player.w,player.h,(Color){255,190,80,255});
    }

    // Obstacles
    for (int i=0;i<obsCount;i++) {
        if (texObstacle.id)
            DrawTexturePro(texObstacle,
                (Rectangle){0,0,texObstacle.width,texObstacle.height},
                (Rectangle){obs[i].x,obs[i].y,obs[i].w,obs[i].h},
                (Vector2){0,0},0,WHITE);
        else
            DrawRectangle(obs[i].x,obs[i].y,obs[i].w,obs[i].h,(Color){200,80,80,255});
    }

    // Pièces
    for (int i=0;i<coinCount;i++) {
        if (texCoin.id)
            DrawTexturePro(texCoin,
                (Rectangle){0,0,texCoin.width,texCoin.height},
                (Rectangle){coins[i].x,coins[i].y,coins[i].w,coins[i].h},
                (Vector2){0,0},0,WHITE);
        else
            DrawCircle(
                coins[i].x + coins[i].w*0.5f,
                coins[i].y + coins[i].h*0.5f,
                coins[i].w*0.5f,
                (Color){255,216,0,255}
            );
    }

    // HUD pour que le joueur sache ce qu'il fait et son statut
    DrawText(TextFormat("Vies: %d", lives), 20,20,18,LIGHTGRAY);

    float meters = distancePixels / pixelsPerMeter;
    float speedMs = speedScroll / pixelsPerMeter;
    const char *hud = TextFormat("%0.1f m | %0.1f m/s | %d", meters, speedMs, collectedCoins);
    int w = MeasureText(hud, 20);
    DrawText(hud, GetScreenWidth() - w - 20, 16, 20, (Color){255,240,160,255});

    // Écran de fin
    if (gameLost || levelCompleted)
        DrawEndScreen(levelCompleted, gameLost, levelCompleted ? collectedCoins : 0);
}

// Déchargement textures (contre les fuites de mémoire)
static void mg_unload(void) {
    if (texPlayer.id) UnloadTexture(texPlayer);
    for (int i=0;i<playerFrameCount;i++)
        if (texPlayerFrames[i].id)
            UnloadTexture(texPlayerFrames[i]);

    if (texObstacle.id) UnloadTexture(texObstacle);
    if (texRoad.id)     UnloadTexture(texRoad);
    if (texCoin.id)     UnloadTexture(texCoin);
    if (texBorderLeft.id)  UnloadTexture(texBorderLeft);
    if (texBorderRight.id) UnloadTexture(texBorderRight);

    texturesReady = false;
}

// Fonction API : renvoie si le minijeu est fini pour le menu principal avec le statut de victoire du jeu etle nombre de pièces
static bool mg_isCompleted(int *coinsOut) {
    if (gameLost || levelCompleted) {
        if (coinsOut)
            *coinsOut = levelCompleted ? collectedCoins : 0;
        return s_endScreen.wantsToExit;
    }
    return false;
}

// API publique du minijeu
MinigameAPI GetMinigameTraffic(void) {
    MinigameAPI api = { mg_init, mg_update, mg_draw, mg_unload, mg_isCompleted };
    return api;
}
