################################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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

add_library(j9gcbase STATIC
	accessBarrier.cpp
	AsyncCallbackHandler.cpp
	ClassLoaderLinkedListIterator.cpp
	ClassLoaderManager.cpp
	FinalizeListManager.cpp
	FinalizerSupport.cpp
	GCExtensions.cpp
	GCObjectEvents.cpp
	GenerationalAccessBarrierComponent.cpp
	IdleGCManager.cpp
	IndexableObjectAllocationModel.cpp
	modronapi.cpp
	ObjectAccessBarrier.cpp
	ObjectCheck.cpp
	OwnableSynchronizerObjectBuffer.cpp
	OwnableSynchronizerObjectList.cpp
	PacketSlotIterator.cpp
	QueryGCStatus.cpp
	ReferenceChainWalker.cpp
	ReferenceObjectBuffer.cpp
	ReferenceObjectList.cpp
	RootScanner.cpp
	ScavengerForwardedHeader.cpp
	StackSlotValidator.cpp
	StringTable.cpp
	UnfinalizedObjectBuffer.cpp
	UnfinalizedObjectList.cpp
	VMInterface.cpp
	VMInterfaceAPI.cpp
	VMThreadInterface.cpp
	Wildcard.cpp
	WorkPacketsIterator.cpp
)

target_link_libraries(j9gcbase
	PRIVATE
		j9vm_interface
		j9vm_gc_includes
		omrgc
)

