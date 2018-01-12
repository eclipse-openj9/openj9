/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

/**
 * @file
 * @ingroup Port
 * @brief  Functions common to Hypervisor
 */

#ifndef J9HYPERVISOR_H_
#define J9HYPERVISOR_H_

#include "j9port.h"

/* MACROS for indicating the presence of Hypervisor */
#define J9HYPERVISOR_PRESENT		1
#define J9HYPERVISOR_NOT_PRESENT	0
#define J9HYPERVISOR_NOT_INITIALIZED	-1

/* MACRO for the IBM_JAVA_HYPERVISOR_SETTINGS environment variable */
#define ENVVAR_IBM_JAVA_HYPERVISOR_SETTINGS "IBM_JAVA_HYPERVISOR_SETTINGS"

intptr_t
detect_hypervisor(struct J9PortLibrary *portLibrary);

intptr_t
detect_hypervisor_from_env(struct J9PortLibrary *portLibrary);

#endif /* J9HYPERVISOR_COMMON_H_ */
