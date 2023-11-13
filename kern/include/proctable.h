
#include <synch.h>
#include <limits.h>
#include <proc.h>
#include <types.h>

#ifndef PROCTABLE_H
#define PROCTABLE_H


struct proctable{
    struct lock * pt_lock;
    struct pid_entry * pt_entries[PID_MAX + 1];
};


struct pid_entry {
    struct lock * pte_lock;
    struct cv * pte_cv;
    struct proc * pte_proc;
};

/**
 * Functions for the filetable 
 */

/* Create proctable */
struct proctable *pt_create(void);

/* Destroy proctable */
void pt_destroy(struct proctable *proctable);

/* Destroy proctable entry */
void pt_destroy_entry(struct pid_entry *pt_entry);

/* Add entry to filetable*/
int pt_add_entry(struct proctable *proctable, struct proc *proc);

/* Add entry to certain PID */
int pt_add_entry_pid(struct proctable *proctable, struct proc *proc, pid_t pid);

/* Remove entry at certain PID*/
void pt_remove_entry(struct proctable *proctable, pid_t pid);

#endif /* _PROC_TABLE_H_ */