#include "pousse_pousse.h"
#include "minigame_end_screen.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define TILE_COUNT 15


// --------------------------------------------------
// LOGIQUE DU TAQUIN 4x4
// --------------------------------------------------



void init_solved(Board *b) {
    int val = 1;
    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            if (i == SIZE - 1 && j == SIZE - 1)
                b->grid[i][j] = 0; // case vide
            else
                b->grid[i][j] = val++;
        }
    }
    b->empty_row = SIZE - 1;
    b->empty_col = SIZE - 1;
}

// mélange en effectuant des mouvements valides de la case vide
void shuffle_board(Board *b, int moves_count) {
    srand((unsigned int)time(NULL));

    for (int m = 0; m < moves_count; ++m) {
        int directions[4][2] = { {-1,0},{1,0},{0,-1},{0,1} };
        int possible[4];
        int pcount = 0;

        for (int d = 0; d < 4; ++d) {
            int nr = b->empty_row + directions[d][0];
            int nc = b->empty_col + directions[d][1];
            if (nr >= 0 && nr < SIZE && nc >= 0 && nc < SIZE) {
                possible[pcount++] = d;
            }
        }

        int chosen = possible[rand() % pcount];
        int nr = b->empty_row + directions[chosen][0];
        int nc = b->empty_col + directions[chosen][1];

        int tmp = b->grid[nr][nc];
        b->grid[nr][nc] = 0;
        b->grid[b->empty_row][b->empty_col] = tmp;

        b->empty_row = nr;
        b->empty_col = nc;
    }
}

void init_random_board(Board *b) {
    init_solved(b);
    shuffle_board(b, 1000);
}

int can_move_tile(const Board *b, int row, int col) {
    if (row < 0 || row >= SIZE || col < 0 || col >= SIZE) return 0;
    if (b->grid[row][col] == 0) return 0;

    int dr = abs(row - b->empty_row);
    int dc = abs(col - b->empty_col);

    return (dr + dc == 1); // voisin orthogonal du vide
}

int move_tile(Board *b, int row, int col) {
    if (!can_move_tile(b, row, col)) return 0;

    int tmp = b->grid[row][col];
    b->grid[row][col] = 0;
    b->grid[b->empty_row][b->empty_col] = tmp;

    b->empty_row = row;
    b->empty_col = col;
    return 1;
}

int move_tile_by_number(Board *b, int number) {
    if (number < 1 || number > 15) return 0;

    int row = -1, col = -1;
    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            if (b->grid[i][j] == number) {
                row = i;
                col = j;
                break;
            }
        }
        if (row != -1) break;
    }

    if (row == -1) return 0;
    return move_tile(b, row, col);
}

int is_solved(const Board *b) {
    int val = 1;
    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            if (i == SIZE - 1 && j == SIZE - 1) {
                if (b->grid[i][j] != 0) return 0;
            } else {
                if (b->grid[i][j] != val++) return 0;
            }
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
static Texture2D s_tileTextures[TILE_COUNT];   // 15 images
static bool s_tilesLoaded = false;
static int s_moveCount = 0;   // nombre de coups joués
static Texture2D s_bgTexture;            // la texture de fond
static bool      s_bgLoaded = false;     // est-ce qu'on l'a chargée ?
// Timer pour garder l'écran de victoire visible avant retour au menu
static float     s_endTimer = 0.0f;
static EndScreenState s_endScreen = {0};

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
    s_finished   = false;
    s_initialized = true;
    s_moveCount = 0;   // on remet le compteur à zéro
    s_endTimer = 0.0f;
    s_endScreen.wantsToExit = false;
    s_endScreen.wantsToReplay = false;
    
    // Charger les 15 images si pas déjà fait
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
            s_bgTexture = LoadTextureFromImage(bg);
            UnloadImage(bg);
            s_bgLoaded = true;
        } 
        else {
            TraceLog(LOG_WARNING, "Fond pousse_pousse introuvable : assets/pousse_pousse/fond.png");
        }

}

// appelé à chaque frame pour gérer les clics
static void PoussePousse_Update(float dt) {
    if (!s_initialized) return;

    // Si le puzzle est terminé, gérer l'écran de fin
    if (s_finished) {
        UpdateEndScreen(&s_endScreen, true, false);
        if (s_endScreen.wantsToReplay) {
            init_random_board(&s_board);
            s_finished = false;
            s_moveCount = 0;
            s_endScreen.wantsToReplay = false;
            s_endScreen.wantsToExit = false;
        }
        return;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int tileSize, offX, offY;
        GetBoardLayout(&tileSize, &offX, &offY);

        int mx = GetMouseX();
        int my = GetMouseY();

        int boardSize = tileSize * SIZE;
        if (mx >= offX && mx < offX + boardSize &&
            my >= offY && my < offY + boardSize) {

            int col = (mx - offX) / tileSize;
            int row = (my - offY) / tileSize;
            if (move_tile(&s_board, row, col)) {
                s_moveCount++;
            }
        }
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
        // si pas d'image de fond, couleur de fond par défaut
        ClearBackground((Color){ 30, 34, 46, 255 });
    }

    int tileSize, offX, offY;
    GetBoardLayout(&tileSize, &offX, &offY);

    DrawText("Puzzle pousse-pousse : clique sur une case voisine du vide",
             60, 40, 24, RAYWHITE);
    DrawText("Retour (Backspace) pour quitter", 60, 70, 20, LIGHTGRAY);
    DrawText(TextFormat("Coups : %d", s_moveCount),
             60, 100, 20, RAYWHITE);
    
    // === Boucle qui dessine les 16 cases ===
    for (int row = 0; row < SIZE; ++row) {
        for (int col = 0; col < SIZE; ++col) {

            int x = offX + col * tileSize;
            int y = offY + row * tileSize;
            int v = s_board.grid[row][col]; // valeur de 0 à 15

            // --- CASE VIDE ---
            if (v == 0) {
                DrawRectangle(x, y, tileSize, tileSize, GRAY);    // fond gris
                DrawRectangleLines(x, y, tileSize, tileSize, BLACK);
                continue;
            }

            // --- CASE AVEC IMAGE (1 → parfaite1.png, 2 → parfaite2.png, ...) ---
            if (s_tilesLoaded && v >= 1 && v <= TILE_COUNT) {

                // On scale l’image pour remplir une case
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
                // Si jamais texture manquante → on affiche le numéro (sécurité)
                const char *txt = TextFormat("%d", v);
                int fontSize = tileSize / 2;
                int tw = MeasureText(txt, fontSize);
                DrawText(txt, x + (tileSize - tw)/2, y + (tileSize - fontSize)/2, fontSize, BLACK);
                DrawRectangleLines(x, y, tileSize, tileSize, BLACK);
            }
        }
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
    s_initialized = false;
}

// renvoie true quand le mini-jeu est termine, et indique le nombre de pieces
static bool PoussePousse_IsCompleted(int *coins) {
    // Ne signaler la fin que si l'utilisateur veut quitter
    if (!s_finished || !s_endScreen.wantsToExit) return false;
    if (coins) *coins = 15;   // récompense : 15 pièces
    return true;
}

// fonction utilisée par main.c pour récupérer les callbacks
MinigameAPI GetMinigamePoussePousse(void) {
    MinigameAPI api = {0};
    api.init        = PoussePousse_Init;
    api.update      = PoussePousse_Update;
    api.draw        = PoussePousse_Draw;
    api.unload      = PoussePousse_Unload;
    api.isCompleted = PoussePousse_IsCompleted;
    return api;
}