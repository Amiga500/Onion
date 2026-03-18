#include "imagesBrowser.h"

#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define STR_MAX 256

static const char *getFilenameExt(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return "";
    return dot + 1;
}

static char *toLower(char *s)
{
    for (char *p = s; *p; p++) {
        *p = tolower(*p);
    }
    return s;
}

static bool getImagePath(const char *dir_path, const struct dirent *ent,
                         char *image_path)
{
    const int ext_size = 50;
    char ext[ext_size];
    const char *filename = ent->d_name;
    if (filename[0] == '.' || ent->d_type == DT_DIR) {
        return false;
    }
    snprintf(ext, ext_size, "%s", getFilenameExt(filename));
    const char *fileExt = toLower(ext);
    if (strcmp(fileExt, "png") == 0 || strcmp(fileExt, "jpg") == 0 ||
        strcmp(fileExt, "jpeg") == 0) {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s%s", dir_path, filename);
        strncpy(image_path, full_path, PATH_MAX - 1);
        image_path[PATH_MAX - 1] = '\0';
        return true;
    }
    return false;
}

static int getImagesCount(const char *dir_path)
{
    int images_count = 0;

    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        return 0;
    }

    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {
        char image_path[PATH_MAX];
        const bool is_image = getImagePath(dir_path, ent, image_path);
        if (is_image) {
            images_count++;
        }
    }

    closedir(dir);

    return images_count;
}

int compare_strings(const void *a, const void *b)
{
    const char *aa = *(const char **)a;
    const char *bb = *(const char **)b;
    return strcmp(aa, bb);
}

static void freeImagePaths(char **images_paths, int images_paths_count)
{
    if (!images_paths) {
        return;
    }

    for (int i = 0; i < images_paths_count; i++) {
        free(images_paths[i]);
    }
    free(images_paths);
}

bool loadImagesPathsFromDir(const char *dir_path, char ***images_paths,
                            int *images_paths_count)
{
    if (!dir_path || !images_paths || !images_paths_count) {
        return false;
    }

    char normalized_dir_path[PATH_MAX];
    const int dir_path_length = strlen(dir_path);
    if (dir_path_length == 0) {
        return false;
    }
    if (dir_path[dir_path_length - 1] != '/') {
        snprintf(normalized_dir_path, sizeof(normalized_dir_path), "%s/", dir_path);
    }
    else {
        strncpy(normalized_dir_path, dir_path, sizeof(normalized_dir_path) - 1);
        normalized_dir_path[sizeof(normalized_dir_path) - 1] = '\0';
    }

    const int images_count = getImagesCount(normalized_dir_path);

    DIR *dir = opendir(normalized_dir_path);

    if (dir == NULL) {
        return false;
    }

    struct dirent *ent;

    *images_paths_count = 0;
    *images_paths = NULL;
    size_t capacity = images_count > 0 ? (size_t)images_count : 0;
    if (capacity > 0) {
        *images_paths = (char **)malloc(capacity * sizeof(char *));
        if (*images_paths == NULL) {
            closedir(dir);
            return false;
        }
    }

    while ((ent = readdir(dir)) != NULL) {
        char image_path[PATH_MAX];
        const bool is_image =
            getImagePath(normalized_dir_path, ent, image_path);

        if (!is_image) {
            continue;
        }

        if ((size_t)(*images_paths_count) >= capacity) {
            if (capacity > SIZE_MAX / 2 / sizeof(char *)) {
                freeImagePaths(*images_paths, *images_paths_count);
                *images_paths = NULL;
                *images_paths_count = 0;
                closedir(dir);
                return false;
            }
            size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
            char **resized_paths =
                (char **)realloc(*images_paths, new_capacity * sizeof(char *));
            if (resized_paths == NULL) {
                freeImagePaths(*images_paths, *images_paths_count);
                *images_paths = NULL;
                *images_paths_count = 0;
                closedir(dir);
                return false;
            }
            *images_paths = resized_paths;
            capacity = new_capacity;
        }

        (*images_paths)[*images_paths_count] =
            (char *)malloc(PATH_MAX * sizeof(char));
        if ((*images_paths)[*images_paths_count] == NULL) {
            freeImagePaths(*images_paths, *images_paths_count);
            *images_paths = NULL;
            *images_paths_count = 0;
            closedir(dir);
            return false;
        }
        strncpy((*images_paths)[*images_paths_count], image_path, PATH_MAX - 1);
        (*images_paths)[*images_paths_count][PATH_MAX - 1] = '\0';
        (*images_paths_count)++;
    }

    closedir(dir);

    if (*images_paths_count > 1) {
        qsort(*images_paths, *images_paths_count, sizeof(char *), compare_strings);
    }

    return true;
}
