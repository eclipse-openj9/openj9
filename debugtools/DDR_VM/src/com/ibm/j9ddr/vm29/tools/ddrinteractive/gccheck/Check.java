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

import com.ibm.j9ddr.vm29.j9.gc.GCExtensions;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;

abstract class Check
{
	protected CheckEngine _engine;
	protected J9JavaVMPointer _javaVM;
	protected MM_GCExtensionsPointer _extensions;
	
	public Check()
	{
	}
	
	public abstract String getCheckName();
	public abstract void check();
	public abstract void print();
	
	protected CheckReporter getReporter()
	{
		return _engine.getReporter();
	}
	
	public void run(boolean shouldCheck, boolean shouldPrint)
	{
		getReporter().print("Checking " + getCheckName() + "...");
		
		_engine.startNewCheck(this);
		
		long startTime = System.currentTimeMillis();
		if(shouldCheck) {
			check();
		}
		long endTime = System.currentTimeMillis();
		
		if(shouldPrint) {
			print();
		}
		
		getReporter().println("done (" + (endTime - startTime) + " ms).");
	}

	public void initialize(CheckEngine engine)
	{
		_engine = engine;
		_javaVM = engine.getJavaVM();
		_extensions = GCExtensions.getGCExtensionsPointer();
	}
	
	public J9JavaVMPointer getJavaVM() 
	{
		return _javaVM;
	}

	public MM_GCExtensionsPointer getGCExtensions() 
	{
		return _extensions;
	}

}
