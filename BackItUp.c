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

int RESTORE_MODE = 0;

void freeData(info *data) {
    free(data->file);
    free(data->path);
    free(data);
}

void *copyFile(void *arg) {
    info *data = (info *) arg;

    char filePath[SIZE], backupPath[SIZE]; // Create paths to the original and backup file
    sprintf(filePath, "%s/%s", data->path, data->file);
    sprintf(backupPath, "%s/.backup/%s.bak", data->path, data->file);

    // RESTORE: For the above section I recommend switching the paths
    // So we read from the .backup/file and write to the original file

    if (stat(backupPath, &data->backupStats) == -1) printf("Backing up %s\n", data->file);
    else if (data->originalStats.st_mtime > data->backupStats.st_mtime) printf("WARNING: Overwriting %s\n", data->file); // Determine course of action and output message
    else {
        printf("%s is up to date\n", data->file);
        freeData(data);
        pthread_exit(NULL);
    }

    // RESTORE: For the above section I recommend a simple if - else
    // If (Restore) Compare appropriate values to determine if the original file must be overwritten by the .backup/file
    // Else Do what is already present

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
    int num_bytes_copied = 0;
    char buffer[SIZE];
    while ((rd = read(iFd, buffer, SIZE)) > 0) {
        write(oFd, buffer, rd); // Copy file
        num_bytes_copied += rd;
    }

    printf("Copied %d bytes from %s to %s.bak\n", num_bytes_copied, data->file, data->file);

    close(iFd);
    close(oFd);
    freeData(data);
    pthread_exit(NULL);
}

void newThread(info *data) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, &copyFile, (void *) data) != 0) { // Create new thread
        freeData(data);
        return;
    }

    thr *node = (thr *) malloc(sizeof(thr));
    node->thread = thread;
    node->next = NULL;

    if (head == NULL) head = node; // Append thread to linked list
    if (tail != NULL) tail->next = node;
    tail = node;
}

void joinThreads() {
    thr *temp = head;
    while (head != NULL) { // Clean up thread resources in the linked list
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
    sprintf(pathToBackup, "%s/.backup/", directory); // Make .backup/ directory
    mkdir(pathToBackup, 0755);

    struct dirent *file;
    while ((file = readdir(dir)) != NULL) { // For each item in directory
        if (file->d_name[0] == '.') continue; // Skip hidden

        struct stat status;
        char pathToFile[SIZE];
        sprintf(pathToFile, "%s/%s", directory, file->d_name); // Construct path to the item
        if (stat(pathToFile, &status) == -1) continue;

        info *data = (info *) malloc(sizeof(info));
        data->file = strdup(file->d_name);
        data->originalStats = status;
        data->path = strdup(directory);

        if (S_ISDIR(data->originalStats.st_mode)) { // If directory
            freeData(data);
            enterDir(pathToFile); // Recursive call
        }
        else if (S_ISREG(data->originalStats.st_mode)) newThread(data); // If regular file make a new thread
        else freeData(data);
    }

    closedir(dir);
}

void main(int argc, char *argv[]) {

    if (argc > 1) {
        if (!strcmp(argv[1], "-r")) RESTORE_MODE = 1;
        exit(0);
    }

    enterDir(".");
    joinThreads();
    exit(0);
}
