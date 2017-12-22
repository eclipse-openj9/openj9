/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.j9;

import java.util.Properties;

import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;

/**
 * @author jmdisher
 *
 */
public abstract class ImageSection implements com.ibm.dtfj.image.ImageSection
{
	private ImagePointer _start;
	private long _size;
	
	protected ImageSection(ImagePointer start, long size)
	{
		_start = start;
		_size = size;
	}
	
	public ImagePointer getBaseAddress()
	{
		return _start;
	}

	public long getSize()
	{
		return _size;
	}
	
	
	//in general, we don't get image section permission bits (the specific sub-classes can give us more information and override these methods)

	public boolean isExecutable() throws DataUnavailable
	{
		return _start.isExecutable();
	}

	public boolean isReadOnly() throws DataUnavailable
	{
		return _start.isReadOnly();
	}

	public boolean isShared() throws DataUnavailable
	{
		return _start.isShared();
	}
	
	public Properties getProperties() {
		return _start.getProperties();
	}
}
