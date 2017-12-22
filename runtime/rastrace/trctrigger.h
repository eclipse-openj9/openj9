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
#ifndef trctrigger_h
#define trctrigger_h

/* @ddr_namespace: default */
#ifdef __cplusplus
extern "C" {
#endif

#include "rastrace_external.h"

#define RAS_TRACE_METHOD_NAME           "RSME"
#define RAS_TRIGGER_METHOD_RULE_NAME    "RSTM"
#define RAS_TRIGGERED_METHOD_BLOCK_NAME "RSMB"

#if 0
#define RAS_DBGOUT(x)                       \
	if (RAS_GLOBAL(debug)){                 \
		fprintf x;                          \
		fflush(stderr);                     \
		} else;
#else
#define RAS_DBGOUT(x)
#endif

/*
 * ======================================================================
 * Structure header
 * ======================================================================
 */
typedef struct RasHeader {
	char eyecatcher[4];
	int length;
} RasHeader;

/*
 * ======================================================================
 *  Trigger Range (for triggering) (RSTP)
 * ======================================================================
 */
typedef struct RasTriggerTpidRange {
	RasHeader header;
	struct RasTriggerTpidRange *next;
	char *compName;
	unsigned int startTpid;
	unsigned int endTpid;
	uint32_t delay;
	int32_t match;
	uintptr_t spinlock;
	uint32_t actionIndex;
} RasTriggerTpidRange;

/*
 * ======================================================================
 *  Group for Triggering (RSGR)
 * ======================================================================
 */
typedef struct RasTriggerGroup {
	RasHeader header;
	struct RasTriggerGroup *next;
	char *groupName;
	uint32_t delay;
	int32_t match;
	uint32_t actionIndex;
} RasTriggerGroup;

void triggerHit(OMR_VMThread *thr, char *compName, uint32_t traceId, TriggerPhase phase);

#ifdef __cplusplus
}
#endif

#endif /* trctrigger_h */
