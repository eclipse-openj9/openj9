/*******************************************************************************
 * Copyright (c) 2010, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.BaseFieldFormatter;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.FormatWalkResult;
import com.ibm.j9ddr.tools.ddrinteractive.IStructureFormatter;
import com.ibm.j9ddr.vm29.pointer.SelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.WideSelfRelativePointer;

public class J9SRPFormatter extends BaseFieldFormatter
{
	
	private final int typeCode;
	
	private final String srpPrefix;
	
	private final boolean wide;
	
	public J9SRPFormatter(int typeCode, String srpPrefix, boolean wide)
	{
		this.typeCode = typeCode;
		this.srpPrefix = srpPrefix;
		this.wide = wide;
	}

	private String stripSRP(String type) 
	{
		type = type.trim();
		return type.substring(srpPrefix.length(), type.length() - 1);
	}

	@Override
	public FormatWalkResult format(String name, String type, String declaredType,
			int thisTypeCode, long address, PrintStream out,
			Context context, IStructureFormatter structureFormatter) throws CorruptDataException 
	{
		if (thisTypeCode != typeCode) {
			return FormatWalkResult.KEEP_WALKING;
		}
		
		VoidPointer targetAddress = VoidPointer.NULL;
		/* Figure out the resulting address */
		if (wide) {
			WideSelfRelativePointer s = WideSelfRelativePointer.cast(address);
			
			targetAddress = s.get();
		} else {
			SelfRelativePointer s = SelfRelativePointer.cast(address);
			
			targetAddress = s.get();
		}
		
		if (type.contains("(")) {
			/* Strip off one layer of SRP, work out the SRP address, and re-format it */
			type = stripSRP(type);
			
			structureFormatter.formatField(name, type, type, targetAddress.getAddress(), out, context);
		} else {
			/* An SRP without the final type specified */
			
			out.print("!j9x ");
			out.print(targetAddress.getHexAddress());
		}
		
		return FormatWalkResult.STOP_WALKING;
	}
	
}
