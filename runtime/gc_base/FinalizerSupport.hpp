
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef FINALIZERSUPPORT_HPP
#define FINALIZERSUPPORT_HPP

/* @ddr_namespace: default */
#define J9_FINALIZE_FLAGS_MASTER_WAKE_UP 1
#define J9_FINALIZE_FLAGS_RUN_FINALIZATION 2
#define J9_FINALIZE_FLAGS_RUN_FINALIZERS_ON_EXIT 4
#define J9_FINALIZE_FLAGS_SLAVE_AWAKE 8
#define J9_FINALIZE_FLAGS_SLAVE_WORK_COMPLETE 16
#define J9_FINALIZE_FLAGS_SHUTDOWN 32
#define J9_FINALIZE_FLAGS_FORCE_CLASS_LOADER_UNLOAD 64
#define J9_FINALIZE_FLAGS_SLEEPING 65536
#define J9_FINALIZE_FLAGS_SHUTDOWN_COMPLETE 131072
#define J9_FINALIZE_FLAGS_ACTIVE 262144
#define J9_FINALIZE_FLAGS_MASTER_WORK_REQUEST 99

#define J9_FINALIZE_JOB_TYPE_CONTAINS_OBJECT 1
#define J9_FINALIZE_JOB_TYPE_FINALIZATION 1
#define J9_FINALIZE_JOB_TYPE_FREE_CLASS_LOADER 2
#define J9_FINALIZE_JOB_TYPE_REF_ENQUEUE 3

#endif /* FINALIZERSUPPORT_HPP */
