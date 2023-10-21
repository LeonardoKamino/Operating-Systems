/*
* Definition for chdir syscall
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
#include <limits.h>

/**
 * Sets the current directory of the current process 
 * to the directory specifiedi in pathname
 */
int 
sys_chdir(const char *pathname){
    char *kpathname;
    size_t kpath_len;

    int result;

    /**
     * Checks that pathname is a valid address
     */
    if(pathname == NULL){
        return EFAULT;
    }

    kpathname = kmalloc(PATH_MAX);
    if(kpathname == NULL){
        return ENOMEM;
    }
    
    /**
     * Copies the argument in user space, pathname, to the 
     * pointer in kernel space, kpathname.
     */
    result = copyinstr((const_userptr_t) pathname, kpathname, PATH_MAX, &kpath_len);
    if(result){
        kfree(kpathname);
        return result;
    }

    /* Changes the current working directory to the specified path */
    result = vfs_chdir(kpathname);
    kfree(kpathname);

    if(result){
        return result;
    }

    return 0;
}
