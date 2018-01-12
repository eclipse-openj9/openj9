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

#ifndef dynload_h
#define dynload_h

#include "j9comp.h"
#include "j9cfg.h"

#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT))  /* File Level Build Flags */

#define ROUND_TO(granularity, number) ((number) +		\
	(((number) % (granularity)) ? ((granularity) - ((number) % (granularity))) : 0))

#define ROUNDING_GRANULARITY 1024
#define CHECK_BUFFER( buf, currentSize, requiredSize ) \
	\
	if (currentSize < (requiredSize) ) { \
		if (currentSize > 0) { \
			j9mem_free_memory( buf ); \
		} \
		currentSize = requiredSize; \
		buf = j9mem_allocate_memory(currentSize, J9MEM_CATEGORY_CLASSES); \
		if (buf == NULL) { \
			currentSize = 0; \
			return -1; /* Replaced BCT_ERR_OUT_OF_MEMORY as is not -1 */ \
		} \
	}

#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */ /* End File Level Build Flags */

#endif			/* dynload_h */
