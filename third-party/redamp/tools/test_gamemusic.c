// Standalone host test of safe game-music installation and recovery.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../core.h"
#include "../gamemusic.h"

char app_dir[512], data_dir[512], root_dir[512];
settings S;
int current;
void path_join(char *out, int n, const char *a, const char *b) { snprintf(out, n, "%s/%s", a, b); }

static int copy_file(const char *from, const char *to)
{
    FILE *f = fopen(from, "rb"), *o = fopen(to, "wb");
    if (!f || !o) return 0;
    char b[4096]; size_t n;
    while ((n = fread(b, 1, sizeof b, f))) if (fwrite(b, 1, n, o) != n) return 0;
    fclose(f); fclose(o);
    return 1;
}
static int same_file(const char *a, const char *b)
{
    FILE *x = fopen(a, "rb"), *y = fopen(b, "rb");
    if (!x || !y) return 0;
    char a1[4096], a2[4096]; size_t n1, n2; int same = 1;
    do {
        n1 = fread(a1,1,sizeof a1,x); n2 = fread(a2,1,sizeof a2,y);
        if (n1 != n2 || memcmp(a1,a2,n1)) { same = 0; break; }
    } while (n1);
    fclose(x); fclose(y); return same;
}
#define CHECK(condition,desc) do { if (!(condition)) { fprintf(stderr,"FAIL: %s\n",desc); return 1; } } while (0)

int main(int argc, char **argv)
{
    if (argc != 5) return 2;
    snprintf(app_dir,sizeof app_dir,"%s",argv[1]);
    snprintf(data_dir,sizeof data_dir,"%s",argv[2]);
    CHECK(setenv("MA_LIBDIR",argv[3],1)==0,"setenv");
    char orig[1024], cur[1024], saved[1024];
    path_join(cur,sizeof cur,argv[3],"libpadsp.so");
    path_join(orig,sizeof orig,argv[3],"libpadsp_orig.so");
    path_join(saved,sizeof saved,data_dir,"libpadsp_onion.so");
    CHECK(copy_file(argv[4],cur),"prepare original");
    CHECK(!gm_install(),"without explicit consent, install must fail");
    CHECK(gm_set_consent(),"grant user consent");
    CHECK(!gm_installed(),"wrapper absent by default");
    CHECK(gm_install(),"enable hook");
    CHECK(gm_installed(),"hook installed");
    CHECK(same_file(orig,argv[4]) && same_file(saved,argv[4]),"both verified backups");
    CHECK(gm_uninstall(),"disable hook");
    CHECK(!gm_installed() && same_file(cur,argv[4]),"original restored byte-for-byte");
    CHECK(gm_set_consent(),"renew user consent");
    CHECK(gm_install(),"enable hook again");
    unlink(orig); unlink(saved);
    CHECK(!gm_uninstall(),"refuse false-success when no original remains");
    CHECK(gm_installed(),"wrapper retained after failed restore");
    CHECK(copy_file(argv[4],saved),"prepare recovery copy");
    CHECK(gm_uninstall() && same_file(cur,argv[4]),"restore via verified recovery copy");
    unlink(cur); unlink(orig); unlink(saved);
    CHECK(!gm_uninstall(),"missing current and original must FAIL");
    CHECK(!gm_install(),"missing current must not install wrapper");
    CHECK(copy_file(argv[4],cur),"prepare update");
    CHECK(gm_set_consent(),"renew user consent after recovery");
    CHECK(gm_install(),"re-enable after recovery");
    CHECK(gm_uninstall(),"post-recovery disable");
    puts("game-music library install / recovery regressions: PASS");
    return 0;
}
