#include "pendu.h"
#include "raylib.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

static const char *WORDS_2[] = {"NOUNOURS", "COUVERTURE", "OREILLER", "BISOU", "BONBON", "COUETTE", "SOMMEIL", "TENDRESSE", "MOUSTACHE", "AVENTURE", "LUMIERE", "ETOILE", "SOURIRE"};
static const char *WORDS_5[] = {"MOLLETONNAGE", "EFFILOCHAGE", "REMBOURRAGE", "SCINTILLEMENT", "LUMINESCENCE", "SOMNOLENCE", "EMERVEILLEMENT", "APAISEMENT", "BIENVEILLANCE"};
static const char *DEFS_2[] = {
    "une peluche en forme d'ours, souvent tres douce, que les enfants gardent pour dormir ou pour se rassurer quand ils ont peur.",
    "un grand tissu chaud que l'on met sur soi pour ne pas avoir froid, surtout la nuit dans le lit.",
    "un coussin moelleux sur lequel on pose sa tete pour etre bien installe quand on dort.",
    "un petit geste d'affection que l'on fait avec la bouche pour montrer qu'on aime quelqu'un ou pour reconforter.",
    "une petite gourmandise sucre de differentes formes et couleurs qu'on mange pour se faire plaisir.",
    "une grosse couverture bien remplie et toute douce qui garde bien la chaleur quand on dort.",
    "le moment ou le corps et le cerveau se reposent, ou on ferme les yeux et on dort pour reprendre de l'energie.",
    "un sentiment doux et chaleureux qu'on ressent quand on aime quelqu'un et qu'on veut etre gentil avec lui.",
    "des poils qui poussent au-dessus de la bouche, comme on peut en voir chez certains adultes.",
    "une situation nouvelle, excitante ou surprenante ou il se passe des choses qu'on ne connait pas encore.",
    "ce qui eclaire tout autour de nous, que ce soit le soleil, une lampe ou une flamme, et qui nous permet de voir.",
    "un petit point lumineux que l'on voit dans le ciel la nuit et qui brille tres fort meme si elle est tres loin.",
    "une expression du visage quand on releve les levres pour montrer qu'on est content, heureux ou qu'on veut etre gentil."
};
static const char *DEFS_5[] = {
    "une facon de rendre un tissu tres doux, epais et moelleux, comme ceux qu'on trouve dans les doudous.",
    "quand un tissu ou un morceau de fil commence a se defaire et se separe en plein de petits fils.",
    "la matiere douce et legere qu'on met a l'interieur des peluches, des coussins ou des oreillers pour qu'ils soient confortables.",
    "une lumiere qui clignote ou brille par petites etincelles, comme une etoile ou une bougie.",
    "une lumiere speciale qui apparait sans chaleur, comme dans certains jouets fluorescents qui brillent dans le noir.",
    "un etat ou on se sent tres fatigue, ou les yeux se ferment tout seuls et ou on est presque en train de s'endormir.",
    "un sentiment de grande surprise et de joie quand on voit quelque chose de tres beau ou magique.",
    "un moment ou l'on devient calme, tranquille et ou on ne ressent plus de stress ou d'inquietude.",
    "une attitude de gentillesse ou on veut aider les autres, etre respectueux et leur faire du bien."
};
static int currentWordIndex = 0;
static char word[20], guessed[20], letters[26];
static int errors = 0, won = 0, lost = 0, init = 0, isHardWord = 0, wantsToExit = 0;

static void init_game(void) {
    srand((unsigned)time(NULL));
    // Choisir aléatoirement entre mots faciles (2 pièces) et difficiles (5 pièces)
    if (rand() % 2 == 0) {
        currentWordIndex = rand() % 13;
        strcpy(word, WORDS_2[currentWordIndex]);
        isHardWord = 0;
    } else {
        currentWordIndex = rand() % 9;
        strcpy(word, WORDS_5[currentWordIndex]);
        isHardWord = 1;
    }
    memset(guessed, '_', strlen(word));
    guessed[strlen(word)] = 0;
    guessed[0] = word[0]; // Afficher la première lettre
    // Révéler toutes les occurrences de la première lettre
    for (int i = 1; word[i]; i++) {
        if (word[i] == word[0]) guessed[i] = word[0];
    }
    memset(letters, 0, 26);
    // Ne pas marquer la première lettre comme utilisée pour permettre de cliquer dessus si elle apparaît plusieurs fois
    errors = won = lost = wantsToExit = 0;
    init = 1;
}

static void update_game(float dt) {
    // Gestion de Backspace pour retourner
    if (IsKeyPressed(KEY_BACKSPACE) && (won || lost)) {
        wantsToExit = 1;
        return;
    }
    
    // Gestion des boutons de fin
    if (won || lost) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 m = GetMousePosition();
            int cx = GetScreenWidth()/2;
            int by = GetScreenHeight()/2 + 100;
            
            // Bouton Rejouer
            Rectangle replayBtn = {cx - 120, by, 100, 40};
            if (CheckCollisionPointRec(m, replayBtn)) {
                init_game();
                return;
            }
            
            // Bouton Retour
            Rectangle backBtn = {cx + 20, by, 100, 40};
            if (CheckCollisionPointRec(m, backBtn)) {
                wantsToExit = 1;
                return;
            }
        }
        return;
    }
    
    // Gestion du clavier physique (AZERTY/QWERTY compatible avec GetCharPressed)
    int charKey = GetCharPressed();
    if ((charKey >= 'a' && charKey <= 'z') || (charKey >= 'A' && charKey <= 'Z')) {
        int i;
        char c;
        if (charKey >= 'a' && charKey <= 'z') {
            i = charKey - 'a';
            c = 'A' + i; // Convertir en majuscule
        } else {
            i = charKey - 'A';
            c = charKey;
        }
        
        // Vérifier si la lettre est dans le mot et s'il reste des occurrences à révéler
        int hasLetter = 0;
        int canReveal = 0;
        for (int j = 0; word[j]; j++) {
            if (word[j] == c) {
                hasLetter = 1;
                if (guessed[j] == '_') {
                    canReveal = 1;
                    break;
                }
            }
        }
        
        // Si la lettre n'a pas encore été essayée OU si elle peut encore révéler des lettres
        if (!letters[i] || (hasLetter && canReveal)) {
            if (!letters[i]) letters[i] = 1; // Marquer comme essayée
            int found = 0;
            // Révéler toutes les occurrences de cette lettre
            for (int j = 0; word[j]; j++) {
                if (word[j] == c && guessed[j] == '_') {
                    guessed[j] = c;
                    found = 1;
                }
            }
            // Si la lettre n'est pas dans le mot, compter une erreur
            if (!hasLetter) {
                errors++;
                if (errors >= 7) lost = 1;
            } else if (strcmp(word, guessed) == 0) {
                won = 1;
            }
        }
    }
    
    // Gestion du clavier visuel (souris)
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 m = GetMousePosition();
        int sx = GetScreenWidth()/2 - 200, sy = GetScreenHeight() - 100;
        
        for (int i = 0; i < 26; i++) {
            int x = sx + (i % 13) * 35, y = sy + (i / 13) * 35;
            char c = 'A' + i;
            
            if (CheckCollisionPointRec(m, (Rectangle){x, y, 30, 30})) {
                // Vérifier si la lettre est dans le mot
                int hasLetter = 0;
                int canReveal = 0;
                for (int j = 0; word[j]; j++) {
                    if (word[j] == c) {
                        hasLetter = 1;
                        if (guessed[j] == '_') {
                            canReveal = 1;
                            break;
                        }
                    }
                }
                
                // Si la lettre n'a pas encore été essayée OU si elle peut encore révéler des lettres
                if (!letters[i] || (hasLetter && canReveal)) {
                    if (!letters[i]) letters[i] = 1; // Marquer comme essayée
                    int found = 0;
                    // Révéler toutes les occurrences de cette lettre
                    for (int j = 0; word[j]; j++) {
                        if (word[j] == c && guessed[j] == '_') {
                            guessed[j] = c;
                            found = 1;
                        }
                    }
                    // Si la lettre n'est pas dans le mot, compter une erreur
                    if (!hasLetter) {
                        errors++;
                        if (errors >= 7) lost = 1;
                    } else if (strcmp(word, guessed) == 0) {
                        won = 1;
                    }
                    break;
                }
            }
        }
    }
}

static void draw_game(void) {
    if (!init) return;
    ClearBackground(RAYWHITE);
    DrawText("PENDU", GetScreenWidth()/2 - 50, 30, 40, BLACK);
    
    // Pendu simple
    int cx = GetScreenWidth()/2, y = 100;
    if (errors > 0) DrawLine(cx-50, y+100, cx+50, y+100, BLACK);
    if (errors > 1) DrawLine(cx, y, cx, y+100, BLACK);
    if (errors > 2) DrawLine(cx, y, cx+50, y, BLACK);
    if (errors > 3) DrawLine(cx+50, y, cx+50, y+30, BLACK);
    if (errors > 4) DrawCircle(cx+50, y+50, 20, BLACK);
    if (errors > 5) DrawLine(cx+50, y+70, cx+50, y+90, BLACK);
    if (errors > 6) { DrawLine(cx+50, y+80, cx+30, y+60, BLACK); DrawLine(cx+50, y+80, cx+70, y+60, BLACK); }
    
    // Mot
    int len = strlen(guessed);
    for (int i = 0; i < len; i++) {
        char s[2] = {guessed[i], 0};
        DrawText(s, cx - len*15 + i*30, 250, 40, BLUE);
    }
    
    // Clavier
    for (int i = 0; i < 26; i++) {
        int x = GetScreenWidth()/2 - 200 + (i % 13) * 35;
        int y = GetScreenHeight() - 100 + (i / 13) * 35;
        Color c = letters[i] ? (strchr(word, 'A'+i) ? GREEN : RED) : GRAY;
        DrawRectangle(x, y, 30, 30, c);
        char s[2] = {'A' + i, 0};
        DrawText(s, x+8, y+5, 20, BLACK);
    }
    
    // Afficher la définition à gauche si 5 erreurs ou si gagné
    if (errors >= 5 || won) {
        const char *def = isHardWord ? DEFS_5[currentWordIndex] : DEFS_2[currentWordIndex];
        int defX = 20;
        int defY = 200;
        int defW = 300;
        int defH = 250;
        int fontSize = 16;
        int lineHeight = 22;
        int maxWidth = defW - 20;
        
        // Fond pour la définition à gauche
        DrawRectangle(defX, defY, defW, defH, (Color){255, 255, 200, 230});
        DrawRectangleLines(defX, defY, defW, defH, (Color){200, 150, 50, 255});
        
        // Titre
        DrawText("Definition:", defX + 10, defY + 10, 22, DARKBLUE);
        
        // Définition sur plusieurs lignes
        int x = defX + 10;
        int y = defY + 40;
        const char *text = def;
        int textLen = strlen(text);
        int currentPos = 0;
        
        while (currentPos < textLen && y < defY + defH - 20) {
            // Trouver la position de la prochaine coupure (espace ou fin de ligne)
            int lineEnd = currentPos;
            int lineWidth = 0;
            
            // Chercher le prochain espace qui dépasse la largeur
            while (lineEnd < textLen && text[lineEnd] != '\0') {
                char testChar = text[lineEnd];
                int charWidth = MeasureText(&testChar, fontSize);
                
                if (lineWidth + charWidth > maxWidth && lineEnd > currentPos) {
                    // Revenir en arrière pour trouver le dernier espace
                    int spacePos = lineEnd;
                    while (spacePos > currentPos && text[spacePos] != ' ') spacePos--;
                    if (spacePos > currentPos) lineEnd = spacePos + 1;
                    break;
                }
                
                lineWidth += charWidth;
                lineEnd++;
                if (text[lineEnd - 1] == ' ' || text[lineEnd - 1] == '\0') break;
            }
            
            // Afficher la ligne
            char line[200];
            int lineLen = lineEnd - currentPos;
            if (lineLen > 199) lineLen = 199;
            strncpy(line, text + currentPos, lineLen);
            line[lineLen] = '\0';
            DrawText(line, x, y, fontSize, BLACK);
            
            y += lineHeight;
            currentPos = lineEnd;
            // Passer les espaces en début de ligne suivante
            while (currentPos < textLen && text[currentPos] == ' ') currentPos++;
        }
    }
    
    // Rectangle modal pour les boutons de fin
    if (won || lost) {
        int cx = GetScreenWidth()/2;
        int cy = GetScreenHeight()/2;
        int modalW = 400;
        int modalH = 200;
        Rectangle modal = {cx - modalW/2, cy - modalH/2, modalW, modalH};
        
        // Fond semi-transparent
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 180});
        
        // Rectangle modal
        DrawRectangleRec(modal, (Color){240, 240, 240, 255});
        DrawRectangleLinesEx(modal, 3, (Color){100, 100, 100, 255});
        
        int msgY = cy - 60;
        int by = cy + 40;
        
        if (won) {
            char msg[50];
            int coins = isHardWord ? 5 : 2;
            snprintf(msg, sizeof(msg), "Bravo ! Tu as gagne %d piece%s !", coins, coins > 1 ? "s" : "");
            DrawText("BRAVO !", cx - MeasureText("BRAVO !", 40)/2, msgY, 40, GREEN);
            DrawText(msg, cx - MeasureText(msg, 24)/2, msgY + 45, 24, DARKGRAY);
        } else {
            DrawText("PERDU !", cx - MeasureText("PERDU !", 40)/2, msgY, 40, RED);
            char wordMsg[50];
            snprintf(wordMsg, sizeof(wordMsg), "Le mot etait: %s", word);
            DrawText(wordMsg, cx - MeasureText(wordMsg, 24)/2, msgY + 45, 24, DARKGRAY);
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
}

static void unload_game(void) { init = 0; }

static bool is_done(int *coins) {
    // Ne retourner true que si l'utilisateur veut quitter (bouton Retour ou Backspace)
    if (wantsToExit) {
        if (coins) *coins = won ? (isHardWord ? 5 : 2) : 0;
        return 1;
    }
    return 0;
}

MinigameAPI GetMinigamePendu(void) {
    return (MinigameAPI){init_game, update_game, draw_game, unload_game, is_done};
}

