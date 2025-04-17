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
    if (strlen(name) >= 16) {
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
            fs->current_dir[i].size = 0;
            fs->current_dir[i].is_used = 1;
            fs->current_dir[i].is_directory = 0;

            // trova un'entry FAT libera
            int fat_offset = -1;
            for (int j = 0; j < FAT_ENTRIES; j++) {
                if (fs->fat[j].next_block == FREE_BLOCK) {
                    fat_offset = j;
                    fs->fat[j].next_block = FAT_EOF;
                    break;
                }
            }
            if (fat_offset == -1) {
                printf("Error: No free FAT entry for file '%s'.\n", name);
                return -1;
            }

            // memorizza il riferimento alla FAT entry
            fs->current_dir[i].start_block = fat_offset;

            // trova uno spazio libero all'interno del blocco della FAT (blocchi 0 e 1)
            int block_index = fat_offset / (BLOCK_SIZE / sizeof(FATEntry));
            int offset = (fat_offset % (BLOCK_SIZE / sizeof(FATEntry))) * sizeof(FATEntry);

            // debug
            printf("File '%s' created in FAT entry %d (block %d, offset %d).\n", 
                   name, fat_offset, block_index, offset);
            return 0;
        }
    }
    
    printf("Error: No space available in the current directory.\n");
    return -1;
}

// cancella un file dalla directory corrente
int eraseFile(FileSystem* fs, const char *name) {
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

    if (fs->current_dir[file_index].is_directory) {
        printf("Error: you can't delete directory with 'rm' command (use 'rmdir').\n");
        return -1;
    }
    
    // libera i blocchi nella FAT
    int block = fs->current_dir[file_index].start_block;
    while (block != FAT_EOF) {
        int next_block = fs->fat[block].next_block;
        fs->fat[block].next_block = FREE_BLOCK;
        
        // calcola l'offset per la FAT entry
        int fat_offset = (block * sizeof(FATEntry)) % BLOCK_SIZE;
        FATEntry *fat_entry = (FATEntry *)(fs->buffer_fs + (block / (BLOCK_SIZE / sizeof(FATEntry))) * BLOCK_SIZE + fat_offset);
        memset(fat_entry, 0, sizeof(FATEntry));
        
        block = next_block;
    }

    // cancella l'entry del file
    memset(&fs->current_dir[file_index], 0, sizeof(FileEntry));
    
    printf("File '%s' deleted successfully.\n", name);
    return 0;
}

// creazione nuova subdirectory nella directory corrente (WORK IN PROGRESS)
/* int createDir(FileSystem *fs, const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs->current_dir[i].is_used == 0) {
            // Initialize new directory entry in current directory
            strcpy(fs->current_dir[i].name, name);
            fs->current_dir[i].is_used = 1;
            fs->current_dir[i].is_directory = 1;

            // Find a free block in the data region (starting at block 10)
            int new_block = -1;
            for (int j = 10; j < FAT_ENTRIES; j++) {
                if (fs->fat[j].next_block == FREE_BLOCK) {
                    new_block = j;
                    fs->fat[j].next_block = FAT_EOF;

                    fs->current_dir[i].start_block = new_block;

                    // Calculate pointer to new directory's block in memory
                    FileEntry *new_dir = (FileEntry *)(fs->buffer_fs + (new_block * BLOCK_SIZE));
                    memset(new_dir, 0, MAX_FILES * sizeof(FileEntry));

                    // "." entry (self)
                    strcpy(new_dir[0].name, name);
                    new_dir[0].is_used = 1;
                    new_dir[0].is_directory = 1;
                    new_dir[0].start_block = new_block;

                    // ".." entry (parent)
                    strcpy(new_dir[1].name, "..");
                    new_dir[1].is_used = 1;
                    new_dir[1].is_directory = 1;
                    new_dir[1].start_block = fs->current_dir->start_block;

                    // After creating the directory, the current directory should still point to the root or the current directory.
                    printf("Directory '%s' created successfully in block %d.\n", name, new_block);

                    // **FIX**: Ensure current_dir points to the new directory's block
                    fs->current_dir = new_dir;  // Update the current directory to the new directory

                    return 0;
                }
            }

            // No free data block found
            printf("Error: No free data block available for directory '%s'.\n", name);
            return -1;
        }
    }

    // No space left in current directory
    printf("Error: No free slot in current directory for '%s'.\n", name);
    return -1;
} */

// elenca contenuti della directory corrente
void listDir(FileSystem *fs) {
    int n = 0;
    printf("Contents of directory '%s':\n", fs->current_dir->name);
    for (int i=0; i<MAX_FILES; i++) {
        if (fs->current_dir[i].is_used) {
            printf("%s\n", fs->current_dir[i].name);
            n++;
        }
    }
    if (n==0) printf("Current directory is empty.\n");
    return;
}

// cambia directory (WORK IN PROGRESS)
/* int changeDir(FileSystem *fs, const char *name) {
    // Handle root directory
    if (strcmp(name, "/") == 0) {
        fs->current_dir = fs->root;
        printf("Changed to root directory.\n");
        return 0;
    }

    // Handle parent directory ("..")
    if (strcmp(name, "..") == 0) {
        // If we're already at the root, we cannot go up
        if (fs->current_dir == fs->root) {
            printf("Already at root directory.\n");
            return -1;
        }

        // Retrieve the parent directory block from the ".." entry (index 1)
        int parent_block = fs->current_dir[1].start_block;

        // Validate the parent block
        if (parent_block < 0 || parent_block >= FAT_ENTRIES) {
            printf("Error: Invalid parent block.\n");
            return -1;
        }

        // Retrieve the parent directory's entries (block)
        FileEntry *parent_dir = (FileEntry *)(fs->buffer_fs + (parent_block * BLOCK_SIZE));

        // Ensure the directory is valid
        if (parent_dir == NULL) {
            printf("Error: Failed to access parent directory.\n");
            return -1;
        }

        // Update current_dir to the parent directory
        fs->current_dir = parent_dir;

        printf("Changed to parent directory.\n");
        return 0;
    }

    // Handle specific subdirectory change
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs->current_dir[i].is_used && fs->current_dir[i].is_directory && strcmp(fs->current_dir[i].name, name) == 0) {
            int block = fs->current_dir[i].start_block;

            // Validate block index
            if (block < 0 || block >= FAT_ENTRIES) {
                printf("Error: Invalid directory block.\n");
                return -1;
            }

            // Retrieve the new directory's block and update current_dir
            FileEntry *new_dir = (FileEntry *)(fs->buffer_fs + (block * BLOCK_SIZE));
            fs->current_dir = new_dir;

            printf("Changed directory to '%s'.\n", name);
            return 0;
        }
    }

    printf("Error: Directory '%s' not found.\n", name);
    return -1;
} */

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
    else if (strcmp(command, "mk") == 0) {
        if (n == 2)
            createFile(fs, arg1, 0);
        else
            printf("To use this command: mk <filename>\n");
    }
    else if (strcmp(command, "rm") == 0) {
        if (n == 2)
            eraseFile(fs, arg1);
        else
            printf("To use this command: rm <filename>\n");
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
    fs.fat = (FATEntry *)(fs.buffer_fs);
    fs.root = (FileEntry *)(fs.buffer_fs + (FAT_ENTRIES * sizeof(FATEntry)));
    fs.current_dir = fs.root;

    // Inizializza FAT e root directory
    memset(fs.fat, 0, FAT_ENTRIES * sizeof(FATEntry));
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