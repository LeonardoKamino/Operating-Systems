/*
* Definition for close syscall
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
 * Syscall to close a file with a specified file descriptor
 */
int 
sys_close(int fd)
{
    int result;
    struct filetable* filetable = curproc->p_filetable;

    /**
     * Removes the file with the given file descriptor from the
     * current process filetable. 
     * 
     * Acquires the filetable lock to ensure atomicity 
     */
    lock_acquire(filetable->ft_lk);

    result = ft_remove_entry(filetable, fd);

    lock_release(filetable->ft_lk);

    if(result){
        return result;
    }

    return 0;
}
