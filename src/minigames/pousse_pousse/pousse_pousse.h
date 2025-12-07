#ifndef POUSSE_POUSSE_H
#define POUSSE_POUSSE_H

#include "minigame.h"

// Puzzle classique 5x5
#define SIZE 5

// Plateau du puzzle
typedef struct {
    int grid[SIZE][SIZE]; // valeurs 1..25
    // réutilisés pour stocker la pièce en drag
    int empty_row;        // = drag_row
    int empty_col;        // = drag_col
} Board;

// ---- logique du puzzle ----
void init_solved(Board *b);
void shuffle_board(Board *b); // Paramètre 'moves_count' supprimé
void init_random_board(Board *b);
int  can_move_tile(const Board *b, int row, int col);
int  is_solved(const Board *b);
// Note : move_tile et move_tile_by_number ont été supprimés car inutiles.

// ---- intégration moteur
MinigameAPI GetMinigamePoussePousse(void);

#endif