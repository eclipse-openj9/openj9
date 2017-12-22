/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.tools.utils;

import java.util.LinkedList;
import java.util.List;

/**
 * This class first caches all the lines.  The cache will be cleaned 
 * if it encounters an empty line and cache will be restarted. The 
 * cached lines will be released once it is asked to do so.
 * <p>
 * @author Manqing Li, IBM.
 *
 */
public class BlockPrematchHandle implements IPrematchHandle {

	public BlockPrematchHandle() {
		this.cached = new LinkedList<String>();
	}
	
	public void process(String s) {
		if (null == s || 0 == s.trim().length()) {
			cached = new LinkedList<String>();
		} else {
			cached.add(s);
		}
	}

	public String release() {
		StringBuffer sb = new StringBuffer();
		for (String line : cached) {
			sb.append(line);
		}
		cached = new LinkedList<String>();
		return sb.toString();
	}

	private List<String> cached;
}
