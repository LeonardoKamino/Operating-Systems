#include <types.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <uio.h>
#include <vnode.h>
#include <limits.h>
#include <vfs.h>
#include <kern/errno.h>
#include <copyinout.h>
#include <filetable.h>
#include <synch.h>
#include <kern/fcntl.h>


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

int
ft_stdio_init(struct filetable *ft){
    struct vnode * stdin_file;
    struct vnode * stdout_file;
    struct vnode * stderr_file;
    
    int result;

    const char * con = "con:";

     /**
     * Initialize stdio files. The first three file descriptors
     * are dedicated for stdin, stdout and stderr
     *
     *  stdin   -  0
     *  stdout  -  1
     *  stderr  -  2
     *
     * The file descriptors for these are opened using vfs_open
     * and are then used as the first three entries in the 
     * filetable
     */

    result = vfs_open(kstrdup(con), O_RDONLY, 0, &stdin_file);
    if(result){
        return result;
    }
    ft->ft_entries[0] = fte_create(stdin_file, O_RDONLY);

    result = vfs_open(kstrdup(con), O_WRONLY, 0, &stdout_file);
    if(result){
        return result;
    }
    ft->ft_entries[1] = fte_create(stdout_file, O_WRONLY);

    result = vfs_open(kstrdup(con), O_WRONLY, 0, &stderr_file);
    if(result){
        return result;
    }
    ft->ft_entries[2] = fte_create(stderr_file, O_WRONLY);

    return 0;
}

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

int
ft_remove_entry(struct filetable *filetable, int fd)
{
    int result;
    struct ft_entry * ft_entry;

    KASSERT(filetable != NULL);

    result = ft_is_fd_valid(filetable, fd);

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
        fte_destroy(ft_entry);
    }
    return 0;
    
}

/*
* Must hold the filetable lock before calling this function
*/
int
ft_is_fd_valid(struct filetable *filetable, int fd){
    if(fd < 0 || fd > OPEN_MAX || filetable->ft_entries[fd] == NULL){
        return EBADF;
    }

    return 0;
}

/*
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

void
ft_destroy(struct filetable *ft){
    (void)ft;
    for(int i = 0; i < OPEN_MAX; i++){
        fte_destroy(ft->ft_entries[i]);
    }
    lock_destroy(ft->ft_lk);
    kfree(ft->ft_entries);
    kfree(ft->ft_name);
    kfree(ft);
}

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


/*
* Must hold the filetable entry lock before calling it
*/
void 
fte_destroy(struct ft_entry *ft_entry)
{
    KASSERT(ft_entry != NULL);

    vfs_close(ft_entry->fte_file);
    lock_release(ft_entry->fte_lk);
    lock_destroy(ft_entry->fte_lk);
    kfree(ft_entry);
}

