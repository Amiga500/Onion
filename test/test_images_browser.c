#include "onion_test.h"
#include "../src/infoPanel/imagesBrowser.h"
#include "../src/common/utils/file.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int create_file(const char *path)
{
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd == -1) {
        return -1;
    }
    if (write(fd, "x", 1) != 1) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static void free_images_paths(char **images_paths, int images_paths_count)
{
    if (!images_paths) {
        return;
    }

    for (int i = 0; i < images_paths_count; i++) {
        free(images_paths[i]);
    }
    free(images_paths);
}

TEST(loadImagesPathsFromDir_empty_directory) {
    const char *dir = "/tmp/onion_test_images_browser_empty";
    file_remove_recursive(dir);
    ASSERT_EQ(mkdir(dir, 0755), 0);

    char **images_paths = NULL;
    int images_paths_count = -1;

    ASSERT_TRUE(loadImagesPathsFromDir(dir, &images_paths, &images_paths_count));
    ASSERT_EQ(images_paths_count, 0);
    ASSERT_NULL(images_paths);

    file_remove_recursive(dir);
}

TEST(loadImagesPathsFromDir_filters_and_sorts_images) {
    const char *dir = "/tmp/onion_test_images_browser_files";
    const char *subdir = "/tmp/onion_test_images_browser_files/subdir";
    file_remove_recursive(dir);
    ASSERT_EQ(mkdir(dir, 0755), 0);
    ASSERT_EQ(mkdir(subdir, 0755), 0);

    ASSERT_EQ(create_file("/tmp/onion_test_images_browser_files/b.JPG"), 0);
    ASSERT_EQ(create_file("/tmp/onion_test_images_browser_files/a.png"), 0);
    ASSERT_EQ(create_file("/tmp/onion_test_images_browser_files/c.JPEG"), 0);
    ASSERT_EQ(create_file("/tmp/onion_test_images_browser_files/readme.txt"), 0);
    ASSERT_EQ(create_file("/tmp/onion_test_images_browser_files/.hidden.png"), 0);
    ASSERT_EQ(create_file("/tmp/onion_test_images_browser_files/subdir/nested.png"), 0);

    char **images_paths = NULL;
    int images_paths_count = -1;

    ASSERT_TRUE(loadImagesPathsFromDir(dir, &images_paths, &images_paths_count));
    ASSERT_EQ(images_paths_count, 3);
    ASSERT_NOT_NULL(images_paths);
    ASSERT_STREQ(images_paths[0], "/tmp/onion_test_images_browser_files/a.png");
    ASSERT_STREQ(images_paths[1], "/tmp/onion_test_images_browser_files/b.JPG");
    ASSERT_STREQ(images_paths[2], "/tmp/onion_test_images_browser_files/c.JPEG");

    free_images_paths(images_paths, images_paths_count);
    file_remove_recursive(dir);
}

TEST(loadImagesPathsFromDir_rejects_invalid_arguments) {
    char **images_paths = NULL;
    int images_paths_count = 0;

    ASSERT_FALSE(loadImagesPathsFromDir(NULL, &images_paths, &images_paths_count));
    ASSERT_FALSE(loadImagesPathsFromDir("", &images_paths, &images_paths_count));
    ASSERT_FALSE(loadImagesPathsFromDir("/tmp", NULL, &images_paths_count));
    ASSERT_FALSE(loadImagesPathsFromDir("/tmp", &images_paths, NULL));
}

int main(void)
{
    RUN_TEST(loadImagesPathsFromDir_empty_directory);
    RUN_TEST(loadImagesPathsFromDir_filters_and_sorts_images);
    RUN_TEST(loadImagesPathsFromDir_rejects_invalid_arguments);
    TEST_REPORT();
    return test_failures;
}
