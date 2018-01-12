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

import java.io.PrintStream;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.IBootstrapRunnable;
import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;


public class GCCheckRunner implements IBootstrapRunnable
{
	@SuppressWarnings("unchecked")
	public void run(IVMData vmData, Object[] userData)
	{	
		List<String> argList = ((List<String>)userData[0]);
		String[] args = argList.toArray(new String[argList.size()]);
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			run(vm, args, System.out);
		} catch (CorruptDataException e) {
			e.printStackTrace();
		}
	}
	
	public static void run(J9JavaVMPointer vm, String[] args, PrintStream out) throws CorruptDataException
	{
		String options = args.length > 0 ? args[0] : "";
		
		CheckReporter reporter = new CheckReporterTTY(out);
		CheckEngine engine = new CheckEngine(vm, reporter);
		CheckCycle cycle = new CheckCycle(vm, engine, options);
		
		out.println("Starting GC Check");
		long startTime = System.currentTimeMillis();
		cycle.run();
		long endTime = System.currentTimeMillis();
		out.println("Done (" + (endTime - startTime) + "ms)");

//		AbstractPointer.reportClassCacheStats();
//		J9ObjectHelper.reportClassCacheStats();
	}

}
