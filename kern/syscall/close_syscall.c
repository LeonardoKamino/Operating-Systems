/*
* Definition for open syscall
*/
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


int 
sys_close(int fd){
    int result;
    struct filetable* filetable = curproc->p_filetable;

    result = ft_remove_entry(filetable, fd);
    if(result){
        return result;
    }

    return 0;
}
