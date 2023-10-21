#include <vnode.h>
#include <synch.h>
#include <limits.h>


#ifndef FILETABLE_H
#define FILETABLE_H

/**
 * Struct that represents the filetable 
 */
struct filetable {
    char *ft_name; /* Name of this filetable */

    struct ft_entry *ft_entries[OPEN_MAX]; /* Pointer arrays to entries of this filetable */
    
    struct lock *ft_lk; /* Lock for this structure */
};

/**
 * Struct representing a single entry in the filetable 
 */
struct ft_entry {
    struct vnode *fte_file; /* File referenced by this handler */

    off_t fte_offset; /* Current file offset */

    int fte_count;/* Number of file descriptors referencing this entry */
    int fte_flags;/* Flags for filehandler*/

    struct lock *fte_lk;/* Lock for this structure*/
};

/**
 * Functions for the filetable 
 */

/* Create filetable */
struct filetable *ft_create(char *name);

/* Destroy filetable*/
void ft_destroy(struct filetable *filetable);

/* Add entry to filetable*/
int ft_add_entry(struct filetable *filetable, struct ft_entry *ft_entry, int32_t *nextfd);

/* Remove entry from specific fd*/
int ft_remove_entry(struct filetable *filetable, int fd);

/* Check if file descriptor is valid*/
int ft_is_fd_valid(struct filetable *filetable, int fd, bool check_presence);

/* Get next free file descriptor */
int ft_next_available_fd(struct filetable *filetable);

/* Initialize filetable with stdin, stdout and stderr file descriptors */
int ft_stdio_init(struct filetable *ft);

/**
 * Functions for a filetable entry
 */

/* Create new filetable entry */
struct ft_entry *fte_create(struct vnode *fte_file, int fte_flags);

/* Destroy filetable entry */
void fte_destroy(struct ft_entry *ft_entry);

#endif //FILETABLE_H