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
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageStackFrame;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.osthread.IOSStackFrame;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;

/**
 * @author andhall
 *
 */
public class J9DDRImageStackFrame implements ImageStackFrame
{
	private final IProcess process;
	private final IOSStackFrame frame;
	private final ImageThread parentThread;
	
	public J9DDRImageStackFrame(IProcess process, IOSStackFrame frame, ImageThread parent)
	{
		this.frame = frame;
		this.process = process;
		this.parentThread = parent;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageStackFrame#getBasePointer()
	 */
	public ImagePointer getBasePointer() throws CorruptDataException
	{
		return new J9DDRImagePointer(process, frame.getBasePointer());
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageStackFrame#getProcedureAddress()
	 */
	public ImagePointer getProcedureAddress() throws CorruptDataException
	{
		return new J9DDRImagePointer(process, frame.getInstructionPointer());
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageStackFrame#getProcedureName()
	 */
	public String getProcedureName() throws CorruptDataException
	{
		try {
			return process.getProcedureNameForAddress(frame.getInstructionPointer(), true);
		} catch (DataUnavailableException ex) {
			return "<unknown location>";
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(process, e);
		}
	}

	@Override
	public int hashCode()
	{
		final int prime = 31;
		int result = 1;
		result = prime * result + ((frame == null) ? 0 : frame.hashCode());
		result = prime * result + ((process == null) ? 0 : process.hashCode());
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
		if (!(obj instanceof J9DDRImageStackFrame)) {
			return false;
		}
		J9DDRImageStackFrame other = (J9DDRImageStackFrame) obj;
		if (frame == null) {
			if (other.frame != null) {
				return false;
			}
		} else if (!frame.equals(other.frame)) {
			return false;
		}
		if (process == null) {
			if (other.process != null) {
				return false;
			}
		} else if (!process.equals(other.process)) {
			return false;
		}
		return true;
	}
	
	@Override
	public String toString() {
		try {
			return "J9DDRImageStackFrame: " + getProcedureName() + " from Thread " + parentThread.getID();
		} catch (CorruptDataException e) {
			return "J9DDRImageStackFrame <exception getting details>";
		}
	}

}
