#include "pousse_pousse.h"
#include "raylib.h"
#include <stdlib.h>
#include <time.h>

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
}

// appelé à chaque frame pour gérer les clics
static void PoussePousse_Update(float dt) {
    (void)dt;
    if (!s_initialized || s_finished) return;

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
            move_tile(&s_board, row, col);
        }
    }

    if (is_solved(&s_board)) {
        s_finished = true;
    }
}

// appelé à chaque frame pour dessiner
static void PoussePousse_Draw(void) {
    if (!s_initialized) return;

    int tileSize, offX, offY;
    GetBoardLayout(&tileSize, &offX, &offY);
    int boardSize = tileSize * SIZE;

    DrawText("Puzzle pousse-pousse : clique sur une case voisine du vide",
             60, 40, 24, RAYWHITE);
    DrawText("Retour (Backspace) pour quitter", 60, 70, 20, LIGHTGRAY);

    for (int row = 0; row < SIZE; ++row) {
        for (int col = 0; col < SIZE; ++col) {
            int x = offX + col * tileSize;
            int y = offY + row * tileSize;

            DrawRectangleLines(x, y, tileSize, tileSize, DARKGRAY);

            int v = s_board.grid[row][col];
            if (v != 0) {
                const char *txt = TextFormat("%d", v);
                int fontSize = tileSize / 2;
                int tw = MeasureText(txt, fontSize);
                int tx = x + (tileSize - tw) / 2;
                int ty = y + (tileSize - fontSize) / 2;
                DrawText(txt, tx, ty, fontSize, BLACK);
            }
        }
    }

    if (s_finished) {
        DrawText("Bravo ! Puzzle resolu : tu as gagne 1 piece.",
                 60, GetScreenHeight() - 80, 24, (Color){ 120, 230, 140, 255 });
    }
}

// appelé à la fin du mini-jeu
static void PoussePousse_Unload(void) {
    s_initialized = false;
}

// renvoie true quand le mini-jeu est termine, et indique le nombre de pieces
static bool PoussePousse_IsCompleted(int *coins) {
    if (!s_finished) return false;
    if (coins) *coins = 1;   // récompense : 1 pièce
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