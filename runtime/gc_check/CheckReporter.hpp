
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

#if !defined(CHECKREPORTER_HPP_)
#define CHECKREPORTER_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "Base.hpp"
#include "CheckError.hpp"
#include "CheckElement.hpp"

/**
 * Output reports.
 * Accepts an GC_CheckError object and outputs the contents of the error report
 * to the appropriate device.
 * @ingroup GC_Check
 */
class GC_CheckReporter : public MM_Base
{
	/*
	 * Data members
	 */
private:

protected:
	UDATA _maxErrorsToReport;
	J9PortLibrary *_portLibrary;

public:
	J9JavaVM *_javaVM;

	/*
	 * Function members
	 */
private:
protected:
public:

	/**
	 * Release any required resources for the reporter.
	 */
	virtual void kill() = 0;

	/** 
	 * Report an error.
	 * 
	 * Accepts an error object and outputs error to the appropriate device.
	 * @param error The error to be reported
	 */
	virtual void report(GC_CheckError *error) = 0;

	/*
	 * Report information from an object header, class, or some other type
	 */
	virtual void reportGenericType(GC_CheckError *error, GC_CheckElement reference, const char *prefix);

	/**
	 * Report information from an object header.
	 */
	virtual void reportObjectHeader(GC_CheckError *error, J9Object *objectPtr, const char *prefix) = 0;

	/**
	 * Report information from a class
	 */
	virtual void reportClass(GC_CheckError *error, J9Class *clazz, const char *prefix) = 0;

	/**
	 * Report the fact that a fatal error has occurred.
	 */
	virtual void reportFatalError(GC_CheckError *error) = 0;
	
	/**
	 * Report the fact that an error has occurred while walking the heap.
	 */
	virtual void reportHeapWalkError(
		GC_CheckError *error, 
		GC_CheckElement previousObjectPtr1, 
		GC_CheckElement previousObjectPtr2, 
		GC_CheckElement previousObjectPtr3) = 0;

	void setMaxErrorsToReport(UDATA count) { _maxErrorsToReport = count; }
	bool shouldReport(GC_CheckError *error) { 
		return (_maxErrorsToReport == 0) || (error->_errorNumber <= _maxErrorsToReport);
	}

	/**
	 * Create a new CheckReporter object.
	 */
	GC_CheckReporter(J9JavaVM *javaVM)
		: MM_Base()
		, _maxErrorsToReport(0)
		, _portLibrary(javaVM->portLibrary)
		, _javaVM(javaVM)
	{}
};

#endif /* CHECKREPORTER_HPP_ */
