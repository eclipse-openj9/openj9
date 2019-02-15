/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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
package com.ibm.oti.VMCPTool;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

public class ConstantPool {
	private List<PrimaryItem> primaryItems = new ArrayList<PrimaryItem>();
	
	public int constantPoolSize() {
		return primaryItems.size() + 1;
	}
	
	public void add(PrimaryItem item) {
		primaryItems.add(item);
	}
	
	public PrimaryItem findPrimaryItem(Object obj) {
		for (PrimaryItem item : primaryItems) {
			if (obj.equals(item)) {
				return item;
			}
		}
		return null;
	}

	public void writeMacros(PrintWriter out) {
		for (PrimaryItem item : primaryItems) {
			item.writeMacros(this, out);
		}
	}

	public void writeForClassLibrary(int version, Set<String> flags, PrintWriter out) {
		// Two pass process:
		//   1) Compute offsets by writing to a disconnected stream.
		//   2) Connect stream output, reset offset and write to the connected stream.
		ConstantPoolStream ds = new ConstantPoolStream(version, flags, this, primaryItems.size() + 1);
		writeConstantPool(ds);
		
		// Now the offsets have been calculated, start over again
		ds.open(out);
		writeConstantPool(ds);
		ds.close();
	}
	
	private void writeConstantPool(ConstantPoolStream ds) {
		// CP0 is reserved
		ds.writeInt(0);
		ds.writeInt(0);
		
		// Write the primary items.
		int cpIndex=1;
		for (PrimaryItem item : primaryItems) {
			ds.comment("cp[" + cpIndex + "] = " + item.commentText()); //$NON-NLS-1$ //$NON-NLS-2$
			item.write(ds);
			cpIndex += 1;
		}
		
		// Write the secondary items.
		for (PrimaryItem item : primaryItems) {
			item.writeSecondaryItems(ds);
		}

		ds.alignTo(ConstantPoolStream.ITEM_SIZE);
	}

	public int getIndex(PrimaryItem item) {
		return 1 + primaryItems.indexOf(item);
	}
}
