#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

#define SIZE 2048

typedef struct info {
    char *filename, *originalDir, *backupDir;
    struct stat originalStats, backupStats;
    int thread_num;
} Info;

typedef struct tNode {
    pthread_t thread;
    struct tNode *next;
} thr;

// Thread linked list:
thr *head = NULL;
thr *tail = NULL;

// Thread + copy global vars:
pthread_mutex_t total_mtx = PTHREAD_MUTEX_INITIALIZER;
int TOTAL_THREADS = 0;
int TOTAL_FILES_COPIED = 0;
int TOTAL_BYTES_COPIED = 0;
int RESTORE_MODE = 0;


// freeData frees a malloc'd Info struct.
void freeData(Info *data) {
    free(data->filename);
    free(data->originalDir);
    free(data->backupDir);
    free(data);
}

// overwrite is a generic function that completely
// overwrites the destination file with the source file.
// it returns the number of bytes copied if succesful, and -1
// if unsuccesful.
int overwrite(char *source, char *destination) {
    int iFd = open(source, O_RDONLY);
    if (iFd == -1) return -1;

    int oFd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (oFd == -1) {
        close(iFd);
        return -1;
    }

    int rd;
    int num_bytes_copied = 0;
    char buffer[SIZE];
    while ((rd = read(iFd, buffer, SIZE)) > 0) {
        write(oFd, buffer, rd);
        num_bytes_copied += rd;
    }

    close(iFd);
    close(oFd);

    return num_bytes_copied;
}

// restoreFile is responsible for checking if a given file
// should be restored or not, and performing the restore if applicable.
void *restoreFile(void *arg) {
    Info *data = (Info *) arg;

    // get the original filename (without .bak)
    char *originalFilename = strdup(data->filename);
    originalFilename[strlen(data->filename) - 4] = '\0';

    // get the full filepaths for the backup and original files.
    char filePath[SIZE], backupPath[SIZE];
    sprintf(filePath, "%s/%s", data->originalDir, originalFilename);
    sprintf(backupPath, "%s/%s", data->backupDir, data->filename);

    printf("[thread %d] Restoring %s\n", data->thread_num ,originalFilename);

    // figure out whether to restore or not...
    if (stat(filePath, &data->originalStats) != -1 && data->backupStats.st_mtime <= data->originalStats.st_mtime) {
        // no restore needed..
        printf("[thread %d] %s is already the most current version\n", data->thread_num, originalFilename);
        freeData(data);
        free(originalFilename);
        pthread_exit(NULL);
    }

    // perform the restore, and update the global vars...
    int num_bytes_copied = overwrite(backupPath, filePath);
    if (num_bytes_copied > -1) {
        printf("[thread %d] Copied %d bytes from %s to %s\n", data->thread_num, num_bytes_copied, data->filename, originalFilename);

        pthread_mutex_lock(&total_mtx);
        TOTAL_BYTES_COPIED += num_bytes_copied;
        TOTAL_FILES_COPIED++;
        pthread_mutex_unlock(&total_mtx);
    }

    freeData(data);
    free(originalFilename);
    pthread_exit(NULL);
}

// backupFile is responsible for checking if a given file should be backed up
// or not, and performs the backup if applicable.
void *backupFile(void *arg) {
    Info *data = (Info *) arg;

    char filePath[SIZE], backupPath[SIZE];
    sprintf(filePath, "%s/%s", data->originalDir, data->filename);
    sprintf(backupPath, "%s/%s.bak", data->backupDir, data->filename);

    printf("[thread %d] Backing up %s\n", data->thread_num, data->filename);

    if (stat(backupPath, &data->backupStats) == -1) ; 
    else if (data->originalStats.st_mtime > data->backupStats.st_mtime) printf("[thread %d] WARNING: Overwriting %s.bak\n", data->thread_num, data->filename);
    else {
        printf("[thread %d] %s does not need backing up\n", data->thread_num, data->filename);
        freeData(data);
        pthread_exit(NULL);
    }

    // perform the backup and update global vars...
    int num_bytes_copied = overwrite(filePath, backupPath);
    if (num_bytes_copied > -1) {
        printf("[thread %d] Copied %d bytes from %s to %s.bak\n", data->thread_num, num_bytes_copied, data->filename, data->filename);

        pthread_mutex_lock(&total_mtx);
        TOTAL_BYTES_COPIED += num_bytes_copied;
        TOTAL_FILES_COPIED++;
        pthread_mutex_unlock(&total_mtx);
    }

    freeData(data);
    pthread_exit(NULL);
}

// newThread creates a new thread, dispatches it to the correct
// thread function (depending on RESTORE_MODE value), and adds the 
// thread to a linked list for later joining.
void newThread(Info *data) {
    pthread_t thread;
    data->thread_num = ++TOTAL_THREADS;

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

    // create a linked list node and append the thread to the list.
    // this list of threads will be joined at the end of the program.
    thr *node = (thr *) malloc(sizeof(thr));
    node->thread = thread;
    node->next = NULL;

    if (head == NULL) head = node;
    if (tail != NULL) tail->next = node;
    tail = node;
}

// joinThreads is self explanatory. It allows the main
// program to join on all threads created by newThread()
void joinThreads() {
    thr *temp = head;
    while (head != NULL) { // Clean up thread resources in the linked list
        head = head->next;
        pthread_join(temp->thread, NULL);
        free(temp);
        temp = head;
    }
}

// backup is responsible for recursing the file tree, creating .backup dirs
// where needed, and locating files for potential backup.
void backup(char *directory) {
    DIR *dir = opendir(directory);
    if (dir == NULL) return;

    // create the .backup directory if it doesn't already exist
    char pathToBackup[SIZE];
    sprintf(pathToBackup, "%s/.backup/", directory); // Make .backup/ directory
    mkdir(pathToBackup, 0755);

    // recurse through the directory entries
    struct dirent *file;
    while ((file = readdir(dir)) != NULL) { // For each item in directory
        if (file->d_name[0] == '.') continue; // Skip hidden

        // get the file stat
        struct stat status;
        char pathToFile[SIZE];
        sprintf(pathToFile, "%s/%s", directory, file->d_name); // Construct path to the item
        if (stat(pathToFile, &status) == -1) continue;

        if (S_ISDIR(status.st_mode)) backup(pathToFile); // is a directory, not a file. recurse into this file for backup
        
        if (S_ISREG(status.st_mode)) {
            // If regular file, prep for possible backup in new thread;

            Info *data = (Info *) malloc(sizeof(Info));
            data->filename = strdup(file->d_name);
            data->originalDir = strdup(directory);
            data->backupDir = strdup(pathToBackup);
            data->originalStats = status;
            newThread(data);
        } 
    }

    closedir(dir);
}

// restore is responsible for recursing through the file tree, locating
// existing .backup directories, and locating .bak files within those
// directories for possible restore.
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

            // get the backup stat
            struct stat backupStats;
            char backupFilePath[SIZE];
            sprintf(backupFilePath, "%s/.backup/%s", directory, backupFile->d_name);
            if (stat(backupFilePath, &backupStats) == -1) continue;

            if (S_ISREG(backupStats.st_mode)) {
                // if regular file, prep for possible restore in new thread
                Info *data = (Info *) malloc(sizeof(Info));
                data->filename = strdup(backupFile->d_name);
                data->originalDir = strdup(directory);
                data->backupDir = strdup(pathToBackupDir);
                data->backupStats = backupStats;
                newThread(data);
            }
        }

        // done!
        closedir(backupDir);
    }

    // continue traversing file tree
    DIR *dir = opendir(directory);
    if (dir == NULL) return;

    struct dirent *file;
    while ((file = readdir(dir)) != NULL) {
        if (file->d_name[0] == '.') continue;

        // get the file stat
        struct stat status;
        char pathToFile[SIZE];
        sprintf(pathToFile, "%s/%s", directory, file->d_name);
        if (stat(pathToFile, &status) == -1) continue;

        if(S_ISDIR(status.st_mode)) restore(pathToFile);
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
    printf("Successfully copied %d files (%d bytes)\n", TOTAL_FILES_COPIED, TOTAL_BYTES_COPIED);
    exit(0);
}
