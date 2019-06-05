/*******************************************************************************
 * Copyright (c) 2012, 2019 IBM Corp. and others
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

package com.ibm.j9ddr;

import static java.util.logging.Level.FINE;
import static java.util.logging.Level.FINER;

import java.util.Collection;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.logging.Logger;

import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.ISymbol;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.memory.SymbolUtil;
import com.ibm.j9ddr.logging.LoggerNames;

public class DDRSymbolFinder {

	private static boolean addSymbols = true;
	
	private static List<String[][]> paths = new LinkedList<String[][]>();
	
	/* Set of known fields to ignore as StructureName.fieldName */
	private static Set<String> ignoredSymbols = new HashSet<String>();
	
	static {
		// Each table off function pointers to add has a path of pointers to follow to find it
		// below. The path begins at J9JavaVM structure for the process so the first entry in
		// the path is always a field within that.
		// The path is an array of tuples of type and field names.
		// (Except that java hasn't got tuples so it's a 2 element array instead.)
		// All paths are from the J9JavaVM given in ctx.vmAddress.
		// The types are necessary because a lot of the pointers are void * in their parent
		// structure. By default we just add all the pointers from the last structure in the
		// path.
		
		/* Port Library */
		paths.add( new String[][] {	{"J9PortLibrary","portLibrary"}	});
		
		ignoredSymbols.add("J9PortLibrary.portVersion");
		ignoredSymbols.add("J9PortLibrary.portGlobals");
		
		/* Internal Functions. */
		paths.add( new String[][] {	{"J9InternalVMFunctions","internalVMFunctions"}	});
		
		ignoredSymbols.add("J9InternalVMFunctions.reserved0");
		ignoredSymbols.add("J9InternalVMFunctions.reserved1");
		ignoredSymbols.add("J9InternalVMFunctions.reserved2");

		paths.add( new String[][] {	{"J9InternalVMLabels","internalVMLabels"} });
				
		/* GC Functions */
		paths.add( new String[][] {	{"J9MemoryManagerFunctions","memoryManagerFunctions"} });
		
		/* Jni functions */
		paths.add( new String[][] {	{"jniNativeInterface", "jniFunctionTable"} });
		
		ignoredSymbols.add("jniNativeInterface.reserved0");
		ignoredSymbols.add("jniNativeInterface.reserved1");
		ignoredSymbols.add("jniNativeInterface.reserved2");
		ignoredSymbols.add("jniNativeInterface.reserved3");
		
		/* Reflection functions */
		paths.add( new String[][] {	{"J9ReflectFunctionTable", "reflectFunctions"}	});
		
		/* RAS Trace functions */
		paths.add( new String[][] {	{"RasGlobalStorage","j9rasGlobalStorage"},
									{"UtInterface","utIntf"},
									{"UtServerInterface","server"},	});
		
		ignoredSymbols.add("UtServerInterface.header");
			
		paths.add( new String[][] {	{"RasGlobalStorage","j9rasGlobalStorage"},
									{"UtInterface","utIntf"},
									{"UtClientInterface","client"},	});
		
		ignoredSymbols.add("UtClientInterface.header");
		
		paths.add( new String[][] {	{"RasGlobalStorage","j9rasGlobalStorage"},
									{"UtInterface","utIntf"},
									{"UtModuleInterface","module"},	});
		
		/* RAS Dump Functions */
		paths.add( new String[][] {	{"J9RASdumpFunctions","j9rasDumpFunctions"}	});
		
		ignoredSymbols.add("J9RASdumpFunctions.reserved");
	}
	
	public static void addSymbols(IProcess process, long j9RasAddress, StructureReader structureReader) {
		
		long vmAddress = 0;
		
		try {

			vmAddress = followPointerFromStructure("J9RAS", j9RasAddress, "vm", structureReader, process);

		} catch (MemoryFault e) {
			Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
			logger.log(FINE, null, e);
		} catch (com.ibm.j9ddr.NoSuchFieldException e){
			Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
			logger.log(FINE, null, e);
		} catch (Exception e) {
			// This is optional information, we don't want to let an exception escape
			// and block DDR startup!
			Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
			logger.log(FINE, null, e);
		}

		if( vmAddress == 0 ) {
			return;
		}
		
		for( String[][] path: paths) {
			try {
				long currentAddress = vmAddress;
				String currentType = "J9JavaVM";
				for( String[] pathEntry: path ) {
					String structType = pathEntry[0];
					String fieldName = pathEntry[1]; 
					currentAddress = followPointerFromStructure(currentType, currentAddress, fieldName, structureReader, process);
					currentType = structType;
				}
				addVoidPointersAsSymbols(currentType, currentAddress, structureReader, process );
			} catch (MemoryFault e) {
				Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
				logger.log(FINE, null, e);
			} catch (com.ibm.j9ddr.NoSuchFieldException e){
				Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
				logger.log(FINE, null, e);		
			} catch (CorruptDataException e) {
				Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
				logger.log(FINE, null, e);
			} catch (DataUnavailableException e) {
				Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
				logger.log(FINE, null, e);
			} catch (Exception e) {
				// This is optional information, we don't want to let an exception escape
				// and block DDR startup!
				Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
				logger.log(FINE, null, e);
			}
		}
				
			
		
	}

	private static void addVoidPointersAsSymbols(String structureName,
			long structureAddress, StructureReader structureReader, IProcess process) throws DataUnavailableException, CorruptDataException {
		
		if( structureAddress == 0 ) {
			return;
		}
		
		List<String> voidFields = getVoidPointerFieldsFromStructure(structureReader, structureName);
		
		if( voidFields == null ) {
			return;
		}
		
		// Format the hex and structure name the same as the rest of DDR.
		int paddingSize = process.bytesPerPointer() * 2;
		String formatString = "[!%s 0x%0"+ paddingSize + "X->%s]";
	
		for( String field : voidFields ) {

			if( ignoredSymbols.contains(structureName+"."+field)) {
				// Ignore this field, it's not a function.
				continue;
			}
			
			long functionAddress = followPointerFromStructure(structureName, structureAddress, field, structureReader, process);
			if( functionAddress == 0 ) {
				continue; // Skip null pointers.
			}
			// Lower case the structure name to match the rest of the DDR !<struct> commands.
			String symName = String.format(formatString, structureName.toLowerCase(), structureAddress, field);
			if( functionAddress == 0 ) {
				// Null pointer, possibly unset or no longer used.
				continue;
			}
			if( addSymbols ) {
				IModule module = getModuleForInstructionAddress(process, functionAddress);
				if( module == null ) {
					continue;
				}
				boolean found = false;
				// Don't override/duplicate symbols we've already seen.
				for( ISymbol sym : module.getSymbols() ) {
					if( sym.getAddress() == functionAddress ) {
						Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
						logger.log(FINER, "DDRSymbolFinder: Found exact match with " + sym.toString() + " not adding.");
						found = true;
						break;
					}
				}
				if( !found ) {
					Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
					logger.log(FINER, "DDRSymbolFinder: Adding new DDR symbol " + symName);
					SymbolUtil.addDDRSymbolToModule(module, "["+field+"]", symName, functionAddress);
				}
			}
		}
	}
	
	private static long followPointerFromStructure(String structureName, long structureAddress, String fieldName, StructureReader reader, IProcess process) throws MemoryFault, NullPointerException {
		
		if( structureAddress == 0 ) {
			throw new NullPointerException("Null " + structureName + " found.");
		}
			
		for( FieldDescriptor f: reader.getFields(structureName)) {
			if( f.getDeclaredName().equals(fieldName)) {
				long offset = f.getOffset();
				long pointerAddress = structureAddress + offset;
				long pointer = process.getPointerAt(pointerAddress);
				return pointer;
			}
		}
		return 0;
	}
	
	private static List<String> getVoidPointerFieldsFromStructure(StructureReader reader, String structureName) throws MemoryFault {		
		
		List<String> functionPointers = new LinkedList<String>();
		for( FieldDescriptor f: reader.getFields(structureName)) {
			String name = f.getDeclaredName();
			String type = f.getDeclaredType();
			if( "void *".equals(type) ) {
				functionPointers.add(name);
			}
		}
		return functionPointers;
	}

	/* Logic stolen from SymbolUtil. */
	private static IModule getModuleForInstructionAddress(IProcess process, long address) throws CorruptDataException {
		Collection<? extends IModule> modules = process.getModules();
		
		IModule matchingModule = null;
		
OUTER_LOOP:	for (IModule thisModule : modules) {
			for (IMemoryRange thisRange : thisModule.getMemoryRanges()) {
				if (thisRange.contains(address)) {
					matchingModule = thisModule;
					break OUTER_LOOP;
				}
			}
		}
		return matchingModule;
	}
}
