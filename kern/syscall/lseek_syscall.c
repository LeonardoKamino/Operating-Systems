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


int 
sys_lseek(int fd, off_t pos, int whence, int32_t *retval1, int32_t *retval2){
    (void) fd;
    (void) pos;
    (void) whence;
    (void) retval1;
    (void) retval2;
    
    return 0;
}
