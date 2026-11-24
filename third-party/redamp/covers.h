#ifndef REDAMP_COVERS_H
#define REDAMP_COVERS_H
#include <stddef.h>
typedef struct { const char *path,*artist,*album; int folder_safe; } cover_job;
enum { COVER_OK, COVER_CLOCK, COVER_NETWORK, COVER_TLS, COVER_SERVER, COVER_STORAGE,
       COVER_RESPONSE, COVER_CANCELLED, COVER_BUSY };
typedef struct { int total, checked, saved, existing, missing, skipped, failed, status; } cover_result;
/* Consume one completed batch's result (independent of incremental image notifications). */
int covers_take_result(cover_result *out);
int covers_start_manual(const cover_job *jobs,int count,int save_to_folder,const char *data_dir);
/* The library's strings are copied before the worker starts. */
int covers_start(const cover_job *jobs,int count,int save_to_folder,const char *data_dir);
void covers_cancel(void);
void covers_set_paused(int paused);
void covers_poll(int *done,int *total,int *running,int *updated);
void covers_shutdown(void);
#endif
