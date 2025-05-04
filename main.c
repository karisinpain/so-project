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

            return 0;
        }
    }
    
    printf("Error: No space available in the current directory.\n");
    return -1;
}

// elimina un file dalla directory corrente
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

// funzioni ausiliari per createDir
int getBlockFromPtr(FileSystem *fs, FileEntry *dir_ptr) {
    uintptr_t offset = (uintptr_t)dir_ptr - (uintptr_t)fs->buffer_fs;
    return offset / BLOCK_SIZE;
}
int findFreeDataBlockInBuffer(FileSystem *fs) {
    for (int data_block = 10; data_block < BLOCK_ENTRIES; data_block++) {
        if (fs->fat[data_block].next_block == FREE_BLOCK) {
            // trova un blocco libero
            for (int fat_index = 0; fat_index < BLOCK_ENTRIES; fat_index++) {
                if (fs->fat[fat_index].next_block == 0 && fat_index != data_block) {
                    fs->fat[fat_index].next_block = data_block;
                    fs->fat[data_block].next_block = FAT_EOF;
                    printf("Found new data block %d, with fat entry %d.\n", data_block, fat_index);
                    return fat_index;
                }
            }
            printf("No free FAT entry found.\n");
            return -1;
        }
    }
    printf("No free data block found.\n");
    return -1;
}

// creazione nuova subdirectory nella directory corrente (NEED FIX AFTER UPDATE)
int createDir(FileSystem *fs, const char *name) {

    // controlla che il nome sia valido
    if (strlen(name) >= 16) {
        printf("Error: name is too long.\n");
        return -1;
    }
    
    // controlla se esiste già una directory con lo stesso nome
    for (int k = 0; k < MAX_FILES; k++) {
        if (fs->current_dir[k].is_used && strcmp(fs->current_dir[k].name, name) == 0) {
            printf("Error: Entry with name '%s' already exists.\n", name);
            return -1;
        }
    }
    
    // cerca un posto disponibile nella directory corrente
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs->current_dir[i].is_used == 0) {
            // trova un'entry FAT libera
            int fat_offset = -1;
            for (int j = 0; j < FAT_ENTRIES; j++) {
                if (fs->fat[j].next_block == FREE_BLOCK) {
                    fat_offset = j;

                    // inizializza la nuova directory
                    strcpy(fs->current_dir[i].name, name);
                    fs->current_dir[i].is_used = 1;
                    fs->current_dir[i].is_directory = 1;
                    fs->current_dir[i].start_block = fat_offset;
                    fs->current_dir[i].size = 0;

                    // trova un blocco dati libero da assegnare alla nuova directory
                    int dir_block = findFreeDataBlockInBuffer(fs);
                    if (dir_block == -1) {
                        printf("No free data blocks for directory.\n");
                        return -1;
                    }
                    fs->fat[dir_block].next_block = FAT_EOF;

                    // inizializza il blocco per la directory
                    FileEntry *new_dir = (FileEntry *)(fs->buffer_fs + (dir_block * BLOCK_SIZE));
                    memset(new_dir, 0, BLOCK_SIZE);

                    fs->fat[j].next_block = dir_block;
                    
                    // "." entry (self)
                    strcpy(new_dir[0].name, name);
                    new_dir[0].is_used = 1;
                    new_dir[0].is_directory = 1;
                    new_dir[0].start_block = fat_offset;
                    new_dir[0].size = 0;

                    // ".." entry (parent)
                    int parent_block = getBlockFromPtr(fs, fs->current_dir);

                    strcpy(new_dir[1].name, "..");
                    new_dir[1].is_used = 1;
                    new_dir[1].is_directory = 1;
                    new_dir[1].start_block = parent_block;
                    new_dir[1].size = 0;
                    return 0;
                }
            }
            printf("Error: No free data block available for directory '%s'.\n", name);
            return -1;
        }
    }
    printf("Error: No free slot in current directory for '%s'.\n", name);
    return -1;
}

// elimina una directory
int eraseDir(FileSystem *fs, const char *name) {
    if (strcmp(name, "..") == 0 || strcmp(name, "/") == 0) { 
        printf("Impossibile eliminare root e/o directory di riferimento.\n");
        return -1;
    }

    int dir_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs->current_dir[i].is_used && strcmp(fs->current_dir[i].name, name) == 0) {
            dir_index = i;
            break;
        }
    }
    if (dir_index == -1) {
        printf("Error: Directory '%s' not found.\n", name);
        return -1;
    }
    if (fs->current_dir[dir_index].is_directory == 0) {
        printf("Error: you can't delete a file with 'rmdir' command (use 'rm').\n");
        return -1;
    }

    // prendiamo il primo blocco dati assegnato alla directory
    int fat_entry = fs->current_dir[dir_index].start_block;
    int data_block = fs->fat[fat_entry].next_block;

    // controlla che la directory sia vuota
    while (data_block != FAT_EOF) {
        FileEntry *dir_entries = (FileEntry *)(fs->buffer_fs + data_block * BLOCK_SIZE);
        // ciclo per ogni blocco di dati della directory
        for (int i=2; i<ENTRIES_PER_BLOCK; i++) {   // le prime due entry sono riservate ai riferimenti della directory stessa e della directory parent
            if (dir_entries[i].is_used) {
                printf("Impossibile eliminare la directory '%s' dato che contiene ancora dei file. Svuotare la directory prima di procedere con l'eliminazione.\n", name);
                return -1;
            }
        }
        // finito il controllo per un blocco, controlla se ci sono altri blocchi di dati da esaminare
        data_block = fs->fat[data_block].next_block;
    }

    // libera i blocchi nella FAT
    int block = fs->current_dir[dir_index].start_block;
    while (block != FAT_EOF) {
        int next_block = fs->fat[block].next_block;
        fs->fat[block].next_block = FREE_BLOCK;
        
        // calcola l'offset per la FAT entry
        int fat_offset = (block * sizeof(FATEntry)) % BLOCK_SIZE;
        FATEntry *fat_entry = (FATEntry *)(fs->buffer_fs + (block / (BLOCK_SIZE / sizeof(FATEntry))) * BLOCK_SIZE + fat_offset);
        memset(fat_entry, 0, sizeof(FATEntry));
        
        block = next_block;
    }

    // cancella l'entry della directory
    memset(&fs->current_dir[dir_index], 0, sizeof(FileEntry));
    
    printf("Directory '%s' deleted successfully.\n", name);
    return 0;    
    
}

// elenca contenuti della directory corrente
void listDir(FileSystem *fs) {
    int n = 0;
    printf("Contents of directory '%s':\n", fs->current_dir->name);

    int fat_entry = fs->current_dir[0].start_block;
    int data_block = fs->fat[fat_entry].next_block;

    while (data_block != FAT_EOF) {
        FileEntry *dir_entries = (FileEntry *)(fs->buffer_fs + data_block * BLOCK_SIZE);
        // ciclo per ogni blocco di dati della directory
        printf("reading block %d.\n", data_block);
        for (int i=0; i<ENTRIES_PER_BLOCK; i++) {
            if (dir_entries[i].is_used) {
                printf("%s\n", dir_entries[i].name);
                n++;
            }
        }
        // finito il controllo per un blocco, controlla se ci sono altri blocchi di dati da esaminare
        data_block = fs->fat[data_block].next_block;
    }

    if (n==0) printf("Current directory is empty.\n");
    return;
}

// cambia directory
int changeDir(FileSystem *fs, const char *name) {
    // root directory
    if (strcmp(name, "/") == 0) {
        fs->current_dir = fs->root;
        printf("Changed to root directory.\n");
        return 0;
    }

    // parent directory ("..")
    if (strcmp(name, "..") == 0) {
        // If we're already at the root, we cannot go up
        if (fs->current_dir == fs->root) {
            printf("Already at root directory.\n");
            return -1;
        }

        int parent_block = fs->current_dir[1].start_block;
        if (parent_block < 0 || parent_block >= FAT_ENTRIES) {
            printf("Error: Invalid parent block.\n");
            return -1;
        }

        // trova il blocco dati corrispondente alla directory
        FileEntry *parent_dir = (FileEntry *)(fs->buffer_fs + (parent_block * BLOCK_SIZE));

        // controlla che la directory sia valida
        if (parent_dir == NULL) {
            printf("Error: Failed to access parent directory.\n");
            return -1;
        }

        fs->current_dir = parent_dir;

        printf("Changed to parent directory.\n");
        return 0;
    }

    // subdirectory con nome specifico
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs->current_dir[i].is_used && fs->current_dir[i].is_directory && strcmp(fs->current_dir[i].name, name) == 0) {
            int entry = fs->current_dir[i].start_block;
            int data_block = fs->fat[entry].next_block;
            printf("FAT entry number %d, data block number %d.\n", entry, data_block);

            if (data_block < 0) {
                printf("Error: Invalid directory block.\n");
                return -1;
            }

            // trova il blocco corrispondente alla directory
            FileEntry *new_dir = (FileEntry *)(fs->buffer_fs + (data_block * BLOCK_SIZE));
            fs->current_dir = new_dir;

            printf("Changed directory to '%s'.\n", name);
            return 0;
        }
    }

    printf("Error: Directory '%s' not found.\n", name);
    return -1;
}

// l'apertura di un file restituisce un FileHandle
FileHandle openFile(FileSystem *fs, const char *name) {
    FileHandle fh;
    fh.index = -1;
    fh.file_pos = 0;
    fh.block_pos = 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (fs->current_dir[i].is_used &&
            !fs->current_dir[i].is_directory &&
            strcmp(fs->current_dir[i].name, name) == 0) {
            fh.index = i;
            return fh;
        }
    }

    printf("Error: File '%s' not found.\n", name);
    return fh;
}

// chiudi il file attualmente aperto
void closeFile(FileHandle *handle) {
    handle->index = -1;
    handle->file_pos = 0;
    handle->block_pos = 0;
}

// scrivi su file
int writeFile(FileSystem *fs, FileHandle *fh, const void *buffer, int size) {
    if (fh->index == -1) {
        printf("Error: Invalid file handle.\n");
        return -1;
    }

    FileEntry *file = &fs->current_dir[fh->index];
    int bytes_written = 0;
    const char *data = (const char *)buffer;

    int fat_index = file->start_block;
    if (fat_index == -1 || fs->fat[fat_index].next_block == 0 || fs->fat[fat_index].next_block == FAT_EOF) {
        printf("File has no data block yet. Allocating...\n");
        int new_fat_index = findFreeDataBlockInBuffer(fs);
        if (new_fat_index == -1) return -1;
        file->start_block = new_fat_index;
        fat_index = new_fat_index;
    }
    // After allocation, get the real data block
    int block = fs->fat[fat_index].next_block;
    if (block == FAT_EOF || block == 0) {
        printf("Warning: start_block points to FAT_EOF or uninitialized.\n");
        return -1;
    }

    printf("Start at FAT %d → data block %d\n", fat_index, block);

    // Navigate to correct block for file_pos
    int offset_in_file = fh->file_pos;
    int blocks_to_skip = offset_in_file / BLOCK_SIZE;
    int offset_in_block = offset_in_file % BLOCK_SIZE;

    for (int i = 0; i < blocks_to_skip; i++) {
        if (fs->fat[block].next_block == FAT_EOF) {
            int new_fat_index = findFreeDataBlockInBuffer(fs);
            if (new_fat_index == -1) return bytes_written;
            fs->fat[block].next_block = new_fat_index;
        }
        block = fs->fat[block].next_block;
    }

    // Write data
    while (bytes_written < size) {
        printf("Writing in block %d at file position %d\n", block, fh->file_pos);
        void *block_ptr = fs->buffer_fs + (block * BLOCK_SIZE);

        int space_left = BLOCK_SIZE - offset_in_block;
        int bytes_to_write = (size - bytes_written < space_left) ? (size - bytes_written) : space_left;

        memcpy((char *)block_ptr + offset_in_block, data + bytes_written, bytes_to_write);

        bytes_written += bytes_to_write;
        fh->file_pos += bytes_to_write;
        offset_in_block = 0;

        if (bytes_written < size) {
            if (fs->fat[block].next_block == FAT_EOF) {
                int new_fat_index = findFreeDataBlockInBuffer(fs);
                if (new_fat_index == -1) return bytes_written;
                fs->fat[block].next_block = new_fat_index;
            }
            block = fs->fat[block].next_block;
        }
    }

    if (fh->file_pos > file->size)
        file->size = fh->file_pos;

    printf("New file position: %d\n", fh->file_pos);
    return bytes_written;
}

// leggi il file attualmente aperto
int readFile(FileSystem *fs, FileHandle *fh, void *buffer, int size) {
    if (fh->index == -1) {
        printf("Error: Invalid file handle.\n");
        return -1;
    }

    FileEntry *file = &fs->current_dir[fh->index];
    int bytes_read = 0;
    char *data = (char *)buffer;
    int fat_index = file->start_block;
        if (fat_index == -1 || fs->fat[fat_index].next_block == FAT_EOF) {
            return 0; // file has no data
        }
    int block = fs->fat[fat_index].next_block;


    int offset_in_file = fh->file_pos;
    int max_readable = file->size - offset_in_file;

    // Clamp read size to the actual file content
    if (max_readable <= 0) {
        return 0; // nothing to read
    }
    if (size > max_readable) {
        size = max_readable;
    }

    // Skip blocks to reach the right position
    int blocks_to_skip = offset_in_file / BLOCK_SIZE;
    int offset_in_block = offset_in_file % BLOCK_SIZE;

    for (int i = 0; i < blocks_to_skip; i++) {
        if (block == FAT_EOF) {
            return 0; // reached EOF before position
        }
        block = fs->fat[block].next_block;
    }

    // Begin reading
    while (bytes_read < size && block != FAT_EOF) {
        void *block_ptr = fs->buffer_fs + (block * BLOCK_SIZE);

        int space_left = BLOCK_SIZE - offset_in_block;
        int bytes_to_read = (size - bytes_read < space_left) ? (size - bytes_read) : space_left;

        memcpy(data + bytes_read, (char *)block_ptr + offset_in_block, bytes_to_read);

        bytes_read += bytes_to_read;
        fh->file_pos += bytes_to_read;

        offset_in_block = 0; // reset for next block

        if (bytes_read < size) {
            block = fs->fat[block].next_block;
        }
    }

    return bytes_read;
}


// punta ad una posizione specifica nel file
int seekFile(FileSystem *fs, FileHandle *fh, int position) {
    if (fh->index == -1) {
        printf("Error: Invalid file handle.\n");
        return -1;
    }

    // aggiorna la posizione del file
    if (position < 0) {
        printf("Error: Invalid seek position (negative position).\n");
        return -1;
    }

    fh->file_pos = position;

    // bisogna calcolare il nuovo valore di block_pos
    int block = fs->current_dir[fh->index].start_block;
    int offset_in_file = fh->file_pos;
    int blocks_to_skip = offset_in_file / BLOCK_SIZE;
    int offset_in_block = offset_in_file % BLOCK_SIZE;

    for (int i = 0; i < blocks_to_skip; i++) {
        if (block == FAT_EOF) {
            return -1;
        }
        block = fs->fat[block].next_block;
    }

    fh->block_pos = offset_in_block;

    return 0;
}

// chiusura e uscita dal file system
void cleanup(FileSystem *fs) {
    printf("Exiting file system...\n");
    if (munmap(fs->buffer_fs, FS_SIZE) == -1)
        perror("Error unmapping memory.");
    if (close(fs->fs_fd) == -1)
        perror("Error closing file descriptor of the file system.");
    printf("File system closed successfully.\n");
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
    else if (strcmp(command, "mkdir") == 0) {
        if (n == 2)
            createDir(fs, arg1);
        else
            printf("To use this command: mkdir <directoryname>\n");
    }
    else if (strcmp(command, "rmdir") == 0) {
        if (n == 2)
            eraseDir(fs, arg1);
        else
            printf("To use this command: rmdir <directoryname>\n");
    }
    else if (strcmp(command, "ls") == 0)
        listDir(fs);
    else if (strcmp(command, "cd") == 0) {
        if (n == 2)
            changeDir(fs, arg1);
        else
            printf("To use this command: cd <directory> (or ..)");
    }
    else if (strcmp(command, "open") == 0) {
        if (n == 2) {
            current_open_file = openFile(fs, arg1);
            if (current_open_file.index != -1)
                printf("Opened file '%s'.\n", arg1);
        }
        else
            printf("To use this command: open <filename>\n");
    }
    else if (strcmp(command, "close") == 0) {
        if (current_open_file.index != -1) {
            closeFile(&current_open_file);
        } else {
            printf("Error: No file opened to close.\n");
        }
    }
    else if (strcmp(command, "write") == 0) {
        if (current_open_file.index == -1) {
            printf("Error: No file opened. Use 'open <filename>' first.\n");
        }
        else {
            // la funzione prende come testo tutto ciò che c'è dopo il primo spazio
            const char *text_start = strchr(input, ' ');
            if (text_start != NULL) {
                text_start++;
                if (strlen(text_start) > 0) {
                    writeFile(fs, &current_open_file, text_start, strlen(text_start));
                } else {
                    printf("Error: No text provided to write.\n");
                }
            } else {
                printf("Error: No text provided to write.\n");
            }
        }
    }
    else if (strcmp(command, "read") == 0) {
        if (current_open_file.index == -1) {
            printf("Error: No file opened. Use 'open <filename>' first.\n");
        }
        else {
            int buffer_size = 1024;
            char buffer[buffer_size + 1];
            int bytes_read = readFile(fs, &current_open_file, buffer, buffer_size);

            if (bytes_read > 0) {
                buffer[bytes_read] = '\0'; 
                printf("Read %d bytes: %s\n", bytes_read, buffer);
            } else {
                printf("Error: Could not read the file.\n");
            }
        }
    }
    else if (strcmp(command, "seek") == 0) {
        if (n == 2) {
            int position = atoi(arg1);  // Convert the argument to an integer
            if (seekFile(fs, &current_open_file, position) == 0) {
                printf("File pointer moved to position %d.\n", position);
            }
        } else {
            printf("To use this command: seek <position>\n");
        }
    }
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

    int root_block = (FAT_ENTRIES * sizeof(FATEntry)) / BLOCK_SIZE;

    // inizializza FAT e root directory
    memset(fs.fat, 0, FAT_ENTRIES * sizeof(FATEntry));
    memset(fs.root, 0, MAX_FILES * sizeof(FileEntry));

    fs.root[0].is_used = 1;
    fs.root[0].is_directory = 1;
    strcpy(fs.root[0].name, "/");
    fs.root[0].start_block = 1;
    fs.fat[0].next_block = FAT_EOF;
    fs.fat[1].next_block = root_block;
    fs.fat[root_block].next_block = FAT_EOF;
    for (int i=2; i < FAT_ENTRIES; i++)
        fs.fat[i].next_block = FREE_BLOCK;

    char input[128];
    while(1) {
        printf("fs > ");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';  // sostituisce il newline alla fine con un terminatore di stringa
        processCommand(&fs, input);
    }
}