#include <types.h>
#include <proc.h>
#include <synch.h>
#include <proc_table.h>
#include "array.h"
#include "opt-A2.h"

static volatile array *proc_table; // Key: pid, Val: proc
static struct lock *proc_table_lock;

int proc_table_add(struct proc *proc, pid_t *retval) {
  if (proc == NULL) return EINVAL;

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
  lock_acquire(proc_table_lock);

  pid_t pid = 0;
  pid_t freepid = 0;
  // Check if already exists
  for (int i = 1; i < array_num(proc_table); i++) {
    struct proc *ret = array_get(proc_table, i);
    if (ret == proc) {
      pid = i;
    }
    else if (ret == NULL) {
      freepid = i;
    }
  }

  // Search failed, add new proc
  if (pid == 0) {
    if (freepid != 0) {
      pid = freepid;
      array_set(proc_table, pid, proc);
    }
    else {
      int errno;
      do {
        errno = array_add(proc_table, proc, &pid);
      } while (errno != 0 || pid <= 0);
    }
  }

  // Set return value if included
  if (retval != NULL) *retval = pid;

  lock_release(proc_table_lock);
  return 0; // Success
}

int proc_table_remove(pid_t pid) {
  if (pid <= 0 || pid >= array_num(proc_table)) {
    return EDOM;
  }

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
  lock_acquire(proc_table_lock);

  array_set(proc_table, pid, NULL);

  lock_release(proc_table_lock);
  return 0; // Success
}

void proc_table_init() {
  // Initialize proc_table
  proc_table = array_create();
  if (pid_list == NULL) {
    panic("array_create for proc_table_init failed\n");
  }
  // Set index 0 of pid_list to NULL
  int errno = array_setsize(proc_table, 1);
  if (errno != 0) {
    array_destroy(proc_table);
    panic(errno + ": array_setsize for proc_table_init failed\n");
  }
  array_set(proc_table, 0, NULL);
  // Initilize proc_table_lock
  proc_table_lock = lock_create("proc_table_lock");
  if (proc_table_lock == NULL) {
    array_destroy(proc_table);
    panic("lock_create for proc_table_init failed\n");
  }
}

void proc_table_destroy() {
  array_destroy(proc_table);
  lock_destroy(proc_table_lock);
}
