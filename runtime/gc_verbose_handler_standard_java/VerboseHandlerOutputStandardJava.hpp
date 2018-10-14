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

#if !defined(VERBOSEHANDLEROUTPUTSTANDARDJAVA_HPP_)
#define VERBOSEHANDLEROUTPUTSTANDARDJAVA_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "VerboseHandlerOutputStandard.hpp"

class MM_VerboseHandlerOutputStandardJava : public MM_VerboseHandlerOutputStandard
{
private:
protected:
	J9HookInterface** _mmHooks;  /**< Pointers to the mm Hook interface */
	J9HookInterface **_vmHooks;  /**< Pointers to the vm Hook interface */
public:

private:
	/**
	 * Output unfinalized processing summary.
	 * @param env GC thread used for output.
	 * @param indent base level of indentation for the summary.
	 * @param unfinalizedCandidates number of unfinalized candidates encountered.
	 * @param unfinalizedCount number of candidates that transitioned from unfinalized.
	 */
	void outputUnfinalizedInfo(MM_EnvironmentBase *env, UDATA indent, UDATA unfinalizedCandidates, UDATA unfinalizedEnqueuedCount);

	/**
	 * Output ownable synchronizer processing summary.
	 * @param env GC thread used for output.
	 * @param indent base level of indentation for the summary.
	 * @param ownableSynchronizerCandidates number of ownable synchronizers encountered.
	 * @param ownableSynchronizerCleared number of ownable synchronizers cleared.
	 */
	void outputOwnableSynchronizerInfo(MM_EnvironmentBase *env, UDATA indent, UDATA ownableSynchronizerCandidates, UDATA ownableSynchronizerCleared);

	/**
	 * Output reference processing summary.
	 * @param env GC thread used for output.
	 * @param indent base level of indentation for the summary.
	 * @param referenceType character string representation of the reference type.
	 * @param referenceStats summary stats data of the processing.
	 * @param dynamicThreshold dynamic threshold value for reference
	 * @param maxThreshold maximum threshold value for reference. If it equal 0 thresholds should not be included to output line
	 */
	void outputReferenceInfo(MM_EnvironmentBase *env, UDATA indent, const char *referenceType, MM_ReferenceStats *referenceStats, UDATA dynamicThreshold, UDATA maxThreshold);

protected:

	virtual bool initialize(MM_EnvironmentBase *env, MM_VerboseManager *manager);
	virtual void tearDown(MM_EnvironmentBase *env);

	virtual void outputMemoryInfoInnerStanzaInternal(MM_EnvironmentBase *env, UDATA indent, MM_CollectionStatistics *stats);

	virtual bool getThreadName(char *buf, UDATA bufLen, OMR_VMThread *vmThread);
	virtual void writeVmArgs(MM_EnvironmentBase* env);

	MM_VerboseHandlerOutputStandardJava(MM_GCExtensions *extensions) :
		MM_VerboseHandlerOutputStandard(extensions)
		, _mmHooks(NULL)
		, _vmHooks(NULL)
	{};

public:
	static MM_VerboseHandlerOutput *newInstance(MM_EnvironmentBase *env, MM_VerboseManager *manager);

	virtual void enableVerbose();
	virtual void disableVerbose();

	/* Java-specific logic for selected GC events. */
	virtual void handleMarkEndInternal(MM_EnvironmentBase* env, void *eventData);
#if defined(J9VM_GC_MODRON_SCAVENGER)
	virtual void handleScavengeEndInternal(MM_EnvironmentBase* env, void *eventData);
#endif /*defined(J9VM_GC_MODRON_SCAVENGER) */
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	virtual const char* getConcurrentKickoffReason(void *eventData);
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	virtual void handleConcurrentHaltedInternal(MM_EnvironmentBase *env, void *eventData);
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/**
	 * Write verbose stanza for a compact end event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleClassUnloadEnd(J9HookInterface** hook, UDATA eventNum, void* eventData);
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

	/**
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleSlowExclusive(J9HookInterface **hook, UDATA eventNum, void *eventData);
};

#endif /* VERBOSEHANDLEROUTPUTSTANDARDJAVA_HPP_ */
