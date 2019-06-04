################################################################################
# Copyright (c) 2018, 2019 IBM Corp. and others
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

add_library(jvmti_test_src STATIC
	com/ibm/jvmti/tests/addToBootstrapClassLoaderSearch/abcl001.c
	com/ibm/jvmti/tests/addToBootstrapClassLoaderSearch/abcl002.c
	com/ibm/jvmti/tests/addToBootstrapClassLoaderSearch/abcl003.c

	com/ibm/jvmti/tests/addToSystemClassLoaderSearch/ascl001.c
	com/ibm/jvmti/tests/addToSystemClassLoaderSearch/ascl002.c
	com/ibm/jvmti/tests/addToSystemClassLoaderSearch/ascl003.c

	com/ibm/jvmti/tests/agentLibraryNatives/aln001.c

	com/ibm/jvmti/tests/attachOptionsTest/att001.c

	com/ibm/jvmti/tests/BCIWithASM/ta001.c

	com/ibm/jvmti/tests/classModificationAgent/cma001.c

	com/ibm/jvmti/tests/decompResolveFrame/decomp001.c
	com/ibm/jvmti/tests/decompResolveFrame/decomp002.c
	com/ibm/jvmti/tests/decompResolveFrame/decomp003.c
	com/ibm/jvmti/tests/decompResolveFrame/decomp004.c

	com/ibm/jvmti/tests/eventClassFileLoadHook/ecflh001.c

	com/ibm/jvmti/tests/eventMethodEntryGrow/emeng001.c

	com/ibm/jvmti/tests/eventMethodExit/emex001.c

	com/ibm/jvmti/tests/eventThreadStart/ets001.c

	com/ibm/jvmti/tests/eventVMObjectAllocation/evmoa001.c

	com/ibm/jvmti/tests/followReferences/fr001.c
	com/ibm/jvmti/tests/followReferences/fr002.c
	com/ibm/jvmti/tests/followReferences/fr003.c
	com/ibm/jvmti/tests/followReferences/fr004.c
	com/ibm/jvmti/tests/followReferences/tag_manager.c

	com/ibm/jvmti/tests/forceEarlyReturn/fer001.c
	com/ibm/jvmti/tests/forceEarlyReturn/fer002.c
	com/ibm/jvmti/tests/forceEarlyReturn/fer003.c

	com/ibm/jvmti/tests/getAllStackTracesExtended/gaste001.c

	com/ibm/jvmti/tests/getClassFields/gcf001.c

	com/ibm/jvmti/tests/getClassVersionNumbers/gcvn001.c

	com/ibm/jvmti/tests/getConstantPool/gcp001.c
	com/ibm/jvmti/tests/getConstantPool/gcp_bcdump.c

	com/ibm/jvmti/tests/getCurrentThreadCpuTimerInfo/gctcti001.c

	com/ibm/jvmti/tests/getHeapFreeTotalMemory/ghftm001.c

	com/ibm/jvmti/tests/getJ9method/gj9m001.c

	com/ibm/jvmti/tests/getJ9vmThread/gjvmt001.c

	com/ibm/jvmti/tests/getLoadedClasses/glc001.c

	com/ibm/jvmti/tests/getMemoryCategories/gmc001.c

	com/ibm/jvmti/tests/getMethodAndClassNames/gmcpn001.c

	com/ibm/jvmti/tests/getOrSetLocal/gosl001.c

	com/ibm/jvmti/tests/getOwnedMonitorInfo/gomi001.c
	com/ibm/jvmti/tests/getOwnedMonitorInfo/gomi002.c

	com/ibm/jvmti/tests/getOwnedMonitorStackDepthInfo/gomsdi001.c
	com/ibm/jvmti/tests/getOwnedMonitorStackDepthInfo/gomsdi002.c

	com/ibm/jvmti/tests/getPotentialCapabilities/gpc001.c
	com/ibm/jvmti/tests/getPotentialCapabilities/gpc002.c

	com/ibm/jvmti/tests/getStackTrace/gst001.c
	com/ibm/jvmti/tests/getStackTrace/gst002.c

	com/ibm/jvmti/tests/getStackTraceExtended/gste001.c

	com/ibm/jvmti/tests/getThreadGroupChildren/gtgc001.c
	com/ibm/jvmti/tests/getThreadGroupChildren/gtgc002.c

	com/ibm/jvmti/tests/getThreadListStackTracesExtended/gtlste001.c

	com/ibm/jvmti/tests/getThreadState/gts001.c

	com/ibm/jvmti/tests/iterateOverHeap/ioh001.c

	com/ibm/jvmti/tests/iterateOverInstancesOfClass/ioioc001.c

	com/ibm/jvmti/tests/iterateThroughHeap/ith001.c

	com/ibm/jvmti/tests/javaLockMonitoring/jlm001.c

	com/ibm/jvmti/tests/log/log001.c

	com/ibm/jvmti/tests/modularityTests/mt001.c

	com/ibm/jvmti/tests/nestMatesRedefinition/nmr001.c

	com/ibm/jvmti/tests/redefineBreakpointCombo/rbc001.c

	com/ibm/jvmti/tests/redefineClasses/rc001.c
	com/ibm/jvmti/tests/redefineClasses/rc002.c
	com/ibm/jvmti/tests/redefineClasses/rc003.c
	com/ibm/jvmti/tests/redefineClasses/rc004.c
	com/ibm/jvmti/tests/redefineClasses/rc005.c
	com/ibm/jvmti/tests/redefineClasses/rc006.c
	com/ibm/jvmti/tests/redefineClasses/rc007.c
	com/ibm/jvmti/tests/redefineClasses/rc008.c
	com/ibm/jvmti/tests/redefineClasses/rc009.c
	com/ibm/jvmti/tests/redefineClasses/rc010.c
	com/ibm/jvmti/tests/redefineClasses/rc011.c
	com/ibm/jvmti/tests/redefineClasses/rc012.c
	com/ibm/jvmti/tests/redefineClasses/rc013.c
	com/ibm/jvmti/tests/redefineClasses/rc014.c
	com/ibm/jvmti/tests/redefineClasses/rc015.c
	com/ibm/jvmti/tests/redefineClasses/rc016.c
	com/ibm/jvmti/tests/redefineClasses/rc017.c
	com/ibm/jvmti/tests/redefineClasses/rc018.c
	com/ibm/jvmti/tests/redefineClasses/rc019a.c
	com/ibm/jvmti/tests/redefineClasses/rc019b.c

	com/ibm/jvmti/tests/registerNativesWithRetransformation/rnwr001.c

	com/ibm/jvmti/tests/removeAllTags/rat001.c

	com/ibm/jvmti/tests/resourceExhausted/re001.c

	com/ibm/jvmti/tests/retransformationCapableAgent/rca001.c

	com/ibm/jvmti/tests/retransformationIncapableAgent/ria001.c

	com/ibm/jvmti/tests/retransformClasses/rtc001.c

	com/ibm/jvmti/tests/retransformRedefineCombo/rrc001.c

	com/ibm/jvmti/tests/sharedCacheAPI/sca001.c

	com/ibm/jvmti/tests/traceSubscription/ts001.c
	com/ibm/jvmti/tests/traceSubscription/ts002.c

	com/ibm/jvmti/tests/verboseGC/vgc001.c

	com/ibm/jvmti/tests/vmDump/vmd001.c

	com/ibm/jvmti/tests/setNativeMethodPrefix/snmp001.c
	
	com/ibm/jvmti/tests/samplingObjectAllocation/soae001.c
	
	com/ibm/jvmti/tests/getSystemProperty/gsp001.c

	com/ibm/jvmti/tests/eventException/ee001.c
)


target_link_libraries(jvmti_test_src
	PRIVATE
		j9vm_interface
		j9vm_jvmtitest_includes
		jvmti_test_agent
)
