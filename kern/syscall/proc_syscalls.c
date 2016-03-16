#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <proc_table.h>
#include <machine/trapframe.h>
#include <vfs.h>
#include <limits.h>
#include "array.h"
#include "opt-A2.h"

#if OPT_A2
int
sys_fork(struct trapframe *tf,
      pid_t *retval)
{

  struct proc *parent = curproc;
  struct proc *child;
  *retval = -1;

  // Create process structure for child process
  child = proc_create_runprogram(parent->p_name);
  if (child == NULL) return ENOMEM;

  // Create and copy address space (and data) from parent to child
  struct addrspace *c_as = as_create();
  if (c_as == NULL) {
    proc_destroy(child);
    return ENOMEM;
  }
  int x = as_copy(parent->p_addrspace, &c_as);
  if (x) {
    as_destroy(c_as);
    proc_destroy(child);
    return x;
  }

  // Attach the newly created address space to the child process structure
  child->p_addrspace = c_as;

  // Create the parent/child relationship
  proc_table_lock_acquire();

  struct proc_table_entry *c_entry;
  proc_table_get(child->pid, &c_entry);
    KASSERT(c_entry != NULL);
    KASSERT(c_entry->proc == child);
    KASSERT(c_entry->pid == child->pid);
  struct proc_table_entry *p_entry;
  proc_table_get(parent->pid, &p_entry);
    KASSERT(p_entry != NULL);
    KASSERT(p_entry->proc == parent);
    KASSERT(p_entry->pid == parent->pid);

  c_entry->parent = p_entry;
  c_entry->numref++;

  x = array_add((struct array*)p_entry->children, c_entry, NULL);
  if (x) {
    proc_table_remove(child->pid);
    proc_table_lock_release();
    as_destroy(c_as);
    proc_destroy(child);
    return x;
  }
  p_entry->numref++;

  // Create thread for child process (need a safe way to pass the trapframe to
  // the child thread).
  struct trapframe *c_tf = kmalloc(sizeof(struct trapframe) );
  if (c_tf == NULL) {
    proc_table_remove(child->pid);
    proc_table_lock_release();
    as_destroy(c_as);
    proc_destroy(child);
    return ENOMEM;
  }
  *c_tf = *tf;
  x = thread_fork(curthread->t_name, child, enter_forked_process, c_tf, 0);
  if (x) {
    kfree(c_tf);
    proc_table_remove(child->pid);
    proc_table_lock_release();
    as_destroy(c_as);
    proc_destroy(child);
    return x;
  }

  proc_table_lock_release();

  *retval = child->pid;
  return 0; // Success
}
#endif

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;

#if OPT_A2
  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  proc_table_lock_acquire();

  struct proc_table_entry *entry;
  proc_table_get(p->pid, &entry);
    KASSERT(entry != NULL);
    KASSERT(p == entry->proc);
    KASSERT(p->pid == entry->pid);

  // Parent update
  struct proc_table_entry *pe = (struct proc_table_entry*)entry->parent;
  if (pe != NULL) {
      KASSERT(pe->numref > 0);
    pe->numref--;

    // Check cleanup
    if (pe->isdead && pe->numref == 0) {
      proc_table_remove(pe->pid);
    }
  }

  // Children update
  struct array *ca = (struct array*)entry->children;
  for (unsigned i = 0; i < array_num(ca); i++) {
    struct proc_table_entry *ce = array_get(ca, i);
      KASSERT(ce != NULL);
      KASSERT(ce->numref > 0);
    ce->numref--;

    // Check cleanup
    if (ce->isdead && ce->numref == 0) {
      proc_table_remove(ce->pid);
    }
  }

  // Self update
  entry->isdead = true;
  entry->exitcode = exitcode;
  proc_table_broadcastfor(p->pid);
  // Check cleanup
  if (entry->numref == 0) {
    proc_table_remove(entry->pid);
  }

  proc_table_lock_release();

  // Cleanup process
    KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  as = curproc_setas(NULL);
  as_destroy(as);
  proc_remthread(curthread);
  proc_destroy(p);

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");

  /*
  //
  //@@@@@=====@@@@@===== END OF _exit implementation =====@@@@@=====@@@@@
  //
   */
#else
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
#endif
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
#if OPT_A2
    KASSERT(curproc != NULL);
  *retval = curproc->pid;
  return 0; // Success

  /*
  //
  //@@@@@=====@@@@@===== END OF getpid implementation =====@@@@@=====@@@@@
  //
   */
#else
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
#endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{

  int exitstatus;
  int result;

#if OPT_A2
  if (status == NULL) {
    return EFAULT;
  }
  if (options != 0) {
    return EINVAL;
  }

  proc_table_lock_acquire();

  struct proc_table_entry *ce;
  int x = proc_table_get(pid, &ce);
  if (x) return x;

    KASSERT(ce != NULL);
    KASSERT(ce->pid == pid);

  // Check if parent
  if (curproc->pid != ce->parent->pid) return ECHILD;

  x = proc_table_waiton(pid);
  if (x) {
    proc_table_lock_release();
    return x;
  }

  // encode and set status
  exitstatus = _MKWAIT_EXIT(ce->exitcode);

  proc_table_lock_release();

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) return result;

  *retval = pid;
  return 0; // Success

  /*
  //
  //@@@@@=====@@@@@===== END OF waitpid implementation =====@@@@@=====@@@@@
  //
   */
#else
  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */
  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
#endif
}

#if OPT_A2
int
sys_execv(const char *program,
      char **args)
{
  char* kprog;
  size_t prog_len; // includes null
  char** kargs;
  size_t args_len; // includes null

  struct addrspace *as, *old_as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

  if (program == NULL || args == NULL ) {
		return EFAULT;
	}

  /* Count the number of arguments */
  args_len = 0;
  while (true) {
    char *s;
    result = copyin((const_userptr_t)args + (args_len*sizeof(char*)), &s, sizeof(char*));
    if (result) return result;
    args_len++;
    if (s == NULL) break;
  }

  /* Copy the program path into the kernel */
  kprog = kmalloc((PATH_MAX) * sizeof(char));
  if (kprog == NULL) {
    return ENOMEM;
  }
  result = copyinstr((userptr_t)program, kprog, PATH_MAX, &prog_len);
  if (result) {
    kfree(kprog);
    return result;
  }

  /* Copy arguments into the kernel */
  kargs = kmalloc(args_len * sizeof(char*));
  if (kargs == NULL) {
    kfree(kprog);
    return ENOMEM;
  }
  kargs[args_len-1] = NULL;

  size_t sumofbytes = 0;
  for (size_t i = 0; i < args_len-1; i++) {
    size_t len;
    char tmp[PATH_MAX];
    kargs[i] = NULL;

    result = copyinstr((const_userptr_t)args[i], tmp, PATH_MAX, &len);
    if (result) break;
    sumofbytes += len * sizeof(char);
    if (sumofbytes > ARG_MAX) {
      result = E2BIG;
      break;
    }
    kargs[i] = kstrdup((const char*)tmp);
    if (kargs[i] == NULL) {
      result = ENOMEM;
      break;
    }
  }
  if (result) {
    for (size_t i = 0; i < args_len; i++) {
      if (kargs[i] != NULL) kfree(kargs[i]);
    }
    kfree(kargs);
    kfree(kprog);
    return result;
  }

  /* Open the file. */
	result = vfs_open(kprog, O_RDONLY, 0, &v);
	if (result) {
    for (size_t i = 0; i < args_len; i++) {
      if (kargs[i] != NULL) kfree(kargs[i]);
    }
    kfree(kargs);
    kfree(kprog);
		return result;
	}

  /* Save old address space */
  old_as = curproc_getas();

	/* Create a new address space. */
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
    for (size_t i = 0; i < args_len; i++) {
      if (kargs[i] != NULL) kfree(kargs[i]);
    }
    kfree(kargs);
    kfree(kprog);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		curproc_setas(old_as);
    as_destroy(as);
		vfs_close(v);
    for (size_t i = 0; i < args_len; i++) {
      if (kargs[i] != NULL) kfree(kargs[i]);
    }
    kfree(kargs);
    kfree(kprog);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
    curproc_setas(old_as);
    as_destroy(as);
    for (size_t i = 0; i < args_len; i++) {
      if (kargs[i] != NULL) kfree(kargs[i]);
    }
    kfree(kargs);
    kfree(kprog);
		return result;
	}

  /* Copy arguments into user stack */
  char* argsptr[args_len];
  for (size_t j = 0; j < args_len-1; j++) {
    size_t i = args_len - 2 - j;
    size_t len = strlen(kargs[i]) + 1;
    stackptr -= ROUNDUP(len * sizeof(char), 8);
    argsptr[j] = (char*)stackptr;
    result = copyoutstr(kargs[i], (userptr_t)stackptr, PATH_MAX, &len);
    if (result) break;
  }
  if (result) {
    curproc_setas(old_as);
    as_destroy(as);
    for (size_t i = 0; i < args_len; i++) {
      if (kargs[i] != NULL) kfree(kargs[i]);
    }
    kfree(kargs);
    kfree(kprog);
    return result;
  }
    KASSERT(stackptr % 8 == 0);
  /* Copy argv into user stack */
  for (size_t i = 0; i < args_len; i++) {
    stackptr -= sizeof(char*);
    char *s;
    if (i == 0) {
      s = NULL;
    }
    else {
      s = argsptr[i-1];
    }
    result = copyout(&s, (userptr_t)stackptr, sizeof(char*));
    if (result) break;
  }
  if (result) {
    curproc_setas(old_as);
    as_destroy(as);
    for (size_t i = 0; i < args_len; i++) {
      if (kargs[i] != NULL) kfree(kargs[i]);
    }
    kfree(kargs);
    kfree(kprog);
    return result;
  }

  /* Cleanup */
  for (size_t i = 0; i < args_len; i++) {
    if (kargs[i] != NULL) kfree(kargs[i]);
  }
  kfree(kargs);
  kfree(kprog);
  as_destroy(old_as);

  // for (vaddr_t i = stackptr; i <= stackptr; i++) {
  //   kprintf("\t%p: %c\n", (char*) i, (char) *((char*)i));
  // }
  // kprintf("\tVALUE OF ARGV: %p\n", *(char**)stackptr);

	/* Warp to user mode. */
	enter_new_process(args_len-1 /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
			  stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}
/*
//
//@@@@@=====@@@@@===== END OF execv implementation =====@@@@@=====@@@@@
//
 */
#endif
