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
    int program_size;
    char *kprogram;
    char **kargs;
    char **user_args;
    int result;
    int argc;
    struct vnode *file;
    vaddr_t entrypoint, stackptr;
    userptr_t cpaddr;

    *retval = -1;

    if(program == NULL || args == NULL){
        return EFAULT;
    }

    //Copy program  name into kernel space
    program_size = (strlen(program) + 1)* sizeof(char);
    kprogram = kmalloc(program_size);
    if(kprogram == NULL){
        return ENOMEM;
    }

    // Copy arguments into kernel space

    result = copyinstr((const_userptr_t) program, kprogram, program_size, NULL);
    if(result){
        kfree(kprogram);
        return result;
    }

    argc = count_args(args);

    kargs = kmalloc((argc + 1) * sizeof(char *));
    if(kargs == NULL){
        kfree(kprogram);
        return ENOMEM;
    }

    result = copy_args(args, kargs, argc);
    if (result){
        kfree(kprogram);
        return result;
    }

    //Change address space
    result = create_switch_addresspace();
    if(result){
        kfree_args(kargs, argc);
        kfree(kprogram);
        return result;
    }
    
    // Open file
    result = vfs_open(kprogram, O_RDONLY, 0, &file);
    kfree(kprogram);
    if(result){
        kfree_args(kargs, argc);
        return result;
    }
    
    //Load executable
    result = load_elf(file, &entrypoint);
    vfs_close(file);
    if( result ){
        kfree_args(kargs, argc);
        return result;
    }

    //Define stack
    result = as_define_stack(proc_getas(), &stackptr);
    if(result){
        kfree_args(kargs, argc);
        return result;
    }

    // Copy arguments into stack
    /* set up stack with arguments here */
	user_args =  kmalloc(sizeof(char *) * (argc+1));
	if (user_args==NULL)
	{
		kfree_args(kargs, argc);
		return -ENOMEM;
	}

	cpaddr = (userptr_t) stackptr;
    
    for(int i = 0; i < argc; i++){
        size_t arg_size = strlen(kargs[i]) + 1;
        cpaddr -= arg_size;
        int tail = 0;
		if ((int)cpaddr & 0x3)
		{
			tail = (int )cpaddr & 0x3;
			cpaddr -= tail;
		}
        result = copyoutstr(kargs[i], (userptr_t) cpaddr, arg_size, NULL);
        if(result){
            kfree_args(kargs, argc);
            kfree(user_args);
            return result;
        }
        user_args[i] = (char *) cpaddr;
    }

	user_args[argc] = NULL;
	cpaddr -= (sizeof(char *) * (argc) + sizeof(NULL));
	for (int i = 0; i <= argc; i++) {
        result = copyout(&user_args[i], (userptr_t)(cpaddr + i * sizeof(char *)), sizeof(char *));
        if (result) {
            kfree_args(kargs, argc);
            kfree(user_args);
            return result;
        }
    }

    // Free old arguments
    kfree_args(kargs, argc);

    // Warp to user mode
    enter_new_process(
        argc, 
        cpaddr, 
        NULL, 
        stackptr, 
        entrypoint);
        
    return 0;
}

int
create_switch_addresspace(void){
    struct addrspace *as;

    as = proc_getas();
    if(as == NULL){
        return ENOMEM;
    }

    as_destroy(as);

    /* Create a new address space. */
	as = as_create();
	if (as == NULL) {
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	proc_setas(as);
	as_activate();
    return 0;
}

int
copy_args(char **args, char **kargs, int argc)
{
    int result;

    for(int i = 0; i < argc; i++){
        kargs[i] = kmalloc((strlen(args[i]) + 1) * sizeof(char));
        
        if(kargs[i] == NULL){
            kfree_args(kargs, i);
            return ENOMEM;
        }

        size_t string_size = (strlen(args[i]) + 1) * sizeof(char);
        result = copyinstr((const_userptr_t) args[i], kargs[i], string_size, NULL);
        if(result){
            kfree_args(kargs, i);
            return result;
        }
    }
    kargs[argc] = NULL; //Make sure the last element is NULL
    return 0;
}

void
kfree_args(char **kargs, int argc)
{
    for(int i = 0; i < argc; i++){
        kfree(kargs[i]);
    }
    kfree(kargs);
}

int
count_args(char **args)
{
    int count = 0;
    while(args[count] != NULL){
        count++;
    }
    return count;
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