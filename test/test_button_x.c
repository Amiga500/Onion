/*
 * theme/button_x.h: the X button made from a theme's B button (GameSwitcher's
 * X DELETE, Kids Mode's RESTART and HELP), on drawn test buttons: a disc
 * with a letter, a letter cut out of the disc, a disc with a ring around it,
 * and B images that are not buttons (empty, a wide label, four dots, a disc
 * with nothing on it).
 */
#include "onion_test.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "theme/button_x.h"

#define S 54

typedef struct {
    uint8_t px[S * S * 4];
} Img;

static void put(Img *im, int x, int y, uint8_t r, uint8_t g, uint8_t b,
                uint8_t a)
{
    uint8_t *p = im->px + (y * S + x) * 4;
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = a;
}

static const uint8_t *at(const uint8_t *px, int x, int y)
{
    return px + (y * S + x) * 4;
}

// A disc (radius 22, centre 27,27) of colour fill, with a ring of colour
// ring_c ring_w pixels wide at its edge (0: none)
static void disc(Img *im, const uint8_t fill[3], int ring_w,
                 const uint8_t ring_c[3])
{
    memset(im->px, 0, sizeof(im->px));
    for (int y = 0; y < S; y++) {
        for (int x = 0; x < S; x++) {
            int dx = x - 27, dy = y - 27, d2 = dx * dx + dy * dy;
            if (d2 > 22 * 22)
                continue;
            bool on_ring = ring_w > 0 && d2 > (22 - ring_w) * (22 - ring_w);
            const uint8_t *c = on_ring ? ring_c : fill;
            put(im, x, y, c[0], c[1], c[2], 255);
        }
    }
}

// A blocky "B": a stem and three bars, 16 x 20, centred
static bool in_letter(int x, int y)
{
    if (x < 19 || x > 34 || y < 17 || y > 36)
        return false;
    return x <= 22 || y <= 20 || (y >= 25 && y <= 28) || y >= 33 ||
           x >= 31;
}

static void letter(Img *im, const uint8_t c[3], bool cut)
{
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++)
            if (in_letter(x, y))
                put(im, x, y, cut ? 0 : c[0], cut ? 0 : c[1], cut ? 0 : c[2],
                    cut ? 0 : 255);
}

static const uint8_t RED[3] = {200, 30, 40};
static const uint8_t WHITE[3] = {255, 255, 255};
static const uint8_t BLACK[3] = {0, 0, 0};

static int same(const uint8_t *p, const uint8_t c[3], uint8_t a)
{
    return p[0] == c[0] && p[1] == c[1] && p[2] == c[2] && p[3] == a;
}

TEST(white_letter_on_a_red_disc_becomes_a_white_x)
{
    Img b, x;
    disc(&b, RED, 0, NULL);
    letter(&b, WHITE, false);
    ASSERT_TRUE(buttonx_generate(b.px, S, S, x.px));
    // the X crosses where the letter's middle was, in the letter's colour
    ASSERT_TRUE(same(at(x.px, 27, 27), WHITE, 255));
    // the B's stem (off the X's lines) is painted over with the disc colour
    ASSERT_TRUE(same(at(x.px, 20, 27), RED, 255));
    ASSERT_TRUE(same(at(x.px, 33, 27), RED, 255));
    // the disc's edge and the outside are left alone
    ASSERT_TRUE(same(at(x.px, 27, 7), RED, 255));
    ASSERT_EQ(at(x.px, 1, 1)[3], 0);
}

TEST(letter_cut_out_of_the_disc_gives_an_x_cut_out)
{
    Img b, x;
    disc(&b, RED, 0, NULL);
    letter(&b, NULL, true);
    ASSERT_TRUE(buttonx_generate(b.px, S, S, x.px));
    // the old hole is filled (no letter showing through the X)...
    ASSERT_TRUE(same(at(x.px, 20, 27), RED, 255));
    // ...and the X itself is the hole now, like the theme's B
    ASSERT_EQ(at(x.px, 27, 27)[3], 0);
}

TEST(a_ring_around_the_button_stays)
{
    Img b, x;
    disc(&b, WHITE, 3, BLACK);
    letter(&b, BLACK, false);
    ASSERT_TRUE(buttonx_generate(b.px, S, S, x.px));
    // the ring is the button's, not the letter's: still black
    ASSERT_TRUE(same(at(x.px, 27, 6), BLACK, 255));
    ASSERT_TRUE(same(at(x.px, 6, 27), BLACK, 255));
    // the button colour is white (the ring is not the commonest colour),
    // the letter is gone and the X is black
    ASSERT_TRUE(same(at(x.px, 20, 27), WHITE, 255));
    ASSERT_TRUE(same(at(x.px, 27, 27), BLACK, 255));
}

TEST(images_that_are_not_buttons_are_left_to_the_fallback)
{
    Img b, x;
    uint8_t untouched[S * S * 4];
    memset(x.px, 7, sizeof(x.px));
    memcpy(untouched, x.px, sizeof(untouched));

    // empty
    memset(b.px, 0, sizeof(b.px));
    ASSERT_FALSE(buttonx_generate(b.px, S, S, x.px));

    // a disc with nothing on it: no letter to put the X in place of
    disc(&b, RED, 0, NULL);
    ASSERT_FALSE(buttonx_generate(b.px, S, S, x.px));

    // a wide label ("B BACK"): a box 50 x 12
    memset(b.px, 0, sizeof(b.px));
    for (int y = 20; y < 32; y++)
        for (int x0 = 2; x0 < 52; x0++)
            put(&b, x0, y, 30, 30, 30, 255);
    ASSERT_FALSE(buttonx_generate(b.px, S, S, x.px));

    // four dots (a button layout drawing): mostly empty
    memset(b.px, 0, sizeof(b.px));
    for (int k = 0; k < 4; k++) {
        int cx = k == 1 ? 10 : k == 2 ? 44 : 27, cy = k == 0 ? 10 : k == 3 ? 44 : 27;
        for (int y = cy - 5; y <= cy + 5; y++)
            for (int x0 = cx - 5; x0 <= cx + 5; x0++)
                put(&b, x0, y, 200, 200, 200, 255);
    }
    ASSERT_FALSE(buttonx_generate(b.px, S, S, x.px));

    // dst stays as it was
    ASSERT_EQ(memcmp(x.px, untouched, sizeof(untouched)), 0);
}

TEST(bad_sizes_and_null_are_refused)
{
    Img b, x;
    disc(&b, RED, 0, NULL);
    letter(&b, WHITE, false);
    ASSERT_FALSE(buttonx_generate(NULL, S, S, x.px));
    ASSERT_FALSE(buttonx_generate(b.px, S, S, NULL));
    ASSERT_FALSE(buttonx_generate(b.px, 4, 4, x.px));
    ASSERT_FALSE(buttonx_generate(b.px, BUTTONX_MAX_SIDE + 1, 8, x.px));
}

int main(void)
{
    printf("\n=== theme/button_x.h ===\n");
    RUN_TEST(white_letter_on_a_red_disc_becomes_a_white_x);
    RUN_TEST(letter_cut_out_of_the_disc_gives_an_x_cut_out);
    RUN_TEST(a_ring_around_the_button_stays);
    RUN_TEST(images_that_are_not_buttons_are_left_to_the_fallback);
    RUN_TEST(bad_sizes_and_null_are_refused);
    TEST_REPORT();
    return test_failures;
}
