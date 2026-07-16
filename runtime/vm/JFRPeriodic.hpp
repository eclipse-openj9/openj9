/*******************************************************************************
 * Copyright IBM Corp. and others 2026
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
#if !defined(JFRPERIODIC_HPP_)
#define JFRPERIODIC_HPP_

#include "JFRTypeMappings.hpp"
#include "j9protos.h"

class JfrPeriodicEventSet {
public:
	static jboolean requestEvent(J9VMThread *currentThread, jlong id);

private:
	static void requestClassLoaderStatistics(J9VMThread *currentThread);
	static void requestClassLoadingStatistics(J9VMThread *currentThread);
	static void requestCPUInformation(J9VMThread *currentThread);
	static void requestCPULoad(J9VMThread *currentThread);
	static void requestExecutionSample(J9VMThread *currentThread);
	static void requestGCHeapConfiguration(J9VMThread *currentThread);
	static void requestInitialEnvironmentVariable(J9VMThread *currentThread);
	static void requestInitialSystemProperty(J9VMThread *currentThread);
	static void requestJavaThreadStatistics(J9VMThread *currentThread);
	static void requestJVMInformation(J9VMThread *currentThread);
	static void requestModuleExport(J9VMThread *currentThread);
	static void requestModuleRequire(J9VMThread *currentThread);
	static void requestNativeLibrary(J9VMThread *currentThread);
	static void requestNetworkUtilization(J9VMThread *currentThread);
	static void requestOSInformation(J9VMThread *currentThread);
	static void requestPhysicalMemory(J9VMThread *currentThread);
	static void requestSystemProcess(J9VMThread *currentThread);
	static void requestThreadAllocation(J9VMThread *currentThread);
	static void requestThreadContextSwitchRate(J9VMThread *currentThread);
	static void requestThreadCPULoad(J9VMThread *currentThread);
	static void requestThreadDump(J9VMThread *currentThread);
	static void requestVirtualizationInformation(J9VMThread *currentThread);
	static void requestYoungGenerationConfiguration(J9VMThread *currentThread);
	static void requestGCSurvivorConfiguration(J9VMThread *currentThread);
};

#endif /* !defined(JFRPERIODIC_HPP_) */
