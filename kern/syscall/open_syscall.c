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


int 
sys_open(const char *filename, int flags, int32_t *retval1){
    struct vnode* file;
    struct filetable * filetable = curproc->p_filetable;
    char * filepath;        
    size_t filepath_len;
    int result;
    

    //EFAULT filename was an invalid pointer
    if (filename == NULL){
        return EFAULT;
    }

    filepath = kmalloc(NAME_MAX);
    if(filepath == NULL){
        return ENOMEM;
    }

    result = copyinstr((const_userptr_t) filename, filepath, NAME_MAX, &filepath_len);
    if(result){
        kfree(filepath);
        return result;
    }

    result = vfs_open(filepath, flags, 0, &file);
    kfree(filepath);
    if(result){
        return result;
    }

    lock_acquire(filetable->ft_lk);

    result = ft_add_entry(filetable, fte_create(file, flags), retval1);

    lock_release(filetable->ft_lk);

    if(result){
        return result;
    }

    return 0;
}
