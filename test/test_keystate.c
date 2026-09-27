/**
 * @file test_keystate.c
 * @brief Tests for updateKeystate() and keystate_resync() (utils/keystate.h)
 *
 * Tweaks runs an item's action on A PRESSED. It used to force A back to
 * RELEASED after the action, so every key repeat of a held A arrived as a
 * new press and ran the action again ("Start/stop recorder" started and
 * stopped the recording in a loop). keystate_resync() keeps A down while
 * SDL still sees it down, and releases it when a dialog has already read
 * the release.
 *
 * Build and run: make -f Makefile.unit test_keystate
 */

#include "onion_test.h"

#include <stdbool.h>
#include <string.h>

#include "utils/keystate.h"

/* ---- SDL stubs: a scripted event queue and the SDL key array ---- */

static SDL_Event queue[32];
static int queue_len = 0, queue_pos = 0;
static Uint8 sdl_keys[320];

static void push(Uint8 type, SDLKey key)
{
    queue[queue_len].type = type;
    queue[queue_len].key.type = type;
    queue[queue_len].key.keysym.sym = key;
    queue_len++;
}

int SDL_PollEvent(SDL_Event *event)
{
    if (queue_pos >= queue_len)
        return 0;
    *event = queue[queue_pos++];
    /* SDL updates its key array as it hands out key events */
    if (event->type == SDL_KEYDOWN)
        sdl_keys[event->key.keysym.sym] = 1;
    else if (event->type == SDL_KEYUP)
        sdl_keys[event->key.keysym.sym] = 0;
    return 1;
}

Uint8 *SDL_GetKeyState(int *numkeys)
{
    if (numkeys != NULL)
        *numkeys = 320;
    return sdl_keys;
}

static KeyState keystate[320];
static int actions;

static void reset(void)
{
    queue_len = queue_pos = 0;
    memset(sdl_keys, 0, sizeof(sdl_keys));
    memset(keystate, 0, sizeof(keystate));
    actions = 0;
}

/* One event per poll, as Tweaks does: an action on A PRESSED, then the
 * key resynchronised. */
static void run_events(void (*action)(void))
{
    bool quit = false;
    SDLKey changed = SDLK_UNKNOWN;
    while (queue_pos < queue_len) {
        int saved_len = queue_len;
        queue_len = queue_pos + 1; /* hand out a single event */
        if (updateKeystate(keystate, &quit, true, &changed) && keystate[SDLK_SPACE] == PRESSED) {
            actions++;
            if (action != NULL)
                action();
            keystate[SDLK_SPACE] = keystate_resync(SDLK_SPACE);
        }
        queue_len = saved_len;
    }
}

TEST(held_key_runs_the_action_once) {
    reset();
    push(SDL_KEYDOWN, SDLK_SPACE);
    for (int i = 0; i < 6; i++)
        push(SDL_KEYDOWN, SDLK_SPACE); /* SDL key repeat */
    push(SDL_KEYUP, SDLK_SPACE);
    run_events(NULL);
    ASSERT_EQ(actions, 1);
    ASSERT_EQ(keystate[SDLK_SPACE], RELEASED);
}

TEST(each_new_press_runs_the_action) {
    reset();
    push(SDL_KEYDOWN, SDLK_SPACE);
    push(SDL_KEYUP, SDLK_SPACE);
    push(SDL_KEYDOWN, SDLK_SPACE);
    push(SDL_KEYDOWN, SDLK_SPACE);
    push(SDL_KEYUP, SDLK_SPACE);
    run_events(NULL);
    ASSERT_EQ(actions, 2);
}

/* An action whose dialog reads the release itself: the next press must
 * still count (why Tweaks forced RELEASED in the first place). */
static void dialog_reads_release(void)
{
    SDL_Event ev;
    if (queue_pos < queue_len && queue[queue_pos].type == SDL_KEYUP) {
        int saved_len = queue_len;
        queue_len = queue_pos + 1;
        SDL_PollEvent(&ev);
        queue_len = saved_len;
    }
}

TEST(press_after_a_dialog_is_not_lost) {
    reset();
    push(SDL_KEYDOWN, SDLK_SPACE);
    push(SDL_KEYUP, SDLK_SPACE); /* read by the dialog */
    push(SDL_KEYDOWN, SDLK_SPACE);
    push(SDL_KEYUP, SDLK_SPACE);
    run_events(dialog_reads_release);
    ASSERT_EQ(actions, 2);
}

TEST(resync_follows_sdl) {
    reset();
    ASSERT_EQ(keystate_resync(SDLK_SPACE), RELEASED);
    sdl_keys[SDLK_SPACE] = 1;
    ASSERT_EQ(keystate_resync(SDLK_SPACE), PRESSED);
}

int main(void)
{
    printf("\n=== keystate.h Unit Tests ===\n\n");

    RUN_TEST(held_key_runs_the_action_once);
    RUN_TEST(each_new_press_runs_the_action);
    RUN_TEST(press_after_a_dialog_is_not_lost);
    RUN_TEST(resync_follows_sdl);

    TEST_REPORT();
    return test_failures;
}
