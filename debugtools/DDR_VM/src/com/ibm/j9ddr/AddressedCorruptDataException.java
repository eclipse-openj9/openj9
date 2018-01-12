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
package com.ibm.j9ddr;

/**
 * A CorruptDataException with an address.
 * 
 * @author andhall
 *
 */
public class AddressedCorruptDataException extends CorruptDataException
{
	/**
	 * 
	 */
	private static final long serialVersionUID = 536356663314518043L;
	
	protected final long address;
	
	
	public AddressedCorruptDataException(long address, String message, Throwable t)
	{
		super(message, t);
		this.address = address;
	}

	public AddressedCorruptDataException(long address, String message)
	{
		super(message);
		this.address = address;
	}


	public AddressedCorruptDataException(long address, Throwable t)
	{
		super(t);
		this.address = address;
	}

	public long getAddress()
	{
		return address;
	}
}
