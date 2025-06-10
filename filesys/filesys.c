#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"

// Project 4
#include "threads/thread.h"
#include "filesys/fat.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
    struct thread *current = thread_current();

	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();

#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();

	fat_open ();
	current->dir = dir_open_root();
#else
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) {
    if(name == NULL) return false;

    cluster_t cluster = fat_create_chain(0);
    if(cluster == 0) return false;

    disk_sector_t sector = cluster_to_sector(cluster);  

    char path[256];
    struct dir *dir = parse_path(name, path);

    if (strcmp(path, "") == 0) return false;
    if (dir == NULL ||
        inode_removed(dir_get_inode(dir))) return false;

    dir = dir_open(dir_get_inode(dir));

    bool success = (dir != NULL &&
        inode_create_fs(sector, initial_size, FILES) &&
        dir_add(dir, path, sector));

    if (!success && sector != 0) fat_remove_chain(cluster, 1);

    dir_close(dir);
    return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name) {
    struct dir *dir = dir_open_root ();
    struct inode *inode = NULL;

    if(name == NULL || strcmp(name, "") == 0) return NULL;
    if(strlen(name) == 1 && strcmp(name, "/") == 0)
        return file_open(dir_get_inode(dir));

    char path[256];
    dir = parse_path(name, path);

    if (dir == NULL || inode_removed(dir_get_inode(dir))) return NULL;

    dir = dir_open(dir_get_inode(dir));

    if (dir_lookup(dir, path, &inode) == false || inode_removed(inode))
        return NULL;
    
    dir_close(dir);

    return file_open(inode);
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) {
    struct dir* dir = dir_open_root ();
    struct inode* inode = NULL;

    if(name == NULL || strcmp(name, "") == 0) return false;
    if(strlen(name) == 1 && strcmp(name, "/") == 0) return false;

    char path[256];
    bool success = false;

    dir = parse_path(name, path);

    if (dir == NULL ||
        inode_removed(dir_get_inode(dir)) ||
        dir_lookup(dir, path, &inode) == false) return false;
    
    char file_name[NAME_MAX + 1];
    if (dir_readdir(dir, file_name) == false || inode_removed(inode))
        return false;

    dir_close(dir);
    return true;
}

/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create ();
	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}

// handle soft link if necessary and return the dir for last file
struct dir *parse_path(char *name, char *target) {
    struct dir* dir = dir_open_root();
    struct inode* inode = NULL;

    char* save_ptr;
    char* path = malloc(strlen(name) + 1);
    if(path == NULL) return NULL;

    strlcpy(path, name, strlen(name) + 1);

    char* token = strtok_r(path, "/", &save_ptr);
    if (token == NULL){
        free(path);
        return dir;
    }

    char* next_token = strtok_r(NULL, "/", &save_ptr);

    while(next_token != NULL) {
        if (dir_lookup(dir, token, &inode) == false){
            free(path);
            dir_close(dir);
            return NULL;
        }

        while(inode_type(inode) == 2) {
            char link_target[256];
            char* link_path = inode_path(inode);

            struct dir *link_dir = parse_path(link_path, link_target);
            if(link_dir == NULL){
                free(path);
                dir_close(dir);
                return NULL;
            }

            struct inode* new_inode = NULL;
            if (dir_lookup(link_dir, link_target, &new_inode) == false){
                dir_close(link_dir);
                free(path);
                dir_close(dir);
                return NULL;
            }
            
            dir_close(link_dir);
            inode = new_inode;
        }

        dir_close(dir);
        dir = dir_open(inode);
        if(dir == NULL){
            free(path);
            return NULL;
        }

        token = next_token;
        next_token = strtok_r(NULL, "/", &save_ptr);
    }

    strlcpy(target, token, strlen(token) + 1);
    free(path);
    return dir;
}

bool filesys_chdir(const char *dir) {
    if(dir == NULL) return false;

    struct inode *inode = NULL;
    char buffer[256];
    struct dir *directory = parse_path(dir, buffer);
	struct thread *cur = thread_current();

    // check whether directory is valid or not
    if (dir_lookup(directory, buffer, &inode) == false ||
        inode_type(inode) != 0 ||
        inode_removed(inode))
        return false;
    else{
        cur->dir = dir_open(dir_get_inode(directory));
        return true;
    }
}

bool filesys_mkdir(const char *dir) {
    if(dir == NULL) return false;
    if(strlen(dir) == 0) return false;

    cluster_t cluster = fat_create_chain(0);
    if(cluster == 0) return false;

    disk_sector_t sector = cluster_to_sector(cluster);

    char buffer[256];
    struct dir* path = parse_path(dir, buffer);
    if(path == NULL) return false;

    struct dir* directory = dir_open(dir_get_inode(path));
    bool success = false;

    if(directory != NULL &&
        inode_create_fs(sector, 0, DIRECTORY) &&
        dir_add(directory, buffer, sector)){

            struct inode* inode = NULL;
            if(dir_lookup(directory, buffer, &inode)){
                struct dir* new = dir_open(inode);
                if(new != NULL){
                    success = dir_add(new, ".", sector) &&
                                dir_add(new, "..", inode_get_inumber(dir_get_inode(dir)));
                    dir_close(new);
                }
            }
        }

    if(success == false && cluster != 0) fat_remove_chain(cluster, 0);
    dir_close(directory);
    return success;
}

// project4
bool filesys_symlink(const char *target, const char *linkpath) {
    if(target == NULL || linkpath == NULL) return false;
    cluster_t cluster = fat_create_chain(0);
    char name[256];
    struct inode* inode = NULL;

    struct dir *dir = parse_path(linkpath, name);
    if(dir == NULL ||
        inode_removed(dir_get_inode(dir)) ||
        strcmp(name, "") == 0) return false;
    
    disk_sector_t sector = cluster_to_sector(cluster);
    bool success = inode_create_fs(sector, 0, SOFTLINK) &&
                    dir_add(dir, name, sector);
    
    if(success == false){
        fat_remove_chain(cluster, 1);
        return false;
    }
    else{
        dir_lookup(dir, name, &inode);
        strlcpy(inode_path(inode), target, 256);
        return true;
    }
}