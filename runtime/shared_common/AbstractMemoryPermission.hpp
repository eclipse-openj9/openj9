/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

#if !defined(ABSTRACT_MEMORY_PERMS_HPP_INCLUDED)
#define ABSTRACT_MEMORY_PERMS_HPP_INCLUDED

/* @ddr_namespace: default */
#include "j9.h"
#include "ut_j9shr.h"

#define ROUND_UP_TO(granularity, number) ( (((number) % (granularity)) ? ((number) + (granularity) - ((number) % (granularity))) : (number)))
#define ROUND_DOWN_TO(granularity, number) ( (((number) % (granularity)) ? ((number) - ((number) % (granularity))) : (number)))

class AbstractMemoryPermission
{
	public:
	virtual IDATA setRegionPermissions(J9PortLibrary* portLibrary, void *address, UDATA length, UDATA flags) = 0;
	virtual bool isVerbosePages(void) = 0;
	virtual bool isMemProtectEnabled(void) = 0;
	virtual bool isMemProtectPartialPagesEnabled(void) = 0;
	virtual void changePartialPageProtection(J9VMThread *currentThread, void *addr, bool readOnly, bool phaseCheck = true) = 0;
};

#endif /*ABSTRACT_MEMORY_PERMS_HPP_INCLUDED*/
