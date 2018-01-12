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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck;

import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;

public class CheckElement
{
	private J9ObjectPointer object;
	private J9ClassPointer clazz;
	
	public void setObject(J9ObjectPointer object)
	{
		this.object = object;
		this.clazz = null;
	}
	
	public void setClazz(J9ClassPointer clazz)
	{
		this.object = null;
		this.clazz = clazz;
	}
	
	public void setNone()
	{
		this.object = null;
		this.clazz = null;
	}
	
	public J9ObjectPointer getObject()
	{
		if(object == null) {
			throw new IllegalStateException();
		}
		return object;
	}
	
	public J9ClassPointer getClazz()
	{
		if(clazz == null) {
			throw new IllegalStateException();
		}
		return clazz;
	}

	public void copyFrom(CheckElement checkElement)
	{
		this.object = checkElement.object;
		this.clazz = checkElement.clazz;
	}
	
	
	public boolean isNone()
	{
		return (object == null) && (clazz == null);
	}
}
