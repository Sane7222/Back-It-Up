#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#define SIZE 1024

typedef struct node {
    char *file;
    struct stat orig, back;
} info;

void log_result(char *op, char *item) {
    printf("%s | %s | %s\n", op, item, strerror(errno));
    errno = 0;
}

void copyFile(void *arg) {
    info *data = (info *) arg;

    int iFd = open(data->file, O_RDONLY);
    log_result("open", data->file);
    if (iFd == -1) { free(data); return; }

    char path[SIZE];
    sprintf(path, ".backup/%s.bak", data->file);

    int n = stat(path, &data->back);
    log_result("stat", path);

    if (n == -1) printf("Creating Backup of %s\n", data->file);
    else if (data->orig.st_mtime > data->back.st_mtime) printf("WARNING: Overwriting %s\n", data->file);
    else printf("%s is up to date\n", data->file);

    int oFd = open(path, O_WRONLY | O_CREAT, 0644);
    log_result("open", path);
    if (oFd == -1) { close(iFd); free(data); return; }

    int rd;
    char buffer[SIZE];
    while ((rd = read(iFd, buffer, SIZE)) > 0) write(oFd, buffer, rd);

    close(iFd);
    log_result("close", data->file);
    close(oFd);
    log_result("close", path);
    free(data);
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
    while ((file = readdir(dir)) != NULL){
        if (file->d_name[0] == '.') continue;

        info *data = (info *) malloc(sizeof(info));
        data->file = file->d_name;

        int n = stat(file->d_name, &data->orig);
        log_result("stat", file->d_name);
        if (n == -1) { free(data); continue; }

        if (S_ISDIR(data->orig.st_mode)) { enterDir(file->d_name); free(data); }
        else if (S_ISREG(data->orig.st_mode)) copyFile((void *) data);
        else free(data);
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
