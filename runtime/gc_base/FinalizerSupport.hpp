
/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef FINALIZERSUPPORT_HPP
#define FINALIZERSUPPORT_HPP

/* @ddr_namespace: default */
#define J9_FINALIZE_FLAGS_MAIN_WAKE_UP 0x01
#define J9_FINALIZE_FLAGS_RUN_FINALIZATION 0x02
/* unused: 0x04 */
#define J9_FINALIZE_FLAGS_WORKER_AWAKE 0x08
#define J9_FINALIZE_FLAGS_WORKER_WORK_COMPLETE 0x10
#define J9_FINALIZE_FLAGS_SHUTDOWN 0x20
#define J9_FINALIZE_FLAGS_FORCE_CLASS_LOADER_UNLOAD 0x40
#define J9_FINALIZE_FLAGS_SLEEPING 0x10000
#define J9_FINALIZE_FLAGS_SHUTDOWN_COMPLETE 0x20000
#define J9_FINALIZE_FLAGS_ACTIVE 0x40000
#define J9_FINALIZE_FLAGS_MAIN_WORK_REQUEST 0x63 /* MAIN_WAKE_UP | RUN_FINALIZATION | SHUTDOWN | FORCE_CLASS_LOADER_UNLOAD */

#define J9_FINALIZE_JOB_TYPE_CONTAINS_OBJECT 1
#define J9_FINALIZE_JOB_TYPE_FINALIZATION 1
#define J9_FINALIZE_JOB_TYPE_FREE_CLASS_LOADER 2
#define J9_FINALIZE_JOB_TYPE_REF_ENQUEUE 3

#endif /* FINALIZERSUPPORT_HPP */
