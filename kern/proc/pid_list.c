#include "pid_list.h"
#include "array.h"
#include "synch.h"
#include <kern/errno.h>
#include "opt-A2.h"

static volatile array *pid_list;
static volatile array *parent_list;
static struct lock *pid_lock;

/**
 * returns the pid of proc, assigning one if needed
 * @param  proc   process to get pid
 * @param  retval target to store pid
 * @return        error code, 0 if successful
 */
int pid_list_getpid(struct proc *proc, pid_t *retval) {
  if (proc == NULL || retval == NULL) {
    return EINVAL;
  }
    KASSERT(pid_list != NULL);
    KASSERT(parent_list != NULL);
    KASSERT(pid_lock != NULL);
  lock_acquire(pid_lock);

  int errno = 0;
  pid_t freepid = 0;
  *retval = 0;
  // Search for proc in pid_list
  for (int i = 1; i < array_num(pid_list); i++) {
    struct proc *ret = array_get(pid_list, i);
    if (ret == proc) {
      *retval = i;
    }
    else if (ret == NULL) {
      freepid = i;
    }
  }
  // Not assigned, add proc to pid_list
  if (*retval == 0) {
    if (freepid != 0) {
      *retval = freepid;
      array_set(pid_list, freepid, proc);
      array_set(parent_list, freepid, NULL);
    }
    else {
      errno = array_add(pid_list, proc, retval);
      if (errno != 0) {
        lock_release(pid_lock);
        return errno;
      }
      errno = array_add(parent_list, NULL, NULL);
    }
  }

  lock_release(pid_lock);
  return errno;
}

/**
 * returns the parent pid of proc
 * @param  proc   process to get parent pid
 * @param  retval target to store pid
 * @return        error code, 0 if successful
 */
int pid_list_getparent(struct proc *proc, pid_t *retval) {
  if (proc == NULL || retval == NULL) {
    return EINVAL;
  }
    KASSERT(pid_list != NULL);
    KASSERT(parent_list != NULL);
    KASSERT(pid_lock != NULL);
  lock_acquire(pid_lock);
  int errno = 0;

  // Search for pid of proc
  pid_t procpid = 0;
  errno = pid_list_getpid(proc, &procpid);
  if (errno != 0) {
    lock_release(pid_lock);
    return errno;
  }
    KASSERT(procpid != 0);
  // get pid of parent
  void *parentpid = array_get(parent_list, procpid);
  if (parentpid == NULL) {
    lock_release(pid_lock);
    return ESRCH;
  }

  *retval = (int)parentpid;
  lock_release(pid_lock);
  return errno;
}

/**
 * sets the parent of proc to parentpid
 * @param  proc      process to set parent
 * @param  parentpid pid of proc's parent
 * @return           error code, 0 if successful
 */
int pid_list_setparent(struct proc *proc, pid_t parentpid) {
  if (proc == NULL || parentpid == 0) {
    return EINVAL;
  }
    KASSERT(pid_list != NULL);
    KASSERT(parent_list != NULL);
    KASSERT(pid_lock != NULL);
  lock_acquire(pid_lock);
  int errno = 0;

  // Search for pid of proc
  pid_t procpid = 0;
  errno = pid_list_getpid(proc, &procpid);
  if (errno != 0) {
    lock_release(pid_lock);
    return errno;
  }
    KASSERT(procpid != 0);
  // Set parent
  array_set(parent_list, procpid, (void*)parentpid);

  lock_release(pid_lock);
  return errno;
}

/**
 * Initialize pid related resources
 */
void pid_list_init() {
  // Initialize pid_list
  pid_list = array_create();
  if (pid_list == NULL) {
    panic("array_create pid_list for pid_list_init failed\n");
  }
  // Set index 0 of pid_list to NULL
  int errno = array_setsize(pid_list, 1);
  if (errno == ENOMEM) {
    array_destroy(pid_list);
    panic("array_setsize pid_list for pid_list_init failed\n");
  }
  array_set(pid_list, 0, NULL);
  // Initialize parent_list
  parent_list = array_create();
  if (parent_list == NULL) {
    array_destroy(pid_list);
    panic("array_create parent_list for pid_list_init failed\n");
  }
  // Set index 0 of parent_list to NULL
  errno = array_setsize(parent_list, 1);
  if (errno == ENOMEM) {
    array_destroy(pid_list);
    array_destroy(parent_list);
    panic("array_setsize parent_list for pid_list_init failed\n");
  }
  array_set(parent_list, 0, NULL);
  // Initilize pid_lock
  pid_lock = lock_create("pid_lock");
  if (pid_lock == NULL) {
    array_destroy(pid_list);
    array_destroy(parent_list);
    panic("lock_create pid_lock for pid_list_init failed\n");
  }
}

/**
 * destroy pid related resources
 */
void pid_list_destroy() {
  array_destroy(pid_list);
  array_destroy(parent_list);
  lock_destroy(pid_lock);
}
