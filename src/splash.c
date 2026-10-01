#include "splash.h"
#include "splash_sprites.h"

// Frames as in assets/splash/reference.gif, on a 128x32 area:
//  1-7    the shoe drops in from the top left
//  8-14   it lands and the splash particles fly out
//  14-23  the name slides in from the right, pushing the shoe off
//  25-44  the name holds
//  45-49  a wipe clears the screen from the top

static const int8_t DROP[7][2] = {
    { 3, -28 }, { 6, -24 }, { 8, -20 }, { 10, -16 }, { 13, -12 }, { 14, -8 }, { 15, -4 },
};

static const sprite_t *const SPLASH_LEFT[7] = {
    &SPRITE_SPLASH_LEFT_1, &SPRITE_SPLASH_LEFT_2, &SPRITE_SPLASH_LEFT_3,
    &SPRITE_SPLASH_LEFT_4, &SPRITE_SPLASH_LEFT_5, &SPRITE_SPLASH_LEFT_6,
    &SPRITE_SPLASH_LEFT_7,
};

static const sprite_t *const SPLASH_RIGHT[7] = {
    &SPRITE_SPLASH_RIGHT_1, &SPRITE_SPLASH_RIGHT_2, &SPRITE_SPLASH_RIGHT_3,
    &SPRITE_SPLASH_RIGHT_4, &SPRITE_SPLASH_RIGHT_5, &SPRITE_SPLASH_RIGHT_6,
    &SPRITE_SPLASH_RIGHT_7,
};

#define LANDED_X        16
#define LANDED_FRAME    8
#define PUSH_FRAME      14      // name appears, shoe starts moving
#define PUSH_SPEED      5       // shoe, px per frame
#define NAME_SPEED      12      // name, px per frame
#define NAME_START_X    116
#define NAME_X          9
#define NAME_Y          9
#define WIPE_FRAME      44
#define WIPE_SPEED      3       // rows per frame

bool splash_draw(gfx_t *g, uint32_t t_ms) {
    int f = t_ms / SPLASH_FRAME_MS;
    if (f >= SPLASH_FRAMES) {
        return false;
    }
    int ox = (g->width - 128) / 2;
    int oy = (g->height - 32) / 2;
    gfx_clear(g);

    if (f >= 1 && f < LANDED_FRAME) {
        gfx_blit(g, &SPRITE_SHOE, ox + DROP[f - 1][0], oy + DROP[f - 1][1]);
    } else if (f >= LANDED_FRAME) {
        int x = LANDED_X - (f > PUSH_FRAME ? (f - PUSH_FRAME) * PUSH_SPEED : 0);
        gfx_blit(g, &SPRITE_SHOE, ox + x, oy);
    }

    if (f >= LANDED_FRAME && f < LANDED_FRAME + 7) {
        gfx_blit(g, SPLASH_LEFT[f - LANDED_FRAME], ox + 7, oy + 16);
        gfx_blit(g, SPLASH_RIGHT[f - LANDED_FRAME], ox + 53, oy + 17);
    }

    if (f >= PUSH_FRAME) {
        int x = NAME_START_X - (f - PUSH_FRAME) * NAME_SPEED;
        gfx_blit(g, &SPRITE_NAME, ox + (x > NAME_X ? x : NAME_X), oy + NAME_Y);
    }

    if (f > WIPE_FRAME) {
        gfx_fill(g, 0, 0, g->width, oy + NAME_Y + (f - WIPE_FRAME) * WIPE_SPEED, false);
    }
    return true;
}
