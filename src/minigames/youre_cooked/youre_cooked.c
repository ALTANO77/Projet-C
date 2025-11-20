#include "youre_cooked.h"

#include "raylib.h"
#include "raymath.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ITEMS 96
#define MAX_ORDERS 3
#define DIRTY_QUEUE 6
#define STAND_COUNT 20
#define ORDER_TARGET 4

typedef struct {
    Rectangle rect;
    Color color;
} DecoRect;

#define MAX_WALL_RECTS 6
#define MAX_COUNTER_RECTS 12

static Rectangle kitchenArea = {0};
static float kitchenTile = 80.0f;
static DecoRect wallRects[MAX_WALL_RECTS];
static int wallRectCount = 0;
static DecoRect counterRects[MAX_COUNTER_RECTS];
static int counterRectCount = 0;

typedef enum {
    ITEM_NONE = 0,
    ITEM_INGREDIENT,
    ITEM_COOKWARE,
    ITEM_PLATE,
    ITEM_PORTION,
    ITEM_EXTINGUISHER
} ItemType;

typedef enum {
    COOKWARE_NONE = 0,
    COOKWARE_PAN,
    COOKWARE_POT
} CookwareType;

typedef enum {
    FOOD_NONE = 0,
    FOOD_RICE,
    FOOD_PASTA,
    FOOD_FISH,
    FOOD_SHRIMP,
    FOOD_BEEF
} FoodType;

typedef enum {
    FOOD_STATE_NONE = 0,
    FOOD_STATE_RAW,
    FOOD_STATE_WASHED,
    FOOD_STATE_PREPPED,
    FOOD_STATE_COOKING,
    FOOD_STATE_COOKED,
    FOOD_STATE_BURNT,
    FOOD_STATE_CHARRED
} FoodState;

typedef enum {
    STAND_FISH = 0,
    STAND_SHRIMP,
    STAND_RICE,
    STAND_PASTA,
    STAND_BEEF,
    STAND_CUTTER,
    STAND_DEVEINER,
    STAND_RICE_WASHER,
    STAND_PAN,
    STAND_STOVE_A,
    STAND_STOVE_B,
    STAND_STOVE_C,
    STAND_FAUCET,
    STAND_PLATE_CABINET,
    STAND_DIRTY_PLATES,
    STAND_POT_CABINET,
    STAND_POT_WASHER,
    STAND_DELIVERY,
    STAND_EXTINGUISHER,
    STAND_TRASH
} StandType;

typedef struct {
    bool active;
    ItemType type;
    CookwareType cookware;
    FoodType food;
    FoodState foodState;
    FoodState auxState;
    bool isBasePortion;
    bool hasWater;
    bool isDirty;
    bool isHeld;
    bool inStand;
    int standIndex;
    float processTimer;
    Vector2 pos;
    Vector2 size;
    bool onFire;
    float fireTimer;
    bool usable;
    FoodType plateBase;
    FoodType plateProtein;
    FoodState plateBaseState;
    FoodState plateProteinState;
} Item;

typedef struct {
    StandType type;
    Rectangle area;
    Color baseColor;
    const char *label;
    int itemIndex;
    float timer;
    float duration;
    bool highlight;
    bool onFire;
} Stand;

typedef struct {
    bool active;
    FoodType base;
    FoodType protein;
    float timer;
    float duration;
} Order;

typedef struct {
    bool active;
    float timer;
} DirtyJob;

typedef struct {
    Vector2 pos;
    float speed;
    int heldItem;
} CookPlayer;

static Item items[MAX_ITEMS];
static Stand stands[STAND_COUNT];
static Order orders[MAX_ORDERS];
static DirtyJob dirtyQueue[DIRTY_QUEUE];
static CookPlayer player;


static Texture2D backgroundTex = {0};
static bool backgroundLoaded = false;
static Sound alarmSound = {0};
static bool alarmSoundLoaded = false;

static int cleanPlateStock = 5;
static int cleanPotStock = 6;
static int ordersCompleted = 0;
static int ordersFailed = 0;
static int coinsEarned = 0;
static float orderSpawnTimer = 0.0f;
static float levelTimer = 240.0f;
static bool levelComplete = false;
static bool levelFailed = false;

static bool alarmActive = false;
static float alarmFlash = 0.0f;
static float shakeTimer = 0.0f;

static int extinguisherIndex = -1;

/* ---------- Helpers ---------- */

static Rectangle itemRect(const Item *it) {
    return (Rectangle){ it->pos.x, it->pos.y, it->size.x, it->size.y };
}

static Vector2 rectCenter(Rectangle r) {
    return (Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f };
}

static Vector2 itemCenter(const Item *it) {
    Rectangle r = itemRect(it);
    return rectCenter(r);
}

static Vector2 standCenter(const Stand *s) {
    return rectCenter(s->area);
}

static Rectangle standLocalRect(float offsetX, float offsetY, float w, float h) {
    Rectangle r = {
        kitchenArea.x + offsetX,
        kitchenArea.y + offsetY,
        w,
        h
    };
    return r;
}

static void addWallRect(Rectangle rect, Color color) {
    if (wallRectCount >= MAX_WALL_RECTS) return;
    wallRects[wallRectCount++] = (DecoRect){ rect, color };
}

static void addCounterRect(Rectangle rect, Color color) {
    if (counterRectCount >= MAX_COUNTER_RECTS) return;
    counterRects[counterRectCount++] = (DecoRect){ rect, color };
}

static void setupKitchenArea(void) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    const float targetW = 980.0f;
    const float targetH = 620.0f;
    kitchenArea.width = fminf(targetW, sw - 100.0f);
    kitchenArea.height = fminf(targetH, sh - 120.0f);
    if (kitchenArea.width < 720.0f) kitchenArea.width = 720.0f;
    if (kitchenArea.height < 520.0f) kitchenArea.height = 520.0f;
    kitchenArea.x = (sw - kitchenArea.width) * 0.5f;
    kitchenArea.y = (sh - kitchenArea.height) * 0.5f;
    kitchenTile = 72.0f;

    wallRectCount = 0;
    counterRectCount = 0;
    Color wallCol = (Color){ 135, 96, 64, 255 };
    addWallRect((Rectangle){ kitchenArea.x - 26, kitchenArea.y - 26, kitchenArea.width + 52, 28 }, wallCol);
    addWallRect((Rectangle){ kitchenArea.x - 26, kitchenArea.y + kitchenArea.height - 2, kitchenArea.width + 52, 28 }, wallCol);
    addWallRect((Rectangle){ kitchenArea.x - 26, kitchenArea.y - 26, 28, kitchenArea.height + 52 }, wallCol);
    addWallRect((Rectangle){ kitchenArea.x + kitchenArea.width - 2, kitchenArea.y - 26, 28, kitchenArea.height + 52 }, wallCol);

    Color counterCol = (Color){ 48, 86, 110, 255 };
    addCounterRect((Rectangle){ kitchenArea.x, kitchenArea.y, kitchenArea.width, 32 }, counterCol);
    addCounterRect((Rectangle){ kitchenArea.x, kitchenArea.y + kitchenArea.height - 32, kitchenArea.width, 32 }, counterCol);
    addCounterRect((Rectangle){ kitchenArea.x, kitchenArea.y, 32, kitchenArea.height }, counterCol);
    addCounterRect((Rectangle){ kitchenArea.x + kitchenArea.width - 32, kitchenArea.y, 32, kitchenArea.height }, counterCol);

    Color islandCol = (Color){ 40, 84, 110, 255 };
    addCounterRect((Rectangle){
        kitchenArea.x + kitchenArea.width * 0.32f,
        kitchenArea.y + kitchenArea.height * 0.32f,
        kitchenArea.width * 0.36f,
        36
    }, islandCol);
    addCounterRect((Rectangle){
        kitchenArea.x + kitchenArea.width * 0.45f - 18.0f,
        kitchenArea.y + kitchenArea.height * 0.28f,
        36,
        kitchenArea.height * 0.44f
    }, islandCol);
}

static int findFreeItemSlot(void) {
    for (int i = 0; i < MAX_ITEMS; ++i) if (!items[i].active) return i;
    return -1;
}

static void destroyItem(int idx) {
    if (idx < 0 || idx >= MAX_ITEMS) return;
    items[idx] = (Item){0};
}

static Color foodColor(FoodType f) {
    switch (f) {
        case FOOD_RICE: return (Color){ 230, 230, 210, 255 };
        case FOOD_PASTA: return (Color){ 255, 223, 150, 255 };
        case FOOD_FISH: return (Color){ 180, 210, 255, 255 };
        case FOOD_SHRIMP: return (Color){ 255, 180, 180, 255 };
        case FOOD_BEEF: return (Color){ 180, 60, 60, 255 };
        default: return (Color){ 220, 220, 220, 255 };
    }
}

static const char *foodName(FoodType f) {
    switch (f) {
        case FOOD_RICE: return "Riz";
        case FOOD_PASTA: return "Pates";
        case FOOD_FISH: return "Poisson";
        case FOOD_SHRIMP: return "Crevettes";
        case FOOD_BEEF: return "Boeuf";
        default: return "Rien";
    }
}

static float standBoost(StandType t) {
    switch (t) {
        case STAND_CUTTER:
        case STAND_DEVEINER:
        case STAND_RICE_WASHER:
        case STAND_POT_WASHER: return 30.0f;
        case STAND_PAN:
        case STAND_STOVE_A:
        case STAND_STOVE_B:
        case STAND_STOVE_C: return 60.0f;
        default: return 0.0f;
    }
}

static void triggerAlarm(void) {
    if (alarmActive) return;
    alarmActive = true;
    alarmFlash = 0.0f;
    shakeTimer = 0.0f;
    if (alarmSoundLoaded) PlaySound(alarmSound);
}

static void stopAlarmIfNeeded(void) {
    bool anyFire = false;
    for (int i = 0; i < STAND_COUNT; ++i) if (stands[i].onFire) { anyFire = true; break; }
    if (!anyFire) alarmActive = false;
}

static void triggerFire(int standIdx, int itemIdx) {
    if (standIdx < 0 || itemIdx < 0) return;
    Stand *st = &stands[standIdx];
    Item *it = &items[itemIdx];
    if (st->onFire) return;
    st->onFire = true;
    it->onFire = true;
    it->fireTimer = 0.0f;
    it->foodState = FOOD_STATE_BURNT;
    triggerAlarm();
}

static void releaseItemFromStand(int standIdx) {
    if (standIdx < 0) return;
    Stand *st = &stands[standIdx];
    if (st->itemIndex < 0) return;
    Item *it = &items[st->itemIndex];
    it->inStand = false;
    it->standIndex = -1;
    st->itemIndex = -1;
    st->timer = 0.0f;
    st->duration = 0.0f;
}

/* ---------- Spawning ---------- */

static int spawnItemDefaults(Vector2 pos, Vector2 size) {
    int idx = findFreeItemSlot();
    if (idx < 0) return -1;
    items[idx] = (Item){
        .active = true,
        .type = ITEM_INGREDIENT,
        .cookware = COOKWARE_NONE,
        .food = FOOD_NONE,
        .foodState = FOOD_STATE_NONE,
        .auxState = FOOD_STATE_NONE,
        .isBasePortion = false,
        .hasWater = false,
        .isDirty = false,
        .isHeld = false,
        .inStand = false,
        .standIndex = -1,
        .processTimer = 0.0f,
        .pos = pos,
        .size = size,
        .onFire = false,
        .fireTimer = 0.0f,
        .usable = true,
        .plateBase = FOOD_NONE,
        .plateProtein = FOOD_NONE,
        .plateBaseState = FOOD_STATE_NONE,
        .plateProteinState = FOOD_STATE_NONE
    };
    return idx;
}

static int spawnIngredient(FoodType food, Vector2 pos) {
    int idx = spawnItemDefaults(pos, (Vector2){ 40, 32 });
    if (idx < 0) return -1;
    items[idx].food = food;
    items[idx].type = ITEM_INGREDIENT;
    items[idx].foodState = FOOD_STATE_RAW;
    return idx;
}

static int spawnPlate(Vector2 pos) {
    int idx = spawnItemDefaults(pos, (Vector2){ 52, 12 });
    if (idx < 0) return -1;
    items[idx].type = ITEM_PLATE;
    items[idx].isDirty = false;
    return idx;
}

static int spawnPot(Vector2 pos) {
    int idx = spawnItemDefaults(pos, (Vector2){ 58, 28 });
    if (idx < 0) return -1;
    items[idx].type = ITEM_COOKWARE;
    items[idx].cookware = COOKWARE_POT;
    items[idx].food = FOOD_NONE;
    items[idx].foodState = FOOD_STATE_NONE;
    items[idx].isDirty = false;
    return idx;
}

static int spawnExtinguisher(Vector2 pos) {
    int idx = spawnItemDefaults(pos, (Vector2){ 26, 46 });
    if (idx < 0) return -1;
    items[idx].type = ITEM_EXTINGUISHER;
    items[idx].food = FOOD_NONE;
    return idx;
}

static int spawnPortion(FoodType food, FoodState state, bool isBase, Vector2 pos) {
    int idx = spawnItemDefaults(pos, (Vector2){ 36, 16 });
    if (idx < 0) return -1;
    items[idx].type = ITEM_PORTION;
    items[idx].food = food;
    items[idx].foodState = state;
    items[idx].isBasePortion = isBase;
    return idx;
}

/* ---------- Orders ---------- */

static void resetOrders(void) {
    for (int i = 0; i < MAX_ORDERS; ++i) orders[i].active = false;
    orderSpawnTimer = 2.0f;
    ordersCompleted = 0;
    ordersFailed = 0;
    coinsEarned = 0;
}

static void spawnOrder(void) {
    for (int i = 0; i < MAX_ORDERS; ++i) {
        if (orders[i].active) continue;
        FoodType base = (GetRandomValue(0, 1) == 0) ? FOOD_RICE : FOOD_PASTA;
        int p = GetRandomValue(0, 3);
        FoodType protein = FOOD_NONE;
        if (p == 0) protein = FOOD_FISH;
        else if (p == 1) protein = FOOD_SHRIMP;
        else if (p == 2) protein = FOOD_BEEF;
        orders[i] = (Order){
            .active = true,
            .base = base,
            .protein = protein,
            .timer = 90.0f,
            .duration = 90.0f
        };
        break;
    }
    orderSpawnTimer = 25.0f + (float)GetRandomValue(-5, 5);
}

static void failOrder(int idx) {
    if (idx < 0 || idx >= MAX_ORDERS) return;
    orders[idx].active = false;
    ordersFailed++;
    coinsEarned = coinsEarned > 0 ? coinsEarned - 1 : 0;
}

static bool deliverPlate(int plateIdx) {
    if (plateIdx < 0) return false;
    Item *plate = &items[plateIdx];
    if (plate->plateBase == FOOD_NONE) return false;
    if (plate->plateBaseState == FOOD_STATE_CHARRED) return false;
    if (plate->plateProteinState == FOOD_STATE_CHARRED) return false;
    for (int i = 0; i < MAX_ORDERS; ++i) {
        Order *o = &orders[i];
        if (!o->active) continue;
        if (o->base != plate->plateBase) continue;
        FoodType reqProtein = o->protein;
        FoodType plateProtein = plate->plateProtein;
        if (reqProtein == FOOD_NONE && plateProtein != FOOD_NONE) continue;
        if (reqProtein != FOOD_NONE && plateProtein != reqProtein) continue;
        o->active = false;
        ordersCompleted++;
        coinsEarned += 3;
        destroyItem(plateIdx);
        return true;
    }
    return false;
}

/* ---------- Dirty plate queue ---------- */

static void schedulePlateReturn(void) {
    for (int i = 0; i < DIRTY_QUEUE; ++i) {
        if (!dirtyQueue[i].active) {
            dirtyQueue[i].active = true;
            dirtyQueue[i].timer = 12.0f;
            return;
        }
    }
}

static void updateDirtyQueue(float dt) {
    for (int i = 0; i < DIRTY_QUEUE; ++i) {
        if (!dirtyQueue[i].active) continue;
        dirtyQueue[i].timer -= dt;
        if (dirtyQueue[i].timer <= 0.0f) {
            dirtyQueue[i].active = false;
            if (cleanPlateStock < 5) cleanPlateStock++;
        }
    }
}

/* ---------- Stand layout ---------- */

static void configureStand(StandType type, Rectangle area, Color color, const char *label) {
    int idx = (int)type;
    stands[idx] = (Stand){
        .type = type,
        .area = area,
        .baseColor = color,
        .label = label,
        .itemIndex = -1,
        .timer = 0.0f,
        .duration = 0.0f,
        .highlight = false,
        .onFire = false
    };
}

static void initLayout(void) {
    setupKitchenArea();
    const float standW = 118.0f;
    const float standH = 74.0f;
    const float margin = 26.0f;
    const float leftX = margin;
    const float leftY = 36.0f;
    const float leftStep = standH + 18.0f;

    configureStand(STAND_RICE, standLocalRect(leftX, leftY + leftStep * 0, standW, standH), (Color){ 90, 80, 60, 255 }, "Riz");
    configureStand(STAND_PASTA, standLocalRect(leftX, leftY + leftStep * 1, standW, standH), (Color){ 120, 100, 60, 255 }, "Pates");
    configureStand(STAND_FISH, standLocalRect(leftX, leftY + leftStep * 2, standW, standH), (Color){ 70, 90, 140, 255 }, "Poisson");
    configureStand(STAND_SHRIMP, standLocalRect(leftX, leftY + leftStep * 3, standW, standH), (Color){ 70, 110, 90, 255 }, "Crevettes");
    configureStand(STAND_BEEF, standLocalRect(leftX, leftY + leftStep * 4, standW, standH), (Color){ 120, 50, 50, 255 }, "Boeuf");

    const float prepY = 32.0f;
    configureStand(STAND_RICE_WASHER, standLocalRect(210.0f, prepY, standW, standH), (Color){ 60, 100, 120, 255 }, "Lave riz");
    configureStand(STAND_CUTTER, standLocalRect(350.0f, prepY, standW, standH), (Color){ 90, 90, 110, 255 }, "Coupe");
    configureStand(STAND_DEVEINER, standLocalRect(490.0f, prepY, standW, standH), (Color){ 90, 120, 110, 255 }, "Decort");
    configureStand(STAND_FAUCET, standLocalRect(630.0f, prepY, standW, standH), (Color){ 60, 120, 170, 255 }, "Robinet");
    configureStand(STAND_PAN, standLocalRect(770.0f, prepY, standW, standH), (Color){ 120, 70, 70, 255 }, "Poele");

    const float stoveY = prepY + standH + 40.0f;
    configureStand(STAND_STOVE_A, standLocalRect(470.0f, stoveY, standW, standH), (Color){ 130, 90, 70, 255 }, "Feu A");
    configureStand(STAND_STOVE_B, standLocalRect(610.0f, stoveY, standW, standH), (Color){ 130, 90, 70, 255 }, "Feu B");
    configureStand(STAND_STOVE_C, standLocalRect(750.0f, stoveY, standW, standH), (Color){ 130, 90, 70, 255 }, "Feu C");

    const float rightX = kitchenArea.width - standW - margin;
    float columnY = 48.0f;
    configureStand(STAND_PLATE_CABINET, standLocalRect(rightX, columnY, standW, standH), (Color){ 150, 150, 150, 255 }, "Assiettes");
    columnY += standH + 12.0f;
    configureStand(STAND_DIRTY_PLATES, standLocalRect(rightX, columnY, standW, standH), (Color){ 110, 80, 60, 255 }, "Sales");
    columnY += standH + 12.0f;
    configureStand(STAND_POT_CABINET, standLocalRect(rightX, columnY, standW, standH), (Color){ 150, 120, 90, 255 }, "Casseroles");
    columnY += standH + 12.0f;
    configureStand(STAND_POT_WASHER, standLocalRect(rightX, columnY, standW, standH), (Color){ 70, 110, 90, 255 }, "Lavage");

    const float deliveryHeight = 110.0f;
    float deliveryY = kitchenArea.height - deliveryHeight - 100.0f;
    if (deliveryY < columnY + standH + 20.0f) deliveryY = columnY + standH + 20.0f;
    configureStand(STAND_DELIVERY, standLocalRect(rightX, deliveryY, standW, deliveryHeight), (Color){ 150, 90, 90, 255 }, "Envoi");

    const float extinguisherH = 70.0f;
    float extinguisherY = kitchenArea.height - extinguisherH - 18.0f;
    configureStand(STAND_EXTINGUISHER, standLocalRect(rightX, extinguisherY, standW, extinguisherH), (Color){ 150, 60, 60, 255 }, "Extincteur");

    configureStand(STAND_TRASH, standLocalRect(margin, kitchenArea.height - standH - 28.0f, standW, standH + 16.0f), (Color){ 80, 60, 50, 255 }, "Poubelle");
}

/* ---------- Stand acceptance ---------- */

static bool standAcceptsItem(int standIdx, int itemIdx) {
    if (standIdx < 0 || itemIdx < 0) return false;
    Stand *st = &stands[standIdx];
    Item *it = &items[itemIdx];
    if (st->onFire) return false;
    switch (st->type) {
        case STAND_CUTTER:
            return it->type == ITEM_INGREDIENT && it->food == FOOD_FISH && it->foodState == FOOD_STATE_RAW;
        case STAND_DEVEINER:
            return it->type == ITEM_INGREDIENT && it->food == FOOD_SHRIMP && it->foodState == FOOD_STATE_RAW;
        case STAND_RICE_WASHER:
            return it->type == ITEM_INGREDIENT && it->food == FOOD_RICE && it->foodState == FOOD_STATE_RAW;
        case STAND_PAN:
            return it->type == ITEM_INGREDIENT && it->food == FOOD_FISH && it->foodState == FOOD_STATE_PREPPED;
        case STAND_STOVE_A:
        case STAND_STOVE_B:
        case STAND_STOVE_C:
            if (it->type != ITEM_COOKWARE || it->cookware != COOKWARE_POT) return false;
            if (it->isDirty) return false;
            if (it->food == FOOD_NONE) return false;
            if (it->food == FOOD_RICE && (!it->hasWater || it->foodState != FOOD_STATE_WASHED)) return false;
            if (it->food == FOOD_PASTA && !it->hasWater) return false;
            if (it->food == FOOD_BEEF && it->foodState != FOOD_STATE_RAW) return false;
            return true;
        case STAND_POT_WASHER:
            return it->type == ITEM_COOKWARE && it->cookware == COOKWARE_POT && it->isDirty;
        case STAND_DELIVERY:
            return it->type == ITEM_PLATE && !it->isDirty && it->plateBase != FOOD_NONE;
        case STAND_TRASH:
            return (it->type == ITEM_INGREDIENT || it->type == ITEM_PORTION) &&
                   (it->foodState == FOOD_STATE_BURNT || it->foodState == FOOD_STATE_CHARRED || !it->usable);
        default:
            return false;
    }
}

static bool placeItemOnStand(int standIdx, int itemIdx) {
    if (!standAcceptsItem(standIdx, itemIdx)) return false;
    Stand *st = &stands[standIdx];
    Item *it = &items[itemIdx];
    switch (st->type) {
        case STAND_DELIVERY: {
            bool ok = deliverPlate(itemIdx);
            if (ok) {
                schedulePlateReturn();
                if (ordersCompleted >= ORDER_TARGET) levelComplete = true;
            } else {
                coinsEarned = coinsEarned > 0 ? coinsEarned - 1 : 0;
                schedulePlateReturn();
                destroyItem(itemIdx);
            }
            return ok;
        }
        case STAND_TRASH:
            destroyItem(itemIdx);
            return true;
        case STAND_POT_WASHER:
        case STAND_CUTTER:
        case STAND_DEVEINER:
        case STAND_RICE_WASHER:
        case STAND_PAN:
        case STAND_STOVE_A:
        case STAND_STOVE_B:
        case STAND_STOVE_C:
            st->itemIndex = itemIdx;
            st->timer = 0.0f;
            st->duration = standBoost(st->type);
            it->inStand = true;
            it->standIndex = standIdx;
            it->isHeld = false;
            it->processTimer = 0.0f;
            it->pos = (Vector2){ st->area.x + 12, st->area.y + 20 };
            return true;
        default:
            return false;
    }
}

/* ---------- Player ---------- */

static void resetPlayer(void) {
    player.pos = (Vector2){
        kitchenArea.x + kitchenArea.width * 0.5f,
        kitchenArea.y + kitchenArea.height * 0.7f
    };
    player.speed = 280.0f;
    player.heldItem = -1;
}

static Rectangle kitchenBounds(void) {
    return (Rectangle){
        kitchenArea.x + 20.0f,
        kitchenArea.y + 20.0f,
        kitchenArea.width - 40.0f,
        kitchenArea.height - 40.0f
    };
}

static void updatePlayerMovement(float dt) {
    Vector2 dir = { 0 };
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) dir.x -= 1.0f;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) dir.x += 1.0f;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) dir.y -= 1.0f;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) dir.y += 1.0f;
    if (Vector2Length(dir) > 0.0f) dir = Vector2Normalize(dir);
    player.pos = Vector2Add(player.pos, Vector2Scale(dir, player.speed * dt));
    Rectangle bounds = kitchenBounds();
    if (player.pos.x < bounds.x) player.pos.x = bounds.x;
    if (player.pos.y < bounds.y) player.pos.y = bounds.y;
    if (player.pos.x > bounds.x + bounds.width) player.pos.x = bounds.x + bounds.width;
    if (player.pos.y > bounds.y + bounds.height) player.pos.y = bounds.y + bounds.height;
    if (player.heldItem >= 0) {
        Item *held = &items[player.heldItem];
        held->pos = (Vector2){ player.pos.x - held->size.x * 0.5f, player.pos.y - 50 };
    }
}

static int findNearestItem(float maxDist) {
    int best = -1;
    float bestDist = maxDist;
    for (int i = 0; i < MAX_ITEMS; ++i) {
        Item *it = &items[i];
        if (!it->active || it->isHeld) continue;
        if (it->inStand) {
            Stand *st = &stands[it->standIndex];
            if (st && st->type != STAND_PAN && st->type != STAND_STOVE_A &&
                st->type != STAND_STOVE_B && st->type != STAND_STOVE_C &&
                st->type != STAND_POT_WASHER && st->type != STAND_CUTTER &&
                st->type != STAND_DEVEINER && st->type != STAND_RICE_WASHER) {
                // static due to stand, allow pickup
            } else if (st && st->timer < st->duration) {
                continue;
            }
        }
        float dist = Vector2Distance(itemCenter(it), player.pos);
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

static int standNearPlayer(float radius) {
    for (int i = 0; i < STAND_COUNT; ++i) {
        if (Vector2Distance(player.pos, standCenter(&stands[i])) <= radius) return i;
    }
    return -1;
}

static void dropHeldItem(void) {
    if (player.heldItem < 0) return;
    Item *it = &items[player.heldItem];
    it->isHeld = false;
    it->pos = (Vector2){ player.pos.x - it->size.x * 0.5f, player.pos.y };
    player.heldItem = -1;
}

static void takeItem(int idx) {
    if (idx < 0) return;
    if (player.heldItem >= 0) return;
    Item *it = &items[idx];
    if (it->inStand) {
        Stand *st = (it->standIndex >= 0) ? &stands[it->standIndex] : NULL;
        if (st) {
            st->itemIndex = -1;
            st->timer = 0.0f;
            st->duration = 0.0f;
        }
        it->inStand = false;
        it->standIndex = -1;
    }
    it->isHeld = true;
    player.heldItem = idx;
}

/* ---------- Interactions ---------- */

static void giveIngredient(StandType type) {
    if (player.heldItem >= 0) return;
    Vector2 pos = (Vector2){ player.pos.x - 20, player.pos.y - 50 };
    FoodType f = FOOD_NONE;
    switch (type) {
        case STAND_FISH: f = FOOD_FISH; break;
        case STAND_SHRIMP: f = FOOD_SHRIMP; break;
        case STAND_RICE: f = FOOD_RICE; break;
        case STAND_PASTA: f = FOOD_PASTA; break;
        case STAND_BEEF: f = FOOD_BEEF; break;
        default: return;
    }
    int idx = spawnIngredient(f, pos);
    if (idx >= 0) takeItem(idx);
}

static void givePlate(void) {
    if (player.heldItem >= 0) return;
    if (cleanPlateStock <= 0) return;
    cleanPlateStock--;
    int idx = spawnPlate((Vector2){ player.pos.x - 24, player.pos.y - 40 });
    if (idx >= 0) takeItem(idx);
}

static void givePot(void) {
    if (player.heldItem >= 0) return;
    if (cleanPotStock <= 0) return;
    cleanPotStock--;
    int idx = spawnPot((Vector2){ player.pos.x - 30, player.pos.y - 40 });
    if (idx >= 0) takeItem(idx);
}

static void ensureExtinguisher(void) {
    if (extinguisherIndex >= 0 && items[extinguisherIndex].active) return;
    extinguisherIndex = spawnExtinguisher((Vector2){ stands[STAND_EXTINGUISHER].area.x + 40, stands[STAND_EXTINGUISHER].area.y + 10 });
}

static void fillPotAtFaucet(void) {
    if (player.heldItem < 0) return;
    Item *it = &items[player.heldItem];
    if (it->type != ITEM_COOKWARE || it->cookware != COOKWARE_POT) return;
    it->hasWater = true;
}

static bool tryPourCookware(void) {
    if (player.heldItem < 0) return false;
    Item *it = &items[player.heldItem];
    if (it->type != ITEM_COOKWARE) return false;
    if (it->food == FOOD_NONE) return false;
    if (it->foodState != FOOD_STATE_COOKED && it->foodState != FOOD_STATE_BURNT) return false;
    bool isBase = (it->food == FOOD_RICE || it->food == FOOD_PASTA);
    Vector2 spawnPos = (Vector2){ player.pos.x - 18, player.pos.y - 20 };
    int idx = spawnPortion(it->food, it->foodState, isBase, spawnPos);
    if (idx < 0) return false;
    it->food = FOOD_NONE;
    it->foodState = FOOD_STATE_NONE;
    it->hasWater = false;
    it->isDirty = true;
    return true;
}

static bool tryUseExtinguisher(void) {
    if (player.heldItem < 0) return false;
    Item *it = &items[player.heldItem];
    if (it->type != ITEM_EXTINGUISHER) return false;
    for (int i = 0; i < STAND_COUNT; ++i) {
        if (!stands[i].onFire) continue;
        if (Vector2Distance(player.pos, standCenter(&stands[i])) > 80.0f) continue;
        stands[i].onFire = false;
        if (stands[i].itemIndex >= 0) {
            Item *target = &items[stands[i].itemIndex];
            target->onFire = false;
            target->fireTimer = 0.0f;
        }
        stopAlarmIfNeeded();
        return true;
    }
    return false;
}

static void combinePortionsWithPlates(void) {
    for (int i = 0; i < MAX_ITEMS; ++i) {
        Item *portion = &items[i];
        if (!portion->active || portion->isHeld || portion->inStand) continue;
        if (portion->type != ITEM_PORTION && portion->type != ITEM_INGREDIENT) continue;
        for (int j = 0; j < MAX_ITEMS; ++j) {
            Item *plate = &items[j];
            if (!plate->active || plate->type != ITEM_PLATE || plate->isDirty || plate->isHeld) continue;
            Rectangle rp = itemRect(portion);
            Rectangle rplate = itemRect(plate);
            if (!CheckCollisionRecs(rp, rplate)) continue;
            if (portion->type == ITEM_PORTION && portion->isBasePortion) {
                if (plate->plateBase == FOOD_NONE && portion->foodState != FOOD_STATE_CHARRED) {
                    plate->plateBase = portion->food;
                    plate->plateBaseState = portion->foodState;
                    destroyItem(i);
                    break;
                }
            } else {
                // Proteins
                if (plate->plateProtein == FOOD_NONE && portion->foodState != FOOD_STATE_CHARRED) {
                    plate->plateProtein = portion->food;
                    plate->plateProteinState = portion->foodState;
                    destroyItem(i);
                    break;
                }
            }
        }
    }
}

static void combineIngredientsWithPots(void) {
    for (int i = 0; i < MAX_ITEMS; ++i) {
        Item *ingredient = &items[i];
        if (!ingredient->active || ingredient->isHeld || ingredient->inStand) continue;
        if (ingredient->type != ITEM_INGREDIENT) continue;
        for (int j = 0; j < MAX_ITEMS; ++j) {
            Item *pot = &items[j];
            if (!pot->active || pot->type != ITEM_COOKWARE || pot->cookware != COOKWARE_POT) continue;
            if (pot->isDirty || pot->food != FOOD_NONE) continue;
            if (pot->isHeld) continue;
            Rectangle ri = itemRect(ingredient);
            Rectangle rpot = itemRect(pot);
            if (!CheckCollisionRecs(ri, rpot)) continue;
            if (ingredient->food == FOOD_RICE && ingredient->foodState == FOOD_STATE_WASHED) {
                pot->food = FOOD_RICE;
                pot->foodState = FOOD_STATE_WASHED;
                destroyItem(i);
                break;
            }
            if (ingredient->food == FOOD_PASTA) {
                pot->food = FOOD_PASTA;
                pot->foodState = FOOD_STATE_RAW;
                destroyItem(i);
                break;
            }
            if (ingredient->food == FOOD_BEEF && ingredient->foodState == FOOD_STATE_RAW) {
                pot->food = FOOD_BEEF;
                pot->foodState = FOOD_STATE_RAW;
                destroyItem(i);
                break;
            }
        }
    }
}

static bool tryLoadHeldIngredientIntoPot(void) {
    if (player.heldItem < 0) return false;
    Item *held = &items[player.heldItem];
    if (!held->active || held->type != ITEM_INGREDIENT) return false;
    for (int i = 0; i < MAX_ITEMS; ++i) {
        Item *pot = &items[i];
        if (!pot->active || pot->type != ITEM_COOKWARE || pot->cookware != COOKWARE_POT) continue;
        if (pot->isDirty || pot->food != FOOD_NONE) continue;
        float dist = Vector2Distance(itemCenter(held), itemCenter(pot));
        if (dist > 90.0f) continue;
        bool accepted = false;
        if (held->food == FOOD_RICE && held->foodState == FOOD_STATE_WASHED && pot->hasWater) {
            pot->food = FOOD_RICE;
            pot->foodState = FOOD_STATE_WASHED;
            accepted = true;
        } else if (held->food == FOOD_PASTA && pot->hasWater) {
            pot->food = FOOD_PASTA;
            pot->foodState = FOOD_STATE_RAW;
            accepted = true;
        } else if (held->food == FOOD_BEEF && held->foodState == FOOD_STATE_RAW) {
            pot->food = FOOD_BEEF;
            pot->foodState = FOOD_STATE_RAW;
            accepted = true;
        }
        if (accepted) {
            destroyItem(player.heldItem);
            player.heldItem = -1;
            return true;
        }
    }
    return false;
}

static void autoAttachLooseItems(void) {
    for (int s = 0; s < STAND_COUNT; ++s) {
        Stand *st = &stands[s];
        if (st->itemIndex >= 0) continue;
        for (int i = 0; i < MAX_ITEMS; ++i) {
            Item *it = &items[i];
            if (!it->active || it->isHeld || it->inStand) continue;
            if (!CheckCollisionRecs(itemRect(it), st->area)) continue;
            if (placeItemOnStand(s, i)) break;
        }
    }
}

static void handleInteraction(void) {
    if (IsKeyPressed(KEY_E)) {
        if (player.heldItem >= 0 && tryLoadHeldIngredientIntoPot()) return;
        int nearStand = standNearPlayer(110.0f);
        if (nearStand >= 0) {
            StandType t = stands[nearStand].type;
            if (t == STAND_FISH || t == STAND_SHRIMP || t == STAND_RICE ||
                t == STAND_PASTA || t == STAND_BEEF) {
                giveIngredient(t);
                return;
            }
            if (t == STAND_PLATE_CABINET) { givePlate(); return; }
            if (t == STAND_POT_CABINET) { givePot(); return; }
            if (t == STAND_EXTINGUISHER) {
                ensureExtinguisher();
                int pickup = extinguisherIndex;
                takeItem(pickup);
                return;
            }
            if (t == STAND_FAUCET) {
                fillPotAtFaucet();
                return;
            }
            if (player.heldItem >= 0) {
                if (placeItemOnStand(nearStand, player.heldItem)) {
                    player.heldItem = -1;
                    return;
                }
            }
        }
        if (player.heldItem < 0) {
            int idx = findNearestItem(80.0f);
            if (idx >= 0) {
                takeItem(idx);
                return;
            }
        } else {
            dropHeldItem();
        }
    }
    if (IsKeyPressed(KEY_Q)) {
        dropHeldItem();
    }
    if (IsKeyPressed(KEY_SPACE)) {
        if (tryUseExtinguisher()) return;
        if (tryPourCookware()) return;
    }
}

/* ---------- Processing ---------- */

static void updateStands(float dt) {
    for (int i = 0; i < STAND_COUNT; ++i) {
        Stand *st = &stands[i];
        st->highlight = false;
        if (st->itemIndex < 0) continue;
        Item *it = &items[st->itemIndex];
        if (!it->active) { st->itemIndex = -1; continue; }
        switch (st->type) {
            case STAND_CUTTER:
                st->timer += dt;
                if (st->timer >= st->duration) {
                    it->foodState = FOOD_STATE_PREPPED;
                    releaseItemFromStand(i);
                }
                break;
            case STAND_DEVEINER:
                st->timer += dt;
                if (st->timer >= st->duration) {
                    it->foodState = FOOD_STATE_PREPPED;
                    releaseItemFromStand(i);
                }
                break;
            case STAND_RICE_WASHER:
                st->timer += dt;
                if (st->timer >= st->duration) {
                    it->foodState = FOOD_STATE_WASHED;
                    releaseItemFromStand(i);
                }
                break;
            case STAND_POT_WASHER:
                st->timer += dt;
                if (st->timer >= st->duration) {
                    cleanPotStock = cleanPotStock < 6 ? cleanPotStock + 1 : cleanPotStock;
                    destroyItem(st->itemIndex);
                    releaseItemFromStand(i);
                }
                break;
            case STAND_PAN:
            case STAND_STOVE_A:
            case STAND_STOVE_B:
            case STAND_STOVE_C:
                it->processTimer += dt;
                if (!it->onFire && it->processTimer >= st->duration && it->foodState != FOOD_STATE_COOKED) {
                    it->foodState = FOOD_STATE_COOKED;
                }
                if (!it->onFire && it->processTimer >= st->duration + 0.5f) {
                    triggerFire(i, st->itemIndex);
                }
                if (it->onFire) {
                    it->fireTimer += dt;
                    if (it->fireTimer >= 30.0f) {
                        it->foodState = FOOD_STATE_CHARRED;
                        it->usable = false;
                    }
                }
                break;
            default: break;
        }
    }
}

static void refreshHighlights(void) {
    for (int s = 0; s < STAND_COUNT; ++s) {
        Stand *st = &stands[s];
        st->highlight = false;
        if (player.heldItem >= 0 && standAcceptsItem(s, player.heldItem)) {
            st->highlight = true;
            continue;
        }
        for (int i = 0; i < MAX_ITEMS; ++i) {
            Item *it = &items[i];
            if (!it->active || it->isHeld) continue;
            if (CheckCollisionRecs(itemRect(it), st->area) && standAcceptsItem(s, i)) {
                st->highlight = true;
                break;
            }
        }
    }
}

static void updateOrders(float dt) {
    for (int i = 0; i < MAX_ORDERS; ++i) {
        Order *o = &orders[i];
        if (!o->active) continue;
        o->timer -= dt;
        if (o->timer <= 0.0f) {
            failOrder(i);
        }
    }
    int activeCount = 0;
    for (int i = 0; i < MAX_ORDERS; ++i) if (orders[i].active) activeCount++;
    if (activeCount < MAX_ORDERS) {
        orderSpawnTimer -= dt;
        if (orderSpawnTimer <= 0.0f) spawnOrder();
    }
}

static void updateAlarmFx(float dt) {
    if (alarmActive) {
        alarmFlash += dt * 6.0f;
        shakeTimer += dt;
    } else {
        alarmFlash = 0.0f;
        shakeTimer = 0.0f;
    }
}

/* ---------- Lifecycle ---------- */

static void mg_init(void) {
    memset(items, 0, sizeof(items));
    memset(stands, 0, sizeof(stands));
    memset(orders, 0, sizeof(orders));
    memset(dirtyQueue, 0, sizeof(dirtyQueue));
    cleanPlateStock = 5;
    cleanPotStock = 6;
    levelTimer = 240.0f;
    levelComplete = false;
    levelFailed = false;
    alarmActive = false;
    alarmFlash = 0.0f;
    shakeTimer = 0.0f;
    extinguisherIndex = -1;
    initLayout();
    resetOrders();
    resetPlayer();
    ensureExtinguisher();

    if (!IsAudioDeviceReady()) InitAudioDevice();
    if (FileExists("assets/youre_cooked/sfx/alarm.ogg")) {
        alarmSound = LoadSound("assets/youre_cooked/sfx/alarm.ogg");
        alarmSoundLoaded = alarmSound.frameCount > 0;
    } else {
        alarmSoundLoaded = false;
    }

    if (FileExists("assets/youre_cooked/backgrounds/lvl1.png")) {
        Image img = LoadImage("assets/youre_cooked/backgrounds/lvl1.png");
        if (img.data) {
            backgroundTex = LoadTextureFromImage(img);
            backgroundLoaded = backgroundTex.id != 0;
            UnloadImage(img);
        }
    } else {
        backgroundLoaded = false;
    }
}

static void mg_update(float dt) {
    if (levelComplete || levelFailed) return;
    levelTimer -= dt;
    if (levelTimer <= 0.0f) {
        levelFailed = true;
        return;
    }
    updatePlayerMovement(dt);
    handleInteraction();
    combinePortionsWithPlates();
    combineIngredientsWithPots();
    autoAttachLooseItems();
    updateStands(dt);
    updateOrders(dt);
    updateDirtyQueue(dt);
    refreshHighlights();
    updateAlarmFx(dt);
}

static Vector2 shakeOffset(void) {
    if (!alarmActive) return (Vector2){0};
    float amp = 6.0f;
    return (Vector2){
        sinf(shakeTimer * 25.0f) * amp,
        cosf(shakeTimer * 23.0f) * amp
    };
}

static void drawCheckeredFloor(Vector2 offset) {
    Color a = (Color){ 237, 176, 116, 255 };
    Color b = (Color){ 232, 162, 92, 255 };
    for (float y = 0; y < kitchenArea.height; y += kitchenTile) {
        for (float x = 0; x < kitchenArea.width; x += kitchenTile) {
            int gx = (int)(x / kitchenTile);
            int gy = (int)(y / kitchenTile);
            Color col = ((gx + gy) % 2 == 0) ? a : b;
            float w = fminf(kitchenTile, kitchenArea.width - x);
            float h = fminf(kitchenTile, kitchenArea.height - y);
            DrawRectangle((int)(kitchenArea.x + x + offset.x),
                          (int)(kitchenArea.y + y + offset.y),
                          (int)w, (int)h, col);
        }
    }
}

static void drawWallsAndCounters(Vector2 offset) {
    for (int i = 0; i < wallRectCount; ++i) {
        Rectangle r = wallRects[i].rect;
        r.x += offset.x; r.y += offset.y;
        DrawRectangleRec(r, wallRects[i].color);
    }
    for (int i = 0; i < counterRectCount; ++i) {
        Rectangle r = counterRects[i].rect;
        r.x += offset.x; r.y += offset.y;
        DrawRectangleRec(r, counterRects[i].color);
        DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, (Color){ 15, 30, 40, 120 });
    }
}

static void drawFoodIcon(FoodType food, Vector2 pos, float scale) {
    Color c = foodColor(food);
    float size = 20.0f * scale;
    switch (food) {
        case FOOD_RICE:
            DrawRectangleRounded((Rectangle){ pos.x - size, pos.y - size * 0.4f, size * 2, size * 0.8f }, 0.4f, 6, c);
            break;
        case FOOD_PASTA:
            DrawEllipse(pos.x, pos.y, size * 1.2f, size * 0.6f, c);
            break;
        case FOOD_FISH:
            DrawEllipse(pos.x, pos.y, size, size * 0.6f, c);
            DrawTriangle((Vector2){ pos.x + size, pos.y }, (Vector2){ pos.x + size * 1.4f, pos.y - size * 0.4f },
                         (Vector2){ pos.x + size * 1.4f, pos.y + size * 0.4f }, c);
            break;
        case FOOD_SHRIMP:
            DrawCircleGradient((int)pos.x, (int)pos.y, size, c, (Color){255, 120, 120, 255});
            DrawCircleLines((int)pos.x, (int)pos.y, size, (Color){255, 200, 200, 255});
            break;
        case FOOD_BEEF:
            DrawRectangleRounded((Rectangle){ pos.x - size * 0.8f, pos.y - size * 0.6f,
                                              size * 1.6f, size * 1.2f }, 0.4f, 8, c);
            break;
        default:
            DrawCircleLines((int)pos.x, (int)pos.y, size * 0.5f, (Color){ 200, 200, 200, 255 });
            break;
    }
}

static void drawStandIcon(const Stand *st, Rectangle rect) {
    Vector2 center = { rect.x + rect.width * 0.7f, rect.y + rect.height * 0.5f };
    switch (st->type) {
        case STAND_CUTTER:
            DrawRectangle(rect.x + 8, rect.y + 10, rect.width - 16, rect.height - 20, (Color){80, 80, 80, 255});
            DrawRectangle(rect.x + 12, rect.y + rect.height/2 - 6, rect.width - 24, 12, (Color){160, 160, 160, 255});
            break;
        case STAND_PAN:
            DrawCircle(center.x, center.y, 20, (Color){40, 40, 40, 255});
            DrawRectangle(rect.x + rect.width - 26, center.y - 4, 18, 8, (Color){120, 120, 120, 255});
            break;
        case STAND_STOVE_A:
        case STAND_STOVE_B:
        case STAND_STOVE_C:
            DrawRectangle(rect.x + 10, rect.y + 10, rect.width - 20, rect.height - 20, (Color){60, 60, 60, 255});
            DrawCircleLines(center.x - 20, center.y - 10, 14, ORANGE);
            DrawCircleLines(center.x + 6, center.y + 8, 14, ORANGE);
            break;
        case STAND_PLATE_CABINET:
            DrawEllipse(rect.x + 40, rect.y + rect.height/2, 30, 14, WHITE);
            DrawEllipse(rect.x + 80, rect.y + rect.height/2, 30, 14, WHITE);
            break;
        case STAND_POT_CABINET:
            DrawRectangleRounded((Rectangle){ rect.x + 20, rect.y + 12, rect.width - 40, rect.height - 24 }, 0.2f, 8, (Color){70, 70, 70, 255});
            break;
        case STAND_FAUCET:
            DrawRectangle(rect.x + 16, rect.y + 12, rect.width - 32, rect.height - 24, (Color){80, 120, 180, 255});
            DrawCircle(rect.x + rect.width/2, rect.y + rect.height/2, 10, LIGHTGRAY);
            break;
        case STAND_DELIVERY:
            DrawRectangleRounded((Rectangle){ rect.x + 12, rect.y + 18, rect.width - 24, rect.height - 36 }, 0.35f, 8, (Color){200, 200, 200, 255});
            DrawText("Envoi", rect.x + 20, rect.y + rect.height - 32, 18, MAROON);
            break;
        case STAND_EXTINGUISHER:
            DrawRectangleRounded((Rectangle){ rect.x + rect.width/2 - 12, rect.y + 16, 24, rect.height - 32 }, 0.4f, 8, RED);
            DrawRectangle(rect.x + rect.width/2 + 12, rect.y + 20, 10, 6, DARKGRAY);
            break;
        default:
            drawFoodIcon(st->type == STAND_RICE ? FOOD_RICE :
                         st->type == STAND_PASTA ? FOOD_PASTA :
                         st->type == STAND_FISH ? FOOD_FISH :
                         st->type == STAND_SHRIMP ? FOOD_SHRIMP :
                         st->type == STAND_BEEF ? FOOD_BEEF : FOOD_NONE,
                         (Vector2){ rect.x + rect.width/2, rect.y + rect.height/2 }, 1.0f);
            break;
    }
}

static void drawPlayerSprite(Vector2 offset) {
    Vector2 pos = Vector2Add(player.pos, offset);
    DrawCircle(pos.x, pos.y - 28, 18, (Color){ 255, 235, 220, 255 });
    DrawRectangleRounded((Rectangle){ pos.x - 16, pos.y - 20, 32, 46 }, 0.4f, 6, (Color){ 255, 220, 120, 255 });
    DrawRectangleRounded((Rectangle){ pos.x - 18, pos.y - 44, 36, 12 }, 0.6f, 6, WHITE);
    DrawRectangleRounded((Rectangle){ pos.x - 12, pos.y - 56, 24, 18 }, 0.6f, 6, WHITE); // hat
    if (player.heldItem >= 0) {
        DrawCircleLines(pos.x, pos.y - 50, 22, GOLD);
    }
}

static void drawOrderTicket(const Order *order, Rectangle card) {
    DrawRectangleRounded(card, 0.2f, 8, (Color){ 250, 250, 245, 240 });
    DrawRectangleRounded((Rectangle){ card.x, card.y, card.width, 26 }, 0.2f, 8, (Color){ 240, 240, 240, 255 });
    if (!order->active) {
        DrawText("- libre -", card.x + 20, card.y + 10, 16, DARKGRAY);
        return;
    }
    DrawText("Commande", card.x + 12, card.y + 8, 16, DARKGRAY);
    drawFoodIcon(order->base, (Vector2){ card.x + 36, card.y + 56 }, 1.0f);
    DrawText("+", card.x + 68, card.y + 50, 22, GRAY);
    drawFoodIcon(order->protein == FOOD_NONE ? FOOD_NONE : order->protein,
                 (Vector2){ card.x + 100, card.y + 56 }, 1.0f);
    float ratio = Clamp(order->timer / order->duration, 0.0f, 1.0f);
    DrawRectangleRounded((Rectangle){ card.x + 16, card.y + card.height - 30,
                                      (card.width - 32) * ratio, 12 }, 0.4f, 6,
                         (Color){ (unsigned char)(220 - ratio * 80), (unsigned char)(150 + ratio * 70), 80, 255 });
}

static void drawOrders(Vector2 offset) {
    float cardWidth = 140.0f;
    float spacing = 24.0f;
    float totalWidth = cardWidth * MAX_ORDERS + spacing * (MAX_ORDERS - 1);
    float startX = kitchenArea.x + kitchenArea.width * 0.5f - totalWidth * 0.5f;
    float startY = kitchenArea.y - 90.0f;
    DrawText("Commandes", (int)(startX + offset.x), (int)(startY - 28 + offset.y), 22, WHITE);
    for (int i = 0; i < MAX_ORDERS; ++i) {
        Rectangle card = {
            startX + i * (cardWidth + spacing),
            startY,
            cardWidth,
            96
        };
        card.x += offset.x * 0.2f;
        card.y += offset.y * 0.2f;
        drawOrderTicket(&orders[i], card);
    }
}

static void drawItems(Vector2 offset) {
    for (int i = 0; i < MAX_ITEMS; ++i) {
        Item *it = &items[i];
        if (!it->active) continue;
        Rectangle r = itemRect(it);
        r.x += offset.x;
        r.y += offset.y;
        Color c = (it->type == ITEM_PLATE) ? WHITE : foodColor(it->food);
        if (it->type == ITEM_COOKWARE) c = (Color){ 90, 90, 90, 255 };
        if (it->type == ITEM_EXTINGUISHER) c = (Color){ 200, 40, 40, 255 };
        if (it->type == ITEM_PLATE) {
            DrawRectangleRounded(r, 0.5f, 6, c);
            if (it->plateBase != FOOD_NONE) {
                DrawText(TextFormat("%s", foodName(it->plateBase)), (int)r.x + 4, (int)r.y - 16, 14, GOLD);
            }
            if (it->plateProtein != FOOD_NONE) {
                DrawText(TextFormat("+ %s", foodName(it->plateProtein)), (int)r.x + 4, (int)r.y + 4, 14, ORANGE);
            }
        } else {
            DrawRectangleRounded(r, 0.3f, 6, c);
        }
        if (it->onFire) {
            DrawRectangleLinesEx(r, 2, RED);
        }
    }
}

static void drawStands(Vector2 offset) {
    for (int i = 0; i < STAND_COUNT; ++i) {
        Stand *st = &stands[i];
        Rectangle r = st->area;
        r.x += offset.x;
        r.y += offset.y;
        Color col = st->baseColor;
        DrawRectangleRounded(r, 0.15f, 6, col);
        DrawText(st->label, (int)r.x + 6, (int)r.y + 6, 16, RAYWHITE);
        drawStandIcon(st, r);
        if (st->highlight) DrawRectangleLinesEx(r, 2, YELLOW);
        if (st->itemIndex >= 0 && st->duration > 0) {
            float pct = Clamp(stands[i].timer / st->duration, 0.0f, 1.0f);
            DrawRectangle((int)r.x + 6, (int)(r.y + r.height - 12), (int)((r.width - 12) * pct), 6, GREEN);
        }
        if (st->onFire) {
            DrawRectangleLinesEx(r, 3, RED);
        }
    }
}

static void drawHUD(Vector2 offset) {
    int sw = GetScreenWidth();
    Rectangle leftPanel = { 36 + offset.x, GetScreenHeight() - 120 + offset.y, 320, 80 };
    Rectangle rightPanel = { sw - 360 + offset.x, GetScreenHeight() - 120 + offset.y, 320, 80 };
    DrawRectangleRounded(leftPanel, 0.2f, 8, (Color){ 30, 30, 30, 160 });
    DrawRectangleRounded(rightPanel, 0.2f, 8, (Color){ 30, 30, 30, 160 });
    DrawText(TextFormat("Assiettes: %d / 5", cleanPlateStock), leftPanel.x + 16, leftPanel.y + 18, 20, WHITE);
    DrawText(TextFormat("Casseroles: %d / 6", cleanPotStock), leftPanel.x + 16, leftPanel.y + 44, 20, WHITE);
    DrawText(TextFormat("Commandes: %d / %d", ordersCompleted, ORDER_TARGET), rightPanel.x + 16, rightPanel.y + 18, 20, WHITE);
    DrawText(TextFormat("Pieces: %d", coinsEarned), rightPanel.x + 16, rightPanel.y + 44, 20, GOLD);
    DrawText(TextFormat("Temps restant: %.0fs", levelTimer), sw/2 - 90 + (int)offset.x, 20 + (int)offset.y, 24, RAYWHITE);
    DrawText("E: interagir | Q: drop | Space: verser / eteindre", 40 + (int)offset.x, GetScreenHeight() - 28 + (int)offset.y, 18, LIGHTGRAY);
}

static void mg_draw(void) {
    Vector2 shake = shakeOffset();
    if (backgroundLoaded) {
        DrawTexturePro(backgroundTex,
                       (Rectangle){0, 0, (float)backgroundTex.width, (float)backgroundTex.height},
                       (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
                       (Vector2){0, 0}, 0.0f, WHITE);
    } else {
        ClearBackground((Color){ 30, 30, 40, 255 });
    }
    drawCheckeredFloor(shake);
    drawWallsAndCounters(shake);
    drawStands(shake);
    drawItems(shake);
    drawOrders(shake);

    // Player
    drawPlayerSprite(shake);

    drawHUD(shake);

    if (alarmActive) {
        float alpha = (sinf(alarmFlash) * 0.5f + 0.5f) * 120.0f + 40.0f;
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 255, 40, 40, (unsigned char)alpha });
    }

    if (levelComplete) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 0, 0, 0, 180 });
        DrawText("Service termine !", GetScreenWidth()/2 - 150, GetScreenHeight()/2 - 30, 30, GREEN);
        DrawText("Backspace pour quitter", GetScreenWidth()/2 - 170, GetScreenHeight()/2 + 10, 22, LIGHTGRAY);
    } else if (levelFailed) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 0, 0, 0, 180 });
        DrawText("Service echoue...", GetScreenWidth()/2 - 150, GetScreenHeight()/2 - 30, 30, RED);
        DrawText("Backspace pour quitter", GetScreenWidth()/2 - 170, GetScreenHeight()/2 + 10, 22, LIGHTGRAY);
    }
}

static void mg_unload(void) {
    if (backgroundLoaded && backgroundTex.id) {
        UnloadTexture(backgroundTex);
        backgroundTex = (Texture2D){0};
        backgroundLoaded = false;
    }
    if (alarmSoundLoaded) {
        UnloadSound(alarmSound);
        alarmSoundLoaded = false;
    }
}

static bool mg_isCompleted(int *coinsOut) {
    if (coinsOut) *coinsOut = levelComplete ? coinsEarned : 0;
    return levelComplete || levelFailed;
}

MinigameAPI GetMinigameYoureCooked(void) {
    MinigameAPI api = { mg_init, mg_update, mg_draw, mg_unload, mg_isCompleted };
    return api;
}

