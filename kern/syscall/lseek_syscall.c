/*
* Definition for lseek syscall
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
#include <kern/seek.h>
#include <stat.h>

/**
 * Alters the current seek position of the file handle, 
 * seeking a new position based on pos and whence
 */
int 
sys_lseek(int fd, off_t pos, int whence, int32_t *retval1, int32_t *retval2){
    struct filetable *filetable = curproc->p_filetable;
    struct ft_entry *ft_entry;
    struct stat *file_stats;
    int result;
    off_t new_offset;

    *retval1 = -1;

    /** 
     * Checks to make sure that whence is equal to one of:
     *      SEEK_SET - 0
     *      SEEK_CUR - 1
     *      SEEK_END - 2
     */
    if(whence > SEEK_END || whence < SEEK_SET){
        return EINVAL;
    }
    
    lock_acquire(filetable->ft_lk);

    /* Checks to make sure the file descriptor is valid */
    result = ft_is_fd_valid(filetable, fd, true);
    if(result){
        goto error_release_1;
    }

    ft_entry = filetable->ft_entries[fd];

    lock_acquire(ft_entry->fte_lk);
    
    /** 
     * Check if this file is seekable. All regular files
     * and directories are seekable, but some devices are
     * not.
     */
    result = VOP_ISSEEKABLE(ft_entry->fte_file);
    if(!result){
        result = ESPIPE;
        goto error_release_2;
    }
    
    /* Creates a new offset value depending on whence */
    switch (whence){
        case SEEK_SET:
            new_offset = pos;
            break;
        
        case SEEK_CUR:
            new_offset = ft_entry->fte_offset + pos;
            break;
            
        case SEEK_END: 
            /* Get file stats to know EOF */
            file_stats = kmalloc(sizeof(struct stat));
            if(file_stats == NULL){
                result = ENOMEM;
                goto error_release_2;
            }
            VOP_STAT(ft_entry->fte_file, file_stats);
            new_offset = file_stats->st_size + pos;
            kfree(file_stats);
            break;
    }

    /*Ensures that the new offset is not less than 0 */
    if(new_offset < 0) {
        result = EINVAL;
        goto error_release_2;
    }

    /**
     * Sets the files offset to the new offset and splits the 
     * 64 bit result into two 32 bit return values to be used 
     * in the v0 and v1 registers
     */
    ft_entry->fte_offset = new_offset;

    *retval1 = new_offset >> 32;
    *retval2 = new_offset & 0xFFFFFFFF;

    lock_release(ft_entry->fte_lk);
    lock_release(filetable->ft_lk);
    return 0;

error_release_2:
    lock_release(ft_entry->fte_lk);
error_release_1:
    lock_release(filetable->ft_lk);
    return result;
}
