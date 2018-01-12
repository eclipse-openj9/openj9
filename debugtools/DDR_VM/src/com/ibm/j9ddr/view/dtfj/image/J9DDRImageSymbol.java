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

import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSymbol;

/**
 * @author andhall
 *
 */
public class J9DDRImageSymbol implements ImageSymbol
{
	private final String name;
	private final ImagePointer address;
	
	public J9DDRImageSymbol(String name, ImagePointer address)
	{
		this.name = name;
		this.address = address;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageSymbol#getAddress()
	 */
	public ImagePointer getAddress()
	{
		return address;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageSymbol#getName()
	 */
	public String getName()
	{
		return name;
	}

	@Override
	public boolean equals(Object obj)
	{
		if (obj instanceof J9DDRImageSymbol) {
			J9DDRImageSymbol other = (J9DDRImageSymbol) obj;
			
			return other.address.equals(this.address) && other.name.equals(this.name);
		} else {
			return false;
		}
	}

	@Override
	public int hashCode()
	{
		return address.hashCode();
	}

	@Override
	public String toString()
	{
		return "Symbol " + getName() + " = " + getAddress();
	}

}
