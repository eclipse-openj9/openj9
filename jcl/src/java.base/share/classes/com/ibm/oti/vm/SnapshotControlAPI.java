/*[INCLUDE-IF Sidecar18-SE]*/
package com.ibm.oti.vm;
/*******************************************************************************
 * Copyright (c) 2010, 2020 IBM Corp. and others
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

import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.PriorityQueue;

import com.ibm.oti.vm.SnapshotHook.SnapshotHookPriority;

/**
 * SnapshotControlAPI
 *
 */
public class SnapshotControlAPI {

	private static PriorityQueue<SnapshotHook> postRestoreHooks = new PriorityQueue<>();
	private static Object[] temparr = null;
	/**
	 *  Creates a snapshot immediately
	 * @param fileName
	 */
	public static void createSnapshot(String fileName) {
		throw new UnsupportedOperationException();
	}

	/**
	 * Registers a hook that will run before the snapshot is taken
	 *
	 * @param hook
	 */
	public static void registerPreSnapshotHooks(Runnable hook) {
		throw new UnsupportedOperationException();
	}

	/**
	 * Registers a hook that will run after the snapshot is restored and before the JVM application resumes
	 *
	 * @param hook
	 */
	public static void registerPostRestoreHooks(Runnable hook) {
		postRestoreHooks.add(new SnapshotHook(SnapshotHookPriority.MED, hook));
	}

	/**
	 * Registers a hook that will run after the snapshot is restored and before the JVM application resumes
	 *
	 * @param priority
	 * @param hook
	 */
	public static void registerHighPriorityPostRestoreHooks(Runnable hook) {
		postRestoreHooks.add(new SnapshotHook(SnapshotHookPriority.HIGH, hook));
	}

	private static void runPostRestoreHooks() {
		temparr = postRestoreHooks.toArray();
		Arrays.sort(temparr);
		for (Object hookWrapper : temparr) {
			((SnapshotHook)hookWrapper).getHook().run();
		}
	}
}
