/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <copyinout.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <limits.h>
#include "opt-A2.h"

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
// #if OPT_A2
// int
// runprogram(char *progname, char **args)
// {
//   size_t args_len; // includes null
//
//   struct addrspace *as;
// 	struct vnode *v;
// 	vaddr_t entrypoint, stackptr;
// 	int result;
//
//   if (progname == NULL || args == NULL ) {
// 		return EFAULT;
// 	}
//
//   /* Count the number of arguments */
//   args_len = 0;
//   while (true) {
//     char *s = args[args_len];
//     args_len++;
//     if (s == NULL) break;
//   }
//
// 	/* Count argument length */
// 	size_t sumofbytes = 0;
//   for (size_t i = 0; i < args_len-1; i++) {
//     size_t len = 0;
// 		for (size_t j = 0; j <= PATH_MAX; j++) {
// 			if (j == PATH_MAX) return E2BIG;
// 			sumofbytes += sizeof(char);
// 			if (args[i][j] == '\0') break;
// 		}
//     sumofbytes += len * sizeof(char);
//     if (sumofbytes > ARG_MAX) return E2BIG;
//   }
//
// 	/* Open the file. */
// 	result = vfs_open(progname, O_RDONLY, 0, &v);
// 	if (result) {
// 		return result;
// 	}
//
// 	/* We should be a new process. */
// 	KASSERT(curproc_getas() == NULL);
//
// 	/* Create a new address space. */
// 	as = as_create();
// 	if (as ==NULL) {
// 		vfs_close(v);
// 		return ENOMEM;
// 	}
//
// 	/* Switch to it and activate it. */
// 	curproc_setas(as);
// 	as_activate();
//
// 	/* Load the executable. */
// 	result = load_elf(v, &entrypoint);
// 	if (result) {
// 		/* p_addrspace will go away when curproc is destroyed */
// 		vfs_close(v);
// 		return result;
// 	}
//
// 	/* Done with the file now. */
// 	vfs_close(v);
//
// 	/* Define the user stack in the address space */
// 	result = as_define_stack(as, &stackptr);
// 	if (result) {
// 		/* p_addrspace will go away when curproc is destroyed */
// 		return result;
// 	}
//
// 	/* Copy arguments into user stack */
//   char* argsptr[args_len];
//   for (size_t j = 0; j < args_len-1; j++) {
//     size_t i = args_len - 2 - j;
//     size_t len = strlen(args[i]) + 1;
//     stackptr -= ROUNDUP(len * sizeof(char), 8);
//     argsptr[j] = (char*)stackptr;
//     result = copyoutstr(args[i], (userptr_t)stackptr, PATH_MAX, &len);
//     if (result) break;
//   }
//   if (result) {
//     return result;
//   }
//     KASSERT(stackptr % 8 == 0);
//   /* Copy argv into user stack */
//   for (size_t i = 0; i < args_len; i++) {
//     stackptr -= sizeof(char*);
//     char *s;
//     if (i == 0) {
//       s = NULL;
//     }
//     else {
//       s = argsptr[i-1];
//     }
//     result = copyout(&s, (userptr_t)stackptr, sizeof(char*));
//     if (result) break;
//   }
//   if (result) {
//     return result;
//   }
//
// 	/* Warp to user mode. */
// 	enter_new_process(args_len-1 /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
// 			  stackptr, entrypoint);
//
// 	/* enter_new_process does not return. */
// 	panic("enter_new_process returned\n");
// 	return EINVAL;
// }
// /**
//  *
//  *	================ END OF RUNPROGRAM WITH ARGUMENTS
//  *
//  */
// #else
int
runprogram(char *progname)
{
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	KASSERT(curproc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}
// #endif
