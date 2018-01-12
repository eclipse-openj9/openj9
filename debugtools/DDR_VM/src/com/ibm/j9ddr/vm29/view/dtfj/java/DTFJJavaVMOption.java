/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.view.dtfj.java;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaVMOption;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.vm29.pointer.generated.JavaVMOptionPointer;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

public class DTFJJavaVMOption implements JavaVMOption {
	private final JavaVMOptionPointer option;
	private ImagePointer info = null;
	
	public DTFJJavaVMOption(JavaVMOptionPointer ptr) {
		option = ptr;
	}
	
	public ImagePointer getExtraInfo() throws DataUnavailable, CorruptDataException {
		if(info == null) {
			try {
				long address = option.extraInfo().getAddress();
				info = DTFJContext.getImagePointer(address);
			} catch (Throwable t) {
				throw J9DDRDTFJUtils.handleAllButDataUnavailAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
		return info;
	}

	public String getOptionString() throws DataUnavailable,	CorruptDataException {
		try {
			return option.optionString().getCStringAtOffset(0);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButDataUnavailAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

}
