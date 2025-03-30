#ifndef fat_fs
#define fat_fs

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define FS_SIZE (1024 * 1024)  // 1 mb
#define BLOCK_SIZE 512
#define FAT_ENTRIES (FS_SIZE / BLOCK_SIZE)  // 256 byte
#define MAX_FILES 128
#define FAT_EOF -1
#define FREE_BLOCK 0

typedef struct {
    char name[16];
    int start_block;
    int size;
    short is_used;
    short is_directory;
} FileEntry;

typedef struct {
    int next_block;
} FATEntry;

typedef struct {
    int index;
    int file_pos;  // position in file
    int block_pos;  // position in block
} FileHandle;

typedef struct {
    int fs_fd;  // file descriptor del file system
    FileEntry *current_dir;
    FileEntry *root; 
    FATEntry *fat;   // file allocation table
    void *buffer_fs;  // buffer sul quale mappare i dati
} FileSystem;

int createFile(FileSystem *fs, const char *name, int file_size);
int eraseFile(FileSystem *fs, const char *name);
int createDir(FileSystem *fs, const char *name);
int eraseDir(FileSystem *fs, const char *name);
int changeDir(FileSystem *fs, const char *dir);
void listDir(FileSystem *fs);
void processCommand(FileSystem *fs, const char *input);
void cleanup(FileSystem *fs);

// TO IMPLEMENT:
// write (potentially extending the file boundaries)
// read
// seek

#endif