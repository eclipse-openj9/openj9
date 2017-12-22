/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.stack;

import java.util.Properties;

import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.javacore.builder.BuilderFailureException;
import com.ibm.dtfj.javacore.builder.IBuilderData;
import com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;
import com.ibm.dtfj.javacore.builder.IJavaRuntimeBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.SectionParser;

/**
 * Provides parser for environment (CI) section in the javacore
 * @throws ParserException
 */
public class StackSectionParser extends SectionParser implements IStackTypes {

	private IImageAddressSpaceBuilder fImageAddressSpaceBuilder;
	private IImageProcessBuilder fImageProcessBuilder;
	
	public StackSectionParser() {
		super(STACK_SECTION);
	}

	/**
	 * Overall controls of parsing for the native stack section
	 * @throws ParserException
	 */
	protected void topLevelRule() throws ParserException {
		
		fImageAddressSpaceBuilder = fImageBuilder.getCurrentAddressSpaceBuilder();
		fImageProcessBuilder = fImageAddressSpaceBuilder.getCurrentImageProcessBuilder();
		
		parseStackLine();
	}
	
	/**
	 * Parse the native stack line information
	 * @throws ParserException
	 */
	private void parseStackLine() throws ParserException {
		IAttributeValueMap results = null;
		
		// Process the version lines
		while ((results = processTagLineOptional(T_BTTHREADID)) != null) {
			long threadID = results.getLongValue(STACK_THREAD);
			while ((results = processTagLineOptional(T_1BTSTACKENT)) != null) {
				String module = results.getTokenValue(STACK_MODULE);
				String routine = results.getTokenValue(STACK_ROUTINE);
				long address = results.getLongValue(STACK_PROC_ADDRESS);
				long routine_address = results.getLongValue(STACK_ROUTINE_ADDRESS);
				long offset = results.getLongValue(STACK_OFFSET);
				String file = results.getTokenValue(STACK_FILE);
				int line = results.getIntValue(STACK_LINE);
				
				// Allow for missing data
				if (routine_address == IBuilderData.NOT_AVAILABLE
					&& address != IBuilderData.NOT_AVAILABLE
					&& offset != IBuilderData.NOT_AVAILABLE) {
					routine_address = address - offset;
				} else if (offset == IBuilderData.NOT_AVAILABLE
						&& address != IBuilderData.NOT_AVAILABLE
						&& routine_address != IBuilderData.NOT_AVAILABLE) {
					offset = address - routine_address;
				} else if (address == IBuilderData.NOT_AVAILABLE
						&& offset != IBuilderData.NOT_AVAILABLE
						&& routine_address != IBuilderData.NOT_AVAILABLE) {
					address = routine_address + offset;
				}
				
				String name;
				if (module != null) {
					name = module;
				} else {
					name = "";
				}
				if (file != null) {
					name = name + "(" + file;
					if (line != IBuilderData.NOT_AVAILABLE) {
						name = name + ":" + line;
					}
					name = name+")";
				}
				if (module != null) {
					ImageModule mod = fImageProcessBuilder.addLibrary(module);
					if (routine != null
							&& address != IBuilderData.NOT_AVAILABLE
							&& offset != IBuilderData.NOT_AVAILABLE
							&& routine_address != IBuilderData.NOT_AVAILABLE) {
						fImageProcessBuilder.addRoutine(mod, routine, routine_address);
						name = name+"::"+routine+(offset >= 0 ? "+" : "-") + offset;
					} else {
						if (offset != IBuilderData.NOT_AVAILABLE) {
							name = name+(offset >= 0 ? "+" : "-") + offset;
						} else {
							if (address != IBuilderData.NOT_AVAILABLE) {
								name = name+"::0x"+Long.toHexString(address);
							}
						}
					}
				} else {
					if (routine != null) {
						if (offset != IBuilderData.NOT_AVAILABLE) {
							name = "::"+routine+(offset >= 0 ? "+" : "-") + offset;
						} else {	
							name = "::"+routine;
						}
					} else {
						if (address != IBuilderData.NOT_AVAILABLE) {
							name = "::0x"+Long.toHexString(address);
						} else {
							name = null;
						}
					}
				}
				fImageProcessBuilder.addImageStackFrame(threadID, name, 0, address);
			}
		}
	}
	
	/**
	 * Empty hook for now.
	 */
	protected void sovOnlyRules(String startingTag) throws ParserException {

	}
}
