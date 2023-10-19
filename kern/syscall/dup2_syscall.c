/*
* Definition for dup2 syscall
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
sys_dup2(int oldfd, int newfd, int32_t *retval1){
    (void) oldfd;
    (void) newfd;
    (void) retval1;
    return 0;
}
