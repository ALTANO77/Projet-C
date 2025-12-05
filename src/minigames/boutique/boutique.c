// Module Boutique - Gestion de la boutique d'habits
#include "boutique.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================

// Texture de fond de la boutique
static Texture2D backgroundTexture = {0};
static bool hasBackground = false;

// ============================================================================
// FONCTIONS UTILITAIRES
// ============================================================================

// Charge une texture depuis un fichier si disponible
// Retourne une texture vide si le fichier n'existe pas
static Texture2D loadTextureIfAvailable(const char *path) {
    Texture2D tex = {0};
    Image img = LoadImage(path);
    if (img.data) {
        tex = LoadTextureFromImage(img);
        UnloadImage(img);
    }
    return tex;
}

// Dessine du texte centré horizontalement à la position Y donnée
static void drawCentered(const char *text, int y, int fontSize, Color color) {
    DrawText(text, (GetScreenWidth() - MeasureText(text, fontSize)) / 2, y, fontSize, color);
}

// Dessine le compteur de pièces en haut à droite de l'écran
static void drawCoinCounter(int collectibles) {
    const char *label = TextFormat("Pieces : %d", collectibles);
    int fontSize = 30;
    int textWidth = MeasureText(label, fontSize);
    int padding = 18;
    Rectangle box = {
        GetScreenWidth() - textWidth - padding * 2 - 40,
        30,
        (float)textWidth + padding * 2,
        50
    };
    DrawRectangleRounded(box, 0.12f, 6, (Color){ 0, 0, 0, 160 });
    DrawText(label, (int)(box.x + padding), (int)(box.y + 12), fontSize, GOLD);
}

// Calcule la position et la taille d'une tenue selon son index
// Cette fonction est utilisée à la fois pour le dessin et pour la détection de clic
static void getOutfitRect(int index, Rectangle *outRect) {
    // Taille de base des tenues (25% de la plus petite dimension de l'écran)
    float outfitSize = (float)fminf(GetScreenWidth(), GetScreenHeight()) * 0.25f;
    float spacing = outfitSize * 1.05f; // Espacement entre les tenues
    float totalWidth = 5 * spacing; // Largeur totale pour 5 tenues
    float startX = (GetScreenWidth() - totalWidth) / 2.0f; // Position X de départ (centré)
    float startY = GetScreenHeight() * 0.50f; // Position Y de départ (50% de la hauteur)
    
    // Agrandir toutes les tenues sauf l'aviateur (index 1)
    float scale = (index == 1) ? 1.0f : 1.6f;
    // Noël (index 0) est encore un peu plus large
    float currentWidth = (index == 0) ? outfitSize * 1.15f * scale : outfitSize * scale;
    float currentHeight = outfitSize * scale;
    
    // Décalages spécifiques pour chaque tenue (positionnement personnalisé)
    float offsetX = 0.0f;
    float offsetY = -60.0f; // Toutes les tenues remontées de 60 pixels
    if (index == 0) {
        offsetX = -160.0f; // Noël vers la gauche
    } else if (index == 1) {
        offsetX = -50.0f; // Aviateur vers la gauche
        offsetY = 10.0f; // Aviateur un peu plus bas que les autres
    } else if (index == 2) {
        offsetX = -90.0f; // Plage encore plus vers la gauche
    } else if (index == 3) {
        offsetX = -30.0f; // Vampire vers la gauche
        offsetY = -80.0f; // Vampire encore plus haut
    } else if (index == 4) {
        offsetX = -10.0f; // Maillot un peu à gauche
        offsetY = -80.0f; // Maillot encore plus haut
    }
    
    // Calculer la position finale du rectangle
    outRect->x = startX + index * spacing + offsetX;
    outRect->y = startY + offsetY;
    outRect->width = currentWidth;
    outRect->height = currentHeight;
}

// ============================================================================
// INITIALISATION
// ============================================================================

// Initialise la boutique : charge les textures et définit les prix
void Boutique_Init(BoutiqueData *data) {
    data->wantsToExit = false;
    
    // Noms des tenues (affichés dans le jeu)
    data->outfitNames[0] = "Noël";
    data->outfitNames[1] = "Aviateur";
    data->outfitNames[2] = "Plage";
    data->outfitNames[3] = "Vampire";
    data->outfitNames[4] = "Maillot";
    
    // Fichiers des tenues dans assets/boutique/
    const char *outfitFiles[5] = {
        "assets/boutique/tenue_noel.png",
        "assets/boutique/tenue_aviateur.png",
        "assets/boutique/tenue_plage.png",
        "assets/boutique/tenue_vampire.png",
        "assets/boutique/tenue_maillot.png"
    };
    
    // Prix des tenues
    data->outfitPrices[0] = 15; // Noël
    data->outfitPrices[1] = 20; // Aviateur
    data->outfitPrices[2] = 15; // Plage
    data->outfitPrices[3] = 20; // Vampire
    data->outfitPrices[4] = 15; // Maillot
    
    // Charger les textures des tenues
    for (int i = 0; i < 5; ++i) {
        data->outfits[i] = loadTextureIfAvailable(outfitFiles[i]);
        data->hasOutfit[i] = data->outfits[i].id != 0;
        data->outfitOwned[i] = false; // Aucune tenue achetée au départ
    }
    
    // Charger le fond de la boutique
    backgroundTexture = loadTextureIfAvailable("assets/boutique/fond_boutique.png");
    hasBackground = (backgroundTexture.id != 0);
}

// ============================================================================
// MISE À JOUR (appelée chaque frame, environ 60 fois/seconde)
// ============================================================================

// Gère les interactions : clics sur les tenues et sur le bouton retour
void Boutique_Update(BoutiqueData *data) {
    data->wantsToExit = false;
    
    // Vérifier les clics seulement si le bouton gauche de la souris est pressé
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;
    
    Vector2 mouse = GetMousePosition();
    
    // Vérifier le clic sur le bouton "Retour au salon"
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int buttonWidth = MeasureText("Retour au salon", 28) + 40;
    Rectangle backButton = { (sw - buttonWidth) / 2, sh - 840, buttonWidth, 50 };
    
    if (CheckCollisionPointRec(mouse, backButton)) {
        data->wantsToExit = true;
        return;
    }
    
    // Vérifier les clics sur les tenues
    for (int i = 0; i < 5; ++i) {
        Rectangle outfitRect;
        getOutfitRect(i, &outfitRect);
        
        // Si on clique sur cette tenue
        if (CheckCollisionPointRec(mouse, outfitRect)) {
            // Si la tenue est déjà achetée, la sélectionner (la porter)
            if (data->outfitOwned[i]) {
                *data->currentBearOutfit = i;
            }
            // Sinon, si le joueur a assez de pièces, acheter la tenue
            else if (*data->collectibles >= data->outfitPrices[i]) {
                data->outfitOwned[i] = true;
                *data->collectibles -= data->outfitPrices[i];
                *data->currentBearOutfit = i; // Sélectionner automatiquement après l'achat
            }
            break; // Une seule tenue peut être cliquée à la fois
        }
    }
}

// ============================================================================
// DESSIN (appelé chaque frame, environ 60 fois/seconde)
// ============================================================================

// Dessine tout le contenu de la boutique
void Boutique_Draw(BoutiqueData *data) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    
    // Dessiner le fond de la boutique
    if (hasBackground) {
        // Calculer le scale pour que l'image remplisse l'écran sans être coupée
        float scaleX = (float)sw / backgroundTexture.width;
        float scaleY = (float)sh / backgroundTexture.height;
        float scale = (scaleX < scaleY) ? scaleX : scaleY; // Prendre le plus petit pour ne pas couper
        float w = backgroundTexture.width * scale;
        float h = backgroundTexture.height * scale;
        DrawTextureEx(backgroundTexture, (Vector2){(sw - w) / 2.0f, (sh - h) / 2.0f}, 0.0f, scale, WHITE);
    } else {
        // Fond gris clair si pas d'image de fond
        DrawRectangle(0, 0, sw, sh, (Color){ 245, 245, 245, 255 });
    }
    
    // Afficher le compteur de pièces en haut à droite
    drawCoinCounter(*data->collectibles);
    
    // Afficher le texte d'instruction (centré)
    drawCentered("Clique sur un habit pour habiller Gros Nounours.", 390, 32, DARKGRAY);
    drawCentered("Si tu n'as pas assez de pièces, retourne jouer pour en gagner !", 430, 32, DARKGRAY);
    
    // Dessiner les 5 tenues
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < 5; ++i) {
        Rectangle outfitRect;
        getOutfitRect(i, &outfitRect);
        
        // Déterminer l'état de la tenue
        bool canAfford = *data->collectibles >= data->outfitPrices[i];
        bool isOwned = data->outfitOwned[i];
        
        // Bordure dorée si c'est la tenue actuellement portée
        if (isOwned && *data->currentBearOutfit == i) {
            DrawRectangleLinesEx(outfitRect, 4.0f, (Color){ 255, 215, 0, 255 });
        }
        
        // Dessiner la tenue avec opacité réduite si non achetée
        Color tint = isOwned ? WHITE : (canAfford ? (Color){ 255, 255, 255, 180 } : (Color){ 150, 150, 150, 180 });
        
        if (data->hasOutfit[i]) {
            Rectangle src = { 0, 0, (float)data->outfits[i].width, (float)data->outfits[i].height };
            
            // Préserver le ratio d'aspect de l'image (éviter qu'elle soit déformée)
            float imgAspect = (float)data->outfits[i].width / (float)data->outfits[i].height;
            float rectAspect = outfitRect.width / outfitRect.height;
            
            Rectangle dstRect = outfitRect;
            if (imgAspect > rectAspect) {
                // L'image est plus large que le rectangle, ajuster la hauteur
                float newHeight = outfitRect.width / imgAspect;
                dstRect.height = newHeight;
                dstRect.y = outfitRect.y + (outfitRect.height - newHeight) / 2.0f;
            } else {
                // L'image est plus haute que le rectangle, ajuster la largeur
                float newWidth = outfitRect.height * imgAspect;
                dstRect.width = newWidth;
                dstRect.x = outfitRect.x + (outfitRect.width - newWidth) / 2.0f;
            }
            
            DrawTexturePro(data->outfits[i], src, dstRect, (Vector2){ 0, 0 }, 0.0f, tint);
        } else {
            // Afficher un carré gris avec un "?" si la texture n'a pas pu être chargée
            DrawRectangleRec(outfitRect, (Color){ 100, 100, 100, 255 });
            DrawText("?", (int)(outfitRect.x + outfitRect.width/2 - 10), (int)(outfitRect.y + outfitRect.height/2 - 10), 20, WHITE);
        }
    }
    
    // Dessiner le bouton "Retour au salon"
    const char *buttonText = "Retour au salon";
    int buttonWidth = MeasureText(buttonText, 28) + 40;
    Rectangle backButton = { (sw - buttonWidth) / 2, sh - 840, buttonWidth, 50 };
    
    Color buttonColor = CheckCollisionPointRec(mouse, backButton) ? 
        (Color){ 160, 120, 80, 255 } : (Color){ 139, 90, 43, 255 };
    
    DrawRectangleRec(backButton, buttonColor);
    DrawRectangleLinesEx(backButton, 2.0f, (Color){ 101, 67, 33, 255 });
    DrawText(buttonText, backButton.x + 20, backButton.y + 12, 28, WHITE);
}

// ============================================================================
// NETTOYAGE
// ============================================================================

// Libère toutes les ressources (textures) chargées par la boutique
void Boutique_Unload(BoutiqueData *data) {
    // Décharger les textures des tenues
    for (int i = 0; i < 5; ++i) {
        if (data->hasOutfit[i]) {
            UnloadTexture(data->outfits[i]);
        }
    }
    // Décharger le fond
    if (hasBackground) {
        UnloadTexture(backgroundTexture);
        hasBackground = false;
    }
}
