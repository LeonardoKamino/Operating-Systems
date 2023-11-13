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
    char *kprogram;
    char **kargs;
    int result;
    int argc;
    struct vnode *file;
    struct addrspace *old_addrspace;
    vaddr_t entrypoint, stackptr;
    userptr_t argv_addr;

    *retval = -1;

    if(program == NULL || args == NULL){
        return EFAULT;
    }

    //Copy program  name into kernel space
    kprogram = kmalloc(PATH_MAX * sizeof(char));
    if(kprogram == NULL){
        return ENOMEM;
    }

    result = copyinstr((const_userptr_t) program, kprogram, PATH_MAX * sizeof(char) + 1, NULL);
    if(result){
        kfree(kprogram);
        return result;
    }

    // Check if program name is empty
    if (program[0] == '\0'){
        kfree(kprogram);
        return EINVAL;
    }

    // Copy arguments into kernel space
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

    result = copy_args(args, kargs, argc);
    if (result){
        kfree(kprogram);
        return result;
    }

    //Change address space
    old_addrspace = proc_getas();
    if(old_addrspace == NULL){
        return ENOMEM;
    }
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
        back_to_old_as(old_addrspace);
        return result;
    }
    
    //Load executable
    result = load_elf(file, &entrypoint);
    vfs_close(file);
    if( result ){
        kfree_args(kargs, argc);
        back_to_old_as(old_addrspace);
        return result;
    }

    //Define stack
    result = as_define_stack(proc_getas(), &stackptr);
    if(result){
        kfree_args(kargs, argc);
        back_to_old_as(old_addrspace);
        return result;
    }

    // Copy arguments into stack
    result = copy_args_userspace(kargs, argc, &stackptr, &argv_addr);
    if(result){
        kfree_args(kargs, argc);
        back_to_old_as(old_addrspace);
        return result;
    }   

    // Free old arguments
    kfree_args(kargs, argc);

    //Destroy old address space
    as_destroy(old_addrspace);

    // Warp to user mode
    enter_new_process(
        argc, 
        argv_addr, 
        NULL, 
        stackptr, 
        entrypoint);
        
    return 0;
}


int
get_padding(int size)
{
    int padding = 0;
    if(size % 4 != 0){
        padding = 4 - (size % 4);
    }
    return padding;
}

int
copy_args_userspace (char **kargs,int argc , vaddr_t *stackptr, userptr_t *argv_addr)
{
	KASSERT(kargs != NULL);
	KASSERT(stackptr != NULL);

	int result;
	size_t str_size, padding;
	userptr_t stack_top, stack_bottom;
    userptr_t user_arg[argc];

	stack_top = (userptr_t) *stackptr;

	for (int i = 0; i < argc; i++){
		str_size = strlen(kargs[i]) + 1; // +1 for null terminator
		padding = get_padding(str_size);
		if (padding) {
			stack_top -= padding;
			bzero(stack_top, padding);
		}
		stack_top -= str_size;
        // Copy string into stack
		result = copyoutstr(kargs[i], stack_top, str_size, NULL);
		if (result){
            return result;
        }
        user_arg[i] = stack_top;
	}

    stack_bottom = stack_top - (argc + 1) * sizeof(userptr_t);
    *argv_addr = stack_bottom;
    *stackptr = (vaddr_t) stack_bottom;

    for (int i = 0; i < argc; i++){
        // Copy pointer to string into stack
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

int
sys_waitpid(pid_t pid, int *status, int options)
{
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
    struct pid_entry *pt_entry = proctable->pt_entries[curproc->pid];

    lock_acquire(pt_entry->pte_lock);
    proc_update_children(curproc);

    spinlock_acquire(&pt_entry->pte_proc->p_lock);
    pt_entry->pte_proc->exitcode = exitcode;
    spinlock_release(&pt_entry->pte_proc->p_lock);
    
    /* Process has no parent*/
    if(pt_entry->pte_proc->p_status == P_ORPHAN){        
        
        proc_destroy(pt_entry->pte_proc);

    } else {
        pt_entry->pte_proc->p_status = P_ZOMBIE;

        cv_broadcast(pt_entry->pte_cv, pt_entry->pte_lock);
    }

    lock_release(pt_entry->pte_lock);

    thread_exit();
}