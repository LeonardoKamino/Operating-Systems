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


int 
sys_chdir(const char *pathname){
    char *newpath;
    size_t newpath_len;

    int result;

    if(pathname == NULL){
        return EFAULT;
    }

    newpath = kmalloc(PATH_MAX);
    if(newpath == NULL){
        return ENOMEM;
    }
  
    result = copyinstr((const_userptr_t) pathname, newpath, PATH_MAX, &newpath_len);
    if(result){
        kfree(newpath);
        return result;
    }

    result = vfs_chdir(newpath);
    kfree(newpath);

    if(result){
        return result;
    }

    return 0;
}
