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

int sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval1) {
    (void) pid;
    (void) status;  
    (void) options;
    (void) retval1;


    return 0;
}
