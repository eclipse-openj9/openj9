/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
addStatistic(J9JavaVM *javaVM, U_8 *name, U_8 dataType)
{
	J9Statistic *statistic = NULL;

	/* J9Statistic->name[1] accounts +1 for the null character. */
	IDATA size = sizeof(*statistic) + (sizeof(char) * strlen((char *)name));

	PORT_ACCESS_FROM_JAVAVM(javaVM);

	/* Allocate and fill the structure */
	statistic = j9mem_allocate_memory(size, OMRMEM_CATEGORY_VM);
	if (NULL != statistic) {
		statistic->dataSlot = 0;
		statistic->dataType = (U_32) dataType;
		strcpy((char *)statistic->name, (char *)name);

		/* Grab the monitor - if initialized (some statistics set prior to vmthinit.c) */
		if (NULL != javaVM->statisticsMutex) {
			omrthread_monitor_enter(javaVM->statisticsMutex);
		}

		/* Connect the new statistic */
		statistic->nextStatistic = javaVM->nextStatistic;
		javaVM->nextStatistic = statistic;

		/* Release the monitor */
		if (NULL != javaVM->statisticsMutex) {
			omrthread_monitor_exit(javaVM->statisticsMutex);
		}
	}

	return (void *)statistic;
}


void
deleteStatistics(J9JavaVM *javaVM)
{
	J9Statistic *statistic = NULL;

	PORT_ACCESS_FROM_JAVAVM(javaVM);

	if (NULL != javaVM->statisticsMutex) {
		omrthread_monitor_enter(javaVM->statisticsMutex);
	}

	statistic = javaVM->nextStatistic;

	/* Delete the linked list */
	while (NULL != statistic) {
		J9Statistic *nextStatistic = statistic->nextStatistic;
		j9mem_free_memory(statistic);
		statistic = nextStatistic;
	}
	javaVM->nextStatistic = NULL;

	if (NULL != javaVM->statisticsMutex) {
		omrthread_monitor_exit(javaVM->statisticsMutex);
	}
}


void *
getStatistic(J9JavaVM *javaVM, U_8 *name)
{
	J9Statistic *statistic = NULL;
	
	if (NULL != javaVM->statisticsMutex) {
		omrthread_monitor_enter(javaVM->statisticsMutex);
	}
	
	statistic = javaVM->nextStatistic;

	/* Walk the linked list */
	while (NULL != statistic) {
		if (strcmp((char *)name, (char *)statistic->name) == 0) {
			break;
		}
		statistic = statistic->nextStatistic;
	}

	if (NULL != javaVM->statisticsMutex) {
		omrthread_monitor_exit(javaVM->statisticsMutex);
	}

	return (void *)statistic;
}

