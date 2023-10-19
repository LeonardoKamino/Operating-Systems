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


int 
sys_read(int fd, void *buf, size_t buflen, int32_t *retval1){
    (void) fd;
    (void) buf;
    (void) buflen;
    (void) retval1;
    
    return 0;
}
