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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions;

import java.io.PrintStream;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.BaseStructureFormatter;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.FormatWalkResult;
import com.ibm.j9ddr.tools.ddrinteractive.IFieldFormatter;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;

/**
 * Structure Formatter that adds a suffix like this:
 * 
 * Class name: com/ibm/oti/vm/BootstrapClassLoader
 * To view static fields, use !j9statics 0x02C16800
 * To view instance shape, use !j9classshape 0x02C16800
 * 
 * When a !j9class is formatted
 */
public class J9ClassStructureFormatter extends BaseStructureFormatter 
{

	@Override
	public FormatWalkResult postFormat(String type, long address,
			PrintStream out, Context context,
			List<IFieldFormatter> fieldFormatters, String[] extraArgs) 
	{
		if (type.equalsIgnoreCase("j9class") && address != 0) {
			J9ClassPointer clazz = J9ClassPointer.cast(address);
			
			try {
				out.println("Class name: " + J9ClassHelper.getName(clazz));
			} catch (CorruptDataException e) {
				//Do nothing
				return FormatWalkResult.KEEP_WALKING;
			}
			out.println("To view static fields, use !j9statics " + clazz.getHexAddress());
			out.println("To view instance shape, use !j9classshape " + clazz.getHexAddress());
		}
		
		return FormatWalkResult.KEEP_WALKING;
	}

	
}
