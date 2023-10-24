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

/**
 * Syscall that reads buflen bytes from a buffer given a 
 * file descriptor
 */
int 
sys_read(int fd, void *buf, size_t buflen, int32_t *retval1)
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
     * trying to read a negative number of bytes
     */
    if(buf == NULL || buflen <= 0){
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
     * a read only or read and write file. If it is
     * write only, the function will return the error
     * EBADF
     */
    if(ft_entry->fte_flags & O_WRONLY){
        result = EBADF;
        goto error_release_2;
    }

    kbuf = kmalloc(buflen);
    if(kbuf == NULL){
        result = ENOMEM;
        goto error_release_2;
    }

    /**
     * Initializes an instance of the uio struct to be used
     * in the kernel space read
     */
    uio_kinit(&iov, &u, kbuf, buflen, ft_entry->fte_offset, UIO_READ);

    /**
     * Reads the provided data inside a uio in kernel space
     */
    result = VOP_READ(ft_entry->fte_file, &u);
    if(result){
        goto error_release_2;
    }

    /**
     * Copies the data that was read in kernel space to user space
     */
    result = copyout(kbuf, buf, buflen);
    kfree(kbuf);
    if(result){
        goto error_release_2;
    }

    /**
     * Sets the offset value for the file to keep track of what has
     * been read and the return value is set to the total number of
     * bytes that were read
     */
    ft_entry->fte_offset = u.uio_offset;
    *retval1 = buflen - u.uio_resid;

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
