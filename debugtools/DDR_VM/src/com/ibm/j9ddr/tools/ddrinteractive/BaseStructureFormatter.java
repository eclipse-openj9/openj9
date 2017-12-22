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
 * Default IStructureFormatter implementation with safe default behaviour
 *
 */
public abstract class BaseStructureFormatter implements IStructureFormatter 
{

	private List<IFieldFormatter> fieldFormatters;
	
	protected void setFieldFormatters(List<IFieldFormatter> fieldFormatters)
	{
		this.fieldFormatters = fieldFormatters;
	}
	
	public FormatWalkResult format(String type, long address, PrintStream out,
			Context context, List<IFieldFormatter> fieldFormatters, String[] extraArgs) 
	{
		return FormatWalkResult.KEEP_WALKING;
	}

	public FormatWalkResult postFormat(String type, long address,
			PrintStream out, Context context,
			List<IFieldFormatter> fieldFormatters, String[] extraArgs) 
	{
		return FormatWalkResult.KEEP_WALKING;
	}

	public FormatWalkResult preFormat(String type, long address,
			PrintStream out, Context context,
			List<IFieldFormatter> fieldFormatters, String[] extraArgs) 
	{
		return FormatWalkResult.KEEP_WALKING;
	}
	
	public void formatField(String name, String type, String declaredType,
			long address, PrintStream out, Context context) throws CorruptDataException 
	{
		int typeCode = StructureCommandUtil.getTypeCode(type, context);
		
		/* preFormat run */
		for (IFieldFormatter formatter : fieldFormatters) {
			FormatWalkResult result = formatter.preFormat(name, type, declaredType, typeCode, address, out, context, this);
			
			if (result == FormatWalkResult.STOP_WALKING) {
				break;
			}
		}
		
		boolean found = false;
		/* Format */
		for (IFieldFormatter formatter : fieldFormatters) {
			FormatWalkResult result = formatter.format(name, type, declaredType, typeCode, address, out, context, this);
			
			if (result == FormatWalkResult.STOP_WALKING) {
				found = true;
				break;
			}
		}
		
		if (! found) {
			//Default behaviour - write out !j9x address
			out.print(String.format("!j9x 0x%x", address));
		}
		
		/* postFormat */
		for (IFieldFormatter formatter : fieldFormatters) {
			FormatWalkResult result = formatter.postFormat(name, type, declaredType, typeCode, address, out, context, this);
			
			if (result == FormatWalkResult.STOP_WALKING) {
				break;
			}
		}
	}

}
