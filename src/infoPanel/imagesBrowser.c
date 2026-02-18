#include "imagesBrowser.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define STR_MAX 256
/* dir_path <= STR_MAX-1 bytes, trailing '/', filename <= NAME_MAX-1 bytes */
#define IMAGES_PATH_SIZE (STR_MAX * 2)

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
                         char *image_path, size_t image_path_size)
{
    char ext[50];
    const char *filename = ent->d_name;
    if (filename[0] == '.' || S_ISDIR(ent->d_type & DT_DIR)) {
        return false;
    }
    strncpy(ext, getFilenameExt(filename), sizeof(ext) - 1);
    ext[sizeof(ext) - 1] = '\0';
    const char *fileExt = toLower(ext);
    if (strcmp(fileExt, "png") == 0 || strcmp(fileExt, "jpg") == 0 ||
        strcmp(fileExt, "jpeg") == 0) {
        snprintf(image_path, image_path_size, "%s%s", dir_path, filename);
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
        char image_path[IMAGES_PATH_SIZE];
        const bool is_image = getImagePath(dir_path, ent, image_path, sizeof(image_path));
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

bool loadImagesPathsFromDir(const char *dir_path, char ***images_paths,
                            int *images_paths_count)
{
    char normalized_dir_path[IMAGES_PATH_SIZE];
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
    *images_paths = (char **)malloc(images_count * sizeof(char *));
    if (*images_paths == NULL) {
        closedir(dir);
        return false;
    }

    while ((ent = readdir(dir)) != NULL) {
        char image_path[IMAGES_PATH_SIZE];
        const bool is_image =
            getImagePath(normalized_dir_path, ent, image_path, sizeof(image_path));

        if (!is_image) {
            continue;
        }

        char *img_entry = (char *)malloc(IMAGES_PATH_SIZE * sizeof(char));
        if (img_entry == NULL)
            break;
        strncpy(img_entry, image_path, IMAGES_PATH_SIZE - 1);
        img_entry[IMAGES_PATH_SIZE - 1] = '\0';
        (*images_paths)[*images_paths_count] = img_entry;
        (*images_paths_count)++;

        if ((*images_paths_count) >= images_count) {
            // we found more images than allocated memory
            // TODO: handle this
            break;
        }
    }

    closedir(dir);

    qsort(*images_paths, *images_paths_count, sizeof(char *), compare_strings);

    return true;
}