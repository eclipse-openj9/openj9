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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.RasGlobalStoragePointer;
import com.ibm.j9ddr.vm29.pointer.generated.UtComponentDataPointer;
import com.ibm.j9ddr.vm29.pointer.generated.UtComponentListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.UtGlobalDataPointer;
import com.ibm.j9ddr.vm29.pointer.generated.UtModuleInfoPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.structure.RastraceInternalConstants;
import com.ibm.j9ddr.vm29.structure.UteModuleConstants;

public class TraceConfigCommand extends Command {

	private final boolean verbose = false;
	
	public TraceConfigCommand() 
	{
		addCommand("tpconfig", "[<component>|all]", "Lists trace components or tracepoints enabled for specified components");
	}
	
	public void run(String command, String[] args, Context context,
			final PrintStream out) throws DDRInteractiveCommandException {

		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			// !RasGlobalStorage
			RasGlobalStoragePointer rasGlobal = RasGlobalStoragePointer.cast(vm.j9rasGlobalStorage());
			// !UtGlobalData
			UtGlobalDataPointer utGlobal = UtGlobalDataPointer.cast(rasGlobal.utGlobalData());
			// !UtComponentList componentList
			UtComponentListPointer componentList = utGlobal.componentList();
			// !UtComponentData head
			UtComponentDataPointer head = componentList.head();

			if( args.length == 0 ) {
				walkTraceComponents(new ModuleVisitor() {
					public void visit(UtModuleInfoPointer modInfo)
					throws CorruptDataException {
						out.println(moduleName(modInfo));
					}
				}, head, context, out);
			} else if( "all".equals(args[0])){
				walkTraceComponents(new ModuleVisitor() {
					public void visit(UtModuleInfoPointer modInfo)
					throws CorruptDataException {
						printActiveTracePoints(modInfo, out);
					}
				}, head, context, out);
			} else {
				for( int i = 0; i < args.length; i++ ) {
					final String componentName = args[i];
					walkTraceComponents(new ModuleVisitor() {
						public void visit(UtModuleInfoPointer modInfo)
							throws CorruptDataException {
							if(componentName.equals(moduleName(modInfo))) {
								printActiveTracePoints(modInfo, out);
							}
						}
					}, head, context, out);
				}
			}
		} catch (CorruptDataException e) {
			e.printStackTrace();
		}
	}

	private void printActiveTracePoints(
			UtModuleInfoPointer modInfo, PrintStream out) throws CorruptDataException {
		
		int count = modInfo.count().intValue();
		U8Pointer active = modInfo.active();
		String moduleName = moduleName(modInfo);
		for( int i = 0; i < count; i++ ) {
			long state = active.at(i).longValue();
			if( state != RastraceInternalConstants.UT_NONE || verbose) {
				String stateStr = decodeTraceActivationState(state);
				out.println(moduleName + "." + i + " =" + stateStr);
			}
		}
	}
	
//	#define UT_MINIMAL                    1
//	#define UT_MAXIMAL                    2
//	#define UT_COUNT                      4
//	#define UT_PRINT                      8
//	#define UT_PLATFORM                   16
//	#define UT_EXCEPTION                  32
//	#define UT_EXTERNAL                   64
//	#define UT_TRIGGER                    128
//	#define UT_NONE                       0
//	#define UT_SPECIAL_ASSERTION 0x00400000
	
	private String decodeTraceActivationState(long activeState) {
		String state = "";
		if( (activeState == RastraceInternalConstants.UT_NONE) ) {
			state += " NONE";
		}
		if( (activeState & RastraceInternalConstants.UT_MAXIMAL) != 0 ) {
			state += " MAXIMAL";
		}
		if( (activeState & RastraceInternalConstants.UT_MINIMAL) != 0 ) {
			state += " MINIMAL";
		}
		if( (activeState & RastraceInternalConstants.UT_COUNT ) != 0 ) {
			state += " COUNT";
		}
		if( (activeState & RastraceInternalConstants.UT_PRINT) != 0 ) {
			state += " PRINT";
		}
		if( (activeState & RastraceInternalConstants.UT_PLATFORM) != 0 ) {
			state += " PLATFORM";
		}
		if( (activeState & RastraceInternalConstants.UT_EXCEPTION) != 0 ) {
			state += " EXCEPTION";
		}
		if( (activeState & RastraceInternalConstants.UT_TRIGGER) != 0 ) {
			state += " TRIGGER";
		}
		if( (activeState & RastraceInternalConstants.UT_EXTERNAL) != 0 ) {
			state += " EXTERNAL";
		}
		if( (activeState & RastraceInternalConstants.UT_NONE) != 0 ) {
			state += " NONE";
		}
		if( (activeState & UteModuleConstants.UT_SPECIAL_ASSERTION) != 0 ) {
			state += " ASSERTION";
		}
		if( state.equals("") ) {
			state = " <error unknown state>";
		}
		return state;
	}

	private static String moduleName(UtModuleInfoPointer modInfo) throws CorruptDataException {
		UtModuleInfoPointer containerModInfo = modInfo.containerModule();
		String moduleName = modInfo.name().getCStringAtOffset(0, modInfo.namelength().intValue());
		if( containerModInfo != null && containerModInfo.notNull() ) {
			String containerName = containerModInfo.name().getCStringAtOffset(0, containerModInfo.namelength().intValue());
			return containerName + "(" +  moduleName + ")";
		} else {
			return moduleName;
		}
	}
	
	/**
	 * Walk the trace components and use the visitor pattern to save us writing this
	 * loop several times.
	 * 
	 * @param visitor the visitor object to run on each UtModuleInfo.
	 * @param head the pointer to the first item in the list of UtComponentData
	 * @param context
	 * @param out
	 */
	private static void walkTraceComponents(final ModuleVisitor visitor, final UtComponentDataPointer head, Context context, PrintStream out) {
		UtComponentDataPointer current = head;
		try {
			while (current != null && current.notNull()) {
				UtModuleInfoPointer modInfo = current.moduleInfo();
				visitor.visit(modInfo);
				current = current.next();
			}
		} catch (CorruptDataException e) {
			e.printStackTrace();
		}
	}
	
	public static interface ModuleVisitor {
		public void visit(UtModuleInfoPointer modInfo) throws CorruptDataException;
	}
	
}
