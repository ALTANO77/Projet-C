// Jeu "Gateau" - version simple, claire et commentée pour un étudiant débutant.
// Remarques : remplacez ensuite les images et sons dans les chemins indiqués.
// Utilise raylib pour l'affichage et les entrées.

#include "gateau.h"     // api du minigame (déjà présent dans le projet)
#include "raylib.h"     // raylib pour fenêtre, textures, sons, entrées
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define ING_COUNT 20    // nombre d'ingrédients dans le frigo
#define DECOR_COUNT 10  // nombre d'objets de décoration
#define MAX_CAKE 20     // nombre max d'ingrédients dans le bol

// État du mini-jeu
typedef enum {
    STATE_FRIDGE_CLOSED,   // frigo fermé
    STATE_FRIDGE_OPENING,  // animation d'ouverture
    STATE_MIXING,          // on peut prendre les ingrédients et les mettre dans le bol
    STATE_DECORATING,      // phase décoration
    STATE_DONE             // fini, on peut retourner
} GameState;

// Représentation d'un ingrédient (ou décor)
typedef struct {
    int id;                 // id simple: 1..ING_COUNT pour ingrédients, 1..DECOR_COUNT pour décors
    Texture2D tex;          // texture (placeholder pour l'instant)
    Rectangle rect;         // rectangle d'affichage / interaction
    bool in_bol;            // vrai si déjà mis dans le bol (pour ingrédients)
    bool is_dragging;       // vrai si en cours de drag
    float offset_x;         // décalage souris -> coin pour drag
    float offset_y;
} Item;

// Sons (optionnel : chargez vos fichiers .wav/.ogg dans les chemins indiqués)
static Sound s_open = {0};
static Sound s_pick = {0};
static Sound s_drop = {0};
static Sound s_decor = {0};

// Éléments du jeu
static Item ingredients[ING_COUNT];   // tableau d'ingrédients
static Item decors[DECOR_COUNT];      // tableau de décors
static Item cake_items[MAX_CAKE];     // ingrédients déposés dans le bol (copie)
static int cake_count = 0;            // nombre d'ingrédients dans le bol

// Etats et variables
static GameState state = STATE_FRIDGE_CLOSED; // état courant
static float fridge_open = 0.0f;    // animation ouverture 0.0..1.0
static int score = 0;               // score du joueur
static Rectangle fridge_rect;       // zone du frigo
static Rectangle bowl_rect;         // zone du bol (cible)
static Rectangle score_rect;        // panneau score
static Font font_default;           // police par défaut (optionnel)
// Dimensions dynamiques pour mise à l'échelle 1080p
static int ing_w = 48;              // largeur texture ingrédient (calculée)
static int ing_h = 48;              // hauteur texture ingrédient (calculée)
static int decor_w = 36;            // largeur texture décor (calculée)
static int decor_h = 36;            // hauteur texture décor (calculée)

// fonctions internes
static void init_textures_and_items(void);
static void unload_textures_and_sounds(void);
static bool point_in_rect(Vector2 p, Rectangle r);
static bool rects_overlap(Rectangle a, Rectangle b);
static bool is_good_ingredient_combination(int id); // règle simple pour "bon gâteau"

// API minigame
static void mg_init(void) {
    // initialisation audio et police
    InitAudioDevice();                    // initialise l'audio (safe si pas de son)
    font_default = GetFontDefault();      // police par défaut

    // charger sons si vous avez des fichiers :
    // s_open = LoadSound("assets/sfx/fridge_open.wav");
    // s_pick = LoadSound("assets/sfx/pick.wav");
    // s_drop = LoadSound("assets/sfx/drop.wav");
    // s_decor = LoadSound("assets/sfx/decor.wav");
    // Pour l'instant on laisse les Sound vides ; PlaySound fera rien si id invalide.

    // définir rectangles d'interface en fonction de la résolution courante
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    // Zones principales (adaptées pour 1920x1080 mais proportionnelles)
    fridge_rect = (Rectangle){ sw * 0.02f, sh * 0.08f, sw * 0.22f, sh * 0.80f };  // frigo à gauche
    bowl_rect   = (Rectangle){ sw * 0.32f, sh * 0.44f, sw * 0.36f, sh * 0.28f };  // bol au centre
    score_rect  = (Rectangle){ sw * 0.72f, sh * 0.06f, sw * 0.24f, sh * 0.20f };  // panneau score à droite

    // Calcul des tailles d'icônes d'ingrédients (5 colonnes x 4 lignes)
    float slot_w = (fridge_rect.width - 20.0f) / 5.0f;
    float slot_h = (fridge_rect.height - 20.0f - 3.0f * 6.0f) / 4.0f; // 4 lignes, 3 espacements verticaux
    ing_w = (int)(slot_w - 12.0f);
    ing_h = (int)(slot_h - 12.0f);
    if (ing_h > ing_w) ing_h = ing_w; // garder carré si possible

    // Tailles des décors
    decor_w = (int)(sw * 0.025f);
    decor_h = decor_w;

    // initialiser items (textures de substitution et positions)
    init_textures_and_items();

    // état initial
    state = STATE_FRIDGE_CLOSED;
    fridge_open = 0.0f;
    score = 0;
    cake_count = 0;
}

static void mg_update(float dt) {
    // dt non utilisé de façon critique ici, mais on le garde pour animation
    (void)dt;

    Vector2 mouse = GetMousePosition(); // position souris

    // gestion ouverture du frigo : si fermé et clic sur frigo -> ouvrir
    if (state == STATE_FRIDGE_CLOSED) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && point_in_rect(mouse, fridge_rect)) {
            state = STATE_FRIDGE_OPENING;
            // PlaySound(s_open); // jouer sons si chargés
        }
    }

    // animer ouverture
    if (state == STATE_FRIDGE_OPENING) {
        fridge_open += 4.0f * dt; // vitesse d'ouverture
        if (fridge_open >= 1.0f) {
            fridge_open = 1.0f;
            state = STATE_MIXING; // on passe à la phase de mélange
        }
    }

    // mise à jour des items (drag & drop)
    // ingrédients dans le frigo (uniquement accessibles si frigo ouvert)
    for (int i = 0; i < ING_COUNT; ++i) {
        Item *it = &ingredients[i];

        // si frigo fermé, ne pas autoriser drag
        if (state != STATE_MIXING && state != STATE_DECORATING) {
            it->is_dragging = false;
            continue;
        }

        // début du drag : clic sur item
        if (!it->is_dragging) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && point_in_rect(mouse, it->rect) && !it->in_bol) {
                it->is_dragging = true;
                it->offset_x = mouse.x - it->rect.x;
                it->offset_y = mouse.y - it->rect.y;
                // PlaySound(s_pick);
            }
        } else {
            // on suit la souris tant que bouton maintenu
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                it->rect.x = mouse.x - it->offset_x;
                it->rect.y = mouse.y - it->offset_y;
            } else {
                // relâché : vérifier où on a lâché
                it->is_dragging = false;
                // si on lâche dans le bol, on ajoute au cake
                if (rects_overlap(it->rect, bowl_rect)) {
                    if (!it->in_bol && cake_count < MAX_CAKE) {
                        // marquer comme dans le bol et ajouter au cake_items
                        it->in_bol = true;
                        cake_items[cake_count++] = *it;
                        // mise à jour du score simple :
                        if (is_good_ingredient_combination(it->id)) {
                            score += 20; // bon ingrédient -> +20
                        } else {
                            score -= 10; // mauvais ingrédient -> -10
                        }
                        // PlaySound(s_drop);
                    }
                } else {
                    // si pas dans bol, on peut remettre l'ingrédient dans sa place d'origine
                    // pour simplicité, on réinitialise la position d'affichage (à gauche)
                    // la position d'origine est stockée via id -> calculez la grille
                    int col = i % 5;
                    int row = i / 5;
                    float slot_w = (fridge_rect.width - 20) / 5.0f;
                    float slot_h = (fridge_rect.height - 20.0f - 3.0f * 6.0f) / 4.0f;
                    it->rect.x = fridge_rect.x + 10 + col * slot_w;
                    it->rect.y = fridge_rect.y + 10 + row * (slot_h + 6);
                }
            }
        }
    }

    // décorations : phase DECORATING (après mélange)
    for (int i = 0; i < DECOR_COUNT; ++i) {
        Item *d = &decors[i];

        if (state != STATE_DECORATING) {
            d->is_dragging = false;
            continue;
        }

        if (!d->is_dragging) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && point_in_rect(mouse, d->rect)) {
                d->is_dragging = true;
                d->offset_x = mouse.x - d->rect.x;
                d->offset_y = mouse.y - d->rect.y;
                // PlaySound(s_pick);
            }
        } else {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                d->rect.x = mouse.x - d->offset_x;
                d->rect.y = mouse.y - d->offset_y;
            } else {
                // relâché : on laisse la décoration où elle est (libre placement)
                d->is_dragging = false;
                // PlaySound(s_decor);
            }
        }
    }

    // Transition : lorsque on a mis des ingrédients, l'utilisateur peut appuyer sur Enter pour passer à décorer
    if (state == STATE_MIXING) {
        if (IsKeyPressed(KEY_ENTER)) {
            state = STATE_DECORATING;
        }
    }

    // Fin : dans la phase décoration, l'utilisateur appuie sur BACKSPACE pour quitter le mini-jeu
    if (state == STATE_DECORATING || state == STATE_DONE) {
        if (IsKeyPressed(KEY_BACKSPACE)) {
            state = STATE_DONE;
        }
    }
}

static void mg_draw(void) {
    // dessin simple, clair et commenté

    // 1) Fond et panneaux
    ClearBackground(RAYWHITE); // fond blanc pâle

    // 2) Frigo (à gauche) : dessiner la porte (qui s'ouvre via fridge_open)
    DrawRectangleRec(fridge_rect, LIGHTGRAY); // corps du frigo

    // porte : on dessine un rectangle qui se décale selon fridge_open
    float door_w = fridge_rect.width * (0.95f); // largeur de la porte
    Rectangle door = { fridge_rect.x, fridge_rect.y, door_w, fridge_rect.height };
    // animation : on "fait glisser" la porte vers la gauche en augmentant fridge_open
    door.x = fridge_rect.x - door_w * (1.0f - fridge_open);
    DrawRectangleRec(door, DARKGRAY);
    DrawText("FRIGO", (int)(fridge_rect.x + 10), (int)(fridge_rect.y + 6), 20, WHITE);

    // si frigo ouvert, afficher ingrédients (grille)
    if (fridge_open >= 0.999f) {
        for (int i = 0; i < ING_COUNT; ++i) {
            Item *it = &ingredients[i];
            // dessiner texture (ou rectangle de couleur)
            DrawTextureRec(it->tex, (Rectangle){0,0,(float)it->tex.width,(float)it->tex.height}, (Vector2){it->rect.x, it->rect.y}, WHITE);
            // affichage du numéro d'ingrédient pour repère
            char label[8];
            sprintf(label, "%d", it->id);
            DrawText(label, (int)(it->rect.x + 6), (int)(it->rect.y + 6), 12, BLACK);
            // si l'ingrédient est déjà dans le bol, on marque avec un petit X
            if (it->in_bol) {
                DrawText("OK", (int)(it->rect.x + it->rect.width - 24), (int)(it->rect.y + 6), 12, GREEN);
            }
        }
    } else {
        // si fermé, afficher un message simple
        DrawText("Cliquez sur le frigo pour ouvrir", (int)(fridge_rect.x + 10), (int)(fridge_rect.y + fridge_rect.height - 30), 12, BLACK);
    }

    // 3) Bol (centre)
    DrawRectangleRec(bowl_rect, BEIGE);
    DrawText("Bol", (int)(bowl_rect.x + 8), (int)(bowl_rect.y + 6), 18, BROWN);

    // dessiner ingrédients déposés dans le bol (petites pastilles)
    for (int i = 0; i < cake_count; ++i) {
        // positionner en grille dans le bol
        float cell_w = ing_w * 0.75f;
        float cell_h = ing_h * 0.75f;
        int cols = (int)(bowl_rect.width / (cell_w + 6));
        int r = i / cols;
        int c = i % cols;
        float x = bowl_rect.x + 10 + c * (cell_w + 6);
        float y = bowl_rect.y + 36 + r * (cell_h + 6);
        DrawTextureRec(cake_items[i].tex, (Rectangle){0,0,(float)cake_items[i].tex.width,(float)cake_items[i].tex.height}, (Vector2){x,y}, WHITE);
    }

    // bouton pour terminer mélange (indication)
    DrawText("Appuyez sur ENTRER pour décorer", (int)(bowl_rect.x + 6), (int)(bowl_rect.y + bowl_rect.height - 22), 12, DARKGRAY);

    // 4) Décorations (en haut centre) : toujours affichées, actives seulement en DECORATING
    DrawText("Décors (glisser sur le gâteau)", 300, 10, 14, DARKGRAY);
    for (int i = 0; i < DECOR_COUNT; ++i) {
        Item *d = &decors[i];
        DrawTextureRec(d->tex, (Rectangle){0,0,(float)d->tex.width,(float)d->tex.height}, (Vector2){d->rect.x, d->rect.y}, WHITE);
    }

    // 5) Panneau score (droite)
    DrawRectangleRec(score_rect, LIGHTGRAY);
    DrawText("SCORE", (int)score_rect.x + 10, (int)score_rect.y + 6, 20, BLACK);
    char buf[64];
    sprintf(buf, "Points : %d", score);
    DrawText(buf, (int)score_rect.x + 10, (int)score_rect.y + 36, 16, DARKBLUE);
    sprintf(buf, "Ingrédients : %d", cake_count);
    DrawText(buf, (int)score_rect.x + 10, (int)score_rect.y + 60, 14, DARKBLUE);

    // 6) Indications d'aide
    DrawText("Backspace : retour au menu", 540, 140, 12, DARKGRAY);
    DrawText("EN : passer au décor", 540, 160, 12, DARKGRAY);

    // si le frigo est en cours d'ouverture on peut dessiner une transition (optionnel)
    if (state == STATE_FRIDGE_OPENING) {
        // simple barre d'animation
        DrawRectangle((int)(fridge_rect.x + fridge_rect.width + 6), (int)(fridge_rect.y + 10), (int)(20 * fridge_open), 12, SKYBLUE);
    }
}

static void mg_unload(void) {
    unload_textures_and_sounds();
    CloseAudioDevice();
}

/* ----- fonctions utilitaires ----- */

// crée des textures de substitution (Images colorées) et initialise positions
static void init_textures_and_items(void) {
    // générer 20 textures colorées simples pour ingrédients (dimensionnées dynamiquement)
    for (int i = 0; i < ING_COUNT; ++i) {
        int w = ing_w;
        int h = ing_h;
        // couleur différente selon l'id (simple variation)
        Color c = ColorFromHSV((i * 18) % 360, 0.6f, 0.9f);
        Image img = GenImageColor(w, h, c);        // image unie
        // dessiner un cercle blanc au centre pour variation visuelle
        ImageDrawCircle(&img, w/2, h/2, 14, Fade(WHITE, 0.7f));
        Texture2D tex = LoadTextureFromImage(img);
        UnloadImage(img);
        ingredients[i].tex = tex;
        ingredients[i].id = i + 1;
        ingredients[i].in_bol = false;
        ingredients[i].is_dragging = false;
        // placer en grille dans le frigo (5 colonnes)
        int col = i % 5;
        int row = i / 5;
        float slot_w = (fridge_rect.width - 20) / 5.0f;
        float slot_h = (fridge_rect.height - 20.0f - 3.0f * 6.0f) / 4.0f;
        ingredients[i].rect.width = (float)w;
        ingredients[i].rect.height = (float)h;
        ingredients[i].rect.x = fridge_rect.x + 10 + col * slot_w;
        ingredients[i].rect.y = fridge_rect.y + 10 + row * (slot_h + 6);
        ingredients[i].rect.x += 6; // petit offset
        ingredients[i].rect.y += 6;
    }

    // Remplacer l'ingrédient 1 par l'image chocolat si disponible
    // Chemin attendu: assets/gateau/chocolat.png
    {
        Image img = LoadImage("assets/gateau/chocolat.png");
        if (img.data) {
            // Libérer la texture générée pour l'ingrédient 1
            UnloadTexture(ingredients[0].tex);
            // Redimensionner l'image pour s'adapter à la case du frigo
            ImageResize(&img, ing_w, ing_h);
            ingredients[0].tex = LoadTextureFromImage(img);
            UnloadImage(img);
        }
    }

    // générer textures pour décors (dimensionnés dynamiquement)
    for (int i = 0; i < DECOR_COUNT; ++i) {
        int w = decor_w;
        int h = decor_h;
        Color c = ColorFromHSV((i * 36) % 360, 0.7f, 0.95f);
        Image img = GenImageColor(w, h, c);
        // ajout d'un petit motif
        ImageDrawCircle(&img, 8, 8, 6, Fade(WHITE, 0.9f));
        Texture2D tex = LoadTextureFromImage(img);
        UnloadImage(img);
        decors[i].tex = tex;
        decors[i].id = i + 1;
        decors[i].in_bol = false;
        decors[i].is_dragging = false;
        // position initiale en haut, organisé en ligne
        decors[i].rect.width = (float)w;
        decors[i].rect.height = (float)h;
        decors[i].rect.x = bowl_rect.x + (i * (w + 12));
        decors[i].rect.y = bowl_rect.y - (h + 24);
    }
}

// libération des textures et sons
static void unload_textures_and_sounds(void) {
    for (int i = 0; i < ING_COUNT; ++i) {
        UnloadTexture(ingredients[i].tex);
    }
    for (int i = 0; i < DECOR_COUNT; ++i) {
        UnloadTexture(decors[i].tex);
    }
    // si vous avez chargé des sons, déchargez-les :
    // UnloadSound(s_open);
    // UnloadSound(s_pick);
    // UnloadSound(s_drop);
    // UnloadSound(s_decor);
}

// utilitaire : point dans rectangle
static bool point_in_rect(Vector2 p, Rectangle r) {
    return (p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height);
}

// utilitaire : rectangles se chevauchent
static bool rects_overlap(Rectangle a, Rectangle b) {
    return (a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y);
}

// règle simple : retourne vrai si l'ingrédient est "bon"
// ici on définit une petite liste d'ids "bons" pour simuler la combinaison
static bool is_good_ingredient_combination(int id) {
    // ingrédients "bons" (exemple) : {2,5,7,11,13}
    const int good[] = {2,5,7,11,13};
    for (size_t i = 0; i < sizeof(good)/sizeof(good[0]); ++i) {
        if (good[i] == id) return true;
    }
    return false;
}

// fonction d'export pour l'API du projet
MinigameAPI GetMinigameGateau(void) {
    MinigameAPI api = { mg_init, mg_update, mg_draw, mg_unload };
    return api;
}


