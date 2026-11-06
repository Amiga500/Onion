/**
 * @file test_tweaks_smb.c
 * @brief Tweaks > Network > File sharing (Samba): the shares of smb.conf
 *
 * smb_shares.h on real files:
 *  - RedOnion's smb.conf: four shares, their paths, all available;
 *  - a share without "available" is available (Samba's default), and its
 *    toggle adds the line to that share (the old toggle switched the share
 *    before it);
 *  - "path =" in [global] or before any section is ignored (the old reader
 *    wrote to the share at index -1);
 *  - an indented "available" line keeps its indent and leaves no stray
 *    text; yes/no values, comments, "__" names;
 *  - a share that isn't there leaves the file as it was.
 *
 * Build and run: make -f Makefile.unit test_tweaks_smb
 */

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../src/tweaks/smb_shares.h"

#define TEST_DIR "/tmp/onion_test_tweaks_smb"
#define CONF TEST_DIR "/smb.conf"

static void put(const char *text)
{
    FILE *f = fopen(CONF, "w");
    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
}

static char *slurp(void)
{
    return file_read(CONF);
}

static bool conf_is(const char *text)
{
    char *got = slurp();
    bool same = got != NULL && strcmp(got, text) == 0;
    if (!same)
        printf("    smb.conf is:\n%s\n", got != NULL ? got : "(none)");
    free(got);
    return same;
}

static Share *read_conf(int *count)
{
    FILE *f = fopen(CONF, "r");
    if (f == NULL)
        return NULL;
    Share *shares = smb_readShares(f, count);
    fclose(f);
    return shares;
}

static void reset_dir(void)
{
    if (system("rm -rf " TEST_DIR) != 0)
        fprintf(stderr, "rm failed\n");
    mkdir(TEST_DIR, 0755);
}

TEST(redonion_smb_conf)
{
    reset_dir();
    // From static/configs/.tmp_update/config/smb.conf (trimmed).
    put("## Onion smb.conf\n\n[global]\n\nworkgroup = workgroup\nlog file = /var/log/%m.log\n"
        "pid directory = /var/run/\n\n## Edit your shares below\n\n"
        "[BIOS]\nvalid users = onion\npath = /mnt/SDCARD/BIOS\npublic = 1\navailable = 1\n\n"
        "[Media]\npath = /mnt/SDCARD/Media\navailable = 1\n\n"
        "[Overlays]\npath = /mnt/SDCARD/RetroArch/.retroarch/overlay\navailable = 1\n\n"
        "[Roms]\npath = /mnt/SDCARD/Roms\navailable = 0\n");
    int count = 0;
    Share *s = read_conf(&count);
    ASSERT_EQ(count, 4);
    ASSERT_STREQ(s[0].name, "BIOS");
    ASSERT_STREQ(s[0].path, "/mnt/SDCARD/BIOS");
    ASSERT_EQ(s[0].available, 1);
    ASSERT_STREQ(s[2].path, "/mnt/SDCARD/RetroArch/.retroarch/overlay");
    ASSERT_EQ(s[3].available, 0);
    free(s);

    ASSERT_TRUE(smb_setAvailable(CONF, "Media", false));
    s = read_conf(&count);
    ASSERT_EQ(s[1].available, 0);
    ASSERT_EQ(s[0].available, 1);
    ASSERT_EQ(s[2].available, 1);
    free(s);
}

TEST(share_without_available_line)
{
    reset_dir();
    put("[A]\npath = /a\navailable = 1\n\n[B]\npath = /b\n\n[C]\npath = /c\navailable = 1\n");
    int count = 0;
    Share *s = read_conf(&count);
    ASSERT_EQ(count, 3);
    ASSERT_EQ(s[1].available, 1); // Samba's default
    free(s);

    // Toggling B changes B, not A (the old code reused A's offset).
    ASSERT_TRUE(smb_setAvailable(CONF, "B", false));
    ASSERT_TRUE(conf_is("[A]\npath = /a\navailable = 1\n\n[B]\npath = /b\n\navailable = 0\n[C]\npath = /c\navailable = 1\n"));

    // The last share, in a file without a final newline.
    put("[A]\npath = /a");
    ASSERT_TRUE(smb_setAvailable(CONF, "A", false));
    ASSERT_TRUE(conf_is("[A]\npath = /a\navailable = 0\n"));
}

TEST(path_before_any_share_is_ignored)
{
    reset_dir();
    put("path = /orphan\navailable = 0\n[global]\nusershare path = /x\npath = /y\n[S]\npath = /s\n");
    int count = 0;
    Share *s = read_conf(&count);
    ASSERT_EQ(count, 1);
    ASSERT_STREQ(s[0].path, "/s");
    ASSERT_EQ(s[0].available, 1);
    free(s);

    // A [global] after a share ends that share.
    put("[S]\npath = /s\n[global]\npath = /g\navailable = 0\n");
    s = read_conf(&count);
    ASSERT_EQ(count, 1);
    ASSERT_STREQ(s[0].path, "/s");
    ASSERT_EQ(s[0].available, 1);
    free(s);

    // Nothing at all: no shares, no crash.
    put("[global]\npath = /y\n");
    s = read_conf(&count);
    ASSERT_TRUE(s == NULL);
    ASSERT_EQ(count, 0);
}

TEST(indented_line_and_values)
{
    reset_dir();
    put("[S]\n   path   =   /s  \n   available = yes\n   ; available = 0\n");
    int count = 0;
    Share *s = read_conf(&count);
    ASSERT_EQ(count, 1);
    ASSERT_STREQ(s[0].path, "/s");
    ASSERT_EQ(s[0].available, 1); // "yes", and the comment doesn't count
    free(s);

    ASSERT_TRUE(smb_setAvailable(CONF, "S", false));
    ASSERT_TRUE(conf_is("[S]\n   path   =   /s  \n   available = 0\n   ; available = 0\n"));
    ASSERT_TRUE(smb_setAvailable(CONF, "s", true)); // Samba ignores case
    ASSERT_TRUE(conf_is("[S]\n   path   =   /s  \n   available = 1\n   ; available = 0\n"));
}

TEST(names_with_double_underscore)
{
    reset_dir();
    put("[__Saves]\npath = /saves\n");
    int count = 0;
    Share *s = read_conf(&count);
    ASSERT_EQ(count, 1);
    ASSERT_STREQ(s[0].name, "Saves (!)");
    ASSERT_STREQ(s[0].section, "__Saves");
    free(s);
    ASSERT_TRUE(smb_setAvailable(CONF, "__Saves", false));
    ASSERT_TRUE(conf_is("[__Saves]\npath = /saves\navailable = 0\n"));
}

TEST(missing_share_changes_nothing)
{
    reset_dir();
    put("[A]\navailable = 1\n");
    ASSERT_FALSE(smb_setAvailable(CONF, "Nope", false));
    ASSERT_TRUE(conf_is("[A]\navailable = 1\n"));
    ASSERT_FALSE(smb_setAvailable(TEST_DIR "/missing.conf", "A", false));
}

int main(void)
{
    printf("\n=== tweaks/smb_shares.h ===\n\n");
    RUN_TEST(redonion_smb_conf);
    RUN_TEST(share_without_available_line);
    RUN_TEST(path_before_any_share_is_ignored);
    RUN_TEST(indented_line_and_values);
    RUN_TEST(names_with_double_underscore);
    RUN_TEST(missing_share_changes_nothing);
    if (system("rm -rf " TEST_DIR) != 0)
        fprintf(stderr, "rm failed\n");
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
