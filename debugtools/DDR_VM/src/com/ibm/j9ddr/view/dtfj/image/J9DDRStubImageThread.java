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
package com.ibm.j9ddr.view.dtfj.image;

import java.util.Collections;
import java.util.Iterator;
import java.util.Properties;

import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.j9ddr.corereaders.memory.IProcess;

/**
 * ImageThread used for when we can't find a matching thread in the image.
 * 
 * Can only return its ID.
 * @author andhall
 */
public class J9DDRStubImageThread extends J9DDRBaseImageThread implements ImageThread
{
	private final long id;
	
	public J9DDRStubImageThread(IProcess process, long id)
	{
		super(process);
		this.id = id;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageThread#getProperties()
	 */
	public Properties getProperties()
	{
		return new Properties();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageThread#getRegisters()
	 */
	public Iterator<?> getRegisters()
	{
		return Collections.EMPTY_LIST.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageThread#getStackFrames()
	 */
	public Iterator<?> getStackFrames() throws DataUnavailable
	{
		return Collections.EMPTY_LIST.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageThread#getStackSections()
	 */
	public Iterator<?> getStackSections()
	{
		return Collections.EMPTY_LIST.iterator();
	}

	@Override
	public long getThreadId() throws com.ibm.j9ddr.CorruptDataException
	{
		return id;
	}

	@Override
	public int hashCode()
	{
		final int prime = 31;
		int result = 1;
		result = prime * result + (int) (id ^ (id >>> 32));
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
		if (!(obj instanceof J9DDRStubImageThread)) {
			return false;
		}
		J9DDRStubImageThread other = (J9DDRStubImageThread) obj;
		if (id != other.id) {
			return false;
		}
		return true;
	}
	
}
