#include <types.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <uio.h>
#include <vnode.h>
#include <limits.h>
#include <vfs.h>
#include <copyinout.h>
#include <filetable.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/unistd.h>


/**
 * Creates a filetable
 */
struct filetable *
ft_create(char *name)
{
    struct filetable *ft;

    ft = kmalloc(sizeof(struct filetable));
    if(ft == NULL){
        return NULL;
    }

    ft->ft_name = kstrdup(name);
	if (ft->ft_name == NULL) {
		kfree(ft);
		return NULL;
	}

    ft->ft_lk = lock_create("Filetable Lock");
    if(ft->ft_lk == NULL){
		kfree(ft);
        return NULL;
    }    

   return ft;
}

/**
 * Destroys the specified filetable, freeing all allocated
 * memory and destroying all locks associated with the 
 * filetable
 */
void
ft_destroy(struct filetable *ft)
{
    (void)ft;
    for(int i = 0; i < OPEN_MAX; i++){
        fte_destroy(ft->ft_entries[i]);
    }
    lock_destroy(ft->ft_lk);
    kfree(ft->ft_entries);
    kfree(ft->ft_name);
    kfree(ft);
}

/**
 * Initializes the filetable's first three entries. 
 * The first three file descriptors (0, 1 and 2)
 * are considered as standard input, standard
 * output and standard error respectively.
 */
int
ft_stdio_init(struct filetable *ft)
{
    struct vnode *stdin_file;
    struct vnode *stdout_file;
    struct vnode *stderr_file;
    
    int result;

    /**
     * The initial path value for stdin, stdout and stderr. 
     * These file descriptors start out attached to the 
     * console device "con:"
     */
    const char *con = "con:";

     /**
     * Initialize stdio files. The first three file descriptors
     * are dedicated for stdin, stdout and stderr
     *
     *  stdin   -  0
     *  stdout  -  1
     *  stderr  -  2
     *
     * The files are opened using vfs_open to retrieve
     * pointers to each of their vnodes and are then 
     * used as the first three entries in the filetable
     */
    result = vfs_open(kstrdup(con), O_RDONLY, 0, &stdin_file);
    if(result){
        return result;
    }
    ft->ft_entries[STDIN_FILENO] = fte_create(stdin_file, O_RDONLY);
    ft->ft_entries[STDIN_FILENO]->fte_count += 1;

    result = vfs_open(kstrdup(con), O_WRONLY, 0, &stdout_file);
    if(result){
        return result;
    }
    ft->ft_entries[STDOUT_FILENO] = fte_create(stdout_file, O_WRONLY);
    ft->ft_entries[STDOUT_FILENO]->fte_count += 1;

    result = vfs_open(kstrdup(con), O_WRONLY, 0, &stderr_file);
    if(result){
        return result;
    }
    ft->ft_entries[STDERR_FILENO] = fte_create(stderr_file, O_WRONLY);
    ft->ft_entries[STDERR_FILENO]->fte_count += 1;

    return 0;
}

/**
 * Add entry to next available fd in filetable
 */
int
ft_add_entry(struct filetable *filetable, struct ft_entry *ft_entry, int32_t *nextfd)
{
    int fd;
    KASSERT(ft_entry != NULL);
    KASSERT(filetable != NULL);

    fd = ft_next_available_fd(filetable);
    if(fd < 0){
        return EMFILE;
    }

    *nextfd = fd;

    filetable->ft_entries[fd] = ft_entry;

    lock_acquire(ft_entry->fte_lk);
    ft_entry->fte_count++;
    lock_release(ft_entry->fte_lk);
    
    return 0;
}

/**
 * Remove entry from specified fd
 * Must hold filetable lock before calling this function
 */
int
ft_remove_entry(struct filetable *filetable, int fd)
{
    int result;
    struct ft_entry *ft_entry;

    KASSERT(filetable != NULL);

    result = ft_is_fd_valid(filetable, fd, true);
    if (result){
        return result;
    }   

    ft_entry = filetable->ft_entries[fd];
    filetable->ft_entries[fd] = NULL;

    lock_acquire(ft_entry->fte_lk);

    ft_entry->fte_count--;
    if(ft_entry->fte_count > 0) {
        lock_release(ft_entry->fte_lk);
    } else{
        /* Destroy entry if no fd references it */
        fte_destroy(ft_entry); 
    }
    return 0;
    
}

/**
 * Check if fd is within valid range [0, OPENMAX]
 * the check_presence if statement makes sure the fd has an 
 * entry referenced by it
 * 
 * Must hold the filetable lock before calling this function
 */
int
ft_is_fd_valid(struct filetable *filetable, int fd, bool check_presence){
    if(fd < 0 || fd > OPEN_MAX - 1 ){
        return EBADF;
    }

    if(check_presence && filetable->ft_entries[fd] == NULL){
        return EBADF;
    }

    return 0;
}

/*
 * Finds the next available file descriptor in the filetable
 * 
 * Must hold the filetable lock before calling this function
 */
int 
ft_next_available_fd(struct filetable *filetable)
{
    KASSERT(filetable != NULL);
    
    for(int i = 0; i < OPEN_MAX; i++){
        if(filetable->ft_entries[i] == NULL){
            return i;
        }
    }

    return -1;
}


/**
 * Creates and returns a filetable entry that can be
 * used and added to a filteable
 */
struct ft_entry *
fte_create(struct vnode *fte_file, int fte_flags)
{
    KASSERT(fte_file != NULL);

    struct ft_entry *ft_entry;
    
    ft_entry = kmalloc(sizeof(struct ft_entry));
    if( ft_entry == NULL){
        return NULL;
    }

    ft_entry->fte_lk = lock_create("ft lock");
    if(ft_entry->fte_lk == NULL){
        kfree(ft_entry);
        return NULL;
    }

    ft_entry->fte_count = 0;
    ft_entry->fte_offset = 0;
    ft_entry->fte_flags = fte_flags;
    ft_entry->fte_file = fte_file;

    return ft_entry;
}


/**
 * Destroys a specified filetable entry
 * 
 * Must hold the filetable entry lock before calling it
 */
void 
fte_destroy(struct ft_entry *ft_entry)
{
    KASSERT(ft_entry != NULL);

    vfs_close(ft_entry->fte_file);
    lock_destroy(ft_entry->fte_lk);
    kfree(ft_entry);
}

/**
 * Copy pointers from source filetable to copy filetable
 * 
 * Must hold the filetable lock before calling this function
 */
void
ft_copy(struct filetable *source, struct filetable *copy)
{
    for(int i = 0; i < OPEN_MAX; i++) {
        if(source->ft_entries[i] != NULL) {
            copy->ft_entries[i] = source->ft_entries[i];

            lock_acquire(copy->ft_entries[i]->fte_lk);
            copy->ft_entries[i]->fte_count++;
            lock_release(copy->ft_entries[i]->fte_lk);
        }
    }
}

