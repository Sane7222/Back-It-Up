#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

void log_result(char *op, char *item) {
    printf("%s | %s | %s\n", op, item, strerror(errno));
    errno = 0;
}

void copyFile(char *file) {
    printf("%s\n", file);
}

void enterDir(char *directory) {
    DIR *dir = opendir(directory);
    log_result("opendir", directory);
    if (dir == NULL) return;

    struct dirent *file;
    struct stat status;
    while ((file = readdir(dir)) != NULL){
        if (file->d_name[0] == '.') continue;

        int n = stat(file->d_name, &status);
        log_result("stat", file->d_name);
        if (n) continue;

        char path[1024];
        sprintf(path, "%s/%s", directory, file->d_name);

        if (S_ISDIR(status.st_mode)) enterDir(path);
        else copyFile(path);
    }

    closedir(dir);
}

void main() {
    int n = mkdir(".backup", 0644);
    log_result("mkdir", ".backup");

    enterDir(".");
    exit(0);
}
