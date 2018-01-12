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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base;

import java.io.PrintStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.BaseFieldFormatter;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.FormatWalkResult;
import com.ibm.j9ddr.tools.ddrinteractive.IStructureFormatter;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.types.Scalar;

/**
 * Formatter for all numeric scalar types (U* and I*)
 * @author andhall
 */
public class ScalarFormatter extends BaseFieldFormatter
{
	private final int typeCode;
	
	private final Method castMethod;
	
	private final Method atMethod;
	
	private final Class<? extends AbstractPointer> pointerClass;
	
	public ScalarFormatter(int typeCode, Class<? extends AbstractPointer> pointerClass)
	{
		this.typeCode = typeCode;
		try {
			this.castMethod = pointerClass.getMethod("cast", new Class<?>[]{long.class});
			this.atMethod = pointerClass.getMethod("at", new Class<?>[]{long.class});
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
		this.pointerClass = pointerClass;
	}
	
	protected void formatShortScalar(Scalar value, PrintStream out) 
	{
		out.print(value.getHexValue());
		out.print(" (");
		out.print(value.longValue());
		out.print(")");
	}

	@Override
	public FormatWalkResult format(String name, String type, String declaredType,
			int thisTypeCode, long address, PrintStream out,
			Context context, IStructureFormatter structureFormatter) throws CorruptDataException 
	{
		if (thisTypeCode == this.typeCode) {
			Object o = null;
			try {
				o = castMethod.invoke(null, address);
			} catch (Exception e) {
				throw new RuntimeException("Cast failed on " + pointerClass.getName(),e);
			}
			
			try {
				Scalar value = (Scalar)atMethod.invoke(o, 0L);
				
				formatShortScalar(value, out);
			} catch (InvocationTargetException e) {
				Throwable cause = e.getCause();
				
				if (cause instanceof CorruptDataException) {
					throw (CorruptDataException)cause;
				} else if (cause instanceof RuntimeException) {
					throw (RuntimeException) cause;
				} else {
					throw new RuntimeException("Problem invoking at() method on " + pointerClass.getName(), e);
				}
			} catch (Exception e) {
				throw new RuntimeException("Problem invoking at() method on " + pointerClass.getName(),e);
			}
			return FormatWalkResult.STOP_WALKING;
		} else {
			return FormatWalkResult.KEEP_WALKING;
		}
	}
	
}
