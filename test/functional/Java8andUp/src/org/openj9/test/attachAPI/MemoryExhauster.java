/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package org.openj9.test.attachAPI;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

@SuppressWarnings({"boxing", "nls"})
public class MemoryExhauster {

	public static final int OOM_ERROR_STATUS=3;
	@SuppressWarnings("unused")
	private MemoryExhauster link;
	@SuppressWarnings("unused")
	private Long bloat;

	public MemoryExhauster(MemoryExhauster predecessor, long i) {
		try {
			link = predecessor;
			bloat = new Long(i);
		} catch (OutOfMemoryError e) {
			bloat = null;
			link = null;
			System.exit(OOM_ERROR_STATUS);
		}
	}

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		BufferedReader inRdr = new BufferedReader(new InputStreamReader(
				System.in));
		try {
			/* prevent CNFE by preloading a class which will be required on shutdown */
			Class.forName("java.util.AbstractList$SimpleListIterator", true, null);
		} catch (ClassNotFoundException e1) {
			System.err.println("ClassNotFoundException");
		}
		try {
			MemoryExhauster root = new MemoryExhauster(null, 1);
			try {
				String inLine = inRdr.readLine();
				System.out.println(inLine);
				Long i = 1L;
				while (i++ < 1000000000) {
					root = new MemoryExhauster(root, i);
				}
			} catch (OutOfMemoryError e) {
				root = null; /* make it garbage */
				System.exit(OOM_ERROR_STATUS);
			}
			System.out.println(TargetManager.STATUS_PREAMBLE
					+ TargetManager.STATUS_INIT_SUCESS);
		} catch (IOException e) {
			System.out.println("IOException_caught");			
			e.printStackTrace();
		}
		System.out.close();
		System.exit(0);
	}
}
