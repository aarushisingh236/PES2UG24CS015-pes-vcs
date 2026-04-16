// index.c — Staging area implementation

// Text format of .pes/index (one entry per line, sorted by path):

//   <mode-octal> <64-char-hex-hash> <mtime-seconds> <size> <path>

// Example:
//   100644 a1b2c3d4e5f6...  1699900000 42 README.md
//   100644 f7e8d9c0b1a2...  1699900100 128 src/main.c

// This is intentionally a simple text format. No magic numbers, no
// binary parsing. The focus is on the staging area CONCEPT (tracking
// what will go into the next commit) and ATOMIC WRITES (temp+rename).

// PROVIDED functions: index_find, index_remove, index_status
// TODO functions:     index_load, index_save, index_add

#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// ─── PROVIDED ────────────────────────────────────────────────────────────────

// Find an index entry by path (linear scan).
IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

// Remove a file from the index.
// Returns 0 on success, -1 if path not in index.
int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;
            if (remaining > 0)
                memmove(&index->entries[i], &index->entries[i + 1],
                        remaining * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    fprintf(stderr, "error: '%s' is not in the index\n", path);
    return -1;
}

// Print the status of the working directory.
//
// Identifies files that are staged, unstaged (modified/deleted in working dir),
// and untracked (present in working dir but not in index).
// Returns 0.
int index_status(const Index *index) {
printf("Staged changes:\n");

for (int i = 0; i < index->count; i++) {
    printf("  staged: %s\n", index->entries[i].path);
}

printf("\nUnstaged changes:\n");

for (int i = 0; i < index->count; i++) {
    IndexEntry *e = &index->entries[i];

    struct stat st;
    if (stat(e->path, &st) != 0) {
        printf("  deleted: %s\n", e->path);
        continue;
    }

    if (st.st_mtime != e->mtime_sec || st.st_size != e->size) {
        printf("  modified: %s\n", e->path);
    }
}

printf("\nUntracked files:\n");

DIR *dir = opendir(".");
if (dir) {
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
    // Skip hidden files
    if (entry->d_name[0] == '.') continue;

    // Skip build files
    if (strstr(entry->d_name, ".o")) continue;
    if (strcmp(entry->d_name, "pes") == 0) continue;
    if (strcmp(entry->d_name, "Makefile") == 0) continue;

    // Skip source code (optional but recommended)
    if (strstr(entry->d_name, ".c")) continue;
    if (strstr(entry->d_name, ".h")) continue;

    int found = 0;
    for (int i = 0; i < index->count; i++) {
        if (strcmp(entry->d_name, index->entries[i].path) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("  untracked: %s\n", entry->d_name);
    }
}

    closedir(dir);
}

return 0;
}

// ─── TODO: Implement these ───────────────────────────────────────────────────

// Load the index from .pes/index.
//
// HINTS - Useful functions:
//   - fopen (with "r"), fscanf, fclose : reading the text file line by line
//   - hex_to_hash                      : converting the parsed string to ObjectID
//
// Returns 0 on success, -1 on error.
int index_load(Index *index) {
    // TODO: Implement index loading
    // (See Lab Appendix for logical steps)
    FILE *f = fopen(".pes/index", "rb");

if (!f) {
    // No index file yet → start empty
    index->count = 0;
    return 0;
}

// Read entire index struct
if (fread(index, sizeof(Index), 1, f) != 1) {
    fclose(f);
    return -1;
}

fclose(f);
return 0;
}

// Save the index to .pes/index atomically.
//
// HINTS - Useful functions and syscalls:
//   - qsort                            : sorting the entries array by path
//   - fopen (with "w"), fprintf        : writing to the temporary file
//   - hash_to_hex                      : converting ObjectID for text output
//   - fflush, fileno, fsync, fclose    : flushing userspace buffers and syncing to disk
//   - rename                           : atomically moving the temp file over the old index
//
// Returns 0 on success, -1 on error.
int index_save(const Index *index) {
    // TODO: Implement atomic index saving
    // (See Lab Appendix for logical steps)
    FILE *f = fopen(".pes/index", "wb");
if (!f) return -1;

if (fwrite(index, sizeof(Index), 1, f) != 1) {
    fclose(f);
    return -1;
}

fclose(f);
return 0;
}

// Stage a file for the next commit.
//
// HINTS - Useful functions and syscalls:
//   - fopen, fread, fclose             : reading the target file's contents
//   - object_write                     : saving the contents as OBJ_BLOB
//   - stat / lstat                     : getting file metadata (size, mtime, mode)
//   - index_find                       : checking if the file is already staged
//
// Returns 0 on success, -1 on error.
int index_add(Index *index, const char *path) {
    // TODO: Implement file staging
    // (See Lab Appendix for logical steps)
FILE *f = fopen(path, "rb");
if (!f) return -1;

// 2. Get file size
fseek(f, 0, SEEK_END);
long size = ftell(f);
rewind(f);

// 3. Read file content
void *data = malloc(size);
if (!data) {
    fclose(f);
    return -1;
}

fread(data, 1, size, f);
fclose(f);

// 4. Write blob object
ObjectID hash;
if (object_write(OBJ_BLOB, data, size, &hash) != 0) {
    free(data);
    return -1;
}
free(data);

// 5. Create new index entry
IndexEntry *e = &index->entries[index->count++];

e->mode = 0100644;  // regular file

e->hash = hash;

struct stat st;
stat(path, &st);
e->mtime_sec = st.st_mtime;
e->size = st.st_size;

strcpy(e->path, path);

// 6. Save index
return index_save(index);
}
