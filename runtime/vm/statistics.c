/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#include	"j9.h"
#include	"j9port.h"


void *
addStatistic (J9JavaVM* javaVM, U_8 * name, U_8 dataType)
{
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	IDATA stringSize = strlen((char*)name) + 1;
	IDATA size = stringSize;
	J9Statistic * statistic;

	/* Adjust for the default size in the struct array of 4 */
	size += offsetof(J9Statistic, name);

	/* Allocate and fill the structure */
	statistic = j9mem_allocate_memory(size, OMRMEM_CATEGORY_VM);
	if (statistic != NULL) {
		statistic->dataSlot = 0;
		statistic->dataType = (U_32) dataType;
		strcpy((char*)&(statistic->name), (char*)name);

		/* Grab the monitor - if initialized (some statistics set prior to vmthinit.c) */
		if (javaVM->statisticsMutex) {
			omrthread_monitor_enter(javaVM->statisticsMutex);
		}

		/* Connect the new statistic */
		statistic->nextStatistic = javaVM->nextStatistic;
		javaVM->nextStatistic = statistic;

		/* Release the monitor */
		if (javaVM->statisticsMutex) {
			omrthread_monitor_exit(javaVM->statisticsMutex);
		}
	}

	return (void *) statistic;
}


void
deleteStatistics (J9JavaVM* javaVM)
{
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	J9Statistic * statistic;

	if (javaVM->statisticsMutex) {
		omrthread_monitor_enter(javaVM->statisticsMutex);
	}

	statistic = javaVM->nextStatistic;

	/* Delete the linked list */
	while (statistic != NULL) {
		J9Statistic * nextStatistic = statistic->nextStatistic;
		j9mem_free_memory(statistic);
		statistic = nextStatistic;
	}
	javaVM->nextStatistic = NULL;

	if (javaVM->statisticsMutex) {
		omrthread_monitor_exit(javaVM->statisticsMutex);
	}
}


void *
getStatistic (J9JavaVM* javaVM, U_8 * name)
{
	J9Statistic * statistic;
	
	if (javaVM->statisticsMutex) {
		omrthread_monitor_enter(javaVM->statisticsMutex);
	}
	
	statistic = javaVM->nextStatistic;

	/* Walk the linked list */
	while (statistic != NULL) {
		if (strcmp((char*)name, (char*)&(statistic->name)) == 0) {
			break;
		}
		statistic = statistic->nextStatistic;
	}

	if (javaVM->statisticsMutex) {
		omrthread_monitor_exit(javaVM->statisticsMutex);
	}

	return (void *) statistic;
}

