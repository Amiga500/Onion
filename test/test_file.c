#include "onion_test.h"

#include "utils/file.h"
#include "utils/str.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_tmp[256];

static void setup_tmp(void)
{
    snprintf(g_tmp, sizeof(g_tmp), "/tmp/onion_unit_file_%d", (int)getpid());
    char cmd[320];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' && mkdir -p '%s'", g_tmp, g_tmp);
    system(cmd);
}

static void teardown_tmp(void)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", g_tmp);
    system(cmd);
}

static void join(char *out, size_t n, const char *name)
{
    snprintf(out, n, "%s/%s", g_tmp, name);
}

TEST(exists_and_types)
{
    char path[512];
    join(path, sizeof(path), "a.txt");
    FILE *fp = fopen(path, "w");
    ASSERT_NOT_NULL(fp);
    fputs("x", fp);
    fclose(fp);

    ASSERT_TRUE(exists(path));
    ASSERT_TRUE(is_file(path));
    ASSERT_FALSE(is_dir(path));
    ASSERT_TRUE(exists(g_tmp));
    ASSERT_TRUE(is_dir(g_tmp));
    ASSERT_FALSE(is_file(g_tmp));

    char missing[512];
    join(missing, sizeof(missing), "nope");
    ASSERT_FALSE(exists(missing));
    ASSERT_FALSE(is_file(missing));
    ASSERT_FALSE(is_dir(missing));
}

TEST(basename_gnu_style)
{
    ASSERT_STREQ(file_basename("/path/to/file.txt"), "file.txt");
    ASSERT_STREQ(file_basename("file.txt"), "file.txt");
    ASSERT_STREQ(file_basename("/path/to/"), "");
}

TEST(mkdirs_creates_then_false)
{
    char dir[512];
    join(dir, sizeof(dir), "sub/a/b");
    ASSERT_TRUE(mkdirs(dir));
    ASSERT_TRUE(is_dir(dir));
    ASSERT_FALSE(mkdirs(dir));
}

TEST(mkdirs_null_or_empty)
{
    ASSERT_FALSE(mkdirs(NULL));
    ASSERT_FALSE(mkdirs(""));
}

TEST(file_read_missing_is_null)
{
    char path[512];
    join(path, sizeof(path), "missing.txt");
    ASSERT_NULL(file_read(path));
}

TEST(file_read_empty_is_allocated_empty_string)
{
    /* Current contract: empty file -> malloc'd "", not NULL. */
    char path[512];
    join(path, sizeof(path), "empty.txt");
    FILE *fp = fopen(path, "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);

    char *s = file_read(path);
    ASSERT_NOT_NULL(s);
    ASSERT_STREQ(s, "");
    free(s);
}

TEST(file_read_content)
{
    char path[512];
    join(path, sizeof(path), "hello.txt");
    FILE *fp = fopen(path, "w");
    ASSERT_NOT_NULL(fp);
    fputs("hello", fp);
    fclose(fp);

    char *s = file_read(path);
    ASSERT_NOT_NULL(s);
    ASSERT_STREQ(s, "hello");
    free(s);
}

TEST(file_read_unreadable_is_null)
{
    /* exists() true + fopen fail (mode 000). Skip if running as root. */
    if (geteuid() == 0)
        return;
    char path[512];
    join(path, sizeof(path), "noperm.txt");
    FILE *fp = fopen(path, "w");
    ASSERT_NOT_NULL(fp);
    fputs("secret", fp);
    fclose(fp);
    ASSERT_EQ(chmod(path, 0), 0);
    char *s = file_read(path);
    chmod(path, 0644);
    ASSERT_NULL(s);
}

TEST(file_write_existing)
{
    char path[512];
    join(path, sizeof(path), "w.txt");
    FILE *fp = fopen(path, "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);

    ASSERT_TRUE(file_write(path, "ab", 2));
    char *s = file_read(path);
    ASSERT_STREQ(s, "ab");
    free(s);
}

TEST(file_write_missing_is_false)
{
    char path[512];
    join(path, sizeof(path), "missing_write.txt");
    ASSERT_FALSE(file_write(path, "ab", 2));
}

TEST(removeExtension_simple)
{
    char *s = file_removeExtension("game.gba");
    ASSERT_STREQ(s, "game");
    free(s);
}

TEST(removeExtension_no_dot)
{
    char *s = file_removeExtension("game");
    ASSERT_STREQ(s, "game");
    free(s);
}

TEST(dirname_absolute)
{
    char *s = file_dirname("/mnt/SDCARD/Roms/GBA/game.gba");
    ASSERT_STREQ(s, "/mnt/SDCARD/Roms/GBA");
    free(s);
}

TEST(dirname_no_slash)
{
    ASSERT_NULL(file_dirname("game.gba"));
}

TEST(getExtension)
{
    ASSERT_STREQ(file_getExtension("game.gba"), "gba");
    ASSERT_STREQ(file_getExtension("game"), "");
    ASSERT_STREQ(file_getExtension(".hidden"), "");
}

TEST(cleanName_strips_region)
{
    char out[STR_MAX];
    file_cleanName(out, "Legend_of_Zelda_(USA).gba");
    ASSERT_STREQ(out, "Legend of Zelda");
}

TEST(parseKeyValue)
{
    char path[512];
    join(path, sizeof(path), "cfg.txt");
    FILE *fp = fopen(path, "w");
    ASSERT_NOT_NULL(fp);
    fputs("name = onion\ncore = gpsp\n", fp);
    fclose(fp);

    char val[256];
    char *ret = file_parseKeyValue(path, "core", val, '=', 0);
    ASSERT_NOT_NULL(ret);
    ASSERT_STREQ(val, "gpsp");
}

TEST(parseKeyValue_missing_and_null)
{
    char path[512];
    join(path, sizeof(path), "cfg2.txt");
    FILE *fp = fopen(path, "w");
    fputs("a=1\na=2\n", fp);
    fclose(fp);
    char val[256];
    val[0] = 'x';
    ASSERT_NULL(file_parseKeyValue(path, "nope", val, '=', 0));
    ASSERT_NULL(file_parseKeyValue(NULL, "a", val, '=', 0));
    char *second = file_parseKeyValue(path, "a", val, '=', 1);
    ASSERT_NOT_NULL(second);
    ASSERT_STREQ(val, "2");
}

TEST(resolvePath_dotdot)
{
    char *s = file_resolvePath("/mnt/SDCARD/Emu/GBA/../../Roms/GBA/game.gba");
    ASSERT_NOT_NULL(s);
    ASSERT_STREQ(s, "/mnt/SDCARD/Roms/GBA/game.gba");
    free(s);
}

TEST(resolvePath_null)
{
    ASSERT_NULL(file_resolvePath(NULL));
}

TEST(read_lineN)
{
    char path[512];
    join(path, sizeof(path), "lines.txt");
    FILE *fp = fopen(path, "w");
    ASSERT_NOT_NULL(fp);
    fputs("one\ntwo\nthree\n", fp);
    fclose(fp);

    char *l2 = file_read_lineN(path, 2);
    ASSERT_NOT_NULL(l2);
    ASSERT_STREQ(l2, "two\n");
    free(l2);
    ASSERT_NULL(file_read_lineN(path, 9));
}

TEST(copy_and_findNewest)
{
    char a[512], b[512];
    join(a, sizeof(a), "first.bin");
    join(b, sizeof(b), "second.bin");
    FILE *fp = fopen(a, "w");
    fputs("a", fp);
    fclose(fp);
    sleep(1);
    file_copy(a, b);
    ASSERT_TRUE(is_file(b));

    char newest[256];
    ASSERT_TRUE(file_findNewest(g_tmp, newest, sizeof(newest)));
    ASSERT_STREQ(newest, "second.bin");
}

TEST(copy_path_with_spaces)
{
    char src[512], dst[512];
    join(src, sizeof(src), "my file.bin");
    join(dst, sizeof(dst), "copy of file.bin");
    FILE *fp = fopen(src, "w");
    ASSERT_NOT_NULL(fp);
    fputs("xyz", fp);
    fclose(fp);
    file_copy(src, dst);
    char *s = file_read(dst);
    ASSERT_STREQ(s, "xyz");
    free(s);
}

TEST(changeKeyValue_uses_sidecar_tmp)
{
    char path[512];
    join(path, sizeof(path), "kv.txt");
    FILE *fp = fopen(path, "w");
    ASSERT_NOT_NULL(fp);
    fputs("name=old\n", fp);
    fclose(fp);
    file_changeKeyValue(path, "name", "name=new");
    char *s = file_read(path);
    ASSERT_NOT_NULL(s);
    ASSERT_TRUE(strstr(s, "name=new") != NULL);
    free(s);
    ASSERT_FALSE(exists("temp"));
}

TEST(delete_line_uses_sidecar_tmp)
{
    char path[512];
    join(path, sizeof(path), "lines2.txt");
    FILE *fp = fopen(path, "w");
    fputs("a\nb\nc\n", fp);
    fclose(fp);
    file_delete_line(path, 2);
    char *s = file_read(path);
    ASSERT_STREQ(s, "a\nc\n");
    free(s);
    ASSERT_FALSE(exists("temp.txt"));
}

TEST(add_line_to_beginning)
{
    char path[512];
    join(path, sizeof(path), "head.txt");
    FILE *fp = fopen(path, "w");
    ASSERT_NOT_NULL(fp);
    fputs("a\nb\n", fp);
    fclose(fp);
    file_add_line_to_beginning(path, "Z\n");
    char *s = file_read(path);
    ASSERT_STREQ(s, "Z\na\nb\n");
    free(s);
}

TEST(copy_tree_and_move_children)
{
    char src[512], dst[512], nested[512], leaf[512];
    join(src, sizeof(src), "tree_src");
    join(dst, sizeof(dst), "tree_dst");
    join(nested, sizeof(nested), "tree_src/sub");
    ASSERT_TRUE(mkdirs(nested));
    join(leaf, sizeof(leaf), "tree_src/sub/a.txt");
    FILE *fp = fopen(leaf, "w");
    ASSERT_NOT_NULL(fp);
    fputs("L", fp);
    fclose(fp);

    file_copy_tree(src, dst);
    char copied[512];
    join(copied, sizeof(copied), "tree_dst/sub/a.txt");
    char *s = file_read(copied);
    ASSERT_STREQ(s, "L");
    free(s);

    char moved_root[512], moved[512];
    join(moved_root, sizeof(moved_root), "tree_moved");
    file_move_children(dst, moved_root);
    join(moved, sizeof(moved), "tree_moved/sub/a.txt");
    s = file_read(moved);
    ASSERT_STREQ(s, "L");
    free(s);
    ASSERT_FALSE(is_dir(dst));
}

int main(void)
{
    printf("\n=== file.c unit tests ===\n\n");
    setup_tmp();

    RUN_TEST(exists_and_types);
    RUN_TEST(basename_gnu_style);
    RUN_TEST(mkdirs_creates_then_false);
    RUN_TEST(mkdirs_null_or_empty);
    RUN_TEST(file_read_missing_is_null);
    RUN_TEST(file_read_empty_is_allocated_empty_string);
    RUN_TEST(file_read_content);
    RUN_TEST(file_read_unreadable_is_null);
    RUN_TEST(file_write_existing);
    RUN_TEST(file_write_missing_is_false);
    RUN_TEST(removeExtension_simple);
    RUN_TEST(removeExtension_no_dot);
    RUN_TEST(dirname_absolute);
    RUN_TEST(dirname_no_slash);
    RUN_TEST(getExtension);
    RUN_TEST(cleanName_strips_region);
    RUN_TEST(parseKeyValue);
    RUN_TEST(parseKeyValue_missing_and_null);
    RUN_TEST(resolvePath_dotdot);
    RUN_TEST(resolvePath_null);
    RUN_TEST(read_lineN);
    RUN_TEST(copy_and_findNewest);
    RUN_TEST(copy_path_with_spaces);
    RUN_TEST(changeKeyValue_uses_sidecar_tmp);
    RUN_TEST(delete_line_uses_sidecar_tmp);
    RUN_TEST(add_line_to_beginning);
    RUN_TEST(copy_tree_and_move_children);

    teardown_tmp();
    return onion_test_report("test_file");
}
