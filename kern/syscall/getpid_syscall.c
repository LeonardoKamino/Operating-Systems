/*
* Definition for open syscall
*/
#include <types.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <uio.h>
#include <vfs.h>
#include <mips/trapframe.h>
#include <kern/errno.h>
#include <copyinout.h>
#include <kern/syscall.h>
#include <kern/fcntl.h>
#include <kern/unistd.h>
#include <addrspace.h>

int
sys_getpid(int32_t *retval)
{
    KASSERT(retval != NULL);

    *retval = curproc->pid;

    return 0;
}
