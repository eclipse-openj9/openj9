/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;
import java.util.Properties;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.annotations.DebugExtension;
import com.ibm.j9ddr.vm29.pointer.helper.J9JavaVMHelper;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

public class VersionInfoCommand extends Command{
    
    public VersionInfoCommand() {
		addCommand("versioninfo", "", "Prints out the java version of the JDK");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out)
			throws DDRInteractiveCommandException 
	{
		initialize();
		Object javaFullVersion = properties.get("java.fullversion");
		Object javaRuntimeVersion = properties.get("java.runtime.version");
		Object javaRuntimeName = properties.get("java.runtime.name");
		if (javaRuntimeName == null) {
			javaRuntimeName = "Java(TM) SE Runtime Environment";
		}
		Object javaVMName = properties.get("java.vm.name");
		String version = "";;
		
		if (javaRuntimeVersion != null) {
			version = "" + javaRuntimeName + " (build " + javaRuntimeVersion + ")\n";
		}
		version += javaVMName + "(" + javaFullVersion + ")";
		
		out.println(version);		
	}
	
	private void initialize() throws DDRInteractiveCommandException {
		if (properties == null) {
			try {
				properties = J9JavaVMHelper.getSystemProperties(DTFJContext.getVm());
			} catch (CorruptDataException e) {
				throw new DDRInteractiveCommandException(e);
			}
		}
    }
    
	private static Properties properties = null;
}
