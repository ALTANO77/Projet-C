#ifndef POUSSE_POUSSE_H
#define POUSSE_POUSSE_H

#include "minigame.h"   // pour MinigameAPI

#define SIZE 4   // taquin 4x4

// Plateau du puzzle
typedef struct {
    int grid[SIZE][SIZE]; // 1..15, 0 = vide
    int empty_row;
    int empty_col;
} Board;

// ---- logique du taquin ----
void init_solved(Board *b);
void shuffle_board(Board *b, int moves_count);
void init_random_board(Board *b);
int  can_move_tile(const Board *b, int row, int col);
int  move_tile(Board *b, int row, int col);
int  move_tile_by_number(Board *b, int number);
int  is_solved(const Board *b);

// ---- intégration dans le moteur de mini-jeux ----
// (utilisé par main.c : GetMinigamePoussePousse())
MinigameAPI GetMinigamePoussePousse(void);

#endif
