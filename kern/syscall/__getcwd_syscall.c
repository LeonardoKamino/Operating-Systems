/*
* Definition for __getcwd syscall
*/
#include <types.h>
#include <syscall.h>
#include <kern/syscall.h>
#include <current.h>
#include <proc.h>
#include <uio.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/errno.h>
#include <copyinout.h>

int 
sys___getcwd(char *buf, size_t buflen, int32_t *retval1){
    (void) buf;
    (void) buflen,
    (void) retval1;
    
    return 0;
}
