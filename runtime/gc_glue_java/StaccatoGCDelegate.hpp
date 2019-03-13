/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#if !defined(STACCATOGCDELEGATE_HPP_)
#define STACCATOGCDELEGATE_HPP_

#include "j9.h"
#include "j9cfg.h"

#if defined(J9VM_GC_REALTIME)

#include "omr.h"
#include "BaseNonVirtual.hpp"
#include "EnvironmentRealtime.hpp"
#include "RealtimeAccessBarrier.hpp"
#include "RealtimeGCDelegate.hpp"

class MM_StaccatoGC;

class MM_StaccatoGCDelegate : public MM_BaseNonVirtual
{
private:
	J9JavaVM *_vm;
	MM_StaccatoGC *_staccatoGC;

public:
	MM_StaccatoGCDelegate(MM_EnvironmentBase *env) :
		_vm((J9JavaVM *)env->getOmrVM()->_language_vm),
		_staccatoGC(NULL) {}

	virtual MM_RealtimeAccessBarrier* allocateAccessBarrier(MM_EnvironmentBase *env);
	virtual void enableDoubleBarrier(MM_EnvironmentBase* env);
	virtual void disableDoubleBarrierOnThread(MM_EnvironmentBase* env, OMR_VMThread* vmThread);
	virtual void disableDoubleBarrier(MM_EnvironmentBase* env);

	/* New methods */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	bool doClassTracing(MM_EnvironmentRealtime* env);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	virtual bool doTracing(MM_EnvironmentRealtime* env);

	/*
	 * Friends
	 */
	friend class MM_StaccatoGC;
};

#endif /* defined(J9VM_GC_REALTIME) */

#endif /* defined(STACCATOGCDELEGATE_HPP_) */	

