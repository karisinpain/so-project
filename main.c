#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "fs_struct.h"

// creazione nuovo file nella directory corrente
int createFile(FileSystem *fs, const char *name, int file_size) {

    // controlla che il nome sia valido
    if (strlen(name) >= 32) {
        printf("Error: name is too long.\n");
        return -1;
    }

    // controlla se esiste già un file con lo stesso nome
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs->current_dir[i].is_used == 1 && strcmp(fs->current_dir[i].name, name) == 0) {
            printf("Error: A file with the name '%s' already exists.\n", name);
            return -1;
        }
    }
    
    // cerca un posto disponibile nella directory corrente
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs->current_dir[i].is_used == 0) {
            strcpy(fs->current_dir[i].name, name);
            fs->current_dir[i].start_block = -1;  // Indica che il file non ha ancora un blocco di partenza
            fs->current_dir[i].size = 0;  // La dimensione iniziale è 0 (file vuoto)
            fs->current_dir[i].is_used = 1;
            fs->current_dir[i].is_directory = 0;  // Non è una directory

            // trova un blocco libero per il file
            for (int j = 0; j < FAT_ENTRIES; j++) {
                if (fs->fat[j].next_block == FREE_BLOCK) {
                    fs->current_dir[i].start_block = j;
                    fs->fat[j].next_block = FAT_EOF;
                    printf("File '%s' created successfully.\n", name);
                    return 0;
                }
            }
            
            // se non c'è abbastanza spazio libero nella FAT
            printf("Error: Not enough free space for file '%s'.\n", name);
            return -1;
        }
    }
    
    // se non c'è spazio disponibile nella directory
    printf("Error: No space available in the current directory.\n");
    return -1;
}

// cancella un file dalla directory corrente
int eraseFile(FileSystem* fs, const char *name) {

    if (strcmp(name, "/") == 0) {
        printf("You can't delete the root directory!\n");
        return -1;
    }
    
    // trova il file nella directory
    int file_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs->current_dir[i].is_used && strcmp(fs->current_dir[i].name, name) == 0) {
            file_index = i;
            break;
        }
    }
    if (file_index == -1) {
        printf("Error: File '%s' not found.\n", name);
        return -1;
    }
    
    // libera i blocchi nella FAT
    int block = fs->current_dir[file_index].start_block;
    while (block != FAT_EOF) {
        int next_block = fs->fat[block].next_block;
        fs->fat[block].next_block = FREE_BLOCK;
        block = next_block;
    }

    // cancella l'entry del file
    memset(&fs->current_dir[file_index], 0, sizeof(FileEntry));
    
    printf("File '%s' deleted successfully.\n", name);
    return 0;
}


// elenca contenuti della directory corrente
void listDir(FileSystem *fs) {
    int n = 0;
    printf("Contents of directory '%s':\n", fs->current_dir->name);
    for (int i=1; i<MAX_FILES; i++) {
        if (fs->current_dir[i].is_used) {
            printf("%s\n", fs->current_dir[i].name);
            n++;
        }
    }
    if (n==0) printf("Current directory is empty.\n");
    return;
}

// chiusura e uscita dal file system
void cleanup(FileSystem *fs) {
    printf("Exiting file system...\n");
    if (munmap(fs->buffer_fs, FS_SIZE) == -1)
        perror("Error unmapping memory.");
    if (close(fs->fs_fd) == -1)
        perror("Error closing file descriptor of the file system.");
    printf("File system closed succesfully.\n");
    exit(0);
}

// elabora comando ricevuto in input
void processCommand(FileSystem *fs, const char *input) {
    char command[32], arg1[32], arg2[32];
    int n = sscanf(input, "%s %s %s", command, arg1, arg2);
    if (strcmp(command, "exit") == 0) {
        cleanup(fs);
    }
    else if (strcmp(command, "create") == 0) {
        if (n == 2)
            createFile(fs, arg1, 0);
        else
            printf("To use this command: create <filename>\n");
    }
    else if (strcmp(command, "delete") == 0) {
        if (n == 2)
            eraseFile(fs, arg1);
        else
            printf("To use this command: delete <filename>\n");
    }
    else if (strcmp(command, "ls") == 0)
        listDir(fs);
    else
        printf("Command unkown or not implemented yet.\n");
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
    fs.root[0].is_used = 1;
    fs.root[0].is_directory = 1;
    strcpy(fs.root[0].name, "/");
    fs.root[0].start_block = -1;

    char input[128];
    while(1) {
        printf("fs > ");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';  // sostituisce il newline alla fine con un terminatore di stringa
        processCommand(&fs, input);
    }
}