/*
* Definition for open syscall
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
sys_open(const char *filename, int flags, int32_t *retval1){
    (void) filename;
    (void) flags;
    (void) retval1;
    
    return 0;
}
