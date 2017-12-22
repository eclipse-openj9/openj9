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

import com.ibm.j9ddr.CorruptDataException;

/**
 * Interface for objects that format fields in structures.
 * 
 * e.g. turning 
 * 
 * struct J9NativeLibrary * librariesTail
 * 
 * into
 * 
 * !j9nativelibrary 0x0019CDA8
 * 
 * 
 * The IFieldFormatters form a chain of responsibility. The structure ICommand
 * calls each IFieldFormatter in turn three times.
 * 
 * The first pass calls preFormat() (stopping either when a IFieldFormatter
 * returns STOP_WALKING, or the end of the chain is reached.)
 * The second pass calls format() - where the default formatting behaviour
 * is run.
 * The third pass calls postFormat()
 *
 */
public interface IFieldFormatter 
{
	
	/**
	 * Method called by the Structure command before the format() method walk.
	 * 
	 * @param name Name of field being formatted.
	 * @param type Type of field to be formatted. E.g. struct J9JavaVM 
	 * @param declaredType Type as declared in C. E.g. This may be char* where type is U8*
	 * @param typeCode StructureHelper type code. E.g. TYPE_STRUCTURE
	 * @param address Address of field to be formatted
	 * @param out PrintStream to write result to. Do not write a new line at the end of your output.
	 * @param context Current DDRInteractive context
	 * @param formattingDirector FormattingDirector; useful if you need to rerun a different field format (e.g. when working with J9SRPs)
	 * @return KEEP_WALKING if the format walk should keep going (delegating to other formatters), STOP_WALKING if the format walk should stop
	 */
	public FormatWalkResult preFormat(String name, String type, String declaredType, int typeCode, long address, PrintStream out, Context context, IStructureFormatter structureFormatter) throws CorruptDataException;
	
	/**
	 * Method called by the Structure command to format a field, after preFormat() and before postFormat()
	 * 
	 * @param name Name of field being formatted.
	 * @param type Type of field to be formatted. E.g. struct J9JavaVM 
	 * @param declaredType Type as declared in C. E.g. This may be char* where type is U8* 
	 * @param typeCode StructureHelper type code. E.g. TYPE_STRUCTURE
	 * @param address Address of field to be formatted
	 * @param out PrintStream to write result to. Do not write a new line at the end of your output.
	 * @param context Current DDRInteractive context
	 * @param formattingDirector FormattingDirector; useful if you need to rerun a different field format (e.g. when working with J9SRPs)
	 * @return KEEP_WALKING if the format walk should keep going (delegating to other formatters), STOP_WALKING if the format walk should stop
	 */
	public FormatWalkResult format(String name, String type, String declaredType, int typeCode, long address, PrintStream out, Context context, IStructureFormatter structureFormatter) throws CorruptDataException;
	
	/**
	 * Method called by the Structure command after preFormat() and format().
	 * 
	 * @param name Name of field being formatted.
	 * @param type Type of field to be formatted. E.g. struct J9JavaVM 
	 * @param declaredType Type as declared in C. E.g. This may be char* where type is U8*
	 * @param typeCode StructureHelper type code. E.g. TYPE_STRUCTURE
	 * @param address Address of field to be formatted
	 * @param out PrintStream to write result to. Do not write a new line at the end of your output.
	 * @param context Current DDRInteractive context
	 * @param structureFormatter StructureFormatter; useful if you need to rerun a different field format (e.g. when working with J9SRPs)
	 * @return KEEP_WALKING if the format walk should keep going (delegating to other formatters), STOP_WALKING if the format walk should stop
	 */
	public FormatWalkResult postFormat(String name, String type, String declaredType, int typeCode, long address, PrintStream out, Context context, IStructureFormatter structureFormatter) throws CorruptDataException;
}
