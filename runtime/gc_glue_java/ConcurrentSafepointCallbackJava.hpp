/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#if !defined(CONCURRENTSAFEPOINTCALLBACKJAVA_HPP_)
#define CONCURRENTSAFEPOINTCALLBACKJAVA_HPP_

#include "j9cfg.h"

#if defined (OMR_GC_MODRON_CONCURRENT_MARK)

#include "j9.h"
#include "vmhook.h"
#include "j9comp.h"
#include "ConcurrentSafepointCallback.hpp"
#include "EnvironmentStandard.hpp"

/**
 * @todo Provide typedef documentation
 * @ingroup GC_Modron_Standard
 */

class MM_ConcurrentSafepointCallbackJava : public MM_ConcurrentSafepointCallback
{
private:
	intptr_t _asyncEventKey;
#if defined(AIXPPC) || defined(LINUXPPC)
	bool _cancelAfterGC;
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
protected:
public:

private:
	bool initialize(MM_EnvironmentBase *env);
	static void vmInitialized(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
	static void vmTerminating(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
	static void reportGlobalGCComplete(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
	static void registerAsyncEventHandler(MM_EnvironmentBase *env, MM_ConcurrentSafepointCallbackJava *callback);
	static void asyncEventHandler(J9VMThread *vmThread, intptr_t handlerKey, void *userData);

	void globalGCComplete(MM_EnvironmentBase *env);

protected:
public:
#if defined(AIXPPC) || defined(LINUXPPC)
	virtual void registerCallback(MM_EnvironmentBase *env, SafepointCallbackHandler handler, void *userData, bool cancelAfterGC = false);
#else
	virtual void registerCallback(MM_EnvironmentBase *env, SafepointCallbackHandler handler, void *userData);
#endif /* defined(AIXPPC) || defined(LINUXPPC) */

	virtual void cancelCallback(MM_EnvironmentBase *env);

	virtual void requestCallback(MM_EnvironmentBase *env);

	static MM_ConcurrentSafepointCallbackJava *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	/**
	 * Create a MM_ConcurrentSafepointCallbackJava object
	 */
	MM_ConcurrentSafepointCallbackJava(MM_EnvironmentBase *env)
		: MM_ConcurrentSafepointCallback(env)
		,_asyncEventKey(-1)
#if defined(AIXPPC) || defined(LINUXPPC)
		,_cancelAfterGC(false)
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* defined (OMR_GC_MODRON_CONCURRENT_MARK)*/

#endif /* CONCURRENTSAFEPOINTCALLBACKJAVA_HPP_ */
