################################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
################################################################################

add_library(j9gcvrbevents STATIC
	VerboseEvent.cpp
	VerboseEventAFEnd.cpp
	VerboseEventClassUnloadingEnd.cpp
	VerboseEventClassUnloadingStart.cpp
	VerboseEventCompactEnd.cpp
	VerboseEventCompactStart.cpp
	VerboseEventCompletedConcurrentSweep.cpp
	VerboseEventConcurrentAborted.cpp
	VerboseEventConcurrentCompleteTracingEnd.cpp
	VerboseEventConcurrentCompleteTracingStart.cpp
	VerboseEventConcurrentEnd.cpp
	VerboseEventConcurrentFinalCardCleaningEnd.cpp
	VerboseEventConcurrentFinalCardCleaningStart.cpp
	VerboseEventConcurrentHalted.cpp
	VerboseEventConcurrentKickOff.cpp
	VerboseEventConcurrentlyCompletedSweepPhase.cpp
	VerboseEventConcurrentRSScanEnd.cpp
	VerboseEventConcurrentRSScanStart.cpp
	VerboseEventConcurrentStart.cpp
	VerboseEventCopyForwardAbortRaised.cpp
	VerboseEvent.cpp
	VerboseEventAFStart.cpp
	VerboseEventExcessiveGCRaised.cpp
	VerboseEventGCEnd.cpp
	VerboseEventGCInitialized.cpp
	VerboseEventGCStart.cpp
	VerboseEventGlobalGCEnd.cpp
	VerboseEventGlobalGCStart.cpp
	VerboseEventHeapResize.cpp
	VerboseEventLocalGCEnd.cpp
	VerboseEventLocalGCStart.cpp
	VerboseEventMarkEnd.cpp
	VerboseEventMarkStart.cpp
	VerboseEventMetronomeCycleEnd.cpp
	VerboseEventMetronomeCycleStart.cpp
	VerboseEventMetronomeGCEnd.cpp
	VerboseEventMetronomeGCStart.cpp
	VerboseEventMetronomeNonMonotonicTime.cpp
	VerboseEventMetronomeOutOfMemory.cpp
	VerboseEventMetronomeSynchronousGCEnd.cpp
	VerboseEventMetronomeSynchronousGCStart.cpp
	VerboseEventMetronomeTriggerEnd.cpp
	VerboseEventMetronomeTriggerStart.cpp
	VerboseEventMetronomeUtilizationTrackerOverflow.cpp
	VerboseEventPercolateCollect.cpp
	VerboseEventSweepEnd.cpp
	VerboseEventSweepStart.cpp
	VerboseEventSystemGCEnd.cpp
	VerboseEventSystemGCStart.cpp
	VerboseEventTarokIncrementEnd.cpp
	VerboseEventTarokIncrementStart.cpp
)

target_include_directories(j9gcvrbevents
	PRIVATE
		${j9vm_SOURCE_DIR}/gc_verbose_old
)
target_link_libraries(j9gcvrbevents
	PRIVATE
		j9vm_interface

		omrgc
)
