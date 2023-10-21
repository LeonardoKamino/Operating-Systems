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
    struct ft_entry * ft_entry;

    void * kbuf;
    struct iovec iov;
    struct uio u;
    int result;

    *retval1 = -1;

    if(buf == NULL || buflen <= 0){
        return EFAULT;
    }

    lock_acquire(filetable->ft_lk);

    result = ft_is_fd_valid(filetable, fd, true);
    if(result){
        goto error_release_1;
    }

    ft_entry = filetable->ft_entries[fd];

    lock_acquire(ft_entry->fte_lk);

    if(ft_entry->fte_flags & O_WRONLY){
        result = EBADF;
        goto error_release_2;
    }

    kbuf = kmalloc(buflen);
    if(kbuf == NULL){
        result = ENOMEM;
        goto error_release_2;
    }

    uio_kinit(&iov, &u, kbuf, buflen, ft_entry->fte_offset, UIO_READ);

    result = VOP_READ(ft_entry->fte_file, &u);
    if(result){
        goto error_release_2;
    }

    result = copyout(kbuf, buf, buflen);
    kfree(kbuf);
    if(result){
        goto error_release_2;
    }

    ft_entry->fte_offset = u.uio_offset;
    *retval1 = buflen - u.uio_resid;

    lock_release(ft_entry->fte_lk);
    lock_release(filetable->ft_lk);

    return 0;

error_release_2:
    lock_release(ft_entry->fte_lk);
error_release_1:
    lock_release(filetable->ft_lk);
    return result;
}
