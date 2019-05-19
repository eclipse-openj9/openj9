/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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


#include "javaPriority.h"

static void 
initializeRange(int start_index, int end_index, UDATA range_min, UDATA range_max, UDATA values[])
{
	UDATA delta;
	int i;
	UDATA tmpmax;
	UDATA tmpmin;
	UDATA mid;
	int midrange;
	int tailcount;

	 /* give us some room to do some math */
	tmpmax = range_max * 1024;
	tmpmin = range_min * 1024;
	mid = (tmpmin + tmpmax) / 2;
	midrange = (end_index-start_index)/2;

	values[start_index] = range_min; 

	delta = (mid - tmpmin) / midrange;
	for (i = 1; i < midrange; i++) {
	 	values[start_index+midrange-i] = (mid - delta*i)/1024; 
	}

	tailcount = (end_index-start_index)-midrange;
	delta = (tmpmax - mid) / tailcount;
	for (i = 0; i < tailcount; i++) {
	 	values[start_index+midrange+i] = (mid + delta*i)/ 1024;
	}
	values[end_index] = range_max;
}

/*
 * This function initializes the two priority mappings:
 * J9JavaVM.java2J9ThreadPriorityMap
 * J9JavaVM.j9Thread2JavaPriorityMap
 * These mappings convert between the priorities of java threads as assigned to java.lang.Thread objects
 * and omrthread priorities which are managed by the omrthread library.
 */
void initializeJavaPriorityMaps (J9JavaVM *javaVM) {

        int i; 
		UDATA target;
		int javaPriorityMax;

        initializeRange(JAVA_PRIORITY_MIN, JAVA_PRIORITY_MAX, J9THREAD_PRIORITY_USER_MIN, J9THREAD_PRIORITY_USER_MAX, javaVM->java2J9ThreadPriorityMap);

		javaPriorityMax = JAVA_PRIORITY_MAX;

        /* Now we construct the reverse map. */
        /* Initialize all mappings to -1 to indicate they have not been mapped yet */
        for(i=0; i<=J9THREAD_PRIORITY_MAX; i++) {
                javaVM->j9Thread2JavaPriorityMap[i] = -1;
        }

        /* then assign the direct mappings: vm priorities which are direct targets of java priorities */
        for(i=0; i<= javaPriorityMax; i++) {
                javaVM->j9Thread2JavaPriorityMap[javaVM->java2J9ThreadPriorityMap[i]] = i;
        }

        /* now map any unmapped j9 priorities to the first available higher java priority  */
        target = javaPriorityMax;
        for(i=J9THREAD_PRIORITY_MAX; i>=0; i--) {
                UDATA value = javaVM->j9Thread2JavaPriorityMap[i];
                if(value == (UDATA)-1) {
                        javaVM->j9Thread2JavaPriorityMap[i] = target;
                } else {
                        target = value;
                }
        }
}


