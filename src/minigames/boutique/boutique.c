// Module Boutique - Gestion de la boutique d'habits
#include "boutique.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Texture de fond de la boutique
static Texture2D backgroundTexture = {0};
static bool hasBackground = false;

// Fonction utilitaire pour charger une texture si disponible
static Texture2D loadTextureIfAvailable(const char *path) {
    Texture2D tex = {0};
    Image img = LoadImage(path);
    if (img.data) {
        tex = LoadTextureFromImage(img);
        UnloadImage(img);
    }
    return tex;
}

// Fonction utilitaire pour dessiner du texte centré
static void drawCentered(const char *text, int y, int fontSize, Color color) {
    int textWidth = MeasureText(text, fontSize);
    int x = (GetScreenWidth() - textWidth) / 2;
    DrawText(text, x, y, fontSize, color);
}

// Fonction pour dessiner le compteur de pièces
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

// Initialisation de la boutique - charge les textures depuis assets/boutique/
void Boutique_Init(BoutiqueData *data) {
    // Les noms des tenues
    data->outfitNames[0] = "Noël";
    data->outfitNames[1] = "Aviateur";
    data->outfitNames[2] = "Plage";
    data->outfitNames[3] = "Vampire";
    data->outfitNames[4] = "Maillot";
    
    // Les fichiers des tenues dans assets/boutique/
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
    
    // Charger les textures
    for (int i = 0; i < 5; ++i) {
        data->outfits[i] = loadTextureIfAvailable(outfitFiles[i]);
        data->hasOutfit[i] = data->outfits[i].id != 0;
        data->outfitOwned[i] = false; // Aucune tenue achetée au départ
    }
    
    // Charger le fond de la boutique
    backgroundTexture = loadTextureIfAvailable("assets/boutique/fond_boutique.png");
    hasBackground = (backgroundTexture.id != 0);
}

// Mise à jour de la boutique (gestion des clics)
void Boutique_Update(BoutiqueData *data) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        float outfitSize = (float)fminf(GetScreenWidth(), GetScreenHeight()) * 0.25f; // Même taille que dans Draw
        float spacing = outfitSize * 1.05f; // Même espacement que dans Draw
        float totalWidth = 5 * spacing;
        float startX = (GetScreenWidth() - totalWidth) / 2.0f;
        float startY = GetScreenHeight() * 0.50f; // Remonté encore plus
        
        for (int i = 0; i < 5; ++i) {
            // Décaler Noël (0) et Aviateur (1) vers la gauche, Vampire (3) et Maillot (4) vers la droite
            float offsetX = 0.0f;
            if (i == 0 || i == 1) offsetX = -50.0f; // Vers la gauche
            else if (i == 3 || i == 4) offsetX = 50.0f; // Vers la droite
            Rectangle outfitRect = {
                startX + i * spacing + offsetX,
                startY,
                outfitSize,
                outfitSize
            };
            
            // Vérifier si on clique sur une tenue
            if (CheckCollisionPointRec(mouse, outfitRect)) {
                // Si la tenue est déjà achetée, la sélectionner (la porter)
                if (data->outfitOwned[i]) {
                    *data->currentBearOutfit = i;
                }
                // Sinon, si le joueur a assez de pièces, acheter la tenue
                else if (*data->collectibles >= data->outfitPrices[i]) {
                    data->outfitOwned[i] = true;
                    *data->collectibles -= data->outfitPrices[i];
                    // Sélectionner automatiquement la tenue après l'achat
                    *data->currentBearOutfit = i;
                }
                break;
            }
        }
    }
}

// Affichage de la boutique
void Boutique_Draw(BoutiqueData *data) {
    // Afficher le fond sans le couper (fit to screen)
    if (hasBackground) {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        float scaleX = (float)sw / backgroundTexture.width;
        float scaleY = (float)sh / backgroundTexture.height;
        // Utiliser le scale le plus petit pour ne pas couper l'image
        float scale = (scaleX < scaleY) ? scaleX : scaleY;
        float w = backgroundTexture.width * scale;
        float h = backgroundTexture.height * scale;
        float x = (sw - w) / 2.0f;
        float y = (sh - h) / 2.0f;
        DrawTextureEx(backgroundTexture, (Vector2){x, y}, 0.0f, scale, WHITE);
    } else {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 245, 245, 245, 255 });
    }
    
    // Afficher le compteur de pièces
    drawCoinCounter(*data->collectibles);

    // Afficher les 5 tenues côte à côte
    float outfitSize = (float)fminf(GetScreenWidth(), GetScreenHeight()) * 0.25f; // Taille inchangée
    float spacing = outfitSize * 1.05f; // Espacement inchangé
    float totalWidth = 5 * spacing;
    float startX = (GetScreenWidth() - totalWidth) / 2.0f;
    float startY = GetScreenHeight() * 0.50f; // Remonté encore plus
    Vector2 mouse = GetMousePosition();
    
    for (int i = 0; i < 5; ++i) {
        // Agrandir toutes les tenues sauf l'aviateur (index 1)
        float scale = (i == 1) ? 1.0f : 1.6f; // Aviateur garde sa taille, les autres sont 60% plus grandes
        float currentWidth = (i == 0) ? outfitSize * 1.15f * scale : outfitSize * scale;
        float currentHeight = outfitSize * scale;
        // Décaler Noël (0) et Aviateur (1) vers la gauche, Vampire (3) et Maillot (4) vers la droite
        float offsetX = 0.0f;
        if (i == 0 || i == 1) offsetX = -50.0f; // Vers la gauche
        else if (i == 3 || i == 4) offsetX = 50.0f; // Vers la droite
        Rectangle outfitRect = {
            startX + i * spacing + offsetX,
            startY,
            currentWidth,
            currentHeight
        };
        
        bool canAfford = *data->collectibles >= data->outfitPrices[i];
        bool isOwned = data->outfitOwned[i];
        bool hovered = CheckCollisionPointRec(mouse, outfitRect);
        
        // Bordure si survolée ou si c'est la tenue actuellement portée
        if (hovered && !isOwned) {
            DrawRectangleLinesEx(outfitRect, 3.0f, canAfford ? (Color){ 50, 200, 50, 255 } : (Color){ 200, 50, 50, 255 });
        } else if (isOwned && *data->currentBearOutfit == i) {
            // Bordure dorée pour la tenue actuellement portée
            DrawRectangleLinesEx(outfitRect, 4.0f, (Color){ 255, 215, 0, 255 });
        }
        
        // Afficher la tenue avec opacité réduite si non achetée
        Color tint = isOwned ? WHITE : (canAfford ? (Color){ 255, 255, 255, 180 } : (Color){ 150, 150, 150, 180 });
        if (data->hasOutfit[i]) {
            Rectangle src = { 0, 0, (float)data->outfits[i].width, (float)data->outfits[i].height };
            
            // Préserver le ratio d'aspect de l'image
            float imgAspect = (float)data->outfits[i].width / (float)data->outfits[i].height;
            float rectAspect = currentWidth / currentHeight;
            
            Rectangle dstRect = outfitRect;
            if (imgAspect > rectAspect) {
                // L'image est plus large, ajuster la hauteur
                float newHeight = currentWidth / imgAspect;
                dstRect.height = newHeight;
                dstRect.y = outfitRect.y + (currentHeight - newHeight) / 2.0f;
            } else {
                // L'image est plus haute, ajuster la largeur
                float newWidth = currentHeight * imgAspect;
                dstRect.width = newWidth;
                dstRect.x = outfitRect.x + (currentWidth - newWidth) / 2.0f;
            }
            
            DrawTexturePro(data->outfits[i], src, dstRect, (Vector2){ 0, 0 }, 0.0f, tint);
        } else {
            DrawRectangleRec(outfitRect, (Color){ 100, 100, 100, 255 });
            DrawText("?", (int)(outfitRect.x + outfitRect.width/2 - 10), (int)(outfitRect.y + outfitRect.height/2 - 10), 20, WHITE);
        }
        
    }
}

// Déchargement des ressources
void Boutique_Unload(BoutiqueData *data) {
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

