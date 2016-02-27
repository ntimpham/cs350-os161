#include <types.h>
#include <kern/errno.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <proc_table.h>
#include "array.h"
#include "opt-A2.h"

static struct array *proc_table; // Key: pid, Val: proc
struct lock *proc_table_lock;

// Helper functions
int entry_create(struct proc_table_entry **retval);
void entry_destroy(struct proc_table_entry *entry);

/**
 * adds the given process to process table
 * @param  proc   process to add to table
 * @param  retval destination for pid
 * @return        0 success, ESRCH
 */
int proc_table_add(struct proc *proc, pid_t *retval) {
  if (proc == NULL) return EINVAL;

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
    bool iholdlock = lock_do_i_hold(proc_table_lock);
    KASSERT(iholdlock == true);

  pid_t pid = 0;
  pid_t freepid = 0;
  // Check if already exists
  for (unsigned i = 1; i < array_num(proc_table); i++) {
    struct proc_table_entry *ret = array_get(proc_table, i);
    if (ret == NULL) {
      freepid = i;
    }
    else if (ret->proc == proc) {
      pid = i;
    }
  }

  // Search failed, add new proc
  if (pid == 0) {
    struct proc_table_entry *entry;

    int x = entry_create(&entry);
    if (x) {
      return x;
    }

    if (freepid != 0) {
      pid = freepid;
      array_set(proc_table, pid, entry);
    }
    else {
      x = array_add(proc_table, entry, (unsigned*)&pid);
      if (x) {
        return x;
      }
    }
    entry->proc = proc;
    entry->pid = pid;
  }

  // Set return value if included
  if (retval != NULL) *retval = pid;

  return 0; // Success
}

/**
 * removes the process from the process table
 * @param  pid process id of process to remove
 * @return     0 success, -1 error
 */
int proc_table_remove(pid_t pid) {
  if (pid <= 0 || (unsigned)pid >= array_num(proc_table)) {
    return EDOM;
  }

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
    bool iholdlock = lock_do_i_hold(proc_table_lock);
    KASSERT(iholdlock == true);

  entry_destroy(array_get(proc_table, pid) );
  array_set(proc_table, pid, NULL);

  return 0; // Success
}

/**
 * gets the proc_table_entry of the given process
 * @param  pid    process id of entry to get
 * @param  retval destination to store entry
 * @return        error code
 */
int proc_table_get(pid_t pid, struct proc_table_entry **retval) {
  if (pid <= 0 || (unsigned)pid >= array_num(proc_table)) {
    return EDOM; // out of range
  }

    KASSERT(proc_table != NULL);
    KASSERT(proc_table_lock != NULL);
    bool iholdlock = lock_do_i_hold(proc_table_lock);
    KASSERT(iholdlock == true);

  struct proc_table_entry *entry = array_get(proc_table, pid);
  if (entry == NULL) {
    return ESRCH; // process not found
  }
  *retval = entry;

  return 0; // Success
}

/**
 * creates the global process table
 */
void proc_table_init(void) {
  // Initialize proc_table
  proc_table = array_create();
  if (proc_table == NULL) {
    panic("array_create for proc_table_init failed\n");
  }
  // Set index 0 of pid_list to NULL
  int x = array_setsize(proc_table, 1);
  if (x != 0) {
    unsigned size = array_num(proc_table);
    for (unsigned i = 0; i < size; i++) {
      array_remove(proc_table, 0);
    }
    array_destroy(proc_table);
    panic(x + ": array_setsize for proc_table_init failed\n");
  }
  array_set(proc_table, 0, NULL);
  // Initilize proc_table_lock
  proc_table_lock = lock_create("proc_table_lock");
  if (proc_table_lock == NULL) {
    unsigned size = array_num(proc_table);
    for (unsigned i = 0; i < size; i++) {
      array_remove(proc_table, 0);
    }
    array_destroy(proc_table);
    panic("lock_create for proc_table_init failed\n");
  }
}

/**
 * destroys the global process table
 */
void proc_table_destroy(void) {
  for (unsigned i = 0; i < array_num(proc_table); i++) {
    entry_destroy(array_get(proc_table, i) );
  }
  unsigned size = array_num(proc_table);
  for (unsigned i = 0; i < size; i++) {
    array_remove(proc_table, 0);
  }
  array_destroy(proc_table);
  lock_destroy(proc_table_lock);
}

/**
 * creates a new blank proc_table_entry
 * @return new blank proc_table_entry
 */
int entry_create(struct proc_table_entry **retval) {
  if (retval == NULL) {
    return EINVAL;
  }

  struct proc_table_entry *entry;

  entry = kmalloc(sizeof(*entry));
  if (entry == NULL) return ENOMEM;

  entry->proc = NULL;
  entry->pid = 0;
  entry->numref = 0;
  entry->isdead = false;
  entry->exitcode = 0;

  entry->exitcode_cv = cv_create("proc_table_entry_cv");
  if (entry->exitcode_cv == NULL) {
    kfree(entry);
    return ENOMEM;
  }

  entry->parent = NULL;

  entry->children = array_create();
  if (entry->children == NULL) {
    cv_destroy(entry->exitcode_cv);
    kfree(entry);
    return ENOMEM;
  }

  *retval = entry;
  return 0; // Success
}

/**
 * destroys the given proc_table_entry
 * @param entry the proc_table_entry to destroy
 */
void entry_destroy(struct proc_table_entry *entry) {
  if (entry != NULL) {
    cv_destroy(entry->exitcode_cv);
    unsigned size = array_num(entry->children);
    for (unsigned i = 0; i < size; i++) {
      array_remove(entry->children, 0);
    }
    array_destroy(entry->children);
    kfree(entry);
  }
}
