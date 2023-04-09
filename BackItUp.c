#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#define SIZE 1024

void log_result(char *op, char *item) {
    printf("%s | %s | %s\n", op, item, strerror(errno));
    errno = 0;
}

void copyFile(char *file) {
    int iFd = open(file, O_RDONLY);
    log_result("open", file);
    if (iFd == -1) return;

    char path[SIZE];
    sprintf(path, ".backup/%s.bak", file);

    int oFd = open(path, O_WRONLY | O_CREAT, 0644);
    log_result("open", path);
    if (oFd == -1) { close(iFd); return; }

    int rd;
    char buffer[SIZE];
    while ((rd = read(iFd, buffer, SIZE)) > 0) write(oFd, buffer, rd);

    close(iFd);
    log_result("close", file);
    close(oFd);
    log_result("close", path);
}

void enterDir(char *directory) {
    DIR *dir = opendir(directory);
    log_result("opendir", directory);
    if (dir == NULL) return;

    int m = chdir(directory);
    log_result("chdir", directory);
    if (m == -1) return;

    mkdir(".backup", 0755);
    log_result("mkdir", ".backup");

    struct dirent *file;
    struct stat status;
    while ((file = readdir(dir)) != NULL){
        if (file->d_name[0] == '.') continue;

        int n = stat(file->d_name, &status);
        log_result("stat", file->d_name);
        if (n == -1) continue;

        if (S_ISDIR(status.st_mode)) enterDir(file->d_name);
        else if (S_ISREG(status.st_mode)) copyFile(file->d_name);
    }

    chdir("..");
    log_result("chdir", "..");
    closedir(dir);
    log_result("closedir", directory);
}

void main() {
    enterDir(".");
    exit(0);
}
