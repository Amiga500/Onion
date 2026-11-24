#ifndef GAMEMUSIC_H
#define GAMEMUSIC_H

// Legacy recovery only. Native RedOnion integration never installs the wrapper into miyoo/lib
// (the original is kept as libpadsp_orig.so). Return 1 on success.
int gm_install(void);
int gm_uninstall(void);
int gm_installed(void);
int gm_set_consent(void); // explicit UI action before gm_install
void gm_revoke_consent(void);

// background service (redamp --service)
int service_running(void);       // pid, or 0
int service_stop(void);         // asks it to save its place and quit; waits
void service_spawn(void);        // starts it detached
int service_main(void);

#endif
