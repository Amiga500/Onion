/* AudioTune shared EQ profile definitions. SPDX-License-Identifier: MIT. */
#ifndef AUDIOTUNE_PRESETS_H
#define AUDIOTUNE_PRESETS_H
#define AUDIOTUNE_EQ_BANDS 5
#define AUDIOTUNE_EQ_GAIN_LIMIT 12
#define AUDIOTUNE_PROFILE_COUNT 7
/* FLAT, PRESENCE, FULL, CLEAR, WARM, BRIGHT, CUSTOM.
   Slots 1 and 2 keep their existing numeric IDs for settings compatibility.
   All EQ curves are independent of speaker/headphone output selection.
   CUSTOM is loaded from audiotune.ini, not a built-in curve. */
static const int audiotune_preset_gains[AUDIOTUNE_PROFILE_COUNT][AUDIOTUNE_EQ_BANDS] = {
    { 0, 0, 0, 0, 0},  /* Flat */
    {-8,-4, 4, 7, 4},  /* Presence */
    { 6, 3, 0, 4, 4},  /* Full */
    {-6,-3, 4, 8, 6},  /* Clear */
    { 7, 5, 1,-3,-6},  /* Warm */
    {-3,-2, 2, 7, 9},  /* Bright */
    { 0, 0, 0, 0, 0}   /* Custom: gains read from settings */
};
/* Flat is defined by the effective curve, not only the preset selector. */
static inline int audiotune_gains_all_zero(const int gains[AUDIOTUNE_EQ_BANDS]) {
    int i;
    for (i=0; i<AUDIOTUNE_EQ_BANDS; ++i) if (gains[i]!=0) return 0;
    return 1;
}
static const int audiotune_eq_freq[AUDIOTUNE_EQ_BANDS] = {250,500,1000,4000,8000};
#endif
