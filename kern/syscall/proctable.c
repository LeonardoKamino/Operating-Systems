#include <types.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <uio.h>
#include <vnode.h>
#include <limits.h>
#include <vfs.h>
#include <copyinout.h>
#include <proctable.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/unistd.h>

/**
 * Creates the process table structure and initializes its
 * entries to NULL
 */
struct 
proctable *pt_create(void) {
    struct proctable *pt = kmalloc(sizeof(struct proctable));
    if (pt == NULL) {
        return NULL;
    }

    pt->pt_lock = lock_create("pt_lock");
    if (pt->pt_lock == NULL) {
        kfree(pt);
        return NULL;
    }

    for(int i = 1; i <= PID_MAX; i++) {
        pt->pt_entries[i] = NULL;
    }

    return pt;
}

/**
 * Adds an entry into the process table at the first available
 * pid (index).
 */
int 
pt_add_entry(struct proctable *proctable, struct proc *proc){
    KASSERT(proctable != NULL);
    KASSERT(proc != NULL);

    int result;
    bool added = false;
    
    
    for(int i = 1; i <= PID_MAX; i++) {
        if (proctable->pt_entries[i] == NULL) {
            result = pt_add_entry_pid(proctable, proc,(pid_t) i);
            if (result) {
                return result;
            }
            added = true;
            break;
        }
    }

    if (!added) {
        return ENPROC;
    }

    return 0;
}

/*
 * Adds an entry at a specific pid in the proctable
 */
int 
pt_add_entry_pid(struct proctable *proctable, struct proc *proc, pid_t pid)
{
    KASSERT(proctable != NULL);
    KASSERT(proc != NULL);

    /* Ensures that the given pid is valid */
    if (pid < 1 || pid > PID_MAX) {
        return EINVAL;
    }

    /**
     * Ensures that there is no other process that already
     * exists at the given pid
     */
    if (proctable->pt_entries[pid] != NULL) {
        return EEXIST;
    }

    proc->pid = pid;

    proctable->pt_entries[pid] = kmalloc(sizeof(struct pid_entry));

    if (proctable->pt_entries[pid] == NULL) {
        return ENOMEM;
    }

    /* Adds the specified process at the pid in the process table */
    proctable->pt_entries[pid]->pte_proc = proc;

    proctable->pt_entries[pid]->pte_lock = lock_create("pte_lock");

    if (proctable->pt_entries[pid]->pte_lock == NULL) {
        kfree(proctable->pt_entries[pid]);
        proctable->pt_entries[pid] = NULL;

        return ENOMEM;
    }   

    proctable->pt_entries[pid]->pte_cv = cv_create("pte_cv");
    if(proctable->pt_entries[pid]->pte_cv == NULL) {
        lock_destroy(proctable->pt_entries[pid]->pte_lock);

        kfree(proctable->pt_entries[pid]);
        proctable->pt_entries[pid] = NULL;

        return ENOMEM;
    }
    
    return 0;
}

/**
 * Destroys the process table and all of its entries
 */
void
pt_destroy(struct proctable *proctable)
{
    KASSERT(proctable != NULL);

    for(int i = 1; i <= PID_MAX; i++) {
        if (proctable->pt_entries[i] != NULL) {
            pt_remove_entry(proctable, (pid_t) i);
        }
    }

    lock_destroy(proctable->pt_lock);
    kfree(proctable);
}

/* Removes and destroy an entry at a specific pid in the process table*/
void
pt_remove_entry(struct proctable *proctable, pid_t pid)
{
    KASSERT(proctable != NULL);
    KASSERT(pid > 0 && pid <= PID_MAX);

    pt_destroy_entry(proctable->pt_entries[pid]);
    proctable->pt_entries[pid] = NULL;
}

/* Destroys an entry in the process table */
void
pt_destroy_entry(struct pid_entry *pt_entry)
{
    lock_destroy(pt_entry->pte_lock);
    cv_destroy(pt_entry->pte_cv);

    kfree(pt_entry);
    pt_entry = NULL;
}