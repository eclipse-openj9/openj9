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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.regex.Pattern;

import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;

/**
 * Runs DDR extension !buildflags command.
 * 
 * The build flags command will print all of the build flags used to compile the jvm.  Optionally
 * it will print all buildflags which match a pattern.
 *
 */
public class BuildFlagsCommand extends Command 
{

	/**
	 * Create a new instance of the BuildFlagsCommand
	 */
	public BuildFlagsCommand()
	{
		addCommand("buildflags", "[regex]", "Prints all or matching buildflags and their values.");
	}

	/**
	 * Run method for !j9buildflags extension.
	 * 
	 * @param command !j9buildflags
	 * @param args	Args passed by !j9buildflags extension. 
	 * @param context Context of current core file.
	 * @param out PrintStream to print the output to.
	 * @throws DDRInteractiveCommandException
	 */
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		Pattern pattern = null;
		try {
			if (0 == args.length) {
				pattern = null;
			} else if (args.length >= 1) {
				String regex = args[0];
				/* if regex is not well formed, this will generate an exception */
				pattern = Pattern.compile(regex);
				pattern = Pattern.compile("(?i).*(" + regex + ").*");
			}

			Field[] fields = J9BuildFlags.class.getDeclaredFields();
			for (int i = 0; i < fields.length; i++) {
				Field field = fields[i];
				if (Modifier.isStatic(field.getModifiers()) && field.getType() == boolean.class) {
					String fieldName = field.getName();
					if (null != pattern) {
						if (pattern.matcher(fieldName).matches()) {
							out.println(fieldName + " = " + field.getBoolean(null));
						}
					} else {
						out.println(fieldName + " = " + field.getBoolean(null));
					}
				}
			}
		} catch (Exception e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
