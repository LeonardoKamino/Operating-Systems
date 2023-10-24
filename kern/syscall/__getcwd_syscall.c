/*
* Definition for __getcwd syscall
*/
#include <types.h>
#include <syscall.h>
#include <kern/syscall.h>
#include <current.h>
#include <proc.h>
#include <uio.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/errno.h>
#include <copyinout.h>

/**
 * Gets the name of the current working directory
 */
int 
sys___getcwd(char *buf, size_t buflen, int32_t *retval1)
{
    struct iovec iov;
    struct uio u;
    int result;
    void *kbuf;
    
    *retval1 = -1;
    
    if(buf == NULL || buflen <= 0){
        return EFAULT;
    }

    kbuf = kmalloc(buflen);
    if(kbuf == NULL){
        return ENOMEM;
    }

    /* Initialize uio with the buffer in kernel space */
    uio_kinit(&iov, &u, kbuf, buflen, 0, UIO_READ);

    /* Does the actual read */
    result = vfs_getcwd(&u);
    if(result){
        kfree(kbuf);
        return result;
    }

    /* Copies the buffer from kernel space to user space */
    result = copyout(kbuf, (userptr_t) buf, buflen);
    kfree(kbuf);
    if(result){
        return result;
    }

    *retval1 = buflen - u.uio_resid;
    
    return 0;
}
