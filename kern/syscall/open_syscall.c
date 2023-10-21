/*
* Definition for open syscall
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
#include <kern/fcntl.h>


/**
 * Syscall to open a specified file with the given flags
 */
int 
sys_open(const char *filename, int flags, int32_t *retval1){
    struct vnode* file;
    struct filetable * filetable = curproc->p_filetable;
    char * filepath;        
    size_t filepath_len;
    int result;
    

    /* Ensures that the provided file name is valid */
    if (filename == NULL){
        return EFAULT;
    }

    filepath = kmalloc(NAME_MAX);
    if(filepath == NULL){
        return ENOMEM;
    }

    /**
     * Copies the argument in user space, filename, to the 
     * pointer in kernel space, filepath
     */
    result = copyinstr((const_userptr_t) filename, filepath, NAME_MAX, &filepath_len);
    if(result){
        kfree(filepath);
        return result;
    }

    /* Opens the file in kernel space */
    result = vfs_open(filepath, flags, 0, &file);
    kfree(filepath);
    if(result){
        return result;
    }

    /* Adds the opened entry to the current process filetable */
    lock_acquire(filetable->ft_lk);

    result = ft_add_entry(filetable, fte_create(file, flags), retval1);

    lock_release(filetable->ft_lk);

    if(result){
        return result;
    }

    return 0;
}
