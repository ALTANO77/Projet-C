#ifndef BOUTIQUE_H
#define BOUTIQUE_H

#include "raylib.h"

// Structure pour passer les données nécessaires à la boutique
typedef struct {
    int *collectibles;  // Pointeur vers le nombre de pièces du joueur
    Texture2D outfits[5];
    bool hasOutfit[5];
    const char *outfitNames[5];
    int outfitPrices[5];
    bool outfitOwned[5];
    int *currentBearOutfit;  // Pointeur vers l'index de la tenue actuellement portée
    bool wantsToExit;  // Flag pour retourner au hub
} BoutiqueData;

// Fonctions de la boutique
void Boutique_Init(BoutiqueData *data);
void Boutique_Update(BoutiqueData *data);
void Boutique_Draw(BoutiqueData *data);
void Boutique_Unload(BoutiqueData *data);

#endif // BOUTIQUE_H

