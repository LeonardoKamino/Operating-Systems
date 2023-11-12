#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <copyinout.h>
#include <vfs.h>
#include <vnode.h>
#include <openfile.h>
#include <filetable.h>
#include <syscall.h>
#include <mips/trapframe.h>
#include <addrspace.h>

void enter_fork_usermode(void *data1, unsigned long junk);

void
enter_fork_usermode(void *data1, unsigned long junk)
{
    (void)junk;
    struct trapframe child_tf = *(struct trapframe *) data1;

    kfree(data1);
	as_activate();
	mips_usermode(&child_tf);
}

int
sys_fork(struct trapframe *tf, int *retval)
{
    (void) tf;
    (void) retval;
    int result;
    struct proc *child_proc;
    struct trapframe *child_tf;
    
    child_tf = kmalloc(sizeof(struct trapframe));
    if (child_tf == NULL) {
        return ENOMEM;
    }

    memcpy(child_tf, tf, sizeof(struct trapframe));
    child_tf->tf_v0 = 0;
    child_tf->tf_v1 = 0;
    child_tf->tf_a3 = 0;
    child_tf->tf_epc += 4;

    result = proc_fork(&child_proc);
    if (result) {
        kfree(child_tf);
        return result;
    }

    child_proc->p_addrspace = proc_getas();
    result = as_copy(proc_getas(), &child_proc->p_addrspace);
    if (result) {
        proc_destroy(child_proc);
        kfree(child_tf);
        return result;
    }

    result = thread_fork("child", child_proc, enter_fork_usermode, child_tf, 0);
    if(result){
        proc_destroy(child_proc);
        kfree(child_tf);
        return result;
    }

    *retval = child_proc->pid;

    return 0;
}

int
sys_execv(const char *program, char **args, int *retval)
{
    (void) program;
    (void) args;
    (void) retval;
    return 0;
}

int
sys_waitpid(pid_t pid, int *status, int options, int *retval)
{
    (void) pid;
    (void) status;
    (void) options;
    (void) retval;

    struct pid_entry * pt_entry = proctable->pt_entries[pid];
    int result;

    if(pid < 1 || pid > PID_MAX || pt_entry == NULL){
        return ESRCH;
    }

    if(pt_entry->pte_proc->parent_pid != curproc->pid){
        return ECHILD;
    }

    if(options != 0){
        return EINVAL;
    }

    lock_acquire(pt_entry->pte_lock);
    while(pt_entry->pte_proc->p_status != P_ZOMBIE){
        cv_wait(pt_entry->pte_cv, pt_entry->pte_lock);
    }
    lock_release(pt_entry->pte_lock);

    int exitcode = pt_entry->pte_proc->exitcode;

    if(status != NULL){
        result = copyout(&exitcode, (userptr_t) status, sizeof(int));
        if(result){
            return result;
        }
    }   

    return 0;
}

int 
sys_getpid(int *retval)
{
    (void) retval;
    KASSERT(retval != NULL);

    *retval = curproc->pid;

    return 0;
}


void
sys___exit(int exitcode)
{
    (void) exitcode;
    struct pid_entry *pt_entry = proctable->pt_entries[curproc->pid];

    lock_acquire(pt_entry->pte_lock);
    proc_update_children(curproc);
    /* Process has no parent*/
    if(pt_entry->pte_proc->p_status == P_ORPHAN){        
        
        proc_destroy(pt_entry->pte_proc);

    } else {
        pt_entry->pte_proc->p_status = P_ZOMBIE;

        cv_broadcast(pt_entry->pte_cv, pt_entry->pte_lock);
    }

    pt_entry->pte_proc->exitcode = exitcode;

    lock_release(pt_entry->pte_lock);

    thread_exit();
}