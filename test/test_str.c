#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/common/utils/file.h"
#include "../src/common/utils/str.h"

int main(void)
{
    char out[STR_MAX];
    char hidden_suffix[] = {'a', '\0', 'b', '\0'};
    char long_name[STR_MAX + 32];
    size_t trimmed;

    assert(str_count_char("", 'a') == 0);
    assert(str_count_char("abc", 'a') == 1);
    assert(str_count_char("a/b/c", '/') == 2);
    assert(str_count_char("abc", '\0') == 0);
    assert(str_count_char(hidden_suffix, 'b') == 0);

    trimmed = str_trim(out, sizeof(out), "", false);
    assert(trimmed == 0);
    assert(strcmp(out, "") == 0);

    trimmed = str_trim(out, sizeof(out), "   ", false);
    assert(trimmed == 0);
    assert(strcmp(out, "") == 0);

    trimmed = str_trim(out, sizeof(out), "  abc  ", false);
    assert(trimmed == 3);
    assert(strcmp(out, "abc") == 0);

    trimmed = str_trim(out, sizeof(out), "\"abc\"", false);
    assert(trimmed == 3);
    assert(strcmp(out, "abc") == 0);

    str_removeParentheses(out, "Game Title (USA) [Hack]");
    assert(strcmp(out, "Game Title") == 0);

    memset(long_name, 'x', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    str_removeParentheses(out, long_name);
    assert(strlen(out) == STR_MAX - 2);
    for (size_t i = 0; i < strlen(out); i++)
        assert(out[i] == 'x');

    str_serializeTime(out, -1);
    assert(strcmp(out, "0s") == 0);

    str_serializeTime(out, 0);
    assert(strcmp(out, "0s") == 0);

    str_serializeTime(out, 59);
    assert(strcmp(out, "59s") == 0);

    str_serializeTime(out, 60);
    assert(strcmp(out, "1m 0s") == 0);

    str_serializeTime(out, 3600);
    assert(strcmp(out, "1h 0m") == 0);

    char temp_path[] = "/tmp/onion-file-test-XXXXXX";
    int temp_fd = mkstemp(temp_path);
    assert(temp_fd >= 0);
    close(temp_fd);

    int saved_stdin = dup(STDIN_FILENO);
    assert(saved_stdin >= 0);
    assert(close(STDIN_FILENO) == 0);
    int write_ok = file_write(temp_path, "abc", 3);
    assert(dup2(saved_stdin, STDIN_FILENO) == STDIN_FILENO);
    close(saved_stdin);
    assert(write_ok);

    char *file_contents = file_read(temp_path);
    assert(file_contents != NULL);
    assert(strcmp(file_contents, "abc") == 0);
    free(file_contents);
    unlink(temp_path);

    char relative_path[] = "onion-relative-XXXXXX";
    int relative_fd = mkstemp(relative_path);
    assert(relative_fd >= 0);
    assert(write(relative_fd, "world\n", 6) == 6);
    close(relative_fd);

    file_add_line_to_beginning(relative_path, "hello\n");
    file_contents = file_read(relative_path);
    assert(file_contents != NULL);
    assert(strcmp(file_contents, "hello\nworld\n") == 0);
    free(file_contents);
    unlink(relative_path);

    return 0;
}
