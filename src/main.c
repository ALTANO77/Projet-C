// ============================================================================
// GROS NOUNOURS 2D - Jeu principal
// ============================================================================
// Ce fichier contient la logique principale du jeu :
// - Gestion des différents écrans (accueil, hub, zones, boutique, minijeux)
// - Navigation entre les zones
// - Gestion des minijeux
// - Système de pièces et boutique
// ============================================================================

#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "minigames/minigame.h"
#include "minigames/pousse_pousse/pousse_pousse.h"
#include "minigames/traffic/traffic.h"
#include "minigames/Cerisesurgateau/cerisesurgateau.h"
#include "minigames/pendu/pendu.h"
#include "minigames/boutique/boutique.h"

// ============================================================================
// TYPES ET STRUCTURES
// ============================================================================

// Énumération des différents états du jeu
// Chaque état correspond à un écran différent
typedef enum {
    STATE_TITLE = 0,        // Écran d'accueil
    STATE_HUB,              // Menu principal (hub)
    STATE_ZONE_JARDIN,      // Zone du jardin (avant le minijeu Traffic)
    STATE_ZONE_CHAMBRE,     // Zone de la chambre (avant le minijeu Puzzle)
    STATE_ZONE_GRENIER,     // Zone du grenier (avant le minijeu Pendu)
    STATE_ZONE_CUISINE,     // Zone de la cuisine (avant le minijeu Cerise)
    STATE_SHOP,             // Boutique pour acheter des tenues
    STATE_MINIJEU,          // Pendant qu'un minijeu est actif
    STATE_PAUSE             // Écran de pause
} GameState;

// Identifiants des différentes zones du jeu
typedef enum {
    ZONE_NONE = -1,     // Aucune zone active
    ZONE_JARDIN = 0,    // Zone du jardin
    ZONE_CHAMBRE,       // Zone de la chambre
    ZONE_GRENIER,       // Zone du grenier
    ZONE_CUISINE,       // Zone de la cuisine
    ZONE_COUNT          // Nombre total de zones (utilisé pour les tableaux)
} ZoneId;

// Structure pour stocker la progression d'une zone
// Indique si le minijeu de cette zone a été complété
typedef struct {
    bool completed;  // true si le minijeu de cette zone est terminé
} ZoneProgress;

// Structure pour stocker la position et taille d'un rectangle en pourcentage
// Utilisé pour positionner les portails et le nounours de manière relative
// (pour que ça s'adapte à différentes tailles d'écran)
typedef struct {
    float left;    // Position X en pourcentage (0.0 = gauche, 1.0 = droite)
    float top;     // Position Y en pourcentage (0.0 = haut, 1.0 = bas)
    float width;   // Largeur en pourcentage
    float height;  // Hauteur en pourcentage
} RectRatios;

// Structure pour stocker la position et taille du nounours
typedef struct {
    float left;       // Position X en pourcentage
    float top;        // Position Y en pourcentage
    float heightRatio; // Hauteur en pourcentage de l'écran
} BearLayout;

// Structure principale du jeu contenant toutes les données
typedef struct {
    // État actuel du jeu
    GameState state;
    int activeZone;  // Zone actuellement active (ZONE_NONE si aucune)
    
    // Progression du jeu
    int collectibles;  // Nombre de pièces collectées
    ZoneProgress progress[ZONE_COUNT];  // Progression de chaque zone
    
    // Minijeu actuel
    MinigameAPI currentMinigame;  // Interface du minijeu en cours
    bool minigameCompleted;  // true si le minijeu est complété
    
    // Textures du menu principal
    Texture2D menuBackground;  // Fond du menu principal
    Texture2D menuBear;  // Image du nounours par défaut
    bool hasMenuBackground;  // true si la texture est chargée
    bool hasMenuBear;  // true si la texture est chargée
    
    // Textures de l'écran d'accueil
    Texture2D titleBackground;  // Fond d'accueil normal
    Texture2D titleBackgroundHover;  // Fond d'accueil quand on survole la porte
    bool hasTitleBackground;
    bool hasTitleBackgroundHover;
    
    // Tenues du nounours (5 tenues différentes)
    Texture2D bearOutfits[5];  // Images du nounours avec différentes tenues
    bool hasBearOutfit[5];  // true si chaque tenue est chargée
    int currentBearOutfit;  // Index de la tenue actuelle (-1 = tenue de départ)
    
    // Fonds des zones (affichés au survol des portails)
    Texture2D zoneBackgrounds[ZONE_COUNT];
    bool hasZoneBackground[ZONE_COUNT];
    
    // Mode debug (F2 pour activer)
    bool showDebugOverlay;  // true pour afficher les rectangles des portails
    int draggingPortal;  // Index du portail en train d'être déplacé (-1 si aucun)
    bool draggingBear;  // true si on déplace le nounours
    bool draggingShopPortal;  // true si on déplace le portail de la boutique
    bool draggingTitleRect;  // true si on déplace le rectangle de l'écran d'accueil
    Vector2 dragOffset;  // Offset pour le déplacement (pour éviter les sauts)
    
    // Positions des éléments (en pourcentage pour s'adapter à la taille d'écran)
    RectRatios portalLayouts[ZONE_COUNT];  // Positions des portails des zones
    RectRatios shopPortalLayout;  // Position du portail de la boutique
    RectRatios titleRectLayout;  // Position du rectangle cliquable sur l'écran d'accueil
    BearLayout bearLayout;  // Position et taille du nounours
    
    // Détection du survol
    int hoveredPortal;  // Index du portail survolé (-1 si aucun)
    
    // Musique
    Music music;  // Musique de fond
    bool hasMusic;  // true si la musique est chargée
    bool musicMuted;  // true si la musique est en pause
    Texture2D soundOnIcon;  // Icône son activé
    Texture2D soundOffIcon;  // Icône son désactivé
    bool hasSoundOnIcon;
    bool hasSoundOffIcon;
    
    // Données de la boutique (synchronisées avec le module boutique)
    BoutiqueData boutiqueData;
    Texture2D outfits[5];  // Textures des tenues dans la boutique
    bool hasOutfit[5];
    const char *outfitNames[5];  // Noms des tenues
    int outfitPrices[5];  // Prix de chaque tenue
    bool outfitOwned[5];  // true si chaque tenue est possédée
    int selectedOutfit;  // Index de la tenue sélectionnée dans la boutique
    Texture2D outfitPreview;  // Aperçu de la tenue
    bool hasOutfitPreview;
    
    // Timer d'inactivité (pour afficher un message d'aide après 5 secondes)
    float inactivityTimer;
} Game;

// Structure pour associer une zone à son nom d'affichage
typedef struct {
    ZoneId zone;  // Identifiant de la zone
    const char *label;  // Nom à afficher
} HubPortalInfo;

// ============================================================================
// CONSTANTES
// ============================================================================

// Informations sur les portails du hub (zones cliquables)
static const HubPortalInfo HUB_PORTALS[ZONE_COUNT] = {
    { ZONE_JARDIN,  "Jardin" },
    { ZONE_CHAMBRE, "Puzzle" },
    { ZONE_GRENIER, "Bibliothèque" },
    { ZONE_CUISINE, "Cuisine" }
};

// Positions par défaut des portails (en pourcentage de l'écran)
// Format: { gauche, haut, largeur, hauteur }
static const RectRatios DEFAULT_PORTAL_LAYOUTS[ZONE_COUNT] = {
    { 0.065f, 0.25f, 0.11f, 0.30f },  // Jardin
    { 0.225f, 0.25f, 0.11f, 0.30f },  // Puzzle
    { 0.395f, 0.25f, 0.11f, 0.30f },  // Bibliothèque
    { 0.565f, 0.25f, 0.11f, 0.30f }   // Cuisine
};

// Position par défaut du portail de la boutique
static const RectRatios DEFAULT_SHOP_PORTAL = { 0.78f, 0.20f, 0.12f, 0.32f };

// Position par défaut du rectangle cliquable sur l'écran d'accueil
static const RectRatios DEFAULT_TITLE_RECT = { 0.396f, 0.315f, 0.208f, 0.370f };

// Position par défaut du nounours
static const BearLayout DEFAULT_BEAR_LAYOUT = { 0.021f, 0.113f, 0.85f };

// Clés pour sauvegarder les positions dans le fichier de configuration
static const char *PORTAL_KEYS[ZONE_COUNT] = { "jardin", "chambre", "grenier", "cuisine" };

// Fichier de configuration pour sauvegarder les positions
static const char *LAYOUT_FILE = "config/menu_layout.ini";

// Fichiers des fonds de zones
static const char *ZONE_BG_FILES[ZONE_COUNT] = {
    "assets/bg_jardin.png",
    "assets/bg_puzzle.png",
    "assets/bg_bibliotheque.png",
    "assets/bg_cuisine.png"
};

// ============================================================================
// DÉCLARATIONS DE FONCTIONS
// ============================================================================

static void prepareMinigameSession(Game *g, MinigameAPI api);
static void finalizeMinigame(Game *g);
static Rectangle computeBearRect(const Game *g);
static Rectangle computePortalRect(const Game *g, int idx);
static Rectangle computeShopPortalRect(const Game *g);
static Rectangle computeTitleRect(const Game *g);
static void clampBearToScreen(Game *g);

// ============================================================================
// FONCTIONS UTILITAIRES
// ============================================================================

// Limite une valeur entre un minimum et un maximum
// Exemple: clampf(15, 10, 20) retourne 15, clampf(5, 10, 20) retourne 10
static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Dessine un texte centré horizontalement
// txt: le texte à afficher
// y: position verticale
// size: taille de la police
// c: couleur du texte
static void drawCentered(const char *txt, int y, int size, Color c) {
    int textWidth = MeasureText(txt, size);
    int screenWidth = GetScreenWidth();
    int x = (screenWidth - textWidth) / 2;  // Centrer horizontalement
    DrawText(txt, x, y, size, c);
}

// Dessine un sol simple (utilisé si aucune texture de fond n'est chargée)
static void drawGround(void) {
    int screenHeight = GetScreenHeight();
    int screenWidth = GetScreenWidth();
    
    // Herbe (vert)
    DrawRectangle(0, 480, screenWidth, screenHeight - 480, (Color){ 60, 100, 60, 255 });
    // Terre (marron)
    DrawRectangle(0, 460, screenWidth, 20, (Color){ 90, 60, 40, 255 });
}

// Charge une texture depuis un fichier si elle existe
// Retourne une texture vide si le fichier n'existe pas
static Texture2D loadTextureIfAvailable(const char *path) {
    Texture2D tex = {0};  // Texture vide par défaut
    
    // Vérifier si le fichier existe
    if (!FileExists(path)) {
        return tex;  // Retourner une texture vide
    }
    
    // Charger l'image
    Image img = LoadImage(path);
    if (img.data) {
        // Convertir l'image en texture
        tex = LoadTextureFromImage(img);
        UnloadImage(img);  // Libérer l'image (on n'a besoin que de la texture)
    }
    
    return tex;
}

// ============================================================================
// GESTION DES MINIJEUX
// ============================================================================

// Prépare le lancement d'un minijeu
// g: structure du jeu
// api: interface du minijeu à lancer
static void prepareMinigameSession(Game *g, MinigameAPI api) {
    // Enregistrer le minijeu
    g->currentMinigame = api;
    g->minigameCompleted = false;
    
    // Initialiser le minijeu s'il a une fonction d'initialisation
    if (g->currentMinigame.init) {
        g->currentMinigame.init();
    }
    
    // Passer à l'état minijeu
    g->state = STATE_MINIJEU;
}

// Finalise un minijeu (quand on le quitte)
static void finalizeMinigame(Game *g) {
    // Marquer la zone comme complétée
    if (g->activeZone >= 0 && g->activeZone < ZONE_COUNT) {
        g->progress[g->activeZone].completed = true;
    }
    
    // Nettoyer le minijeu
    if (g->currentMinigame.unload) {
        g->currentMinigame.unload();
    }
    
    // Réinitialiser les données du minijeu
    g->currentMinigame = (MinigameAPI){0};
    g->minigameCompleted = false;
    
    // Retourner au hub
    g->state = STATE_HUB;
    g->activeZone = ZONE_NONE;
}

// ============================================================================
// GESTION DES POSITIONS (LAYOUT)
// ============================================================================

// Initialise les positions avec les valeurs par défaut
static void initDefaultLayout(Game *g) {
    // Copier les positions par défaut des portails
    for (int i = 0; i < ZONE_COUNT; ++i) {
        g->portalLayouts[i] = DEFAULT_PORTAL_LAYOUTS[i];
    }
    
    // Positions par défaut pour le nounours, la boutique et l'écran d'accueil
    g->bearLayout = DEFAULT_BEAR_LAYOUT;
    g->shopPortalLayout = DEFAULT_SHOP_PORTAL;
    g->titleRectLayout = DEFAULT_TITLE_RECT;
    g->draggingTitleRect = false;
}

// Trouve l'index d'un portail à partir de son nom
// Retourne -1 si le nom n'est pas trouvé
static int portalIndexFromName(const char *name) {
    for (int i = 0; i < ZONE_COUNT; ++i) {
        if (strcmp(name, PORTAL_KEYS[i]) == 0) {
            return i;
        }
    }
    return -1;
}

// Limite les valeurs d'un rectangle pour qu'il reste dans l'écran
static void clampPortalLayout(RectRatios *r) {
    // Limiter la taille (entre 2% et 100% de l'écran)
    r->width = clampf(r->width, 0.02f, 1.0f);
    r->height = clampf(r->height, 0.02f, 1.0f);
    
    // Limiter la position pour que le rectangle reste dans l'écran
    r->left = clampf(r->left, 0.0f, 1.0f - r->width);
    r->top = clampf(r->top, 0.0f, 1.0f - r->height);
}

// Limite les valeurs de la position du nounours
static void clampBearLayout(BearLayout *b) {
    b->heightRatio = clampf(b->heightRatio, 0.1f, 1.0f);
    b->left = clampf(b->left, 0.0f, 1.0f);
    b->top = clampf(b->top, 0.0f, 1.0f - b->heightRatio);
}

// Charge les positions depuis le fichier de configuration
// Si le fichier n'existe pas, utilise les valeurs par défaut
static void loadMenuLayout(Game *g) {
    // D'abord, initialiser avec les valeurs par défaut
    initDefaultLayout(g);
    
    // Essayer d'ouvrir le fichier de configuration
    FILE *f = fopen(LAYOUT_FILE, "r");
    if (!f) {
        return;  // Pas de fichier, on garde les valeurs par défaut
    }
    
    // Lire le fichier ligne par ligne
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Ignorer les commentaires et lignes vides
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        char key[64];
        float left, top, width, height;
        
        // Lire une position de portail
        // Format: "portal_jardin=0.065,0.25,0.11,0.30"
        if (sscanf(line, "portal_%63[^=]=%f,%f,%f,%f", key, &left, &top, &width, &height) == 5) {
            int idx = portalIndexFromName(key);
            if (idx >= 0) {
                // Portail de zone trouvé
                g->portalLayouts[idx] = (RectRatios){ left, top, width, height };
                clampPortalLayout(&g->portalLayouts[idx]);
            } else if (strcmp(key, "shop") == 0) {
                // Portail de la boutique
                g->shopPortalLayout = (RectRatios){ left, top, width, height };
                clampPortalLayout(&g->shopPortalLayout);
            }
        }
        // Lire la position du nounours
        // Format: "bear=0.021,0.113,0.85"
        else if (sscanf(line, "bear=%f,%f,%f", &left, &top, &height) == 3) {
            g->bearLayout = (BearLayout){ left, top, height };
            clampBearLayout(&g->bearLayout);
        }
    }
    
    fclose(f);
    clampBearToScreen(g);
}

// Sauvegarde les positions dans le fichier de configuration
static void saveMenuLayout(const Game *g) {
    FILE *f = fopen(LAYOUT_FILE, "w");
    if (!f) {
        return;  // Impossible d'écrire, on abandonne
    }
    
    // Sauvegarder les positions des portails de zones
    for (int i = 0; i < ZONE_COUNT; ++i) {
        const RectRatios *r = &g->portalLayouts[i];
        fprintf(f, "portal_%s=%.5f,%.5f,%.5f,%.5f\n",
                PORTAL_KEYS[i], r->left, r->top, r->width, r->height);
    }
    
    // Sauvegarder la position du portail de la boutique
    fprintf(f, "portal_shop=%.5f,%.5f,%.5f,%.5f\n",
            g->shopPortalLayout.left, g->shopPortalLayout.top,
            g->shopPortalLayout.width, g->shopPortalLayout.height);
    
    // Sauvegarder la position du nounours
    fprintf(f, "bear=%.5f,%.5f,%.5f\n",
            g->bearLayout.left, g->bearLayout.top, g->bearLayout.heightRatio);
    
    fclose(f);
}
git config --global user.name ValentinDayon
// ============================================================================
// CALCUL DES RECTANGLES
// ============================================================================

// Calcule le rectangle d'un portail de zone en pixels
// Convertit les pourcentages en coordonnées réelles
static Rectangle computePortalRect(const Game *g, int idx) {
    float screenWidth = (float)GetScreenWidth();
    float screenHeight = (float)GetScreenHeight();
    RectRatios layout = g->portalLayouts[idx];
    
    return (Rectangle){
        layout.left * screenWidth,      // X en pixels
        layout.top * screenHeight,      // Y en pixels
        layout.width * screenWidth,     // Largeur en pixels
        layout.height * screenHeight    // Hauteur en pixels
    };
}

// Calcule le rectangle du portail de la boutique
static Rectangle computeShopPortalRect(const Game *g) {
    float screenWidth = (float)GetScreenWidth();
    float screenHeight = (float)GetScreenHeight();
    RectRatios layout = g->shopPortalLayout;
    
    return (Rectangle){
        layout.left * screenWidth,
        layout.top * screenHeight,
        layout.width * screenWidth,
        layout.height * screenHeight
    };
}

// Calcule le rectangle cliquable de l'écran d'accueil
static Rectangle computeTitleRect(const Game *g) {
    float screenWidth = (float)GetScreenWidth();
    float screenHeight = (float)GetScreenHeight();
    RectRatios layout = g->titleRectLayout;
    
    return (Rectangle){
        layout.left * screenWidth,
        layout.top * screenHeight,
        layout.width * screenWidth,
        layout.height * screenHeight
    };
}

// Calcule le ratio de largeur du nounours (pour le positionnement)
static float getBearWidthRatio(const Game *g) {
    // Si pas de texture, retourner une valeur par défaut
    if (!g->hasMenuBear || g->menuBear.height == 0 || g->menuBear.width == 0) {
        return 0.2f;
    }
    
    // Calculer le ratio largeur/hauteur de l'image
    float aspect = (float)g->menuBear.width / (float)g->menuBear.height;
    
    // Prendre en compte le ratio de l'écran
    float screenRatio = (float)GetScreenHeight() / (float)GetScreenWidth();
    
    // Retourner le ratio de largeur
    return g->bearLayout.heightRatio * aspect * screenRatio;
}

// Calcule le rectangle du nounours en pixels
static Rectangle computeBearRect(const Game *g) {
    // Choisir la texture à utiliser (tenue actuelle ou par défaut)
    Texture2D bearTex = g->menuBear;
    bool hasBear = g->hasMenuBear;
    
    // Si une tenue est sélectionnée, utiliser cette image
    if (g->currentBearOutfit >= 0 && g->currentBearOutfit < 5 && g->hasBearOutfit[g->currentBearOutfit]) {
        bearTex = g->bearOutfits[g->currentBearOutfit];
        hasBear = true;
    }
    
    // Si pas de texture, retourner un rectangle vide
    if (!hasBear) {
        return (Rectangle){ 0 };
    }
    
    // Calculer les dimensions
    float screenHeight = (float)GetScreenHeight();
    float screenWidth = (float)GetScreenWidth();
    float bearHeight = g->bearLayout.heightRatio * screenHeight;
    
    // Calculer la largeur en gardant les proportions de l'image
    float aspect = bearTex.height > 0 ? (float)bearTex.width / (float)bearTex.height : 1.0f;
    float bearWidth = bearHeight * aspect;
    
    return (Rectangle){
        g->bearLayout.left * screenWidth,  // X
        g->bearLayout.top * screenHeight,   // Y
        bearWidth,                          // Largeur
        bearHeight                          // Hauteur
    };
}

// Assure que le nounours reste dans l'écran
static void clampBearToScreen(Game *g) {
    float widthRatio = clampf(getBearWidthRatio(g), 0.0f, 0.99f);
    g->bearLayout.left = clampf(g->bearLayout.left, 0.0f, 1.0f - widthRatio);
    g->bearLayout.top = clampf(g->bearLayout.top, 0.0f, 1.0f - g->bearLayout.heightRatio);
}

// ============================================================================
// AFFICHAGE
// ============================================================================

// Dessine le fond du menu principal
// Change le fond si on survole un portail (affiche le fond de la zone)
static void drawMenuBackground(const Game *g) {
    const Texture2D *tex = &g->menuBackground;
    
    // Si on survole un portail, afficher le fond de cette zone
    if (g->hoveredPortal >= 0 && g->hoveredPortal < ZONE_COUNT && g->
        hasZoneBackground[g->hoveredPortal]) {
        tex = &g->zoneBackgrounds[g->hoveredPortal];
    }
    
    // Afficher la texture si elle existe
    if (tex->id != 0) {
        Rectangle src = { 0, 0, (float)tex->width, (float)tex->height };
        Rectangle dst = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
        DrawTexturePro(*tex, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        // Sinon, dessiner un sol simple
        drawGround();
    }
}

// Dessine le nounours en gros plan
static void drawBearCloseup(const Game *g) {
    // Choisir la texture à utiliser
    Texture2D bearToDraw = g->menuBear;
    bool hasBear = g->hasMenuBear;
    
    // Si une tenue est sélectionnée, utiliser cette image
    if (g->currentBearOutfit >= 0 && g->currentBearOutfit < 5 
        && g->hasBearOutfit[g->currentBearOutfit]) {
        bearToDraw = g->bearOutfits[g->currentBearOutfit];
        hasBear = true;
    }
    
    // Si pas de texture, ne rien afficher
    if (!hasBear) {
        return;
    }
    
    // Calculer où afficher le nounours
    Rectangle dst = computeBearRect(g);
    if (dst.width <= 0 || dst.height <= 0) {
        return;  // Rectangle invalide
    }
    
    // Afficher la texture
    Rectangle src = { 0, 0, (float)bearToDraw.width, (float)bearToDraw.height };
    DrawTexturePro(bearToDraw, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
}

// Dessine le tableau montrant l'état des minijeux (terminé ou non)
static void drawMinigameStatusTable(const Game *g) {
    static const char *zoneLabels[ZONE_COUNT] = { "Jardin", "Puzzle", "Bibliothèque", "Cuisine" };
    
    // Position et taille du tableau
    const float tableX = 60.0f;
    const float tableY = 780.0f;
    const float rowHeight = 42.0f;
    const float tableWidth = 460.0f;
    const float tableHeight = rowHeight * (ZONE_COUNT + 1);
    
    // Fond du tableau (semi-transparent)
    DrawRectangleRounded((Rectangle){ tableX - 10, tableY - 20, tableWidth + 20, tableHeight + 30 },
                         0.08f, 6, (Color){ 0, 0, 0, 160 });
    
    // Titre
    DrawText("Etat des mini-jeux", (int)tableX, (int)tableY - 10, 26, RAYWHITE);
    
    // Ligne de séparation
    DrawLine(tableX, tableY + 18, tableX + tableWidth, tableY + 18, LIGHTGRAY);
    DrawText("Statut", (int)(tableX + tableWidth - 150), (int)tableY + 28, 22, LIGHTGRAY);
    
    // Afficher chaque zone
    for (int i = 0; i < ZONE_COUNT; ++i) {
        float rowY = tableY + 28 + rowHeight * (i + 1);
        const char *status = g->progress[i].completed ? "Terminée" : "Non fait";
        Color statusColor = g->progress[i].completed ? 
            (Color){ 120, 230, 140, 255 } :  // Vert si terminé
            (Color){ 255, 210, 120, 255 };   // Orange si non fait
        
        DrawText(zoneLabels[i], (int)tableX, (int)rowY, 22, RAYWHITE);
        DrawText(status, (int)(tableX + tableWidth - 150), (int)rowY, 22, statusColor);
    }
}

// Dessine le compteur de pièces en haut à droite
static void drawCoinCounter(const Game *g) {
    const char *label = TextFormat("Pieces : %d", g->collectibles);
    int fontSize = 30;
    int textWidth = MeasureText(label, fontSize);
    int padding = 18;
    
    // Calculer la position du rectangle (en haut à droite)
    Rectangle box = {
        GetScreenWidth() - textWidth - padding * 2 - 40,
        30,
        (float)textWidth + padding * 2,
        50
    };
    
    // Fond semi-transparent
    DrawRectangleRounded(box, 0.12f, 6, (Color){ 0, 0, 0, 160 });
    
    // Texte
    DrawText(label, (int)(box.x + padding), (int)(box.y + 12), fontSize, GOLD);
}

// Calcule le rectangle du bouton de musique
static Rectangle getMusicButtonRect(void) {
    float screenWidth = (float)GetScreenWidth();
    float screenHeight = (float)GetScreenHeight();
    
    // Taille proportionnelle à la fenêtre (5% du plus petit côté)
    float base = fminf(screenWidth, screenHeight);
    float size = base * 0.05f;
    
    // Limites min/max
    if (size < 32.0f) size = 32.0f;
    if (size > 96.0f) size = 96.0f;
    
    float margin = size * 0.5f;
    
    // Position en bas à droite
    return (Rectangle){
        screenWidth - size - margin,
        screenHeight - size - margin,
        size,
        size
    };
}

// Dessine le bouton pour activer/désactiver la musique
static void drawMusicButton(const Game *g) {
    if (!g->hasMusic) {
        return;  // Pas de musique, pas de bouton
    }
    
    Rectangle r = getMusicButtonRect();
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, r);
    
    // Couleurs
    Color bg = (Color){ 245, 245, 245, hover ? 255 : 230 };
    Color border = (Color){ 60, 60, 60, 220 };
    
    // Fond du bouton
    DrawRectangleRounded(r, 0.4f, 6, bg);
    DrawRectangleRoundedLines(r, 0.4f, 6, border);
    
    // Choisir l'icône à afficher
    const Texture2D *iconTex = NULL;
    if (!g->musicMuted && g->hasSoundOnIcon) {
        iconTex = &g->soundOnIcon;
    }
    if (g->musicMuted && g->hasSoundOffIcon) {
        iconTex = &g->soundOffIcon;
    }
    
    // Afficher l'icône si disponible
    if (iconTex && iconTex->id != 0) {
        Rectangle src = { 0, 0, (float)iconTex->width, (float)iconTex->height };
        float pad = r.width * 0.15f;  // Marge à l'intérieur
        Rectangle dst = {
            r.x + pad,
            r.y + pad,
            r.width - 2.0f * pad,
            r.height - 2.0f * pad
        };
        DrawTexturePro(*iconTex, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        // Sinon, dessiner un pictogramme simple
        Color iconColor = g->musicMuted ? 
            (Color){ 220, 80, 80, 255 } :    // Rouge si muet
            (Color){ 120, 220, 140, 255 };   // Vert si actif
        
        float cx = r.x + r.width * 0.5f;
        float cy = r.y + r.height * 0.5f;
        float w = r.width * 0.35f;
        float h = r.height * 0.30f;
        
        // Dessiner un haut-parleur simple
        DrawTriangle(
            (Vector2){ cx - w * 0.6f, cy - h * 0.6f },
            (Vector2){ cx - w * 0.6f, cy + h * 0.6f },
            (Vector2){ cx,            cy          },
            iconColor
        );
        DrawRectangleV(
            (Vector2){ cx - w, cy - h * 0.4f },
            (Vector2){ w * 0.4f, h * 0.8f },
            iconColor
        );
        
        // Si muet, dessiner une croix
        if (g->musicMuted) {
            DrawLineEx(
                (Vector2){ cx + w * 0.2f, cy - h * 0.7f },
                (Vector2){ cx + w * 0.8f, cy + h * 0.7f },
                3.0f,
                iconColor
            );
            DrawLineEx(
                (Vector2){ cx + w * 0.8f, cy - h * 0.7f },
                (Vector2){ cx + w * 0.2f, cy + h * 0.7f },
                3.0f,
                iconColor
            );
        } else {
            // Sinon, dessiner des ondes sonores
            DrawCircleLines((int)(cx + w * 0.6f), (int)cy, (int)(h * 0.9f), iconColor);
        }
    }
}


// ============================================================================
// MODE DEBUG
// ============================================================================

// Gère le déplacement des éléments en mode debug (F2)
static void handleDebugDragging(Game *g) {
    // Si le mode debug est désactivé, réinitialiser tout
    if (!g->showDebugOverlay) {
        g->draggingPortal = -1;
        g->draggingBear = false;
        g->draggingShopPortal = false;
        g->draggingTitleRect = false;
        return;
    }
    
    Vector2 mouse = GetMousePosition();
    
    // Détecter le début du clic
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        g->draggingPortal = -1;
        g->draggingBear = false;
        g->draggingShopPortal = false;
        g->draggingTitleRect = false;
        
        // Sur l'écran d'accueil, gérer uniquement le rectangle
        if (g->state == STATE_TITLE) {
            Rectangle titleRect = computeTitleRect(g);
            if (CheckCollisionPointRec(mouse, titleRect)) {
                g->draggingTitleRect = true;
                g->dragOffset = (Vector2){ mouse.x - titleRect.x, mouse.y - titleRect.y };
            }
        } else {
            // Dans le hub, gérer les portails, la boutique et le nounours
            // Vérifier les portails de zones
            for (int i = 0; i < ZONE_COUNT; ++i) {
                Rectangle rect = computePortalRect(g, i);
                if (CheckCollisionPointRec(mouse, rect)) {
                    g->draggingPortal = i;
                    g->dragOffset = (Vector2){ mouse.x - rect.x, mouse.y - rect.y };
                    break;
                }
            }
            
            // Vérifier le portail de la boutique
            if (g->draggingPortal == -1) {
                Rectangle shopRect = computeShopPortalRect(g);
                if (CheckCollisionPointRec(mouse, shopRect)) {
                    g->draggingShopPortal = true;
                    g->dragOffset = (Vector2){ mouse.x - shopRect.x, mouse.y - shopRect.y };
                }
            }
            
            // Vérifier le nounours
            if (!g->draggingShopPortal && g->draggingPortal == -1 && g->hasMenuBear) {
                Rectangle bearRect = computeBearRect(g);
                if (bearRect.width > 0 && CheckCollisionPointRec(mouse, bearRect)) {
                    g->draggingBear = true;
                    g->dragOffset = (Vector2){ mouse.x - bearRect.x, mouse.y - bearRect.y };
                }
            }
        }
    }
    
    // Pendant le déplacement
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float screenWidth = (float)GetScreenWidth();
        float screenHeight = (float)GetScreenHeight();
        
        if (g->draggingTitleRect) {
            // Déplacer le rectangle de l'écran d'accueil
            RectRatios *layout = &g->titleRectLayout;
            layout->left = (mouse.x - g->dragOffset.x) / screenWidth;
            layout->top = (mouse.y - g->dragOffset.y) / screenHeight;
            clampPortalLayout(layout);
        } else if (g->draggingPortal >= 0) {
            // Déplacer un portail de zone
            RectRatios *layout = &g->portalLayouts[g->draggingPortal];
            layout->left = (mouse.x - g->dragOffset.x) / screenWidth;
            layout->top = (mouse.y - g->dragOffset.y) / screenHeight;
            clampPortalLayout(layout);
        } else if (g->draggingShopPortal) {
            // Déplacer le portail de la boutique
            RectRatios *layout = &g->shopPortalLayout;
            layout->left = (mouse.x - g->dragOffset.x) / screenWidth;
            layout->top = (mouse.y - g->dragOffset.y) / screenHeight;
            clampPortalLayout(layout);
        } else if (g->draggingBear) {
            // Déplacer le nounours
            float widthRatio = clampf(getBearWidthRatio(g), 0.0f, 0.99f);
            g->bearLayout.left = clampf((mouse.x - g->dragOffset.x) / screenWidth, 0.0f, 1.0f - widthRatio);
            g->bearLayout.top = clampf((mouse.y - g->dragOffset.y) / screenHeight, 0.0f, 1.0f - g->bearLayout.heightRatio);
        }
    }
    
    // Fin du clic
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        g->draggingPortal = -1;
        g->draggingBear = false;
        g->draggingShopPortal = false;
        g->draggingTitleRect = false;
    }
}

// Affiche les informations de debug (rectangles des portails, position de la souris)
static void drawDebugOverlay(const Game *g) {
    if (!g->showDebugOverlay) {
        return;
    }
    
    Vector2 mouse = GetMousePosition();
    
    // Sur l'écran d'accueil
    if (g->state == STATE_TITLE) {
        const int panelWidth = 400;
        const int panelHeight = 80;
        
        // Fond du panneau
        DrawRectangle(30, 90, panelWidth, panelHeight, (Color){ 0, 0, 0, 160 });
        DrawRectangleLines(30, 90, panelWidth, panelHeight, (Color){ 255, 255, 255, 80 });
        
        // Position de la souris
        DrawText(TextFormat("Mouse: %.0f, %.0f", mouse.x, mouse.y), 40, 100, 20, RAYWHITE);
        
        // Rectangle de l'écran d'accueil
        Rectangle titleRect = computeTitleRect(g);
        DrawRectangleLinesEx(titleRect, 2.0f, (Color){ 255, 0, 0, 120 });
        DrawText(TextFormat("Title Rect: x=%.0f y=%.0f w=%.0f h=%.0f",
                            titleRect.x, titleRect.y, titleRect.width, titleRect.height),
                 40, 130, 18, LIGHTGRAY);
    } else {
        // Dans le hub
        const int panelWidth = 400;
        const int panelHeight = 40 + (ZONE_COUNT + 1) * 24;
        
        // Fond du panneau
        DrawRectangle(30, 90, panelWidth, panelHeight, (Color){ 0, 0, 0, 160 });
        DrawRectangleLines(30, 90, panelWidth, panelHeight, (Color){ 255, 255, 255, 80 });
        
        // Position de la souris
        DrawText(TextFormat("Mouse: %.0f, %.0f", mouse.x, mouse.y), 40, 100, 20, RAYWHITE);
        
        // Afficher les rectangles des portails de zones
        for (int i = 0; i < ZONE_COUNT; ++i) {
            Rectangle rect = computePortalRect(g, i);
            DrawRectangleLinesEx(rect, 2.0f, (Color){ 255, 0, 0, 120 });
            DrawText(TextFormat("%s: x=%.0f y=%.0f w=%.0f h=%.0f",
                                HUB_PORTALS[i].label,
                                rect.x, rect.y, rect.width, rect.height),
                     40, 130 + i * 24, 18, LIGHTGRAY);
        }
        
        // Afficher le rectangle du portail de la boutique
        Rectangle shopRect = computeShopPortalRect(g);
        DrawRectangleLinesEx(shopRect, 2.0f, (Color){ 0, 180, 255, 150 });
        DrawText(TextFormat("Boutique: x=%.0f y=%.0f w=%.0f h=%.0f",
                            shopRect.x, shopRect.y, shopRect.width, shopRect.height),
                 40, 130 + ZONE_COUNT * 24, 18, LIGHTGRAY);
    }
}

// ============================================================================
// CONVERSION ZONE -> ÉTAT
// ============================================================================

// Convertit un identifiant de zone en état de jeu
static GameState zoneToState(ZoneId zone) {
    switch (zone) {
        case ZONE_JARDIN: return STATE_ZONE_JARDIN;
        case ZONE_CHAMBRE: return STATE_ZONE_CHAMBRE;
        case ZONE_GRENIER: return STATE_ZONE_GRENIER;
        case ZONE_CUISINE: return STATE_ZONE_CUISINE;
        default: return STATE_HUB;
    }
}

// ============================================================================
// FONCTION PRINCIPALE
// ============================================================================

int main(int argc, char **argv) {
    (void)argc;  // Paramètres non utilisés
    (void)argv;
    // Initialiser la structure du jeu à zéro
    Game g = {0};
    
    // Initialiser la fenêtre
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(1920, 1080, "Gros Nounours 2D");
    InitAudioDevice();
    
    // Charger l'icône de la fenêtre si elle existe
    if (FileExists("assets/icon.png")) {
        Image icon = LoadImage("assets/icon.png");
        if (icon.data) {
            SetWindowIcon(icon);
            UnloadImage(icon);
        }
    }
    
    // Charger les textures du menu
    g.menuBackground = loadTextureIfAvailable("assets/imagefond.png");
    g.hasMenuBackground = g.menuBackground.id != 0;
    g.menuBear = loadTextureIfAvailable("assets/nounours_depart.png");
    g.hasMenuBear = g.menuBear.id != 0;
    
    // Charger les textures de l'écran d'accueil
    g.titleBackground = loadTextureIfAvailable("assets/ecran acceuil.png");
    g.hasTitleBackground = g.titleBackground.id != 0;
    g.titleBackgroundHover = loadTextureIfAvailable("assets/ecran_accueil_ouvert.png");
    g.hasTitleBackgroundHover = g.titleBackgroundHover.id != 0;
    
    // Charger les tenues du nounours
    const char *bearOutfitFiles[5] = {
        "assets/nounours_noel.png",
        "assets/nounours_aviateur.png",
        "assets/nounours_plage.png",
        "assets/nounours_vampire.png",
        "assets/nounours_maillot.png"
    };
    for (int i = 0; i < 5; ++i) {
        g.bearOutfits[i] = loadTextureIfAvailable(bearOutfitFiles[i]);
        g.hasBearOutfit[i] = g.bearOutfits[i].id != 0;
    }
    g.currentBearOutfit = -1;  // -1 = tenue de départ
    
    // Initialiser la boutique
    g.boutiqueData.collectibles = &g.collectibles;
    g.boutiqueData.currentBearOutfit = &g.currentBearOutfit;
    g.boutiqueData.wantsToExit = false;
    Boutique_Init(&g.boutiqueData);
    
    // Synchroniser les données de la boutique avec la structure Game
    for (int i = 0; i < 5; ++i) {
        g.outfits[i] = g.boutiqueData.outfits[i];
        g.hasOutfit[i] = g.boutiqueData.hasOutfit[i];
        g.outfitNames[i] = g.boutiqueData.outfitNames[i];
        g.outfitPrices[i] = g.boutiqueData.outfitPrices[i];
        g.outfitOwned[i] = g.boutiqueData.outfitOwned[i];
    }
    g.selectedOutfit = 0;
    g.outfitPreview = g.outfits[0];
    g.hasOutfitPreview = g.hasOutfit[0];
    
    // Charger les fonds des zones
    for (int i = 0; i < ZONE_COUNT; ++i) {
        g.zoneBackgrounds[i] = loadTextureIfAvailable(ZONE_BG_FILES[i]);
        g.hasZoneBackground[i] = g.zoneBackgrounds[i].id != 0;
    }
    
    // Charger les positions depuis le fichier de configuration
    loadMenuLayout(&g);
    
    // Initialiser les paramètres du jeu
    SetTargetFPS(60);
    g.state = STATE_TITLE;  // Commencer sur l'écran d'accueil
    g.activeZone = ZONE_NONE;
    g.showDebugOverlay = false;
    g.draggingPortal = -1;
    g.draggingBear = false;
    g.draggingShopPortal = false;
    g.hoveredPortal = -1;
    g.inactivityTimer = 0.0f;
    
    // Charger la musique
    g.music = LoadMusicStream("assets/music.ogg");
    g.hasMusic = g.music.frameCount > 0;
    g.musicMuted = false;
    if (g.hasMusic) {
        SetMusicVolume(g.music, 0.6f);
        PlayMusicStream(g.music);
    }
    
    // Charger les icônes de son
    g.soundOnIcon = loadTextureIfAvailable("assets/sound_on.png");
    g.soundOffIcon = loadTextureIfAvailable("assets/sound_off.png");
    g.hasSoundOnIcon = g.soundOnIcon.id != 0;
    g.hasSoundOffIcon = g.soundOffIcon.id != 0;
    
    // ========================================================================
    // BOUCLE PRINCIPALE DU JEU
    // ========================================================================
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();  // Temps écoulé depuis la dernière frame
        
        // Assurer que le nounours reste dans l'écran
        clampBearToScreen(&g);
        
        // Réinitialiser le portail survolé
        g.hoveredPortal = -1;
        
        // Réinitialiser le timer d'inactivité si on n'est pas dans le hub
        if (g.state != STATE_HUB) {
            g.inactivityTimer = 0.0f;
        }
        
        // Mettre à jour la musique
        if (g.hasMusic && !g.musicMuted) {
            UpdateMusicStream(g.music);
        }
        
        // Raccourcis clavier globaux
        if (IsKeyPressed(KEY_F11)) {
            ToggleFullscreen();  // Plein écran
        }
        if (IsKeyPressed(KEY_F2)) {
            g.showDebugOverlay = !g.showDebugOverlay;  // Mode debug
        }
        
        // ====================================================================
        // GESTION DE L'ÉCRAN D'ACCUEIL
        // ====================================================================
        if (g.state == STATE_TITLE) {
            // Mode debug pour déplacer le rectangle
            handleDebugDragging(&g);
            
            // Détecter le survol et le clic sur le rectangle cliquable
            Vector2 mouse = GetMousePosition();
            Rectangle titleRect = computeTitleRect(&g);
            bool isHovering = CheckCollisionPointRec(mouse, titleRect);
            g.hoveredPortal = isHovering ? 0 : -1;
            
            // Si on clique sur le rectangle (et qu'on n'est pas en mode debug), entrer dans le hub
            if (!g.showDebugOverlay && isHovering && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                g.state = STATE_HUB;
            }
            
            // Touche Entrée pour entrer dans le hub
            if (IsKeyPressed(KEY_ENTER)) {
                g.state = STATE_HUB;
            }
        }
        // Gestion de la pause
        else if (g.state == STATE_PAUSE) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                g.state = STATE_HUB;
            }
        } else {
            // Échap pour mettre en pause (sauf depuis la pause)
            if (IsKeyPressed(KEY_ESCAPE)) {
                g.state = STATE_PAUSE;
            }
        }
        
        // ====================================================================
        // GESTION DES DIFFÉRENTS ÉTATS
        // ====================================================================
        switch (g.state) {
            // ================================================================
            // HUB (MENU PRINCIPAL)
            // ================================================================
            case STATE_HUB: {
                // Mode debug
                handleDebugDragging(&g);
                
                if (!g.showDebugOverlay) {
                    Vector2 mouse = GetMousePosition();
                    
                    // Gérer le timer d'inactivité
                    bool anyClick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
                    if (anyClick) {
                        g.inactivityTimer = 0.0f;  // Réinitialiser sur clic
                    } else {
                        g.inactivityTimer += dt;  // Incrémenter sinon
                    }
                    
                    // Vérifier les clics sur les portails de zones
                    bool portalHovered = false;
                    bool portalClicked = false;
                    for (int i = 0; i < ZONE_COUNT; ++i) {
                        Rectangle rect = computePortalRect(&g, i);
                        if (CheckCollisionPointRec(mouse, rect)) {
                            portalHovered = true;
                            g.hoveredPortal = i;
                            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                                g.state = zoneToState(HUB_PORTALS[i].zone);
                                g.activeZone = HUB_PORTALS[i].zone;
                                portalClicked = true;
                                break;
                            }
                        }
                    }
                    
                    // Vérifier le clic sur le portail de la boutique
                    if (!portalClicked) {
                        Rectangle shopRect = computeShopPortalRect(&g);
                        bool shopHovered = !portalHovered && CheckCollisionPointRec(mouse, shopRect);
                        if (shopHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            g.state = STATE_SHOP;
                            g.activeZone = ZONE_NONE;
                        }
                    }
                }
            } break;
            
            // ================================================================
            // ZONES (ÉCRANS D'INFORMATION AVANT LES MINIJEUX)
            // ================================================================
            case STATE_ZONE_JARDIN:
            case STATE_ZONE_CHAMBRE:
            case STATE_ZONE_GRENIER:
            case STATE_ZONE_CUISINE:
                // Retour arrière pour revenir au hub
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    g.state = STATE_HUB;
                    g.activeZone = ZONE_NONE;
                }
                // Entrée pour lancer le minijeu correspondant
                if (IsKeyPressed(KEY_ENTER)) {
                    if (g.state == STATE_ZONE_JARDIN) {
                        prepareMinigameSession(&g, GetMinigameTraffic());
                    } else if (g.state == STATE_ZONE_CHAMBRE) {
                        prepareMinigameSession(&g, GetMinigamePoussePousse());
                    } else if (g.state == STATE_ZONE_GRENIER) {
                        prepareMinigameSession(&g, GetMinigamePendu());
                    } else {
                        prepareMinigameSession(&g, GetMinigameCeriseSurGateau());
                    }
                }
                break;
            
            // ================================================================
            // BOUTIQUE
            // ================================================================
            case STATE_SHOP:
                // Retour arrière pour revenir au hub
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    g.state = STATE_HUB;
                    g.activeZone = ZONE_NONE;
                }
                // Synchroniser les données avant la mise à jour
                for (int i = 0; i < 5; ++i) {
                    g.boutiqueData.outfitOwned[i] = g.outfitOwned[i];
                }
                // Mettre à jour la boutique
                Boutique_Update(&g.boutiqueData);
                // Vérifier si on veut quitter
                if (g.boutiqueData.wantsToExit) {
                    g.state = STATE_HUB;
                    g.activeZone = ZONE_NONE;
                }
                // Synchroniser les données après la mise à jour
                for (int i = 0; i < 5; ++i) {
                    g.outfitOwned[i] = g.boutiqueData.outfitOwned[i];
                }
                break;
            
            // ================================================================
            // MINIJEU
            // ================================================================
            case STATE_MINIJEU:
                // Retour arrière pour quitter le minijeu
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    if (g.minigameCompleted) {
                        finalizeMinigame(&g);
                    } else {
                        // Quitter sans finaliser (abandon)
                        if (g.currentMinigame.unload) {
                            g.currentMinigame.unload();
                        }
                        g.currentMinigame = (MinigameAPI){0};
                        g.state = STATE_HUB;
                        g.activeZone = ZONE_NONE;
                    }
                    break;
                }
                
                // Mettre à jour le minijeu
                if (g.currentMinigame.update) {
                    g.currentMinigame.update(dt);
                }
                
                // Vérifier si le minijeu est complété et ajouter les pièces
                if (g.currentMinigame.isCompleted) {
                    int coins = 0;
                    g.currentMinigame.isCompleted(&coins);
                    if (coins > 0 && !g.minigameCompleted) {
                        // Ajouter les pièces immédiatement
                        g.collectibles += coins;
                        g.minigameCompleted = true;
                    } else if (coins == 0 && g.minigameCompleted) {
                        // Réinitialiser si on rejoue
                        g.minigameCompleted = false;
                    }
                }
                
                // Vérifier si le joueur veut quitter le minijeu
                if (g.currentMinigame.isCompleted) {
                    int coins = 0;
                    if (g.currentMinigame.isCompleted(&coins)) {
                        // Le minijeu indique qu'on peut quitter
                        if (g.activeZone >= 0 && g.activeZone < ZONE_COUNT) {
                            g.progress[g.activeZone].completed = true;
                        }
                        if (g.currentMinigame.unload) {
                            g.currentMinigame.unload();
                        }
                        g.currentMinigame = (MinigameAPI){0};
                        g.state = STATE_HUB;
                        g.activeZone = ZONE_NONE;
                    }
                }
                break;
            
            default:
                break;
        }
        
        // ====================================================================
        // AFFICHAGE
        // ====================================================================
        BeginDrawing();
        ClearBackground((Color){ 30, 34, 46, 255 });
        
        switch (g.state) {
            // Écran d'accueil
            case STATE_TITLE: {
                // Choisir le fond à afficher
                Texture2D *bgToUse = &g.titleBackground;
                bool hasBg = g.hasTitleBackground && g.titleBackground.id != 0;
                
                // Si on survole le rectangle, utiliser l'image "ouvert"
                if (g.hoveredPortal >= 0 && g.hasTitleBackgroundHover && g.titleBackgroundHover.id != 0) {
                    bgToUse = &g.titleBackgroundHover;
                    hasBg = true;
                }
                
                // Afficher le fond
                if (hasBg && bgToUse->id != 0) {
                    Rectangle src = { 0, 0, (float)bgToUse->width, (float)bgToUse->height };
                    Rectangle dst = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
                    DrawTexturePro(*bgToUse, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
                }
                
                // Texte de bienvenue
                drawCentered("Bienvenue dans l'aventure de Gros Nounours !", 70, 48, RAYWHITE);
                drawCentered("Clique vite sur la porte pour rencontrer Gros Nounours et commencer a jouer", 130, 32, LIGHTGRAY);
                
                // Overlay de debug
                drawDebugOverlay(&g);
            } break;
            
            // Écran de pause
            case STATE_PAUSE:
                drawCentered("Pause", 180, 48, RAYWHITE);
                drawCentered("Échap: Reprendre", 240, 24, LIGHTGRAY);
                break;
            
            // Hub (menu principal)
            case STATE_HUB: {
                // Afficher les éléments du hub
                drawMenuBackground(&g);
                drawBearCloseup(&g);
                drawMinigameStatusTable(&g);
                drawCoinCounter(&g);
                drawDebugOverlay(&g);
                
                // Afficher un message d'aide après 5 secondes d'inactivité
                if (g.inactivityTimer >= 5.0f) {
                    int screenWidth = GetScreenWidth();
                    int screenHeight = GetScreenHeight();
                    int fontSize = 28;
                    int lineHeight = 35;
                    Color textColor = (Color){ 255, 255, 255, 230 };
                    
                    const char *line1 = "Clique sur une porte, la bibliotheque ou le puzzle pour jouer.";
                    const char *line2 = "Clique sur Gros Nounours pour lui acheter des habits.";
                    
                    int line1Width = MeasureText(line1, fontSize);
                    int line2Width = MeasureText(line2, fontSize);
                    
                    // Calculer la taille de la boîte
                    int padding = 20;
                    int boxHeight = lineHeight * 2 + padding * 2;
                    int boxWidth = (line1Width > line2Width ? line1Width : line2Width) + padding * 2;
                    int boxX = (screenWidth - boxWidth) / 2 + 100;  // Décalé vers la droite
                    int boxY = screenHeight - boxHeight - 50;
                    
                    // Fond semi-transparent
                    DrawRectangle(boxX, boxY, boxWidth, boxHeight, (Color){ 0, 0, 0, 180 });
                    DrawRectangleLinesEx((Rectangle){ boxX, boxY, boxWidth, boxHeight }, 2.0f, (Color){ 255, 255, 255, 100 });
                    
                    // Texte
                    DrawText(line1, boxX + padding, boxY + padding, fontSize, textColor);
                    DrawText(line2, boxX + padding, boxY + padding + lineHeight, fontSize, textColor);
                }
            } break;
            
            // Zones (écrans d'information)
            case STATE_ZONE_JARDIN: {
                int y = 80;
                int fontSize = 32;
                int lineHeight = 42;
                Color textColor = RAYWHITE;
                
                drawCentered("Bienvenue dans l'aventure de Gros Nounours.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Ton but est d'aider Gros Nounours a atteindre la fin du chemin.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Pour le deplacer, utilise les fleches de ton clavier : vers la droite, la gauche, l'avant ou l'arriere.", y, fontSize - 2, textColor);
                y += lineHeight * 2;
                drawCentered("Attention tout de meme aux troncs d'arbre qui bloquent le passage.", y, fontSize - 2, textColor);
                y += lineHeight;
                drawCentered("Si Gros Nounours en touche un, il perd une vie.", y, fontSize - 2, textColor);
                y += lineHeight;
                drawCentered("Tu as 3 vies pour reussir le parcours.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Es-tu pret a guider Gros Nounours jusqu'au bout de son aventure ?", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Clique sur le bouton Entree pour commencer a jouer.", y, fontSize, (Color){255, 255, 100, 255});
            } break;
            
            case STATE_ZONE_CHAMBRE: {
                int y = 80;
                int fontSize = 32;
                int lineHeight = 42;
                Color textColor = RAYWHITE;
                
                drawCentered("Bienvenue dans le jeu du puzzle.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Ton objectif est de remettre toutes les pieces a leur bonne place pour reformer l'image.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Pour jouer, c'est tres simple : clique sur une piece du puzzle pour la deplacer.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Pret a reconstruire le puzzle comme un vrai champion ?", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Clique sur le bouton Entree pour commencer a jouer.", y, fontSize, (Color){255, 255, 100, 255});
            } break;
            
            case STATE_ZONE_GRENIER: {
                int y = 80;
                int fontSize = 32;
                int lineHeight = 42;
                Color textColor = RAYWHITE;
                
                drawCentered("Bienvenue dans le jeu du mot mystere.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Ton defi est de deviner le mot cache.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Pour jouer, c'est tres simple :", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Tu peux taper une lettre sur ton clavier ou cliquer sur une lettre de l'alphabet.", y, fontSize - 2, textColor);
                y += lineHeight * 2;
                drawCentered("Tu as 7 chances pour trouver le mot.", y, fontSize, textColor);
                y += lineHeight;
                drawCentered("Mais attention, a chaque erreur, la fleur perd une petale.", y, fontSize - 2, textColor);
                y += lineHeight;
                drawCentered("Quand il n'y en a plus, la partie est terminee.", y, fontSize - 2, textColor);
                y += lineHeight * 2;
                drawCentered("Si tu fais 5 erreurs, une definition secrete apparait pour t'aider a deviner le mot.", y, fontSize - 2, textColor);
                y += lineHeight;
                drawCentered("Et si tu reussis a trouver le mot, tu pourras aussi lire sa definition pour apprendre de nouvelles choses.", y, fontSize - 2, textColor);
                y += lineHeight * 2;
                drawCentered("Pret pour l'aventure des mots ?", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Appuie sur la touche Entree pour commencer a jouer.", y, fontSize, (Color){255, 255, 100, 255});
            } break;
            
            case STATE_ZONE_CUISINE: {
                int y = 80;
                int fontSize = 32;
                int lineHeight = 42;
                Color textColor = RAYWHITE;
                
                drawCentered("Bienvenue dans l'atelier des gateaux gourmands.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Tu vas devoir decorer un gateau comme un vrai patissier.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Au debut, tu verras une image pendant quelques secondes. Regarde-la bien et essaie de tout retenir.", y, fontSize - 2, textColor);
                y += lineHeight;
                drawCentered("Ton but est de refaire exactement le meme gateau pour gagner.", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Prends les ingredients et pose-les sur le gateau au bon endroit.", y, fontSize, textColor);
                y += lineHeight;
                drawCentered("Si tu as termine avant la fin du chrono, clique sur le bouton Termine.", y, fontSize - 2, textColor);
                y += lineHeight * 2;
                drawCentered("Pret a creer le gateau parfait ?", y, fontSize, textColor);
                y += lineHeight * 2;
                drawCentered("Clique sur le bouton Entree pour commencer a jouer.", y, fontSize, (Color){255, 255, 100, 255});
            } break;
            
            // Boutique
            case STATE_SHOP:
                // Synchroniser les données avant l'affichage
                for (int i = 0; i < 5; ++i) {
                    g.boutiqueData.outfitOwned[i] = g.outfitOwned[i];
                }
                Boutique_Draw(&g.boutiqueData);
                break;
            
            // Minijeu
            case STATE_MINIJEU:
                if (g.currentMinigame.draw) {
                    g.currentMinigame.draw();
                }
                break;
            
            default:
                break;
        }
        
        // Bouton musique (toujours visible)
        drawMusicButton(&g);
        if (g.hasMusic) {
            Rectangle musicBtn = getMusicButtonRect();
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), musicBtn)) {
                g.musicMuted = !g.musicMuted;
                if (g.musicMuted) {
                    PauseMusicStream(g.music);
                } else {
                    ResumeMusicStream(g.music);
                }
            }
        }
        
        EndDrawing();
    }
    
    // ========================================================================
    // NETTOYAGE (quand on ferme le jeu)
    // ========================================================================
    
    // Sauvegarder les positions
    saveMenuLayout(&g);
    
    // Libérer les textures
    if (g.hasMenuBackground) UnloadTexture(g.menuBackground);
    if (g.hasTitleBackground) UnloadTexture(g.titleBackground);
    if (g.hasTitleBackgroundHover) UnloadTexture(g.titleBackgroundHover);
    for (int i = 0; i < ZONE_COUNT; ++i) {
        if (g.hasZoneBackground[i]) UnloadTexture(g.zoneBackgrounds[i]);
    }
    if (g.hasMenuBear) UnloadTexture(g.menuBear);
    for (int i = 0; i < 5; ++i) {
        if (g.hasBearOutfit[i]) UnloadTexture(g.bearOutfits[i]);
    }
    if (g.hasSoundOnIcon) UnloadTexture(g.soundOnIcon);
    if (g.hasSoundOffIcon) UnloadTexture(g.soundOffIcon);
    if (g.hasOutfitPreview) UnloadTexture(g.outfitPreview);
    
    // Libérer la musique
    if (g.hasMusic) {
        StopMusicStream(g.music);
        UnloadMusicStream(g.music);
    }
    
    // Libérer les ressources de la boutique
    Boutique_Unload(&g.boutiqueData);
    
    // Fermer la fenêtre et l'audio
    CloseAudioDevice();
    CloseWindow();
    
    return 0;
}
