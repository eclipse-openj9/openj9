/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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
import java.util.Iterator;
import java.util.HashMap;
import java.util.Map;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.gc.GCClassLoaderIterator;
import com.ibm.j9ddr.vm29.j9.walkers.ClassIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMNameAndSignaturePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;

public class MethodForNameCommand extends Command
{
	public MethodForNameCommand()
	{
		addCommand("methodforname", "<name>", "find the method corresponding to name (with wildcards)");
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		try {
			if (args.length == 1) {
				String name = args[0];
				CommandUtils.dbgPrint(out, String.format("Searching for methods named '%s' in VM=%s...\n", name, J9RASHelper.getVM(DataType.getJ9RASPointer()).getHexAddress()));
				int count = dbgGetMethodsForName(out, name);
				CommandUtils.dbgPrint(out, String.format("Found %d method(s) named %s\n", count, name));
			} else {
				throw new DDRInteractiveCommandException(command + ": too " + (args.length < 1 ? "few" : "many") + " arguments. Expected 1");
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	int dbgGetMethodsForName(PrintStream out, String pattern) throws CorruptDataException {
		int dot = pattern.indexOf('.');
		String classStart = (dot == -1) ? "*" : pattern.substring(0, dot);
		int paren = pattern.indexOf('(', dot + 1);
		String nameStart = (paren == -1) ? pattern.substring(dot + 1) : pattern.substring(dot + 1, paren);
		String sigStart = (paren == -1) ? "*" : pattern.substring(paren);
		StringBuffer needleBuffer;

		needleBuffer = new StringBuffer();
		int classMatchFlags = WildCard.parseWildcard(classStart, needleBuffer);
		String classNeedle = needleBuffer.toString();
		if (classMatchFlags == -1) {
			CommandUtils.dbgError(out, "Invalid wildcards in class name\n");
			return 0;
		}

		needleBuffer = new StringBuffer();
		int methodMatchFlags = WildCard.parseWildcard(nameStart, needleBuffer);
		String methodNeedle = needleBuffer.toString();
		if (methodMatchFlags == -1) {
			CommandUtils.dbgError(out, "Invalid wildcards in method name\n");
			return 0;
		}

		needleBuffer = new StringBuffer();
		int sigMatchFlags = WildCard.parseWildcard(sigStart, needleBuffer);
		String sigNeedle = needleBuffer.toString();
		if (methodMatchFlags == -1) {
			CommandUtils.dbgError(out, "Invalid wildcards in method name\n");
			return 0;
		}

		int matchCount = 0;
		Map<String, J9ClassPointer> loadedClasses = new HashMap<>();

		GCClassLoaderIterator classLoaderIterator = GCClassLoaderIterator.from();
		while (classLoaderIterator.hasNext()) {
			J9ClassLoaderPointer loader = classLoaderIterator.next();
			Iterator<J9ClassPointer> classIterator = ClassIterator.fromJ9Classloader(loader);

			while (classIterator.hasNext()) {
				J9ClassPointer clazz = classIterator.next();
				J9ROMClassPointer romClazz = clazz.romClass();
				String className = J9UTF8Helper.stringValue(romClazz.className());

				if (loadedClasses.containsValue(clazz)) {
					continue;
				}

				loadedClasses.put(clazz.toString(), clazz);

				if (WildCard.wildcardMatch(classMatchFlags, classNeedle, className)) {
					J9MethodPointer methodCursor = clazz.ramMethods();
					for (long count = romClazz.romMethodCount().longValue(); count > 0; count -= 1, methodCursor = methodCursor.add(1)) {
						J9ROMMethodPointer romMethod = J9MethodHelper.romMethod(methodCursor);
						J9ROMNameAndSignaturePointer nameAndSignature = romMethod.nameAndSignature();
						String nameUTF = J9UTF8Helper.stringValue(nameAndSignature.name());
						if (WildCard.wildcardMatch(methodMatchFlags, methodNeedle, nameUTF)) {
							String sigUTF = J9UTF8Helper.stringValue(nameAndSignature.signature());
							if (WildCard.wildcardMatch(sigMatchFlags, sigNeedle, sigUTF)) {
								matchCount += 1;
								CommandUtils.dbgPrint(out, String.format("!j9method %s --> %s.%s%s\n", methodCursor.getHexAddress(), className, nameUTF, sigUTF));
							}
						}
					}
				}
			}
		}

		return matchCount;
	}
}
