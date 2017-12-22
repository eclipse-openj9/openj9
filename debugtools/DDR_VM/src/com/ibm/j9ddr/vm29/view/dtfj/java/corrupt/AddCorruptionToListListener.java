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
package com.ibm.j9ddr.vm29.view.dtfj.java.corrupt;

import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.events.IEventListener;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

/**
 * @author hhellyer
 *
 */
/*
 * Utility class to handle the (common) case where we need to register a
 * handler to add corrupt data into a list as we iterate over DDR structures.
 * 
 * Typically an iterator over that list is returned by the DTFJ API.
 */
public class AddCorruptionToListListener implements IEventListener {

	private List<Object> list;
	private boolean fatalCorruption = false;
	
	public AddCorruptionToListListener(List<Object> list) {
		this.list = list;
	}
	
	public void corruptData(final String message,
			final CorruptDataException e, final boolean fatal) {
		list.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e));
		fatalCorruption = true;
	}

	public boolean fatalCorruption() {
		return fatalCorruption;
	}
	
}
