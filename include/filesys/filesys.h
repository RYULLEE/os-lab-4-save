#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

#define DIRECTORY 0
#define FILES 1
#define SOFTLINK 2

/* Disk used for file system. */
extern struct disk *filesys_disk;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size);
struct file *filesys_open (const char *name);
bool filesys_remove (const char *name);

// project4 (new system calls)
struct dir *parse_path(char *path_name, char *target);
bool filesys_chdir(const char *dir);
bool filesys_mkdir(const char *dir);
bool filesys_symlink(const char *target, const char *linkpath);

#endif /* filesys/filesys.h */
