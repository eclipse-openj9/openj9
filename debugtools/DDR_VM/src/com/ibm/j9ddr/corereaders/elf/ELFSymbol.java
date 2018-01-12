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
package com.ibm.j9ddr.corereaders.elf;

class ELFSymbol
{
	private static final byte STT_FUNC = 2;
	private static final byte ST_TYPE_MASK = 0xf;

	final long name;
	final long value;
	// private long _size;
	private byte _info;

	// private byte _other;
	// private int _sectionIndex;

	ELFSymbol(long name, long value, long size, byte info, byte other,
			int sectionIndex)
	{
		this.name = name;
		this.value = value;
		// _size = size;
		_info = info;
		// _other = other;
		// _sectionIndex = sectionIndex;
	}

	boolean isFunction()
	{
		return STT_FUNC == (_info & ST_TYPE_MASK);
	}
}
