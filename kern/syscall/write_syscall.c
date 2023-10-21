/*
* Definition for write syscall
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
sys_write(int fd, const void *buf, size_t nbytes, int32_t *retval1){
   
    struct filetable *filetable = curproc->p_filetable;
    struct ft_entry *ft_entry;
    void * kbuf;
    struct iovec iov;
    struct uio u;
    int result;

    *retval1 = -1;

    if(buf == NULL || nbytes <= 0){
        return EFAULT;
    }

    lock_acquire(filetable->ft_lk);

    result = ft_is_fd_valid(filetable, fd, true);
    if(result){
        goto error_release_1;
    }

    ft_entry = filetable->ft_entries[fd];

    lock_acquire(ft_entry->fte_lk);
    if(!(ft_entry->fte_flags  & (O_WRONLY | O_RDWR))){
        result = EBADF;
        goto error_release_2;
    }

    kbuf = kmalloc(nbytes);
    if(kbuf == NULL){
        result = ENOMEM;
        goto error_release_2;
    }

    result = copyin(buf, kbuf, nbytes);
    if(result){
        kfree(kbuf);
        goto error_release_2;
    }

    uio_kinit(&iov, &u, kbuf, nbytes, ft_entry->fte_offset, UIO_WRITE);

    result = VOP_WRITE(ft_entry->fte_file, &u);
    if(result){
        kfree(kbuf);
        goto error_release_2;
    }

    ft_entry->fte_offset = u.uio_offset;
    *retval1 = nbytes - u.uio_resid;

    lock_release(ft_entry->fte_lk);
    lock_release(filetable->ft_lk);

    return 0;

error_release_2:
    lock_release(ft_entry->fte_lk);
error_release_1:
    lock_release(filetable->ft_lk);
    return result;
}
