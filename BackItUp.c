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

typedef struct info {
    char *filename, *originalDir, *backupDir;
    struct stat originalStats, backupStats;
} Info;

typedef struct tNode {
    pthread_t thread;
    struct tNode *next;
} thr;

thr *head = NULL;
thr *tail = NULL;

int TOTAL_THREADS = 0;
int TOTAL_FILES_COPIED = 0;
int TOTAL_BYTES_COPIED = 0;
int RESTORE_MODE = 0;

void freeData(Info *data) {
    free(data->filename);
    free(data->originalDir);
    free(data->backupDir);
    free(data);
}

void *restoreFile(void *arg) {
    Info *data = (Info *) arg;

    char *originalFilename = strdup(data->filename);
    originalFilename[strlen(data->filename) - 4] = '\0';  // remove '.bak' from filename

    char filePath[SIZE], backupPath[SIZE];
    sprintf(filePath, "%s/%s", data->originalDir, originalFilename);
    sprintf(backupPath, "%s/%s", data->backupDir, data->filename);

    if (stat(filePath, &data->originalStats) == -1 || data->backupStats.st_mtime > data->originalStats.st_mtime) {
        printf("Restoring %s\n", originalFilename);
    } else {
        printf("%s is up to date\n", originalFilename);
        freeData(data);
        free(originalFilename);
        pthread_exit(NULL);
    }

    int iFd = open(backupPath, O_RDONLY);
    if (iFd == -1) {
        freeData(data);
        free(originalFilename);
        pthread_exit(NULL);
    }

    int oFd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (oFd == -1) {
        close(iFd);
        freeData(data);
        free(originalFilename);
        pthread_exit(NULL);
    }

    int rd;
    char buffer[SIZE];
    while ((rd = read(iFd, buffer, SIZE)) > 0) write(oFd, buffer, rd);

    close(iFd);
    close(oFd);
    freeData(data);
    free(originalFilename);
    pthread_exit(NULL);
}

void *backupFile(void *arg) {
    Info *data = (Info *) arg;

    char filePath[SIZE], backupPath[SIZE];
    sprintf(filePath, "%s/%s", data->originalDir, data->filename);
    sprintf(backupPath, "%s/%s.bak", data->backupDir, data->filename);

    if (stat(backupPath, &data->backupStats) == -1) printf("Backing up %s\n", data->filename);
    else if (data->originalStats.st_mtime > data->backupStats.st_mtime) printf("WARNING: Overwriting %s\n", data->filename);
    else {
        printf("%s is up to date\n", data->filename);
        freeData(data);
        pthread_exit(NULL);
    }

    int iFd = open(filePath, O_RDONLY);
    if (iFd == -1) {
        freeData(data);
        pthread_exit(NULL);
    }

    int oFd = open(backupPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

void newThread(Info *data) {
    pthread_t thread;

    if (RESTORE_MODE) {
        if (pthread_create(&thread, NULL, &restoreFile, (void *) data) != 0) {
            freeData(data);
            return;
        }
    } else {
        if (pthread_create(&thread, NULL, &backupFile, (void *) data) != 0) {
            freeData(data);
            return;
        }
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

void backup(char *directory) {
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

        Info *data = (Info *) malloc(sizeof(Info));
        data->filename = strdup(file->d_name);
        data->originalDir = strdup(directory);
        data->backupDir = strdup(pathToBackup);
        data->originalStats = status;

        if (S_ISDIR(data->originalStats.st_mode)) {
            freeData(data);
            backup(pathToFile);
        }
        else if (S_ISREG(data->originalStats.st_mode)) newThread(data);
        else freeData(data);
    }

    closedir(dir);
}

void restore(char *directory) {
    // try to open the backup
    char pathToBackupDir[SIZE];
    sprintf(pathToBackupDir, "%s/.backup", directory);

    DIR *backupDir = opendir(pathToBackupDir);
    if (backupDir != NULL) {
        // traverse files in backup
        struct dirent *backupFile;
        while((backupFile = readdir(backupDir)) != NULL) {
            if (backupFile->d_name[0] == '.') continue;
            if (strlen(backupFile->d_name) < 5) continue; // handle invalid file lengths (need a .bak ending)

            struct stat backupStats;
            char backupFilePath[SIZE];
            sprintf(backupFilePath, "%s/.backup/%s", directory, backupFile->d_name);
            if (stat(backupFilePath, &backupStats) == -1) continue;

            char filename[SIZE];
            sprintf(filename, "%s/%s", directory, backupFile->d_name);
            filename[strlen(backupFile->d_name) - 4] = '\0';  // remove '.bak' from path

            Info *data = (Info *) malloc(sizeof(Info));
            data->filename = strdup(backupFile->d_name);
            data->originalDir = strdup(directory);
            data->backupDir = strdup(pathToBackupDir);
            data->backupStats = backupStats;

            // handle odd case that this is a directory
            if (S_ISREG(data->backupStats.st_mode)) newThread(data);
            else freeData(data);
        }

        closedir(backupDir);
    }

    // continue traversing file tree
    DIR *dir = opendir(directory);
    if (dir == NULL) return;

    struct dirent *file;
    while ((file = readdir(dir)) != NULL) {
        if (file->d_name[0] == '.') continue;

        struct stat status;
        char pathToFile[SIZE];
        sprintf(pathToFile, "%s/%s", directory, file->d_name);
        if (stat(pathToFile, &status) == -1) continue;

        if(S_ISDIR(status.st_mode)) {
            restore(pathToFile);
        }
    }

    closedir(dir);
}

void main(int argc, char *argv[]) {
    if (argc < 2) {
        backup(".");
    } else if (!strcmp(argv[1], "-r")) {
        RESTORE_MODE = 1;
        restore(".");
    } else {
        printf("Invalid arguments...\n");
        exit(1);
    }

    joinThreads();
    exit(0);
}
