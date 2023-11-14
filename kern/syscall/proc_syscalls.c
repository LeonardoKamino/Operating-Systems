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

/**
 * Takes a trapframe and a junk value and enters user mode
 */
void
enter_fork_usermode(void *data1, unsigned long junk)
{
    (void)junk;
    struct trapframe child_tf = *(struct trapframe *) data1;

    kfree(data1);
	as_activate();
    
    /* Switches to user mode using the trapframe*/
	mips_usermode(&child_tf);
}

/**
 * Duplicates the currently running process. The two copies 
 * are identical with one being the parent and the other being
 * the child
 */
int
sys_fork(struct trapframe *tf, int *retval)
{
    int result;
    struct proc *child_proc;
    struct trapframe *child_tf;
    
    /* Trapframe for the child being created */
    child_tf = kmalloc(sizeof(struct trapframe));
    if (child_tf == NULL) {
        return ENOMEM;
    }

    /* Copies the parent trapframe into the child's trapframe */
    memcpy(child_tf, tf, sizeof(struct trapframe));
    child_tf->tf_v0 = 0;
    child_tf->tf_v1 = 0;
    child_tf->tf_a3 = 0;
    child_tf->tf_epc += 4;

    /**
     * Copies all process information (filetable, cwd etc.) and
     * adds new process to process table
     */
    result = proc_fork(&child_proc);
    if (result) {
        kfree(child_tf);
        return result;
    }

    /* Gets the address space of the parent and then copies it to the child process */
    child_proc->p_addrspace = proc_getas();
    result = as_copy(proc_getas(), &child_proc->p_addrspace);
    if (result) {
        proc_destroy(child_proc);
        kfree(child_tf);
        return result;
    }

    /* Forks the parent thread */
    result = thread_fork("child", child_proc, enter_fork_usermode, child_tf, 0);
    if(result){
        proc_destroy(child_proc);
        kfree(child_tf);
        return result;
    }

    *retval = child_proc->pid;

    return 0;
}

/**
 * Replaces the currently executing program with a newly loaded 
 * program. The process id is unchanged
 */
int
sys_execv(const char *program, char **args, int *retval)
{
    char *kprogram;
    char **kargs;
    int result;
    int argc;
    struct vnode *file;
    struct addrspace *old_addrspace;
    vaddr_t entrypoint, stackptr;
    userptr_t argv_addr;

    *retval = -1;

    /* Ensures that the program name and args passed aren't null */
    if(program == NULL || args == NULL){
        return EFAULT;
    }

    kprogram = kmalloc(PATH_MAX * sizeof(char));
    if(kprogram == NULL){
        return ENOMEM;
    }

    /* Copy the program  name into kernel space */
    result = copyinstr((const_userptr_t) program, kprogram, PATH_MAX * sizeof(char) + 1, NULL);
    if(result){
        kfree(kprogram);
        return result;
    }

    if (program[0] == '\0'){
        kfree(kprogram);
        return EINVAL;
    }

    /* Copy the arguments into kernel space */
    result = count_args(args, &argc);
    if(result){
        kfree(kprogram);
        return result;
    }

    kargs = kmalloc((argc + 1) * sizeof(char *));
    if(kargs == NULL){
        kfree(kprogram);
        return ENOMEM;
    }

    /* */
    result = copy_args(args, kargs, argc);
    if (result){
        kfree(kprogram);
        return result;
    }

    /*  */
    old_addrspace = proc_getas();
    if(old_addrspace == NULL){
        return ENOMEM;
    }
    
    /*  */
    result = create_switch_addresspace();
    if(result){
        kfree_args(kargs, argc);
        kfree(kprogram);
        return result;
    }
    
    /* Opens the file for the specific program */
    result = vfs_open(kprogram, O_RDONLY, 0, &file);
    kfree(kprogram);
    if(result){
        kfree_args(kargs, argc);
        back_to_old_as(old_addrspace);
        return result;
    }
    
    /* Loads the executable using the file that was opened */
    result = load_elf(file, &entrypoint);
    vfs_close(file);
    if( result ){
        kfree_args(kargs, argc);
        back_to_old_as(old_addrspace);
        return result;
    }

    /* Defines the user stack space */
    result = as_define_stack(proc_getas(), &stackptr);
    if(result){
        kfree_args(kargs, argc);
        back_to_old_as(old_addrspace);
        return result;
    }

    /* Copy arguments into the user stack */
    result = copy_args_userspace(kargs, argc, &stackptr, &argv_addr);
    if(result){
        kfree_args(kargs, argc);
        back_to_old_as(old_addrspace);
        return result;
    }   

    kfree_args(kargs, argc);
    as_destroy(old_addrspace);
    

    /* Warps to user mode */
    enter_new_process(
        argc, 
        argv_addr, 
        NULL, 
        stackptr, 
        entrypoint);
        
    return 0;
}



int
copy_args_userspace (char **kargs, int argc, vaddr_t *stackptr, userptr_t *argv_addr)
{
	KASSERT(kargs != NULL);
	KASSERT(stackptr != NULL);

	int result;
	size_t str_size, padding;
	userptr_t stack_top, stack_bottom;
    // userptr_t user_arg[argc];

	stack_top = (userptr_t) *stackptr;

	for (int i = 0; i < argc; i++){
		str_size = strlen(kargs[i]) + 1; // +1 for null terminator
		if(str_size % 4 != 0){
            padding = 4 - (str_size % 4);
        }
		if (padding) {
			stack_top -= padding;
			// bzero(stack_top, padding);
		}
		stack_top -= str_size;
        // Copy string into stack
		result = copyoutstr(kargs[i], stack_top, str_size, NULL);
		if (result){
            return result;
        }
        // user_arg[i] = stack_top;
	}

    stack_bottom = stack_top;
    stack_top += (argc + 1) * sizeof(userptr_t)

    *argv_addr = stack_bottom;
    *stackptr = (vaddr_t) stack_bottom;

    for (int i = 0; i < argc; i++){
        // Copy pointer to string into stack
        stack_top
        result = copyout(&user_arg[i], stack_bottom, sizeof(userptr_t));
        if (result){
            return result;
        }
        stack_bottom += sizeof(userptr_t);
    }

	// Null terminate
	bzero(stack_bottom, sizeof(userptr_t));

	return 0;
}

void 
back_to_old_as(struct addrspace *old_as)
{
     struct addrspace *current_as;

    current_as = proc_getas();
    if(current_as != NULL){
        as_deactivate();
        as_destroy(current_as);
    }

    proc_setas(old_as);
    as_activate();
}

int
create_switch_addresspace(void){
    struct addrspace *as;

    as_deactivate();

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
copy_args(char **args, char *kargs[], int argc)
{
    int result;
    char check_arg;

    for(int i = 0; i < argc; i++){
        result = copyin((userptr_t) &args[i][0], (void *) &check_arg, (size_t) sizeof(char));
        if (result) {
            return result;
        }
        size_t string_size = strlen(args[i]) + 1;
        
        kargs[i] = kmalloc(string_size * sizeof(char));
        if(kargs[i] == NULL){
            kfree_args(kargs, i);
            return ENOMEM;
        }
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

/**/
int
count_args(char **args, int *argc)
{
    int num_args = 0;
    int result;
    char *check_arg;
    do {
		result = copyin((userptr_t) &args[num_args], (void *) &check_arg, (size_t) sizeof(char *));
		if (result) {
			return result;
        }
        if(check_arg != NULL){
            num_args++;
        }   
	} while (check_arg != NULL);

    *argc = num_args;
    return 0;
}

/**
 * Waits for the process specified by the pid to exit and 
 * returns an integer representing the status. If the process 
 * has already exited, waitpid returns immediately and if the 
 * process doesn’t exist, waitpid fails.
 * 
 * Waitpid relies on sys___exit's execution
 */
int
sys_waitpid(pid_t pid, int *status, int options)
{
    struct pid_entry * pt_entry = proctable->pt_entries[pid];
    int result;

    /**
     * Ensures that given pid is valid and that the process at this
     * pid actually exists
     */
    if(pid < 1 || pid > PID_MAX || pt_entry == NULL){
        return ESRCH;
    }

    /**
     * A check to make sure that the current process (the parent)
     * is actually the parent of the process that is being waited on.
     */
    if(pt_entry->pte_proc->parent_pid != curproc->pid){
        return ECHILD;
    }

    /* Ensure that options are always 0 */
    if(options != 0){
        return EINVAL;
    }

    /**
     * This is where the wait actually happens. In the event a 
     * parent is waiting on a child to exit and change its 
     * status to ZOMBIE, the process will be put to sleep until
     * the child exits. Whenever an exit happens (sys___exit), the
     * parent is notified via cv_broadcast and the parent will wake 
     * up and continue execution.
     */
    lock_acquire(pt_entry->pte_lock);
    while(pt_entry->pte_proc->p_status != P_ZOMBIE){
        cv_wait(pt_entry->pte_cv, pt_entry->pte_lock);
    }
    lock_release(pt_entry->pte_lock);

    int exitcode = pt_entry->pte_proc->exitcode;

    /* Copies the child process exitcode from kernel to user space */
    if(status != NULL){
        result = copyout(&exitcode, (userptr_t) status, sizeof(int32_t));
        if(result){
            return result;
        }
    }   

    return 0;
}

/**
 * Returns the process id of the current process 
 */
int 
sys_getpid(int *retval)
{
    KASSERT(retval != NULL);

    *retval = curproc->pid;

    return 0;
}

/**
 * Causes the current process to exit. The exit code is returned 
 * back to the other processes to be used in waitpid. 
 * 
 * This syscall takes in the exitcode as a parameter and based
 * on the previous status value of any process, will either
 * destroy it or set it to a ZOMBIE status. 
 * 
 * Waitpid relies on the execution of this syscall. All updates
 * to statuses and exitcodes that are made within this function
 * will be used to inform any waiting processes on the completion
 * of the current process (child).
 */
void
sys___exit(int exitcode)
{
    struct pid_entry *pt_entry = proctable->pt_entries[curproc->pid];

    lock_acquire(pt_entry->pte_lock);
    proc_update_children(curproc);

    spinlock_acquire(&pt_entry->pte_proc->p_lock);
    pt_entry->pte_proc->exitcode = exitcode;
    spinlock_release(&pt_entry->pte_proc->p_lock);
    
    /**
     * If the process has no parent, i.e. p_status = P_ORPHAN
     * we immediately destroy this process without changing 
     * its status as no process will be waiting on its completion.
     * 
     * If the process does have a parent however, we need to 
     * set the status of the process to P_ZOMBIE so that any process
     * waiting on its completion can be notified. cv_broadcast is
     * used to notify all processes waiting on the completion of
     * this process. 
     */
    if(pt_entry->pte_proc->p_status == P_ORPHAN){        
        
        proc_destroy(pt_entry->pte_proc);

    } else {
        pt_entry->pte_proc->p_status = P_ZOMBIE;

        cv_broadcast(pt_entry->pte_cv, pt_entry->pte_lock);
    }

    lock_release(pt_entry->pte_lock);

    /* Exit the current thread to complete the exit syscall */
    thread_exit();
}