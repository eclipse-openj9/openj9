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

#ifndef SEGMENT_H_
#define SEGMENT_H_

#include "omrlinkedlist.h"

#define J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_IS_EMPTY(root) \
	J9_LINEAR_LINKED_LIST_IS_EMPTY(root)

#define J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_IS_TAIL(root, segment) \
	J9_LINEAR_LINKED_LIST_IS_TAIL(nextSegment, previousSegment, root, segment)
	
#define J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_START_DO(root) \
	J9_LINEAR_LINKED_LIST_START_DO(root)

#define J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_NEXT_DO(root, segment) \
	J9_LINEAR_LINKED_LIST_NEXT_DO(nextSegment, previousSegment, root, segment)
	
#define J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_ADD(root, segment) \
	J9_LINEAR_LINKED_LIST_ADD(nextSegment, previousSegment, root, segment)

#define J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_ADD_BEFORE(root, before, segment) \
	J9_LINEAR_LINKED_LIST_ADD_BEFORE(nextSegment, previousSegment, root, before, segment)

#define J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_ADD_AFTER(root, after, segment) \
	J9_LINEAR_LINKED_LIST_ADD_AFTER(nextSegment, previousSegment, root, after, segment)

#define J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_REMOVE(root, segment) \
	J9_LINEAR_LINKED_LIST_REMOVE(nextSegment, previousSegment, root, segment)

#endif /*SEGMENT_H_*/
