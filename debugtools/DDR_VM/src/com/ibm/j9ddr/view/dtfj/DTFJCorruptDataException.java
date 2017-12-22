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
package com.ibm.j9ddr.view.dtfj;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.view.dtfj.image.J9DDRCorruptData;

/**
 * J9DDR CorruptDataException that can take a cause for improved debugging.
 * 
 * @author andhall
 *
 */
public class DTFJCorruptDataException extends CorruptDataException
{

	/**
	 * 
	 */
	private static final long serialVersionUID = -5889215850998445848L;

	public DTFJCorruptDataException(CorruptData data)
	{
		super(data);
	}
	
	public DTFJCorruptDataException(CorruptData data, Throwable t)
	{
		this(data);
		this.initCause(t);
	}
	
	public DTFJCorruptDataException(IProcess proc, com.ibm.j9ddr.CorruptDataException e)
	{
		this(new J9DDRCorruptData(proc, e), e);
	}
}
