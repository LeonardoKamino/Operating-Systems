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

void enter_forked_usermode(void *tf,unsigned long junk );

void 
enter_forked_usermode(void *tf,unsigned long junk )
{
    (void) junk;

    struct trapframe ntf;

	memcpy(&ntf, tf, sizeof(struct trapframe));
    ntf.tf_v0 = 0;
    ntf.tf_v1 = 0;
    ntf.tf_a3 = 0;
    ntf.tf_epc += 4;

    kfree(tf);
    as_activate();
    mips_usermode(&ntf);
}

pid_t 
sys_fork(struct trapframe *tf, int32_t *retval1) {
    (void) tf;
    (void) retval1;

    int result;
    struct proc *child_proc;
    struct trapframe *child_tf;

    *retval1 = -1;

    child_tf = kmalloc(sizeof(struct trapframe));
    if(child_tf == NULL) {
        return ENOMEM;
    }

    memcpy(child_tf, tf, sizeof(struct trapframe));
    
    result = proc_fork(&child_proc);
    if (result) {
        kfree(child_tf);
        return result;
    }

    result = thread_fork("child", child_proc, enter_forked_usermode, child_tf, 0);
    if (result) {
        proc_destroy(child_proc);
        return result;
    }

    *retval1 = child_proc->pid;

    return 0;
}