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

int 
sys___getcwd(char *buf, size_t buflen, int32_t *retval1){
    struct iovec iov;
    struct uio u;
    int result;

    if(buf == NULL || buflen <= 0){
        *retval1 = -1;
        return EFAULT;
    }

    iov.iov_kbase = (userptr_t) buf;
	iov.iov_len = buflen;
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_offset = 0;
	u.uio_resid = buflen;
	u.uio_segflg = UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = curproc->p_addrspace;

    result = vfs_getcwd(&u);
    if(result){
        *retval1 = -1;
        return result;
    }

    *retval1 = buflen - u.uio_resid;
    
    return 0;
}
