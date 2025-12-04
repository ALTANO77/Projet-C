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
    data->wantsToExit = false;
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
    // Réinitialiser le flag à chaque frame
    data->wantsToExit = false;
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        
        // Vérifier le clic sur le bouton "Retour au salon"
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        const char *buttonText = "Retour au salon";
        int buttonFontSize = 28;
        int buttonWidth = MeasureText(buttonText, buttonFontSize) + 40;
        int buttonHeight = 50;
        int buttonX = (sw - buttonWidth) / 2; // Centré horizontalement
        int buttonY = sh - 840; // Position ajustée
        Rectangle backButton = { buttonX, buttonY, buttonWidth, buttonHeight };
        
        if (CheckCollisionPointRec(mouse, backButton)) {
            data->wantsToExit = true;
            return;
        }
        
        float outfitSize = (float)fminf(GetScreenWidth(), GetScreenHeight()) * 0.25f;
        float spacing = outfitSize * 1.05f;
        float totalWidth = 5 * spacing;
        float startX = (GetScreenWidth() - totalWidth) / 2.0f;
        float startY = GetScreenHeight() * 0.50f;
        
        for (int i = 0; i < 5; ++i) {
            // Même logique que dans Draw : agrandir toutes sauf l'aviateur
            float scale = (i == 1) ? 1.0f : 1.6f;
            float currentWidth = (i == 0) ? outfitSize * 1.15f * scale : outfitSize * scale;
            float currentHeight = outfitSize * scale;
            
            // Décaler Noël (0) et Aviateur (1) vers la gauche, Vampire (3) et Maillot (4) vers la droite
            float offsetX = 0.0f;
            float offsetY = -60.0f; // Toutes les tenues remontées
            if (i == 0) {
                offsetX = -160.0f; // Noël légèrement à gauche
            } else if (i == 1) {
                offsetX = -50.0f; // Aviateur vers la gauche
                offsetY = 10.0f; // Aviateur un peu plus bas que les autres
            } else if (i == 2) {
                offsetX = -90.0f; // Plage encore plus vers la gauche
            } else if (i == 3) {
                offsetX = -30.0f; // Vampire vers la gauche
                offsetY = -80.0f; // Vampire encore plus haut
            } else if (i == 4) {
                offsetX = -10.0f; // Maillot un peu à gauche
                offsetY = -80.0f; // Maillot encore plus haut
            }
            Rectangle outfitRect = {
                startX + i * spacing + offsetX,
                startY + offsetY,
                currentWidth,
                currentHeight
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

    // Afficher le texte d'instruction
    const char *instructionText = "Clique sur un habit pour habiller Gros Nounours.\nSi tu n'as pas assez de pièces, retourne jouer pour en gagner !";
    int fontSize = 32;
    int lineHeight = 40;
    int textY = 390;
    int sw = GetScreenWidth();
    
    // Afficher le texte ligne par ligne
    const char *line1 = "Clique sur un habit pour habiller Gros Nounours.";
    const char *line2 = "Si tu n'as pas assez de pièces, retourne jouer pour en gagner !";
    int line1Width = MeasureText(line1, fontSize);
    int line2Width = MeasureText(line2, fontSize);
    DrawText(line1, (sw - line1Width) / 2, textY, fontSize, DARKGRAY);
    DrawText(line2, (sw - line2Width) / 2, textY + lineHeight, fontSize, DARKGRAY);

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
        float offsetY = -60.0f; // Toutes les tenues remontées
        if (i == 0) {
            offsetX = -160.0f; // Noël légèrement à gauche
        } else if (i == 1) {
            offsetX = -50.0f; // Aviateur vers la gauche
            offsetY = 10.0f; // Aviateur un peu plus bas que les autres
        } else if (i == 2) {
            offsetX = -90.0f; // Plage encore plus vers la gauche
        } else if (i == 3) {
            offsetX = -30.0f; // Vampire vers la gauche
            offsetY = -80.0f; // Vampire encore plus haut
        } else if (i == 4) {
            offsetX = -10.0f; // Maillot un peu à gauche
            offsetY = -80.0f; // Maillot encore plus haut
        }
        Rectangle outfitRect = {
            startX + i * spacing + offsetX,
            startY + offsetY,
            currentWidth,
            currentHeight
        };
        
        bool canAfford = *data->collectibles >= data->outfitPrices[i];
        bool isOwned = data->outfitOwned[i];
        bool hovered = CheckCollisionPointRec(mouse, outfitRect);
        
        // Bordure si c'est la tenue actuellement portée (pas de bordure au survol)
        if (isOwned && *data->currentBearOutfit == i) {
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
    
    // Afficher le bouton "Retour au salon"
    int sh = GetScreenHeight();
    const char *buttonText = "Retour au salon";
    int buttonFontSize = 28;
    int buttonWidth = MeasureText(buttonText, buttonFontSize) + 40;
    int buttonHeight = 50;
    int buttonX = (sw - buttonWidth) / 2; // Centré horizontalement
    int buttonY = sh - 840; // Position ajustée
    Rectangle backButton = { buttonX, buttonY, buttonWidth, buttonHeight };
    
    bool hovered = CheckCollisionPointRec(mouse, backButton);
    Color buttonColor = hovered ? (Color){ 160, 120, 80, 255 } : (Color){ 139, 90, 43, 255 }; // Marron
    
    DrawRectangleRec(backButton, buttonColor);
    DrawRectangleLinesEx(backButton, 2.0f, (Color){ 101, 67, 33, 255 }); // Bordure marron foncé
    DrawText(buttonText, buttonX + 20, buttonY + 12, buttonFontSize, WHITE);
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

