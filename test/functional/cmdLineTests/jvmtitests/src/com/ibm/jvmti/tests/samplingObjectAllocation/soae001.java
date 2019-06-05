/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
package com.ibm.jvmti.tests.samplingObjectAllocation;

public class soae001 {
	private final static int DEFAULT_SAMPLING_RATE = 512 * 1024; /* 512 KB */
	
	private native static void reset();	/* reset native internal counters */
	private native static int enable();	/* enable event JVMTI_EVENT_SAMPLED_OBJECT_ALLOC */
	private native static int disable();	/* disable event JVMTI_EVENT_SAMPLED_OBJECT_ALLOC */
	private native static int check();	/* check how many times the event callback was invoked */
	
	public boolean testDefaultInterval() {
		boolean result = false;
		int jvmtiResult = 0;

		reset();
		jvmtiResult = enable();
		if (0 != jvmtiResult) {
			System.out.println("com.ibm.jvmti.tests.samplingObjectAllocation.soae001.enable() failed with: " + jvmtiResult);
		} else {
			byte[] bytes;
			bytes = new byte[DEFAULT_SAMPLING_RATE];
			System.out.println("Allocated a byte array with size " + bytes.length);
			
			int samplingResult = check();
			if (samplingResult < 1) {
				System.out.println("com.ibm.jvmti.tests.samplingObjectAllocation.soae001.check() failed, expected 1+ but got: " + samplingResult);
			} else {
				jvmtiResult = disable();
				if (0 != jvmtiResult) {
					System.out.println("com.ibm.jvmti.tests.samplingObjectAllocation.soae001.disable() failed with: " + jvmtiResult);
				} else {
					reset();
					bytes = new byte[DEFAULT_SAMPLING_RATE];
					System.out.println("Allocated another byte array with size " + bytes.length);
					samplingResult = check();
					if (0 != samplingResult) {
						System.out.println("com.ibm.jvmti.tests.samplingObjectAllocation.soae001.check() failed, expected 0 but got: " + samplingResult);
					} else {
						result = true;
					}
				}
			}
		}
		
		return result;
	}
	public String helpDefaultInterval() {
		return "Test that with default sampling interval the event callback is invoked once as expected after enabled, and not invoked if the event is disable.";
	}
}
