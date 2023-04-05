#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

void main() {
    if (!mkdir(".backup", 0644)) printf("Success: Backup directory created\n");
    else {
        printf("Error: %s\n", strerror(errno));
        exit(1);
    }
    exit(0);
}
