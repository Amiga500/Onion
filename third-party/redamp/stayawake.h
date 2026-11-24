#ifndef REDAMP_STAYAWAKE_H
#define REDAMP_STAYAWAKE_H
/* Onion/RedOnion keymon convention. Never delete another app's flag. */
void stayawake_update(int playing, int standby);
void stayawake_release(void);
void stayawake_cleanup_stale(void);
#endif
