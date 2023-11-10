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

void sys___exit(int exitcode){
    (void) exitcode;

    struct pid_entry *pt_entry = proctable->pt_entries[curproc->pid];

    lock_acquire(pt_entry->pte_lock);

    proc_update_children(curproc);
    
    /* Process has no parent*/
    if(pt_entry->pte_proc->p_status == PROC_ORPHAN){        
        
        proc_destroy(pt_entry->pte_proc);

    } else {
        pt_entry->pte_proc->p_status = PROC_ZOMBIE;

        cv_broadcast(pt_entry->pte_cv, pt_entry->pte_lock);
    }

    pt_entry->pte_proc->exitcode = exitcode;

    lock_release(pt_entry->pte_lock);

    thread_exit();
}
