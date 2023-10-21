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


int 
sys_dup2(int oldfd, int newfd, int32_t *retval1)
{
    struct filetable * filetable = curproc->p_filetable;
    struct ft_entry *ft_entry;
    int result;

    lock_acquire(filetable->ft_lk);
    
    result = ft_is_fd_valid(filetable, oldfd, true);
    if(result){
        *retval1 = -1;
        lock_release(filetable->ft_lk);
        return result;
    }

    //Check if newfd value is within valid range
    result = ft_is_fd_valid(filetable, newfd, false);
    if(result){
        *retval1 = -1;
        lock_release(filetable->ft_lk);
        return result;
    }

    //Check if newfd is pointing to another filetable entry
    result = ft_is_fd_valid(filetable, newfd, true);

    //If pointing to another file, must remove it
    if(!result){
        result = ft_remove_entry(filetable, newfd);
        if(result) {
            *retval1 = -1;
            lock_release(filetable->ft_lk);
            return result;
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
}
