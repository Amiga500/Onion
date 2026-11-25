// The play list, plus a background thread that reads tags for its entries.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include "playlist.h"
#include "decode.h"
#include "radio.h"

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static pthread_t worker;
static int running;
static pl_item *items;
static int count, cap;
static unsigned gen, meta_revision;
static int hint;

void pl_lock(void) { pthread_mutex_lock(&mtx); }
void pl_unlock(void) { pthread_mutex_unlock(&mtx); }

static void name_from_path(const char *path, char *out, int n)
{
    meta_display_name(NULL, path, out, n);
}

static void *meta_worker(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&mtx);
    while (running) {
        int idx = -1;
        // first unread item at or after the hint, then from the top
        for (int k = 0; k < count && idx < 0; k++) {
            int i = (hint + k) % count;
            if (!items[i].has_meta) idx = i;
        }
        if (idx < 0) { pthread_cond_wait(&cv, &mtx); continue; }
        char *path = strdup(items[idx].path);
        if (!path) { items[idx].has_meta = 1; continue; }
        unsigned g = gen;
        pthread_mutex_unlock(&mtx);
        track_meta m;
        int ok = meta_read(path, &m);
        pthread_mutex_lock(&mtx);
        if (g == gen && idx < count && !strcmp(items[idx].path, path)) {
            items[idx].has_meta = 1;
            meta_revision++;
            if (ok) {
                meta_display_name(&m, path, items[idx].name, sizeof items[idx].name);
                items[idx].dur_ms = m.dur_ms;
            }
        }
        free(path);
    }
    pthread_mutex_unlock(&mtx);
    return NULL;
}

void pl_init(void)
{
    running = 1;
    pthread_create(&worker, NULL, meta_worker, NULL);
}

void pl_quit(void)
{
    pthread_mutex_lock(&mtx);
    running = 0;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mtx);
    pthread_join(worker, NULL);
}

unsigned pl_revision(void)
{
    pthread_mutex_lock(&mtx);
    unsigned r=gen+meta_revision;
    pthread_mutex_unlock(&mtx);
    return r;
}

int pl_count(void)
{
    pthread_mutex_lock(&mtx);
    int n = count;
    pthread_mutex_unlock(&mtx);
    return n;
}

void pl_clear(void)
{
    pthread_mutex_lock(&mtx);
    for (int i = 0; i < count; i++) free(items[i].path);
    count = 0;
    gen++;
    pthread_mutex_unlock(&mtx);
}

static int add_locked(const char *path)
{
    if (count >= 10000) return 0;
    if (count == cap) {
        int next = cap ? cap * 2 : 256;
        if (next > 10000) next = 10000;
        pl_item *more = realloc(items, sizeof *items * (size_t)next);
        if (!more) return 0;
        items = more;
        cap = next;
    }
    char *copy = strdup(path);
    if (!copy) return 0;
    pl_item *it = &items[count++];
    memset(it, 0, sizeof *it);
    it->path = copy;
    if (radio_is_url(path)) {
        const char *host=strstr(path,"://")+3;
        const char *end=strchr(host,'/');
        snprintf(it->name,sizeof it->name,"Radio: %.*s",end?(int)(end-host):(int)strlen(host),host);
    } else name_from_path(path, it->name, sizeof it->name);
    return 1;
}

int pl_add_named(const char *path,const char *title)
{
    pthread_mutex_lock(&mtx);
    int ok=add_locked(path);
    if(ok){if(title&&title[0])snprintf(items[count-1].name,sizeof items[count-1].name,"%s",title);gen++;pthread_cond_broadcast(&cv);}
    pthread_mutex_unlock(&mtx);return ok;
}

int pl_add(const char *path)
{
    pthread_mutex_lock(&mtx);
    int ok = add_locked(path);
    if (ok) { gen++; pthread_cond_broadcast(&cv); }
    pthread_mutex_unlock(&mtx);
    return ok;
}

int name_cmp(const char *a, const char *b)
{
    // case-insensitive, numbers compared by value ("2 x" before "10 x")
    while (*a && *b) {
        if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
            char *ea, *eb;
            unsigned long long va = strtoull(a, &ea, 10), vb = strtoull(b, &eb, 10);
            if (va != vb) return va < vb ? -1 : 1;
            a = ea; b = eb;
            continue;
        }
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int cmp_str(const void *x, const void *y) { return name_cmp(*(char *const *)x, *(char *const *)y); }

static int add_dir_rec(const char *dir, int recursive, int depth, int *budget)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    char **files = NULL, **dirs = NULL;
    int nf = 0, nd = 0, cf = 0, cd = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!recursive || depth >= 8) continue;
            if (nd == cd) {
                int next = cd ? cd * 2 : 16;
                char **more = realloc(dirs, sizeof(char *) * (size_t)next);
                if (!more) continue;
                dirs = more; cd = next;
            }
            char *name = strdup(e->d_name);
            if (name) dirs[nd++] = name;
        } else if (dec_supported(e->d_name)) {
            if (nf == cf) {
                int next = cf ? cf * 2 : 64;
                char **more = realloc(files, sizeof(char *) * (size_t)next);
                if (!more) continue;
                files = more; cf = next;
            }
            char *name = strdup(e->d_name);
            if (name) files[nf++] = name;
        }
    }
    closedir(d);
    if (nf) qsort(files, nf, sizeof(char *), cmp_str);
    if (nd) qsort(dirs, nd, sizeof(char *), cmp_str);
    int added = 0;
    for (int i = 0; i < nf; i++) {
        if (*budget > 0) {
            char p[1024];
            snprintf(p, sizeof p, "%s/%s", dir, files[i]);
            pthread_mutex_lock(&mtx);
            int ok = add_locked(p);
            pthread_mutex_unlock(&mtx);
            if (ok) { added++; (*budget)--; }
        }
        free(files[i]);
    }
    for (int i = 0; i < nd; i++) {
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", dir, dirs[i]);
        if (*budget > 0) added += add_dir_rec(p, recursive, depth + 1, budget);
        free(dirs[i]);
    }
    free(files);
    free(dirs);
    return added;
}

static int has_dir_tracks(const char *dir, int recursive, int depth)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while (!found && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char path[1024];
        if (snprintf(path, sizeof path, "%s/%s", dir, e->d_name) >= (int)sizeof path) continue;
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISREG(st.st_mode) && dec_supported(e->d_name)) found = 1;
        else if (S_ISDIR(st.st_mode) && recursive && depth < 8)
            found = has_dir_tracks(path, recursive, depth + 1);
    }
    closedir(d);
    return found;
}

int pl_dir_has_tracks(const char *dir, int recursive)
{
    return has_dir_tracks(dir, recursive, 0);
}

static int playlist_is_pls(const char *path)
{
    const char *e=strrchr(path,'.');
    return e && !strcasecmp(e,".pls");
}
static int playlist_entry(const char *line,int pls,char *out,size_t cap)
{
    const char *p=line;
    while (*p==' ' || *p=='\t') p++;
    if (pls) {
        if (strncasecmp(p,"File",4)) return 0;
        p+=4;
        if (!(*p>='0' && *p<='9')) return 0;
        while (*p>='0' && *p<='9') p++;
        if (*p++!='=') return 0;
    } else if (!*p || *p=='#') return 0;
    while (*p==' ' || *p=='\t') p++;
    if (!*p) return 0;
    snprintf(out,cap,"%s",p);
    size_t n=strlen(out);
    while (n && (out[n-1]==' ' || out[n-1]=='\t')) out[--n]=0;
    return n>0;
}
int pl_m3u_has_tracks(const char *path)
{
    FILE *f=fopen(path,"r");
    if (!f) return 0;
    char line[2048],parsed[2048];
    int found=0,pls=playlist_is_pls(path);
    while (fgets(line,sizeof line,f)) {
        if (!strchr(line,'\n') && !feof(f)) { int c;while ((c=fgetc(f))!='\n' && c!=EOF); continue; }
        line[strcspn(line,"\r\n")]=0;
        if (playlist_entry(line,pls,parsed,sizeof parsed)) { found=1;break; }
    }
    fclose(f);
    return found;
}

int pl_add_dir(const char *dir, int recursive)
{
    int budget = 10000;
    int n = add_dir_rec(dir, recursive, 0, &budget);
    pthread_mutex_lock(&mtx);
    gen++;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mtx);
    return n;
}

int pl_get(int i, pl_item *out)
{
    pthread_mutex_lock(&mtx);
    int ok = i >= 0 && i < count;
    if (ok) *out = items[i];
    pthread_mutex_unlock(&mtx);
    return ok;
}

const char *pl_path(int i)
{
    pthread_mutex_lock(&mtx);
    const char *p = i >= 0 && i < count ? items[i].path : NULL;
    pthread_mutex_unlock(&mtx);
    return p;
}

void pl_set_hint(int first)
{
    pthread_mutex_lock(&mtx);
    hint = first < 0 ? 0 : first;
    pthread_mutex_unlock(&mtx);
}

long long pl_total_ms(int *complete)
{
    pthread_mutex_lock(&mtx);
    long long t = 0;
    int all = 1;
    for (int i = 0; i < count; i++) { t += items[i].dur_ms; if (!items[i].has_meta) all = 0; }
    pthread_mutex_unlock(&mtx);
    if (complete) *complete = all;
    return t;
}

void pl_remove(int i)
{
    pthread_mutex_lock(&mtx);
    if (i >= 0 && i < count) {
        free(items[i].path);
        memmove(items + i, items + i + 1, sizeof *items * (count - i - 1));
        count--;
        gen++;
    }
    pthread_mutex_unlock(&mtx);
}

int pl_save_m3u(const char *path)
{
    char tmp[1200];
    if(snprintf(tmp,sizeof tmp,"%s.tmp.XXXXXX",path)>=(int)sizeof tmp)return 0;
    int fd=mkstemp(tmp);if(fd<0)return 0;
    FILE *f=fdopen(fd,"w");if(!f){close(fd);unlink(tmp);return 0;}
    int ok=fputs("#EXTM3U\n",f)>=0;
    pthread_mutex_lock(&mtx);
    for(int i=0;ok&&i<count;i++){
        if(strpbrk(items[i].path,"\r\n")){ok=0;break;}
        if(radio_is_url(items[i].path)&&fprintf(f,"#EXTINF:-1,%.*s\n",(int)strcspn(items[i].name,"\r\n"),items[i].name)<0)ok=0;
        if(fprintf(f,"%s\n",items[i].path)<0)ok=0;
    }
    pthread_mutex_unlock(&mtx);
    if(fflush(f)||fsync(fd))ok=0;
    if(fclose(f))ok=0;
    if(ok&&rename(tmp,path)==0)return 1;
    unlink(tmp);return 0;
}

int pl_load_m3u(const char *path)
{
    FILE *f=fopen(path,"r");
    if (!f) return 0;
    char line[2048],item[2048],base[1024],next_title[160]="";
    snprintf(base,sizeof base,"%s",path);
    char *sl=strrchr(base,'/');
    if (sl) *sl=0; else strcpy(base,".");
    int pls=playlist_is_pls(path), n=0;
    while (fgets(line,sizeof line,f)) {
        if (!strchr(line,'\n') && !feof(f)) { int c;while ((c=fgetc(f))!='\n' && c!=EOF); continue; }
        line[strcspn(line,"\r\n")]=0;
        if (!pls && !strncmp(line,"#EXTINF:",8)) {
            const char *comma=strchr(line,',');
            if (comma && comma[1]) snprintf(next_title,sizeof next_title,"%s",comma+1);
            continue;
        }
        if (!playlist_entry(line,pls,item,sizeof item)) continue;
        char full[2048];
        if (radio_is_url(item) || item[0]=='/') snprintf(full,sizeof full,"%s",item);
        else if (strstr(item,"://")) continue; /* unsupported URL schemes */
        else if (snprintf(full,sizeof full,"%s/%s",base,item)>=(int)sizeof full) continue;
        if (!radio_is_url(full)) {
            for (char *c=full;*c;c++) if (*c=='\\') *c='/';
        }
        pthread_mutex_lock(&mtx);
        int ok=add_locked(full);
        if (ok && next_title[0])
            snprintf(items[count-1].name,sizeof items[count-1].name,"%s",next_title);
        pthread_mutex_unlock(&mtx);
        n+=ok; next_title[0]=0;
    }
    fclose(f);
    pthread_mutex_lock(&mtx);
    gen++; pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mtx);
    return n;
}
