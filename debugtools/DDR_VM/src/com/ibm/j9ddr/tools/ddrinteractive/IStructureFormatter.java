/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive;

import java.io.PrintStream;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;

/**
 * Interface for objects that format entire structures
 * 
 * e.g. turning 
 * 
 * !mm_gcextensions 0x00119748
 * 
 * into
 * 
 * MM_GCExtensions at 0x119748 {
 *  0x8: struct J9JavaVM* javaVM = !j9javavm 0x000A9A88
 *  0xc: class MM_Forge forge = !class mm_forge 0x00119754
 *  0x74: class GC_ObjectModel objectModel = !class gc_objectmodel 0x001197BC
 * 
 * 
 * The IStructureFormatters form a chain of responsibility. The structure ICommand
 * calls each IStructureFormatters in turn three times.
 * 
 * The first pass calls preFormat()
 * The second pass calls format() - where the default formatting behaviour
 * is run.
 * The third pass calls postFormat()
 * 
 * Each method can return KEEP_WALKING to tell the walker to delegate to the next
 * IStructureFormatter in the chain, or STOP_WALKING to stop the walk.
 *
 */
public interface IStructureFormatter 
{
	
	/**
	 * Method called by the Structure command before the format() method walk.
	 * 
	 * @type type Type being formatted. E.g. j9javavm
	 * @param address Address to be formatted
	 * @param out PrintStream to write result to. Do not write a new line at the end of your output.
	 * @param context Current DDRInteractive context
	 * @param fieldFormatters List of field formatters. This list will be used for other IStructureFormatters,
	 * so any changes you make will affect downstream formatters (but only for this formatting operation). 
	 * @param extraArgs Extra arguments passed by the user after the structure address
	 * @return KEEP_WALKING if the format walk should keep going (delegating to other formatters), STOP_WALKING if the format walk should stop
	 */
	public FormatWalkResult preFormat(String type, long address, PrintStream out, Context context, List<IFieldFormatter> fieldFormatters, String[] extraArgs);
	
	/**
	 * Method called by the Structure command after the preFormat() and before the postFormat() walks.
	 * 
	 * @type type Type being formatted. E.g. j9javavm
	 * @param address Address to be formatted
	 * @param out PrintStream to write result to. Do not write a new line at the end of your output.
	 * @param context Current DDRInteractive context
	 * @param fieldFormatters List of field formatters. This list will be used for other IStructureFormatters,
	 * so any changes you make will affect downstream formatters (but only for this formatting operation).
	 * @param extraArgs Extra arguments passed by the user after the structure address 
	 * @return KEEP_WALKING if the format walk should keep going (delegating to other formatters), STOP_WALKING if the format walk should stop
	 */
	public FormatWalkResult format(String type, long address, PrintStream out, Context context, List<IFieldFormatter> fieldFormatters, String[] extraArgs);
	
	/**
	 * Method called by the Structure command after the format() walk.
	 * 
	 * @type type Type being formatted. E.g. j9javavm
	 * @param address Address to be formatted
	 * @param out PrintStream to write result to. Do not write a new line at the end of your output.
	 * @param context Current DDRInteractive context
	 * @param fieldFormatters List of field formatters. This list will be used for other IStructureFormatters,
	 * so any changes you make will affect downstream formatters (but only for this formatting operation).
	 * @param extraArgs Extra arguments passed by the user after the structure address  
	 * @return KEEP_WALKING if the format walk should keep going (delegating to other formatters), STOP_WALKING if the format walk should stop
	 */
	public FormatWalkResult postFormat(String type, long address, PrintStream out, Context context, List<IFieldFormatter> fieldFormatters, String[] extraArgs);
	
	/**
	 * 
	 * @param name Name of field
	 * @param type Type of field e.g. struct J9JavaVM 
	 * @param declaredType The type as declared in native code
	 * @param address Address of field slot
	 * @param out PrintStream to format to
	 * @param context DDRInteractive context
	 * @throws CorruptDataException If there's a problem accessing the field
	 */
	public void formatField(String name, String type, String declaredType, long address, PrintStream out, Context context) throws CorruptDataException;
}
