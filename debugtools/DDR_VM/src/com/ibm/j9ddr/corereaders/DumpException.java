/*******************************************************************************
 * Copyright (c) 2004, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders;

/**
 * This class is the superclass of all exceptions thrown by Dump classes
 * 
 * @author fbogsanyi
 */
public class DumpException extends Exception
{
	private static final long serialVersionUID = -7947427629541393731L;

	private int _asid;
	private long _address;

	public DumpException(int asid, long address, String description)
	{
		super(description);
		_asid = asid;
		_address = address;
	}

	public DumpException(int asid, long address)
	{
		super();
		_asid = asid;
		_address = address;
	}

	/**
	 * @return the address in the addressSpaceId of the Dump where the exception
	 *         was raised
	 */
	public long getAddress()
	{
		return _address;
	}

	/**
	 * @return the addressSpaceId of the Dump where the exception was raised
	 */
	public int getAddressSpaceId()
	{
		return _asid;
	}
}
