/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck;

import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;

public abstract class CheckReporter
{
	protected long _maxErrorsToReport;

	/** 
	 * Report an error.
	 * 
	 * Accepts an error object and outputs error to the appropriate device.
	 * @param error The error to be reported
	 */
	public abstract void report(CheckError error);

	/**
	 * Report information from an object header.
	 */
	public abstract void reportObjectHeader(CheckError error, J9ObjectPointer objectPtr, String prefix);

	/**
	 * Report information from a class
	 */
	public abstract void reportClass(CheckError error, J9ClassPointer clazz, String prefix);

	/**
	 * Report the fact that a fatal error has occurred.
	 */
	public abstract void reportFatalError(CheckError error);
	
	/**
	 * Report the fact that an error has occurred while walking the heap.
	 */
	public abstract void reportHeapWalkError(CheckError error, CheckElement previousObjectPtr1, CheckElement previousObjectPtr2, CheckElement previousObjectPtr3);

	/**
	 * Report that a forwarded pointer was encountered when running with "midscavenge".
	 */
	public abstract void reportForwardedObject(J9ObjectPointer object, J9ObjectPointer newObject);
	
	/**
	 * Output non-error information
	 */
	public abstract void print(String arg);
	public abstract void println(String arg);
	public void print()
	{
		print("");
	}
	public void println()
	{
		println("");
	}
	
	public void setMaxErrorsToReport(long count)
	{
		_maxErrorsToReport = count; 
	}
	
	public boolean shouldReport(CheckError error) 
	{ 
		return (_maxErrorsToReport == 0) || (error._errorNumber <= _maxErrorsToReport);
	}
	
	/*
	 * Report information from an object header, class, or some other type
	 */
	public void reportGenericType(CheckError error, CheckElement reference, String prefix)
	{
		if(reference.getObject() != null) {
			reportObjectHeader(error, reference.getObject(), prefix);
		} else if(reference.getClazz() != null) {
			reportClass(error, reference.getClazz(), prefix);
		} else {
			// do nothing
		}
	}
}
