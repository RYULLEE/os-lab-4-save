#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/disk.h"

struct bitmap;

void inode_init (void);
bool inode_create (disk_sector_t, off_t);
bool inode_create_fs (disk_sector_t, off_t, int32_t);
struct inode *inode_open (disk_sector_t);
struct inode *inode_reopen (struct inode *);
disk_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

int32_t inode_type(struct inode *);
void inode_set_length(struct inode *inode, off_t length);
disk_sector_t inode_sector(struct inode *inode);
bool inode_removed(struct inode *);
unsigned inode_magic(struct inode *inode);
char *inode_path(struct inode *);
struct inode *check_softlink(struct inode *);

#endif /* filesys/inode.h */
