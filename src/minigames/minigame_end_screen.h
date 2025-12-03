#ifndef MINIGAME_END_SCREEN_H
#define MINIGAME_END_SCREEN_H

#include "raylib.h"
#include <stdbool.h>
#include <stdio.h>

// Structure pour gérer l'écran de fin
typedef struct {
    bool wantsToExit;
    bool wantsToReplay;
} EndScreenState;

// Fonction pour mettre à jour l'écran de fin (gestion des clics sur les boutons)
static inline void UpdateEndScreen(EndScreenState *state, bool won, bool lost) {
    if (!won && !lost) return;
    
    // Gestion de Backspace pour retourner
    if (IsKeyPressed(KEY_BACKSPACE)) {
        state->wantsToExit = true;
        return;
    }
    
    // Gestion des boutons de fin
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 m = GetMousePosition();
        int cx = GetScreenWidth()/2;
        int cy = GetScreenHeight()/2;
        int by = cy + 50;
        
        // Bouton Rejouer
        Rectangle replayBtn = {cx - 120, by, 100, 40};
        if (CheckCollisionPointRec(m, replayBtn)) {
            state->wantsToReplay = true;
            return;
        }
        
        // Bouton Retour
        Rectangle backBtn = {cx + 20, by, 100, 40};
        if (CheckCollisionPointRec(m, backBtn)) {
            state->wantsToExit = true;
            return;
        }
    }
}

// Fonction pour dessiner l'écran de fin
static inline void DrawEndScreen(bool won, bool lost, int coins) {
    if (!won && !lost) return;
    
    int cx = GetScreenWidth()/2;
    int cy = GetScreenHeight()/2;
    int modalW = 450;
    int modalH = 220;
    Rectangle modal = {cx - modalW/2, cy - modalH/2, modalW, modalH};
    
    // Rectangle modal avec ombre légère
    DrawRectangle(cx - modalW/2 + 3, cy - modalH/2 + 3, modalW, modalH, (Color){0, 0, 0, 50});
    DrawRectangleRec(modal, (Color){250, 250, 250, 255});
    DrawRectangleLinesEx(modal, 4, (Color){80, 80, 80, 255});
    
    int msgY = cy - 60;
    int by = cy + 50;
    
    if (won) {
        char msg[50];
        snprintf(msg, sizeof(msg), "Bravo ! Tu as gagne %d piece%s !", coins, coins > 1 ? "s" : "");
        DrawText("BRAVO !", cx - MeasureText("BRAVO !", 40)/2, msgY, 40, GREEN);
        DrawText(msg, cx - MeasureText(msg, 24)/2, msgY + 45, 24, DARKGRAY);
    } else {
        DrawText("PERDU !", cx - MeasureText("PERDU !", 40)/2, msgY, 40, RED);
        if (coins > 0) {
            char msg[50];
            snprintf(msg, sizeof(msg), "Tu as gagne %d piece%s !", coins, coins > 1 ? "s" : "");
            DrawText(msg, cx - MeasureText(msg, 24)/2, msgY + 45, 24, DARKGRAY);
        }
    }
    
    // Boutons
    Rectangle replayBtn = {cx - 120, by, 100, 40};
    Rectangle backBtn = {cx + 20, by, 100, 40};
    Vector2 m = GetMousePosition();
    
    Color replayColor = CheckCollisionPointRec(m, replayBtn) ? (Color){100, 200, 100, 255} : (Color){50, 150, 50, 255};
    Color backColor = CheckCollisionPointRec(m, backBtn) ? (Color){200, 100, 100, 255} : (Color){150, 50, 50, 255};
    
    DrawRectangleRec(replayBtn, replayColor);
    DrawText("Rejouer", cx - 100, by + 12, 20, WHITE);
    DrawRectangleRec(backBtn, backColor);
    DrawText("Retour", cx + 40, by + 12, 20, WHITE);
}

#endif // MINIGAME_END_SCREEN_H

