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

int sys_waitpid(pid_t pid, int *status, int options, pid_t *retval1) {
    (void) pid;
    (void) status;  
    (void) options;
    (void) retval1;

    struct pid_entry * pt_entry = proctable->pt_entries[pid];

    if(pid < 1 || pid > PID_MAX || pt_entry == NULL){
        return ESRCH;
    }

    if(pt_entry->pte_proc->parent_pid != curproc->pid){
        return ECHILD;
    }

    lock_acquire(pt_entry->pte_lock);
    while(pt_entry->pte_proc->p_status != PROC_ZOMBIE){
        cv_wait(pt_entry->pte_cv, pt_entry->pte_lock);
    }
    lock_release(pt_entry->pte_lock);


    int exitcode = pt_entry->pte_proc->exitcode;

    if(status == NULL){
        return EFAULT;
    }

    status = (int *) copyout(&exitcode, (userptr_t) status, sizeof(int));

    if(status){
        return *status;
    }

    return 0;
}
