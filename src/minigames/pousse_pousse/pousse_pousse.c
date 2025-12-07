#include "pousse_pousse.h"
#include "minigame_end_screen.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

// Puzzle 5x5 = 25 tuiles
#define TILE_COUNT 25

// --------------------------------------------------
// LOGIQUE DU PUZZLE 5x5 (plus de case vide)
// --------------------------------------------------

// Met le plateau dans l'état solution : 1..25
void init_solved(Board *b) {
    int val = 1;
    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            b->grid[i][j] = val++;   // 1..25
        }
    }
    b->empty_row = -1;
    b->empty_col = -1;
}

static void swap_cells(Board *b, int r1, int c1, int r2, int c2) {
    int tmp = b->grid[r1][c1];
    b->grid[r1][c1] = b->grid[r2][c2];
    b->grid[r2][c2] = tmp;
}

// Mélange en permutant aléatoirement les 25 cases (Fisher-Yates)
// Signature simplifiée : plus de moves_count inutile
void shuffle_board(Board *b) {
    // Initialisation de base
    init_solved(b);

    srand((unsigned int)time(NULL));

    int total = SIZE * SIZE;

    for (int k = total - 1; k > 0; --k) {
        int r1 = k / SIZE;
        int c1 = k % SIZE;

        int j = rand() % (k + 1);
        int r2 = j / SIZE;
        int c2 = j % SIZE;

        swap_cells(b, r1, c1, r2, c2);
    }

    // Si par hasard le puzzle est déjà résolu, on échange deux cases
    if (is_solved(b)) {
        swap_cells(b, 0, 0, 0, 1);
    }
}

void init_random_board(Board *b) {
    shuffle_board(b); // Appel simplifié sans le 0
}

// Renvoie 1 si la case (row,col) est dans la grille
// (Gardé car utilisé dans PoussePousse_Update)
int can_move_tile(const Board *b, int row, int col) {
    (void)b; // On garde celui-ci car 'b' est dans la signature mais pas utilisé pour la vérif de bornes
    if (row < 0 || row >= SIZE || col < 0 || col >= SIZE) return 0;
    return 1;
}

// Test si le puzzle est dans l'ordre 1..25
int is_solved(const Board *b) {
    int val = 1;
    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            if (b->grid[i][j] != val++) return 0;
        }
    }
    return 1;
}

// --------------------------------------------------
// PARTIE MINI-JEU RAYLIB (intégration avec MinigameAPI)
// --------------------------------------------------

// état interne du mini-jeu
static Board s_board;
static bool  s_initialized = false;
static bool  s_finished = false;
static Texture2D s_tileTextures[TILE_COUNT];   // 25 images
static bool s_tilesLoaded = false;
static int s_moveCount = 0;   // nombre de coups joués
static Texture2D s_bgTexture;            // la texture de fond
static bool      s_bgLoaded = false;     // est-ce qu'on l'a chargée ?
static EndScreenState s_endScreen = {0};

// état du drag & drop
static bool   s_dragging    = false;
static int    s_dragRow     = -1;
static int    s_dragCol     = -1;
static int    s_dragValue   = 0;     // numéro de la tuile (1..25)
static Vector2 s_dragOffset = {0};   // pour garder la même position relative à la souris

// calcule la taille et la position du plateau en fonction de la fenêtre
static void GetBoardLayout(int *tileSize, int *offsetX, int *offsetY) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int minSide = (sw < sh ? sw : sh);

    int t = minSide / (SIZE + 2);   // un peu de marge autour
    if (t < 40) t = 40;
    int boardPixels = t * SIZE;

    *tileSize = t;
    *offsetX  = (sw - boardPixels) / 2;
    *offsetY  = (sh - boardPixels) / 2;
}

// appelé quand on entre dans le mini-jeu
static void PoussePousse_Init(void) {
    init_random_board(&s_board);
    s_finished    = false;
    s_initialized = true;
    s_moveCount   = 0;
    s_endScreen.wantsToExit   = false;
    s_endScreen.wantsToReplay = false;
    s_dragging    = false;
    s_dragRow     = -1;
    s_dragCol     = -1;
    s_dragValue   = 0;

    // Charger les 25 images si pas déjà fait
    if (!s_tilesLoaded) {
        for (int i = 0; i < TILE_COUNT; i++) {
            char path[64];
            snprintf(path, sizeof(path), "assets/pousse_pousse/parfaite%d.png", i+1);
            Image img = LoadImage(path);

            if (img.data != NULL) {
                s_tileTextures[i] = LoadTextureFromImage(img);
                UnloadImage(img);
            } else {
                TraceLog(LOG_WARNING, "Image manquante : %s", path);
            }
        }
        s_tilesLoaded = true;
    }

    // CHARGEMENT DU FOND 
    if (!s_bgLoaded) {
        Image bg = LoadImage("assets/pousse_pousse/fond.png");  
        if (bg.data != NULL) {
            s_bgTexture = LoadTextureFromImage(bg);
            UnloadImage(bg);
            s_bgLoaded = true;
        } else {
            TraceLog(LOG_WARNING, "Fond pousse_pousse introuvable : assets/pousse_pousse/fond.png");
        }
    }
}

// appelé à chaque frame pour gérer les interactions
static void PoussePousse_Update(float dt) {
    (void)dt; // OBLIGATOIRE : Contrat MinigameAPI respecté
    
    if (!s_initialized) return;

    // Si le puzzle est terminé, gérer l'écran de fin
    if (s_finished) {
        UpdateEndScreen(&s_endScreen, true, false);
        if (s_endScreen.wantsToReplay) {
            init_random_board(&s_board);
            s_finished = false;
            s_moveCount = 0;
            s_endScreen.wantsToReplay = false;
            s_endScreen.wantsToExit   = false;
            s_dragging  = false;
            s_dragRow   = -1;
            s_dragCol   = -1;
            s_dragValue = 0;
        } else {
            return; 
        }
    }

    int tileSize, offX, offY;
    GetBoardLayout(&tileSize, &offX, &offY);

    int mx = GetMouseX();
    int my = GetMouseY();

    int boardSize = tileSize * SIZE;
    bool insideBoard =
        (mx >= offX && mx < offX + boardSize &&
         my >= offY && my < offY + boardSize);

    int col = -1, row = -1;
    if (insideBoard) {
        col = (mx - offX) / tileSize;
        row = (my - offY) / tileSize;
    }

    // Démarrage du drag
    if (!s_dragging && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && insideBoard) {
        if (can_move_tile(&s_board, row, col)) {
            s_dragging  = true;
            s_dragRow   = row;
            s_dragCol   = col;
            s_dragValue = s_board.grid[row][col];

            s_dragOffset.x = offX + col * tileSize - mx;
            s_dragOffset.y = offY + row * tileSize - my;
        }
    }

    // Fin du drag
    if (s_dragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        if (insideBoard) {
            int dstRow = row;
            int dstCol = col;

            if (dstRow >= 0 && dstRow < SIZE &&
                dstCol >= 0 && dstCol < SIZE) {

                // si on lâche sur une autre case -> échange
                if (!(dstRow == s_dragRow && dstCol == s_dragCol)) {
                    int tmp = s_board.grid[dstRow][dstCol];
                    s_board.grid[dstRow][dstCol]     = s_dragValue;
                    s_board.grid[s_dragRow][s_dragCol] = tmp;
                    s_moveCount++;
                }
            }
        }

        s_dragging  = false;
        s_dragRow   = -1;
        s_dragCol   = -1;
        s_dragValue = 0;
    }

    if (is_solved(&s_board)) {
        s_finished = true;
    }
}

// appelé à chaque frame pour dessiner
static void PoussePousse_Draw(void) {
    if (!s_initialized) return;

    // DESSIN DU FOND D'ÉCRAN
    if (s_bgLoaded) {
        Rectangle src = { 0, 0, (float)s_bgTexture.width, (float)s_bgTexture.height };
        Rectangle dst = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
        DrawTexturePro(s_bgTexture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    } else {
        ClearBackground((Color){ 30, 34, 46, 255 });
    }

    int tileSize, offX, offY;
    GetBoardLayout(&tileSize, &offX, &offY);

    DrawText("Puzzle 5x5 : fais glisser une piece (drag & drop) pour l'echanger",
             60, 40, 24, RAYWHITE);
    DrawText("Retour (Backspace) pour quitter", 60, 70, 20, LIGHTGRAY);
    DrawText(TextFormat("Coups : %d", s_moveCount),
             60, 100, 20, RAYWHITE);
    
    // === Boucle qui dessine les 25 cases ===
    Vector2 mouse = GetMousePosition();

    for (int row = 0; row < SIZE; ++row) {
        for (int col = 0; col < SIZE; ++col) {

            int x = offX + col * tileSize;
            int y = offY + row * tileSize;
            int v = s_board.grid[row][col]; // valeur 1..25

            // Si cette case est celle actuellement en drag, on la dessinera plus tard
            if (s_dragging && row == s_dragRow && col == s_dragCol)
                continue;

            if (s_tilesLoaded && v >= 1 && v <= TILE_COUNT) {
                float scale = (float)tileSize / s_tileTextures[v-1].width;
                DrawTextureEx(
                    s_tileTextures[v-1],
                    (Vector2){ x, y },
                    0.0f,
                    scale,
                    WHITE
                );
                DrawRectangleLines(x, y, tileSize, tileSize, BLACK); // contour
            } else {
                const char *txt = TextFormat("%d", v);
                int fontSize = tileSize / 2;
                int tw = MeasureText(txt, fontSize);
                DrawRectangle(x, y, tileSize, tileSize, GRAY);
                DrawText(txt, x + (tileSize - tw)/2, y + (tileSize - fontSize)/2, fontSize, BLACK);
                DrawRectangleLines(x, y, tileSize, tileSize, BLACK);
            }
        }
    }

    // Dessiner la tuile en cours de drag au-dessus des autres
    if (s_dragging && s_dragValue >= 1 && s_dragValue <= TILE_COUNT && s_tilesLoaded) {
        float scale = (float)tileSize / s_tileTextures[s_dragValue - 1].width;
        Vector2 pos = {
            mouse.x + s_dragOffset.x,
            mouse.y + s_dragOffset.y
        };
        DrawTextureEx(
            s_tileTextures[s_dragValue - 1],
            pos,
            0.0f,
            scale,
            WHITE
        );
        DrawRectangleLines((int)pos.x, (int)pos.y, tileSize, tileSize, YELLOW);
    }

    // Écran de fin
    if (s_finished) {
        DrawEndScreen(true, false, 15);
    }
}

// appelé à la fin du mini-jeu
static void PoussePousse_Unload(void) {
    if (s_tilesLoaded) {
        for (int i = 0; i < TILE_COUNT; i++) {
            UnloadTexture(s_tileTextures[i]);
        }
        s_tilesLoaded = false;
    }
    if (s_bgLoaded) {
        UnloadTexture(s_bgTexture);
        s_bgLoaded = false;
    }
    s_initialized = false;
}

// renvoie true quand le mini-jeu est termine, et indique le nombre de pieces
static bool PoussePousse_IsCompleted(int *coins) {
    if (!s_finished || !s_endScreen.wantsToExit) return false;
    if (coins) *coins = 15;   // récompense : 15 pièces
    return true;
}

// fonction utilisée par main.c pour récupérer les callbacks
MinigameAPI GetMinigamePoussePousse(void) {
    MinigameAPI api = (MinigameAPI){0};
    api.init        = PoussePousse_Init;
    api.update      = PoussePousse_Update;
    api.draw        = PoussePousse_Draw;
    api.unload      = PoussePousse_Unload;
    api.isCompleted = PoussePousse_IsCompleted;
    return api;
}