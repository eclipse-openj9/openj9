 
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

/**
 * @file
 * @ingroup GC_Metronome
 */

#if !defined(OSINTERFACE_HPP_)
#define OSINTERFACE_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "Base.hpp"
#include "GCExtensions.hpp"

class MM_EnvironmentBase;
class MM_OSInterface;
class MM_ProcessorInfo;

#if defined(WIN32)
#define SCHED_RR 0
#define SCHED_FIFO 0
#endif /* WIN32 */


		
/**
 * @todo Provide class documentation
 * @ingroup GC_Metronome
 */
class MM_OSInterface : public MM_BaseVirtual
{
/* Data members / types */
public:
	MM_GCExtensions *_extensions;
	U_64 _j9time_hires_clock_nanoSecondMultiplyFactor;  /**< The results of j9time_hires_clock must be multiplied by this value, if non-zero, to obtain time in micro-seconds. */
	U_64 _j9time_hires_clock_nanoSecondDivideFactor;  /**< If the MultiplyFactor is zero, the results of j9time_hires_clock must be divided by this value, to obtain time in micro-seconds. */
	I_64 _j9time_hires_clock_nanoSecondOffset;  /**< After multiplying by the above factor, subtract the offset to obtain time relative to machine boot time. */
	U_64 _ticksPerMicroSecond;
protected:
private:
	J9JavaVM *_vm;
	UDATA _numProcessors;
	U_64 _physicalMemoryBytes;
	MM_ProcessorInfo *_processorInfo;
	char _hostname[256];
	char *_ipAddrString;
	
/* Methods */
public:
	static MM_OSInterface *newInstance(MM_EnvironmentBase *env);
	bool initialize(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
	void startup();
	bool changePriority(IDATA policy, IDATA priority);
	IDATA minPriority();
	IDATA maxPriority();
	static UDATA getTid();
	U_64 nanoTime();
	void maskSignals();
	bool hiresTimerAvailable();
	bool rtcTimerAvailable();
	bool itTimerAvailable();
	UDATA getNumbersOfProcessors() {return _numProcessors;}
	UDATA getParameter(UDATA which, char *keyBuffer, I_32 keyBufferSize, char *valueBuffer, I_32 valueBufferSize);
	
	MM_OSInterface() :
		_processorInfo(NULL),
		_ipAddrString(NULL)
	{
		_typeId = __FUNCTION__;
	}
protected:
private:
};


#endif /* OSINTERFACE_HPP_ */
