#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

#define SIZE 1024

typedef struct dataNode {
    char *file;
    char *path;
    struct stat originalStats;
    struct stat backupStats;
} info;

typedef struct tNode {
    pthread_t thread;
    struct tNode *next;
} thr;

thr *head = NULL;
thr *tail = NULL;

void freeData(info *data) {
    free(data->file);
    free(data->path);
    free(data);
}

void *copyFile(void *arg) {
    info *data = (info *) arg;
    printf("in thread: <%s>; <%s>\n", data->path, data->file);

    char filePath[SIZE], backupPath[SIZE];
    sprintf(filePath, "%s/%s", data->path, data->file);
    sprintf(backupPath, "%s/.backup/%s.bak", data->path, data->file);

    if (stat(backupPath, &data->backupStats) == -1) printf("Backing up %s\n", data->file);
    else if (data->originalStats.st_mtime > data->backupStats.st_mtime) printf("WARNING: Overwriting %s\n", data->file);
    else {
        printf("%s is up to date\n", data->file);
        freeData(data);
        pthread_exit(NULL);
    }

    int iFd = open(filePath, O_RDONLY);
    if (iFd == -1) {
        freeData(data);
        pthread_exit(NULL);
    }

    int oFd = open(backupPath, O_WRONLY | O_CREAT, 0644);
    if (oFd == -1) {
        close(iFd);
        freeData(data);
        pthread_exit(NULL);
    }

    int rd;
    char buffer[SIZE];
    while ((rd = read(iFd, buffer, SIZE)) > 0) write(oFd, buffer, rd);

    close(iFd);
    close(oFd);
    freeData(data);
    pthread_exit(NULL);
}

void newThread(info *data) {
    pthread_t thread;
    printf("starting thread: <%s>; <%s>\n", data->path, data->file);
    if (pthread_create(&thread, NULL, &copyFile, (void *) data) != 0) {
        freeData(data);
        return;
    }

    thr *node = (thr *) malloc(sizeof(thr));
    node->thread = thread;
    node->next = NULL;

    if (head == NULL) head = node;
    if (tail != NULL) tail->next = node;
    tail = node;
}

void joinThreads() {
    thr *temp = head;
    while (head != NULL) {
        head = head->next;
        pthread_join(temp->thread, NULL);
        free(temp);
        temp = head;
    }
}

void enterDir(char *directory) {
    DIR *dir = opendir(directory);
    if (dir == NULL) return;

    char pathToBackup[SIZE];
    sprintf(pathToBackup, "%s/.backup/", directory);
    mkdir(pathToBackup, 0755);

    struct dirent *file;
    while ((file = readdir(dir)) != NULL) {
        if (file->d_name[0] == '.') continue;

        struct stat status;
        char pathToFile[SIZE];
        sprintf(pathToFile, "%s/%s", directory, file->d_name);
        if (stat(pathToFile, &status) == -1) continue;

        info *data = (info *) malloc(sizeof(info));
        data->file = strdup(file->d_name);
        data->originalStats = status;
        data->path = strdup(directory);

        if (S_ISDIR(data->originalStats.st_mode)) {
            freeData(data);
            enterDir(pathToFile);
        }
        else if (S_ISREG(data->originalStats.st_mode)) newThread(data);
        else freeData(data);
    }

    closedir(dir);
}

void main() {
    enterDir(".");
    joinThreads();
    exit(0);
}
