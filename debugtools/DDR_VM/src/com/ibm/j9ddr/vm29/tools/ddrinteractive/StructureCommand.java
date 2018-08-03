/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import static com.ibm.j9ddr.StructureTypeManager.*;

import java.io.PrintStream;
import java.util.List;


import com.ibm.j9ddr.tools.ddrinteractive.BaseStructureCommand;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.FormatWalkResult;
import com.ibm.j9ddr.tools.ddrinteractive.IFieldFormatter;
import com.ibm.j9ddr.vm29.pointer.I16Pointer;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.I64Pointer;
import com.ibm.j9ddr.vm29.pointer.IDATAPointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U64Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base.ArrayFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base.BoolFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base.DoubleFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base.EnumFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base.FloatFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base.J9SRPFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base.PointerFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base.ScalarFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base.StructureFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base.StructurePointerFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base.U64ScalarFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base.VoidFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions.CStringFieldFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions.J9ClassFieldFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions.J9ClassStructureFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions.J9MethodFieldFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions.J9MethodStructureFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions.J9ModuleStructureFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions.J9ObjectFieldFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions.J9ObjectStructureFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions.J9PackageStructureFormatter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions.J9ROMMethodStructureFormatter;

/**
 * VM-specific half of the structure formatter.
 * 
 */
public class StructureCommand extends BaseStructureCommand
{
	
	public StructureCommand()
	{
		loadDefaultFormatters();
		
		loadExtensionFormatters();
		
		registerDefaultStructureFormatter(new RuntimeResolvingStructureFormatter());
	}

	private void loadExtensionFormatters() 
	{
		registerFieldFormatter(new J9ClassFieldFormatter());
		registerFieldFormatter(new J9MethodFieldFormatter());
		registerFieldFormatter(new CStringFieldFormatter());
		registerFieldFormatter(new J9ObjectFieldFormatter());
		
		registerStructureFormatter(new J9ObjectStructureFormatter());
		registerStructureFormatter(new J9ClassStructureFormatter());
		registerStructureFormatter(new J9MethodStructureFormatter());
		registerStructureFormatter(new J9ROMMethodStructureFormatter());
		registerStructureFormatter(new J9ModuleStructureFormatter());
		registerStructureFormatter(new J9PackageStructureFormatter());
	}

	private void loadDefaultFormatters() 
	{
		/*NOTE: If you want to add a custom structure formatter - put it in loadExtensionFormatters not here */
		registerFieldFormatter(new StructurePointerFormatter());
		registerFieldFormatter(new PointerFormatter());
		registerFieldFormatter(new StructureFormatter());
		registerFieldFormatter(new ArrayFormatter());
		registerFieldFormatter(new BoolFormatter());
		registerFieldFormatter(new FloatFormatter());
		registerFieldFormatter(new DoubleFormatter());
		registerFieldFormatter(new EnumFormatter());
		registerFieldFormatter(new VoidFormatter());
		
		registerFieldFormatter(new J9SRPFormatter(TYPE_J9SRP, "J9SRP(", false));
		registerFieldFormatter(new J9SRPFormatter(TYPE_J9WSRP, "J9WSRP(", true));
		
		registerFieldFormatter(new ScalarFormatter(TYPE_I16, I16Pointer.class));
		registerFieldFormatter(new ScalarFormatter(TYPE_I32, I32Pointer.class));
		registerFieldFormatter(new ScalarFormatter(TYPE_I64, I64Pointer.class));
		registerFieldFormatter(new ScalarFormatter(TYPE_IDATA, IDATAPointer.class));
		registerFieldFormatter(new ScalarFormatter(TYPE_U8, U8Pointer.class));
		registerFieldFormatter(new ScalarFormatter(TYPE_U16, U16Pointer.class));
		registerFieldFormatter(new ScalarFormatter(TYPE_U32, U32Pointer.class));
		registerFieldFormatter(new U64ScalarFormatter(TYPE_U64, U64Pointer.class));
		registerFieldFormatter(new ScalarFormatter(TYPE_UDATA, UDATAPointer.class));
	}
	
	private class RuntimeResolvingStructureFormatter extends BaseStructureCommand.DefaultStructureFormatter {
		@Override
		public FormatWalkResult format(String type, long address, PrintStream out, Context context, List<IFieldFormatter> fieldFormatters, String[] extraArgs)  {
			type = RuntimeTypeResolutionHelper.findRuntimeType(type, PointerPointer.cast(address), context);
			return super.format(type, address, out, context, fieldFormatters, extraArgs);
		}
	}
	
}
