#include <vnode.h>
#include <synch.h>
#include <limits.h>


#ifndef FILETABLE_H
#define FILETABLE_H

struct filetable {
    struct ft_entry *ft_entries[OPEN_MAX];
    struct lock *ft_lk;
    char *ft_name;
};

struct ft_entry {
    struct vnode *fte_file;
    off_t fte_offset;
    int fte_count;
    int fte_flags;
    struct lock *fte_lk;
};

/**
 * Functions for the filetable 
 */
struct filetable *ft_create(char *name);
void ft_destroy(struct filetable *filetable);
struct filetable *ft_add_file(struct vnode *fte_file);
int ft_add_entry(struct filetable *filetable, struct ft_entry * ft_entry, int32_t *nextfd);
int ft_remove_entry(struct filetable *filetable, int fd);
int ft_next_available_fd(struct filetable *filetable);
int ft_stdio_init(struct filetable *ft);

/**
 * Functions for a filetable entry
 */
struct ft_entry *fte_create(struct vnode *fte_file, int fte_flags);
void fte_destroy(struct ft_entry *ft_entry);
void fte_incref(struct ft_entry *ft_entry);
void fte_decref(struct ft_entry *ft_entry);

#endif //FILETABLE_H