/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.image;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageRegister;
import com.ibm.j9ddr.corereaders.osthread.IRegister;

/**
 * Adapter for IRegister to make it implement ImageRegister
 * 
 * @author andhall
 *
 */
public class J9DDRImageRegister implements ImageRegister
{

	private final IRegister register;
	
	public J9DDRImageRegister(IRegister reg) 
	{
		this.register = reg;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageRegister#getName()
	 */
	public String getName()
	{
		return register.getName();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageRegister#getValue()
	 */
	public Number getValue() throws CorruptDataException
	{
		return register.getValue();
	}

	@Override
	public int hashCode()
	{
		final int prime = 31;
		int result = 1;
		result = prime * result
				+ ((register == null) ? 0 : register.hashCode());
		return result;
	}

	@Override
	public boolean equals(Object obj)
	{
		if (this == obj) {
			return true;
		}
		if (obj == null) {
			return false;
		}
		if (!(obj instanceof J9DDRImageRegister)) {
			return false;
		}
		J9DDRImageRegister other = (J9DDRImageRegister) obj;
		if (register == null) {
			if (other.register != null) {
				return false;
			}
		} else if (!register.equals(other.register)) {
			return false;
		}
		return true;
	}

	@Override
	public String toString() {
		StringBuilder data = new StringBuilder();
		data.append("Register [");
		data.append(register.getName());
		data.append(" = ");
		data.append(register.getValue().toString());
		data.append("]");
		return data.toString();
	}

	
	
}
