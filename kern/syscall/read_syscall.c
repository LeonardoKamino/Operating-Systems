/*
* Definition for read syscall
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
#include <kern/iovec.h>


int 
sys_read(int fd, void *buf, size_t buflen, int32_t *retval1){

    struct filetable *filetable = curproc->p_filetable;
    struct ft_entry * ft_entry = filetable->ft_entries[fd];

    struct iovec iov;
    struct uio u;
    int result;

    if(buf == NULL || buflen <= 0){
        *retval1 = -1;
        return EFAULT;
    }

    lock_acquire(filetable->ft_lk);

    result = ft_is_fd_valid(filetable, fd, true);
    if(result){
        *retval1 = -1;
        lock_release(filetable->ft_lk);
        return result;
    }

    lock_acquire(ft_entry->fte_lk);

    if(ft_entry->fte_flags == O_WRONLY){
        *retval1 = -1;
        lock_release(ft_entry->fte_lk);
        lock_release(filetable->ft_lk);
        return EBADF;
    }

    iov.iov_kbase = (userptr_t) buf;
	iov.iov_len = buflen;
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_offset = ft_entry->fte_offset;
	u.uio_resid = buflen;
	u.uio_segflg = UIO_USERSPACE;
	u.uio_rw = ft_entry->fte_flags;
	u.uio_space = curproc->p_addrspace;

    result = VOP_READ(ft_entry->fte_file, &u);
    if(result){
        *retval1 = -1;
        lock_release(ft_entry->fte_lk);
        lock_release(filetable->ft_lk);
        return result;
    }

    ft_entry->fte_offset = u.uio_offset;
    *retval1 = buflen - u.uio_resid;

    lock_release(ft_entry->fte_lk);
    lock_release(filetable->ft_lk);

    return 0;
}
