/*
* Definition for dup2 syscall
*/
#include <types.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <uio.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/errno.h>
#include <copyinout.h>
#include <kern/syscall.h>

/**
 * Clones the file handle of an old file descriptor onto the 
 * file handle for the new file descriptor. Both handles are 
 * references to the same object and share the same seek pointer
 */
int 
sys_dup2(int oldfd, int newfd, int32_t *retval1)
{
    struct filetable * filetable = curproc->p_filetable;
    struct ft_entry *ft_entry;
    int result;

    *retval1 = -1;

    lock_acquire(filetable->ft_lk);
    
    /* Checks to make sure the file descriptor is valid */
    result = ft_is_fd_valid(filetable, oldfd, true);
    if(result){
        goto error_release_1;
    }

    /* Check if newfd value is within valid range */
    result = ft_is_fd_valid(filetable, newfd, false);
    if(result){
        goto error_release_1;
    }

    /* Check if fds are pointing to same ft_entry */
    if(filetable->ft_entries[oldfd] == filetable->ft_entries[newfd]){
        *retval1 = newfd;
        lock_release(filetable->ft_lk);
        return 0;
    }

    /* Check if newfd is pointing to another filetable entry */
    result = ft_is_fd_valid(filetable, newfd, true);

    /* If pointing to another file, must remove it */
    if(!result){
        result = ft_remove_entry(filetable, newfd);
        if(result) {
            goto error_release_1;
        }
    }

    ft_entry = filetable->ft_entries[oldfd];    

    lock_acquire(ft_entry->fte_lk);
    
    ft_entry->fte_count++;
    filetable->ft_entries[newfd] = ft_entry;
    *retval1 = newfd;

    lock_release(ft_entry->fte_lk);
    lock_release(filetable->ft_lk);

    return 0;

error_release_1:
    lock_release(filetable->ft_lk);
    return result;
}
