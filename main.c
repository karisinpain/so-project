#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "fs_struct.h"

// elenca contenuti di una directory
void listDir(FileSystem *fs) {
    int n = 0;
    printf("Contents of directory:\n");
    for (int i=0; i<MAX_FILES; i++) {
        if (fs->current_dir[i].is_used) {
            printf("%s\n", fs->current_dir[i].name);
            n++;
        }
    }
    if (n==0) printf("Current directory is empty.\n");
    return;
}

// elabora comando ricevuto in input
void processCommand(FileSystem *fs, const char *input) {
    char command[32], arg1[32], arg2[32];
    int n = sscanf(input, "%s %s %s", command, arg1, arg2);
    if (strcmp(command, "ls") == 0)
        listDir(fs);
    else if (strcmp(command, "exit") == 0) {
        cleanup(fs);
    }
    else
        printf("Command unkown or not implemented yet.\n");
}

// chiusura e uscita dal file system
void cleanup(FileSystem *fs) {
    printf("Exiting file system...\n");
    if (munmap(fs->buffer_fs, FS_SIZE) == -1)
        perror("Error unmapping memory.");
    close(fs->fs_fd);
    printf("File system closed succesfully.\n");
    exit(0);
}

// main program
int main() {
    
    // inizializza file system
    int fs_fd = open("fs.img", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fs_fd == -1) {
        perror("Error opening file system.");
        return -1;
    }

    // imposta dimensione del file system
    if (ftruncate(fs_fd, FS_SIZE) == -1) {
        perror("Error setting file size.");
        return -1;
    }

    // effettua mappatura in memoria
    FileSystem fs;
    fs.buffer_fs = mmap(NULL, FS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    if (fs.buffer_fs == MAP_FAILED) {
        perror("Error mapping file in memory.");
        return -1;
    }

    // inizializza struttura del file system
    // il layout della memoria sul buffer è definito in questo ordine: tabella FAT, root directory e blocchi di dati
    fs.fs_fd = fs_fd;
    fs.fat = (FATEntry *) (fs.buffer_fs);
    fs.root = (FileEntry *) (fs.buffer_fs + FAT_ENTRIES); 
    fs.current_dir = fs.root;

    memset(fs.fat, 0, FAT_ENTRIES);
    memset(fs.root, 0, MAX_FILES * sizeof(FileEntry));

    char input[128];
    while(1) {
        printf("fs > ");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';  // sostituisce il newline alla fine con un terminatore di stringa
        processCommand(&fs, input);
    }
}