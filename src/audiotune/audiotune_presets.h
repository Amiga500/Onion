/* AudioTune shared EQ profile definitions. SPDX-License-Identifier: MIT. */
#ifndef AUDIOTUNE_PRESETS_H
#define AUDIOTUNE_PRESETS_H
#define AUDIOTUNE_EQ_BANDS 5
#define AUDIOTUNE_EQ_GAIN_LIMIT 12
#define AUDIOTUNE_PROFILE_COUNT 7
/* FLAT, SPEAKER, HEADPHONES, CLEAR, WARM, BRIGHT, CUSTOM.
   CUSTOM is loaded from settings.ini, not a built-in curve. */
static const int audiotune_preset_gains[AUDIOTUNE_PROFILE_COUNT][AUDIOTUNE_EQ_BANDS] = {
    { 0, 0, 0, 0, 0},  /* Flat */
    {-6,-2, 2, 4, 2},  /* Speaker */
    { 3, 1, 0, 2, 2},  /* Headphones */
    {-4,-2, 2, 5, 4},  /* Clear */
    { 4, 3, 0,-2,-4},  /* Warm */
    {-2,-1, 1, 4, 6},  /* Bright */
    { 0, 0, 0, 0, 0}   /* Custom: gains read from settings */
};
/* Flat is defined by the effective curve, not only the preset selector. */
static inline int audiotune_gains_all_zero(const int gains[AUDIOTUNE_EQ_BANDS]) {
    int i;
    for (i=0; i<AUDIOTUNE_EQ_BANDS; ++i) if (gains[i]!=0) return 0;
    return 1;
}
static const int audiotune_eq_freq[AUDIOTUNE_EQ_BANDS] = {125,500,1000,4000,8000};
#endif
