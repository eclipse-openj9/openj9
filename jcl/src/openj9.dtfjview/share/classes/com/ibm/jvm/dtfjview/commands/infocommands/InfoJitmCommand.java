/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands.infocommands;

import java.io.PrintStream;
import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;
import com.ibm.jvm.dtfjview.commands.helpers.Exceptions;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*", runtime=false)
public class InfoJitmCommand extends BaseJdmpviewCommand {
	
	{
		addCommand("info jitm", "", "Displays JIT'ed methods and their addresses");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length != 0) {
			out.println("\"info jitm\" command does not take any parameters");
			return;
		}
		showJITdMethods();
	}
	
	private void showJITdMethods() {
		JavaRuntime jr = ctx.getRuntime();
		Iterator itJavaClassLoader = jr.getJavaClassLoaders();
		while (itJavaClassLoader.hasNext())
		{
			JavaClassLoader jcl = (JavaClassLoader)itJavaClassLoader.next();
			Iterator itJavaClass = jcl.getDefinedClasses();
			
			while (itJavaClass.hasNext())
			{
				JavaClass jc = (JavaClass)itJavaClass.next();
				Iterator itJavaMethod = jc.getDeclaredMethods();
				
				String jcName;
				try {
					jcName = jc.getName();
				} catch (CorruptDataException e) {
					jcName = Exceptions.getCorruptDataExceptionString();
				}
				
				while (itJavaMethod.hasNext())
				{
					JavaMethod jm = (JavaMethod)itJavaMethod.next();
					String name;
					String sig;
					
					try {
						name = jm.getName();
					} catch (CorruptDataException e) {
						name = Exceptions.getCorruptDataExceptionString();
					}
					
					try {
						sig = jm.getSignature();
					} catch (CorruptDataException e) {
						sig = Exceptions.getCorruptDataExceptionString();
					}

					if (jm.getCompiledSections().hasNext())
					{
						Iterator itImageSection = jm.getCompiledSections();
						while (itImageSection.hasNext())
						{
							ImageSection is = (ImageSection)itImageSection.next();
							long startAddr = is.getBaseAddress().getAddress();
							long size = is.getSize();
							long endAddr = startAddr + size;
							
							out.print("\n\t" + "start=" + Utils.toHex(startAddr) +
									"  " + "end=" + Utils.toHex(endAddr) +
									"   " + jcName + "::" + name + sig);
						}
					}
				}
			}
		}
		out.print("\n");
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("displays JIT'ed methods and their addresses\n\n" +
		"parameters: none\n\n" +
		"prints the following information about each JIT'ed method\n\n" +
		"  - method name and signature\n" +
		"  - method start address\n" +
		"  - method end address\n"
);
		
	}
}
