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

/**
 * Syscall that writes nbytes number of bytes from a buffer given a 
 * file descriptor
 */
int 
sys_write(int fd, const void *buf, size_t nbytes, int32_t *retval1)
{   
    struct filetable *filetable = curproc->p_filetable;
    struct ft_entry *ft_entry;
    void *kbuf;
    struct iovec iov;
    struct uio u;
    int result;

    *retval1 = -1;

    /**
     * Ensures that the buf isn't empty and that we aren't
     * trying to write a negative number of bytes
     */
    if(buf == NULL || nbytes <= 0){
        return EFAULT;
    }

    lock_acquire(filetable->ft_lk);

    /**
     * Checks to make sure that the file descriptor that was
     * passed is valid
     */
    result = ft_is_fd_valid(filetable, fd, true);
    if(result){
        goto error_release_1;
    }

    ft_entry = filetable->ft_entries[fd];

    lock_acquire(ft_entry->fte_lk);

    /**
     * Checks to make sure that the given file is 
     * not a read only file. If it is read only, 
     * the function will return the EBADF error 
     * code
     */
    if(!(ft_entry->fte_flags  & (O_WRONLY | O_RDWR))){
        result = EBADF;
        goto error_release_2;
    }

    kbuf = kmalloc(nbytes);
    if(kbuf == NULL){
        result = ENOMEM;
        goto error_release_2;
    }

    /**
     * Coppies the buffer from user space into an address
     * in kernel space to be used during the kernel space
     * write
     */
    result = copyin(buf, kbuf, nbytes);
    if(result){
        kfree(kbuf);
        goto error_release_2;
    }

    /**
     * Initializes an instance of the uio struct to be used
     * in the kernel space write
     */
    uio_kinit(&iov, &u, kbuf, nbytes, ft_entry->fte_offset, UIO_WRITE);

    /**
     * Writes the data in kernel space given the data in a uio
     */
    result = VOP_WRITE(ft_entry->fte_file, &u);
    if(result){
        kfree(kbuf);
        goto error_release_2;
    }

    /**
     * Sets the offset value for the file to keep track of what has
     * been written and the return value is set to the total number of
     * bytes that were written
     */
    ft_entry->fte_offset = u.uio_offset;
    *retval1 = nbytes - u.uio_resid;

    lock_release(ft_entry->fte_lk);
    lock_release(filetable->ft_lk);

    return 0;

    /**
     * Code to release locks acquired in the event there is an error
     * Returns any error code encountered
     */
error_release_2:
    lock_release(ft_entry->fte_lk);
error_release_1:
    lock_release(filetable->ft_lk);
    return result;
}
