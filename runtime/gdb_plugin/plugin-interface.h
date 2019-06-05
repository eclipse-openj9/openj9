/*******************************************************************************
 * Copyright (c) 2007, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef PLUGIN_INTERFACE_H
#define PLUGIN_INTERFACE_H

#include "omrthread.h"
#include "thrdsup.h"
#include "thrtypes.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef void * CORE_ADDR;
typedef char gdb_byte;

enum command_class
{
	/* Special args to help_list */
	class_deprecated = -3, all_classes = -2, all_commands = -1,
	/* Classes of commands */
	no_class = -1, class_run = 0, class_vars, class_stack,
	class_files, class_support, class_info, class_breakpoint, class_trace,
	class_alias, class_obscure, class_user, class_maintenance,
	class_plugin, class_pseudo, class_tui, class_xdb
};


typedef struct gdb_plugin_thread_info {
	pid_t tid;        /* thread id */
	CORE_ADDR addr;   /* thread address */
} gdb_plugin_thread_info;

typedef struct gdb_plugin_register_info {
	char * name;
	unsigned long long value;
} gdb_plugin_register_info;


#define PLUGIN_VERSION 3

/* plugins compiled with this header file conform to the following version */
#define PLUGIN_CURRENT_VERSION PLUGIN_VERSION

struct plugin_service_ops {

  /* Read len bytes of target memory at address addr, placing the results in
     GDB's memory at buffer.  Returns either 0 for success or an errno value
     if any error occurs. */
  int (*read_inferior_memory) (CORE_ADDR addr, gdb_byte *buffer, int len);

  /* Writes len bytes from buffer to target memory at address addr.
     Returns either 0 for success or an errno value if any error occurs. */
  int (*write_inferior_memory) (CORE_ADDR addr, const gdb_byte *buffer, int len);


  /* Evaluate the expression and return the corresponding address */
  CORE_ADDR (*parse_and_eval_address) (char *arg);


  void (*print_output)(const char *fmt, ...);

  void (*print_error)(const char *fmt, ...);

  /*  FIXME: we need to evaluate if it is better to have a new function that
      calls add_com instead, in order to make any internal GDB changes
      transparent to the user.  */
  struct cmd_list_element *(*add_command)(char *name,
      enum command_class cmd_class, void (*func) (char *, int),
      char *doc);

  /* Ask user a y-or-n question and return 0 if answer is no, 1 if
     answer is yes, or 0 if answer is defaulted.
     The arguments are given to printf to print the question.
     The first, a control string, should end in "? ".
     It should not say how to answer, because we do that.  */
  int (*no_query) (const char *ctlstr, ...);

  /* Ask user a y-or-n question and return 0 if answer is no, 1 if
     answer is yes, or 1 if answer is defaulted.
     The arguments are given to printf to print the question.
     The first, a control string, should end in "? ".
     It should not say how to answer, because we do that.  */
  int (*yes_query) (const char *ctlstr, ...);

  /* Ask user a y-or-n question and return 1 iff answer is yes.
     The arguments are given to printf to print the question.
     The first, a control string, should end in "? ".
     It should not say how to answer, because we do that.  */
  int (*nodefault_query) (const char *ctlstr, ...);

  /* Get a list of threads. If threads is NULL return the count of threads
	 available for retrieval. If threads is non-null, count specifies maximum
	 number of threads to return */
  int (*get_thread_list) (gdb_plugin_thread_info * threads, int * count);   

  /* Get a list of register names and values. The registers array should be
	 large enough to accommodate count registers. If registers is null, count
	 will be initialized with the register count and returned. If the
	 register->name is NULL, a string will be allocated and must be freed
	 by the user once done. If the passed in register->name is non-null, the
	 name will not be set, presumably because the user is re-querying
	 register state */
  int (*get_registers) (pid_t tid, gdb_plugin_register_info * registers, int * count);

  /* Get the architecture of the current target, be it a process or a core */
  int (*get_target_arch) ();

  int (*add_command_alias) (char * aliasName, void (*func) (char *, int));

};

struct plugin_ops {

  const char *name;

  /*  Handle for the shared object plugin file */
  void *handle;

  /*  Function pointers for the plugin functions  */

  int (*get_version) (void);
  const char * (*get_name) (void);
  int (*session_init) (int session_id,
                       struct plugin_service_ops *plugin_service);

  /* pointer to the next loaded plugin */
  struct plugin_ops *next;
};

struct plugin_ops *
init_plugin (char *path);

void
finish_plugin (struct plugin_ops *plugin);

#ifdef __cplusplus
}
#endif

#endif /* PLUGIN_INTERFACE_H */
