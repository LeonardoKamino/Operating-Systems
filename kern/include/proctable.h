
#include <synch.h>
#include <limits.h>
#include <proc.h>
#include <types.h>

#ifndef PROCTABLE_H
#define PROCTABLE_H

/* Struct for the process table */
struct proctable{
    struct lock * pt_lock; /* Used to lock the entire process table */
    struct pid_entry * pt_entries[PID_MAX + 1]; /* Array of processes in the process table*/
};

/* An individual entry of the process table */
struct pid_entry {
    struct lock * pte_lock; /* Used to lock a specific process */
    struct cv * pte_cv; /* Used to put processes to sleep and notify other processes of completion */
    struct proc * pte_proc; /* The actual process of a specific entry in the process table*/
};

/**
 * Functions for the process table 
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