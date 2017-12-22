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
 * @ingroup GC_Check
 */

#if !defined(CHECKFINALIZABLELIST_HPP_)
#define CHECKFINALIZABLELIST_HPP_

#include "j9.h"
#include "j9cfg.h"

#if defined(J9VM_GC_FINALIZATION)

#include "Check.hpp"

/**
 * 
 */
class GC_CheckFinalizableList : public GC_Check
{
private:
	virtual void check(); /**< run the check */
	virtual void print(); /**< dump the check structure to tty */

	j9object_t peekSystemFinalizableObject(GC_FinalizeListManager *finalizeListManager);
	j9object_t peekNextSystemFinalizableObject(GC_FinalizeListManager *finalizeListManager, j9object_t current);
	j9object_t peekDefaultFinalizableObject(GC_FinalizeListManager *finalizeListManager);
	j9object_t peekNextDefaultFinalizableObject(GC_FinalizeListManager *finalizeListManager, j9object_t current);

public:
	static GC_Check *newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine);
	virtual void kill();

	virtual const char *getCheckName() { return "FINALIZABLE"; };

	GC_CheckFinalizableList(J9JavaVM *javaVM, GC_CheckEngine *engine) :
		GC_Check(javaVM, engine)
	{}
};

#endif /* J9VM_GC_FINALIZATION */

#endif /* CHECKFINALIZABLELIST_HPP_ */
