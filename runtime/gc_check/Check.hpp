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

#if !defined(CHECK_HPP_)
#define CHECK_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "Base.hpp"
#include "GCExtensions.hpp"

class GC_CheckEngine;

/**
 * GC_Check - abstract class for defining types of check
 * 
 * An abstract class for defining a type of check gc_check
 * can perform. Actual checks should derive from this then
 * be attached to a "check cycle" that manages all the checks
 * to be performed during one invocation of gc_check
 */
class GC_Check : public MM_Base
{
	/*
	 * Data members
	 */
private:
protected:
	J9JavaVM *_javaVM;
	GC_CheckEngine *_engine;
	MM_GCExtensions *_extensions;
	J9PortLibrary *_portLibrary; 	/**< we use a separate portlibrary (not the one from the javaVM) because this code
										also runs out-of-process, where javaVM is the crashed VM (that doesn't have
										a valid portlibrary) */
										
	GC_Check *_next; /**< pointer to the next check in the list (these objects are chained inside a GC_CheckCycle) */
	UDATA _bitId; /**< identifier of particular Check class that is initialized by appropriate J9MODRON_GCCHK_SCAN_xxx bit */

public:
	
	/*
	 * Function members
	 */
private:
protected:
	virtual void check() = 0; /**< run the check */
	virtual void print() = 0; /**< dump the check structure to tty */

public:
	virtual void kill() = 0;
	
	void setNext(GC_Check *check) { _next = check; }
	GC_Check *getNext(void) { return _next; }
	void setBitId(UDATA bitId) { _bitId = bitId; }
	UDATA getBitId() { return _bitId; }
	
	void run(bool shouldCheck, bool shouldPrint);   /**< run gc_check on the structure */
	virtual const char *getCheckName() = 0; /**< get a string representing this check-type */

	GC_Check(J9JavaVM *javaVM, GC_CheckEngine *engine)
		: MM_Base()
		, _javaVM(javaVM)
		, _engine(engine)
		, _extensions(MM_GCExtensions::getExtensions(javaVM))
		, _portLibrary(javaVM->portLibrary)
		, _next(NULL)
		, _bitId(0)
	{ }
};

#endif /* CHECK_HPP_ */
