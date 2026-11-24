// Music in games: wrapper install/uninstall and the background service.
//
// The service keeps playing RedAmp's playlist after the player is closed. It
// decodes into shared memory (bgm_shm.h); the libpadsp wrapper (padsp_wrap.c)
// mixes that into the sound of whatever program is playing (the menu or a
// game). When no program is playing sound, the service plays directly.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <poll.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/soundcard.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "gamemusic.h"
#include "bgm_shm.h"
#include "core.h"
#include "audio.h"
#include "playlist.h"
#include "stayawake.h"

// ---------------------------------------------------------------- files
static const char *libdir(void)
{
    const char *d = getenv("MA_LIBDIR");
    return d ? d : "/mnt/SDCARD/miyoo/lib";
}

static int exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static int has_marker(const char *path);

// Never silently restore the firmware's libpadsp: it is not compatible with
// OnionOS's audioserver on every Miyoo. Use ONLY verified OnionOS originals.
static int has_marker(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    char buf[16384];
    const char *marker = BGM_WRAP_MARKER;
    size_t ml = strlen(marker), keep = 0, n;
    int found = 0;
    while (!found && (n = fread(buf + keep, 1, sizeof buf - keep, f)) > 0) {
        size_t len = n + keep;
        for (size_t i = 0; i + ml <= len; i++) {
            if (buf[i] == marker[0] && !memcmp(buf + i, marker, ml)) {
                found = 1; break;
            }
        }
        keep = ml - 1 < len ? ml - 1 : len;
        memmove(buf, buf + len - keep, keep);
    }
    fclose(f);
    return found;
}

// Check that a library is at least a readable ELF shared object, with the
// same ELF class, endianness and CPU architecture as the bundled wrapper.
// A matching filename or simply being "not our wrapper" is NOT sufficient.
static int elf_identity(const char *path, unsigned char id[20])
{
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 64) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(id, 1, 20, f);
    fclose(f);
    if (n != 20 || memcmp(id, "\177ELF", 4) ||
        (id[4] != 1 && id[4] != 2) || (id[5] != 1 && id[5] != 2)) return 0;
    // e_type must be ET_DYN (shared object); e_machine is bytes 18..19.
    if (id[5] == 1 && (id[16] != 3 || id[17] != 0)) return 0;
    if (id[5] == 2 && (id[16] != 0 || id[17] != 3)) return 0;
    return 1;
}

static int valid_lib(const char *path, const char *reference, int original)
{
    unsigned char a[20], b[20];
    if (!elf_identity(path, a)) return 0;
    if (original && has_marker(path)) return 0;
    if (reference) {
        if (!elf_identity(reference, b) || a[4] != b[4] ||
            a[5] != b[5] || a[18] != b[18] || a[19] != b[19]) return 0;
    }
    return 1;
}

static const char *fallback_padsp(const char *wrap)
{
    static char p[600];
    path_join(p, sizeof p, data_dir, "libpadsp_onion.so");
    return valid_lib(p, wrap, 1) ? p : NULL;
}

// Use a distinct temporary filename per operation. Never overwrite a running
// program's mapped library in place; rename the completed + synced file.
static int copy_atomic(const char *src, const char *dst)
{
    char tmp[700];
    if (snprintf(tmp, sizeof tmp, "%s.tmp.XXXXXX", dst) >= (int)sizeof tmp) return 0;
    FILE *in = fopen(src, "rb");
    if (!in) return 0;
    struct stat st;
    if (fstat(fileno(in), &st) != 0 || !S_ISREG(st.st_mode)) { fclose(in); return 0; }
    int fd = mkstemp(tmp);
    if (fd < 0) { fclose(in); return 0; }
    FILE *out = fdopen(fd, "wb");
    if (!out) { close(fd); unlink(tmp); fclose(in); return 0; }
    unsigned char buf[16384];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof buf, in)) != 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    }
    if (ferror(in)) ok = 0;
    if (fflush(out) != 0 || fsync(fd) != 0) ok = 0;
    // FAT filesystems may refuse chmod; the mount mode controls permissions.
    (void)fchmod(fd, st.st_mode & 0777);
    if (fclose(out) != 0) ok = 0;
    fclose(in);
    if (ok && rename(tmp, dst) != 0) ok = 0;
    if (!ok) unlink(tmp);
    return ok;
}

static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int same = fa && fb;
    char x[4096], y[4096];
    while (same) {
        size_t na = fread(x, 1, sizeof x, fa), nb = fread(y, 1, sizeof y, fb);
        if (na != nb || memcmp(x, y, na)) same = 0;
        else if (na == 0) {
            if (ferror(fa) || ferror(fb)) same = 0;
            break;
        }
    }
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    return same;
}

// Serialize our own enable/disable/recovery operations. The service has a
// different lock so it can continue running while a setting is changed.
static void consent_path(char *out, size_t n)
{
    path_join(out, (int)n, data_dir, "game_music.consent");
}

// Only the UI's explicit confirmation creates this marker. A stale setting
// or background daemon cannot opt itself into replacing a system library.
int gm_set_consent(void)
{
    char p[600];
    consent_path(p, sizeof p);
    int fd = open(p, O_WRONLY | O_CREAT, 0600);
    if (fd < 0) return 0;
    return close(fd) == 0;
}

void gm_revoke_consent(void)
{
    char p[600];
    consent_path(p, sizeof p);
    unlink(p);
}

static int gm_lock(void)
{
    char p[600];
    path_join(p, sizeof p, data_dir, "gamemusic.lock");
    int fd = open(p, O_CREAT | O_RDWR, 0600);
    if (fd < 0) return -1;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) { close(fd); return -1; }
    return fd;
}

int gm_installed(void)
{
    char cur[600];
    snprintf(cur, sizeof cur, "%s/libpadsp.so", libdir());
    return has_marker(cur);
}

int gm_install(void)
{
    char consent[600];
    consent_path(consent, sizeof consent);
    if (!exists(consent)) {
        fprintf(stderr, "[gm] installation refused: no explicit user consent\n");
        return 0;
    }
    int lock = gm_lock();
    if (lock < 0) { fprintf(stderr, "[gm] install lock unavailable\n"); return 0; }
    char cur[600], orig[600], wrap[600], saved[600];
    snprintf(cur, sizeof cur, "%s/libpadsp.so", libdir());
    snprintf(orig, sizeof orig, "%s/libpadsp_orig.so", libdir());
    path_join(wrap, sizeof wrap, app_dir, "lib/libpadsp_wrap.so");
    path_join(saved, sizeof saved, data_dir, "libpadsp_onion.so");
    int ok = 0;
    // Never create a missing system library or replace an invalid/foreign one.
    if (!valid_lib(wrap, NULL, 0) || !has_marker(wrap) ||
        !valid_lib(cur, wrap, 0)) {
        fprintf(stderr, "[gm] wrapper/current system library missing or incompatible\n");
        goto done;
    }
    if (!has_marker(cur)) {
        // OnionOS may have been updated: preserve the *new* original, rather
        // than restoring an older backup on the next disable.
        if (!copy_atomic(cur, orig) || !same_file(cur, orig) ||
            !copy_atomic(cur, saved) || !same_file(cur, saved)) {
            fprintf(stderr, "[gm] could not verify BOTH original-library backups\n");
            goto done;
        }
    } else if (!valid_lib(orig, wrap, 1)) {
        const char *fb = fallback_padsp(wrap);
        if (!fb || !copy_atomic(fb, orig) || !same_file(fb, orig)) {
            fprintf(stderr, "[gm] no verified OnionOS original; refusing to install\n");
            goto done;
        }
    }
    if (!valid_lib(orig, wrap, 1)) goto done;
    // Also seed the persistent recovery backup when upgrading a pre-existing
    // installation that left only libpadsp_orig.so next to the wrapper.
    if (!valid_lib(saved, wrap, 1) || !same_file(orig, saved)) {
        if (!copy_atomic(orig, saved) || !same_file(orig, saved)) goto done;
    }
    if (same_file(cur, wrap)) { ok = 1; goto done; }
    if (copy_atomic(wrap, cur) && same_file(wrap, cur)) ok = 1;
    fprintf(stderr, "[gm] wrapper %s\n", ok ? "installed" : "install FAILED");
done:
    flock(lock, LOCK_UN);
    close(lock);
    return ok;
}

int gm_uninstall(void)
{
    int lock = gm_lock();
    if (lock < 0) { fprintf(stderr, "[gm] restore lock unavailable\n"); return 0; }
    char cur[600], orig[600], wrap[600];
    snprintf(cur, sizeof cur, "%s/libpadsp.so", libdir());
    snprintf(orig, sizeof orig, "%s/libpadsp_orig.so", libdir());
    path_join(wrap, sizeof wrap, app_dir, "lib/libpadsp_wrap.so");
    int ok = 0;
    // Verify architecture before restoring backups: copying a library built
    // for another CPU would make system audio unusable.
    const char *ref = valid_lib(wrap, NULL, 0) ? wrap :
                      valid_lib(cur, NULL, 0) ? cur : NULL;
    // If an Onion update already replaced our wrapper, preserve that newer
    // library. A missing or corrupt file is NEVER "restored successfully".
    if (valid_lib(cur, ref, 1)) {
        ok = 1;
    } else {
        const char *source = ref && valid_lib(orig, ref, 1) ? orig :
                             ref ? fallback_padsp(ref) : NULL;
        if (!source) {
            fprintf(stderr, "[gm] no verified original; manual repair required\n");
            goto done;
        }
        if (copy_atomic(source, cur) && same_file(source, cur) &&
            valid_lib(cur, ref, 1)) ok = 1;
    }
    if (ok) {
        char consent[600];
        consent_path(consent, sizeof consent);
        unlink(consent); // successful recovery revokes prior permission
        // Remove only the adjacent temporary backup; retain our verified
        // data/ copy for recovery if the app or system is interrupted.
        if (exists(orig) && unlink(orig) != 0) {
            fprintf(stderr, "[gm] warning: adjacent backup could not be removed\n");
        }
    }
done:
    fprintf(stderr, "[gm] original %s\n", ok ? "verified/restored" : "restore FAILED");
    flock(lock, LOCK_UN);
    close(lock);
    return ok;
}

// ---------------------------------------------------------------- service control
static void pid_path(char *out, int n) { path_join(out, n, data_dir, "service.pid"); }

int service_running(void)
{
    char p[600];
    pid_path(p, sizeof p);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    int pid = 0;
    if (fscanf(f, "%d", &pid) != 1) pid = 0;
    fclose(f);
    if (pid <= 0 || kill(pid, 0) != 0) return 0;
    // make sure it's really us (pids get reused)
    char cp[64], cmd[256] = "";
    snprintf(cp, sizeof cp, "/proc/%d/cmdline", pid);
    FILE *c = fopen(cp, "r");
    if (c) { size_t n = fread(cmd, 1, sizeof cmd - 1, c); cmd[n] = 0; fclose(c); }
    for (size_t i = 0; i + 1 < sizeof cmd && (cmd[i] || cmd[i + 1]); i++) if (!cmd[i]) cmd[i] = ' ';
    // Do not kill a different process whose stale/reused PID has --service.
    return strstr(cmd, "--service") &&
           (strstr(cmd, "/redamp") || strstr(cmd, "/miniamp")) ? pid : 0;
}

int service_stop(void)
{
    int pid = service_running();
    if (!pid) return 1;
    kill(pid, SIGTERM);
    for (int i = 0; i < 60 && service_running() == pid; i++) usleep(50000);
    if (service_running() == pid) {
        kill(pid, SIGKILL);
        for (int i = 0; i < 20 && service_running() == pid; i++) usleep(50000);
    }
    if (service_running() == pid) {
        fprintf(stderr, "[gm] could not stop the running service\n");
        return 0;
    }
    return 1;
}

void service_spawn(void)
{
    char exe[600];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0) return;
    exe[n] = 0;
    int pipefd[2];
    if (pipe(pipefd) != 0) return;
    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        char buf[24];
        snprintf(buf, sizeof buf, "%d", pipefd[1]);
        setenv("MA_SERVICE_READY_FD", buf, 1);
        // close everything but stdio so the service doesn't hold the display or input
        int ready = -1;
        const char *rd = getenv("MA_SERVICE_READY_FD");
        if (rd) ready = atoi(rd);
        for (int fd = 3; fd < 256; fd++) if (fd != ready) close(fd);
        // The service plays its own music: it must load OnionOS's original
        // library, never the wrapper now in miyoo/lib (it would mix the music
        // into itself and use it up twice as fast).
        char orig[600];
        snprintf(orig, sizeof orig, "%s/libpadsp_orig.so", libdir());
        const char *pre = getenv("LD_PRELOAD");
        if (exists(orig) && !has_marker(orig)) setenv("LD_PRELOAD", orig, 1);
        else if (pre && has_marker(pre)) {
            const char *fb = fallback_padsp(NULL);
            if (fb) setenv("LD_PRELOAD", fb, 1); else unsetenv("LD_PRELOAD");
        }
        execl(exe, exe, "--service", (char *)NULL);
        _exit(1);
    }
    close(pipefd[1]);
    if (pid > 0) {
        waitpid(pid, NULL, 0); // first fork exits once daemonized
        struct pollfd pfd = { .fd = pipefd[0], .events = POLLIN };
        char ready = 0;
        if (poll(&pfd, 1, 3000) > 0 && read(pipefd[0], &ready, 1) == 1 && ready == '1')
            fprintf(stderr, "[gm] background service ready\n");
        else fprintf(stderr, "[gm] background service did not confirm startup\n");
    }
    close(pipefd[0]);
}

// ---------------------------------------------------------------- service
static volatile sig_atomic_t svc_quit, svc_signal, svc_resumed, svc_input_resumed;
static bgm_shm *shm;
static pthread_mutex_t act = PTHREAD_MUTEX_INITIALIZER;
static int my_pid;

static void on_term(int s) { (void)s; svc_quit = 1; svc_signal = 1; }
static void on_continue(int s) { (void)s; svc_resumed=svc_input_resumed=1; }

static uint32_t now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
}

static void say(const char *fmt, const char *a)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    fprintf(stderr, "[%02d:%02d:%02d] ", tm.tm_hour, tm.tm_min, tm.tm_sec);
    fprintf(stderr, fmt, a);
    fputc('\n', stderr);
}

// programs that play sound through an unwrapped libpadsp (we'd fight them for the audioserver)
static int foreign_audio(void)
{
    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    char line[512];
    while (!found && (e = readdir(d))) {
        if (!isdigit((unsigned char)e->d_name[0])) continue;
        int pid = atoi(e->d_name);
        if (pid == my_pid) continue;
        char p[64];
        snprintf(p, sizeof p, "/proc/%d/maps", pid);
        FILE *f = fopen(p, "r");
        if (!f) continue;
        int padsp = 0, orig = 0;
        while (fgets(line, sizeof line, f)) {
            /* Native RedOnion launcher maps libpadsp_wrap.so alongside the
             * untouched libpadsp.so, instead of libpadsp_orig.so. */
            if (strstr(line, "libpadsp_orig.so") ||
                strstr(line, "libpadsp_wrap.so")) { orig = 1; break; }
            if (strstr(line, "libpadsp")) padsp = 1;
        }
        fclose(f);
        if (padsp && !orig) found = pid;
    }
    closedir(d);
    return found;
}

static int hooked_alive(void)
{
    int any = 0;
    for (int i = 0; i < 8; i++) {
        int pid = shm->dsp_pids[i];
        if (!pid) continue;
        if (kill(pid, 0) != 0) { __sync_bool_compare_and_swap(&shm->dsp_pids[i], pid, 0); continue; }
        any = 1;
    }
    return any;
}

// plays the music directly while no other program is making sound
static void *direct_thread(void *arg)
{
    (void)arg;
    int fd = -1, foreign = 0;
    uint32_t foreign_t = 0;
    static int16_t buf[1024 * 2];
    while (!svc_quit) {
        uint32_t t = now_ms();
        int owner = shm->owner_pid;
        int fresh = owner && owner != my_pid && t - shm->owner_ms < 300;
        int want = !fresh && !hooked_alive() && audio_state() == A_PLAYING;
        if (want && t - foreign_t > 2000) { foreign = foreign_audio(); foreign_t = t; }
        if (want && foreign) want = 0;
        if (!want) {
            if (fd >= 0) { close(fd); fd = -1; say("direct playback off%s", foreign ? " (another program has the sound)" : ""); }
            usleep(20000);
            continue;
        }
        if (fd < 0) {
            fd = open("/dev/dsp", O_WRONLY);
            if (fd < 0) { usleep(1000000); continue; }
            int frag = (4 << 16) | 12, fmt = AFMT_S16_LE, ch = 2, rate = BGM_RATE;
            ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag);
            ioctl(fd, SNDCTL_DSP_SETFMT, &fmt);
            ioctl(fd, SNDCTL_DSP_CHANNELS, &ch);
            ioctl(fd, SNDCTL_DSP_SPEED, &rate);
            say("direct playback on%s", "");
        }
        uint32_t r = shm->rpos, avail = shm->wpos - r;
        if (avail > BGM_RING) avail = 0;
        uint32_t n = avail < 1024 ? avail : 1024;
        if (n < 256) { usleep(5000); continue; }
        for (uint32_t i = 0; i < n; i++) {
            const int16_t *m = &shm->ring[((r + i) & (BGM_RING - 1)) * 2];
            buf[i * 2] = m[0];
            buf[i * 2 + 1] = m[1];
        }
        shm->rpos = r + n;
        shm->owner_pid = my_pid;
        shm->owner_ms = now_ms();
        const char *p = (const char *)buf;
        size_t left = n * 4;
        while (left && !svc_quit) {
            ssize_t w = write(fd, p, left);
            if (w <= 0) break;
            p += w; left -= (size_t)w;
        }
    }
    if (fd >= 0) close(fd);
    return NULL;
}

// SELECT + L/R skip, + Up/Down volume, + START pause
static void *input_thread(void *arg)
{
    (void)arg;
    if (!S.game_hotkeys) return NULL; /* avoid conflicting game/Onion hotkeys by default */
    int fd = open("/dev/input/event0", O_RDONLY);
    if (fd < 0) return NULL;
    int select_down = 0;
    struct input_event ev;
    while (!svc_quit) {
        if(svc_input_resumed){svc_input_resumed=0;select_down=0;}
        fd_set rs;
        FD_ZERO(&rs);
        FD_SET(fd, &rs);
        struct timeval tv = { 0, 200000 };
        if (select(fd + 1, &rs, NULL, NULL, &tv) <= 0) continue;
        if (read(fd, &ev, sizeof ev) != sizeof ev) continue;
        if (ev.type != EV_KEY) continue;
        if (ev.code == 97) { select_down = ev.value != 0; continue; }
        if (!select_down || ev.value != 1) continue;
        pthread_mutex_lock(&act);
        switch (ev.code) {
        case 18: core_skip(-1); say("SELECT+L: previous%s", ""); break;
        case 20: core_skip(1); say("SELECT+R: next%s", ""); break;
        case 103: S.volume = S.volume + 10 > 100 ? 100 : S.volume + 10; core_apply_audio(); say("SELECT+Up: volume up%s", ""); break;
        case 108: S.volume = S.volume - 10 < 0 ? 0 : S.volume - 10; core_apply_audio(); say("SELECT+Down: volume down%s", ""); break;
        case 28: core_toggle_pause(); say("SELECT+START: %s", audio_state() == A_PAUSED ? "paused" : "playing"); break;
        }
        pthread_mutex_unlock(&act);
    }
    close(fd);
    return NULL;
}

static void save_place(void)
{
    if (current >= 0) { S.cur_index = current; S.cur_pos_ms = audio_pos_ms(); }
    settings_save();
}

int service_main(void)
{
    // detach from the player
    pid_t pid = fork();
    if (pid < 0) return 1;
    if (pid > 0) _exit(0);
    setsid();
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);
    signal(SIGCONT, on_continue);
    my_pid = getpid();
    char lock_path[600];
    path_join(lock_path, sizeof lock_path, data_dir, "service.lock");
    int service_lock = open(lock_path, O_RDWR | O_CREAT, 0600);
    if (service_lock < 0 || flock(service_lock, LOCK_EX | LOCK_NB) != 0) {
        fprintf(stderr, "[gm] background service already active or cannot lock\n");
        if (service_lock >= 0) close(service_lock);
        return 1;
    }

    char p[600];
    path_join(p, sizeof p, data_dir, "service.log");
    struct stat st;
    int fd = open(p, O_WRONLY | O_CREAT | (stat(p, &st) == 0 && st.st_size > 262144 ? O_TRUNC : O_APPEND), 0644);
    if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); }
    int nul = open("/dev/null", O_RDONLY);
    if (nul >= 0) { dup2(nul, 0); close(nul); }
    setvbuf(stderr, NULL, _IOLBF, 0);
    pid_path(p, sizeof p);
    FILE *pf = fopen(p, "w");
    if (pf) { fprintf(pf, "%d\n", my_pid); fclose(pf); }

    settings_load();
    stayawake_cleanup_stale();
    say("===== background service started (RedAmp %s)", getenv("MA_VERSION") ? getenv("MA_VERSION") : "");
    if (!S.game_music) { say("music in games is off%s", ""); unlink(p); return 0; }
    /* RedOnion's launch hook now preloads the wrapper only for opted-in
     * applications. Never replace the shared system library here. */
    if (gm_installed()) {
        say("legacy system wrapper detected; repair it before using native integration%s", "");
        unlink(p);
        return 1;
    }

    pl_init();
    path_join(p, sizeof p, data_dir, "playlist.m3u");
    pl_load_m3u(p);
    if (S.cur_index < 0 || S.cur_index >= pl_count()) {
        say("nothing to play%s", ""); pid_path(p, sizeof p); unlink(p); pl_quit(); return 0;
    }

    // Only the single lock holder can create/truncate shared memory.
    int sfd = open(BGM_SHM_PATH, O_RDWR | O_CREAT, 0600);
    if (sfd < 0 || ftruncate(sfd, sizeof(bgm_shm)) != 0) {
        say("shared memory failed%s", "");
        if (sfd >= 0) close(sfd);
        pid_path(p, sizeof p); unlink(p); pl_quit(); return 1;
    }
    shm = mmap(NULL, sizeof(bgm_shm), PROT_READ | PROT_WRITE, MAP_SHARED, sfd, 0);
    close(sfd);
    if (shm == MAP_FAILED) {
        say("mmap failed%s", "");
        unlink(BGM_SHM_PATH);
        pid_path(p, sizeof p); unlink(p); pl_quit(); return 1;
    }
    memset(shm, 0, sizeof *shm);
    shm->version = 2;
    /* Independent headroom for mixing game audio with music. S.volume is
     * already applied before samples enter the ring. */
    shm->volume = S.game_mix_volume * 256 / 100;
    __sync_synchronize();
    shm->magic = BGM_MAGIC;

    setenv("MA_GAME_SERVICE","1",1);
    audio_init_ring(shm);
    core_apply_audio();
    if (S.shuffle) core_build_shuffle();
    core_play_index(S.cur_index, S.cur_pos_ms, 0);
    say("playing %s", pl_path(S.cur_index));
    const char *ready_fd = getenv("MA_SERVICE_READY_FD");
    if (ready_fd) {
        int fd = atoi(ready_fd);
        if (fd >= 3) { (void)write(fd, "1", 1); close(fd); }
        unsetenv("MA_SERVICE_READY_FD");
    }

    pthread_t th_direct, th_input;
    int direct_ok = pthread_create(&th_direct, NULL, direct_thread, NULL) == 0;
    int input_ok = pthread_create(&th_input, NULL, input_thread, NULL) == 0;
    if (!direct_ok) say("warning: direct audio output thread failed%s", "");
    if (!input_ok) say("warning: control input thread failed%s", "");

    uint32_t last_save = now_ms(), last_stats = now_ms(), r_stats = 0, und_stats = 0;
    int errors = 0;
    while (!svc_quit) {
        usleep(50000);
        pthread_mutex_lock(&act);
        if (audio_take_finished()) {
            errors = 0;
            if (!core_track_finished()) { say("end of the playlist%s", ""); svc_quit = 1; }
            else say("next: %s", pl_path(current));
            save_place();
        }
        if (audio_take_error()) {
            say("could not play %s", pl_path(current));
            if (++errors >= pl_count()) svc_quit = 1;
            else { int k = core_next_index(1); if (k >= 0) core_play_index(k, 0, 0); else svc_quit = 1; }
            save_place();
        }
        uint32_t t = now_ms();
        if(svc_resumed){svc_resumed=0;say("resumed from standby%s", "");}
        stayawake_update(audio_state() == A_PLAYING,S.play_standby);
        if (t - last_save > 60000) { save_place(); last_save = t; }
        pthread_mutex_unlock(&act);
        if (t - last_stats > 60000) {
            char b[200];
            uint32_t r = shm->rpos;
            snprintf(b, sizeof b, "music %.0f%% of real time, carried by %s, underruns %u",
                     (r - r_stats) / (double)BGM_RATE / ((t - last_stats) / 1000.0) * 100,
                     shm->owner_pid == my_pid ? "RedAmp" : shm->owner_pid ? (const char *)shm->owner_name : "nobody",
                     shm->underruns - und_stats);
            say("%s", b);
            r_stats = r; und_stats = shm->underruns; last_stats = t;
        }
    }
    pthread_mutex_lock(&act);
    S.resume_playing = svc_signal && audio_state() == A_PLAYING;
    save_place();
    pthread_mutex_unlock(&act);
    stayawake_release();
    shm->magic = 0;
    __sync_synchronize();
    if (direct_ok) pthread_join(th_direct, NULL);
    if (input_ok) pthread_join(th_input, NULL);
    audio_quit();
    pl_quit();
    munmap(shm, sizeof *shm);
    unlink(BGM_SHM_PATH);
    pid_path(p, sizeof p);
    unlink(p);
    say("stopped%s", svc_signal ? " (RedAmp opened)" : "");
    flock(service_lock, LOCK_UN);
    close(service_lock);
    return 0;
}
