/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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
package jit.test.vich;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import jit.test.vich.utils.Timer;

/**
 * @version 	1.0
 * @author
 */
public class JNIObjectArray {

	private static Logger logger = Logger.getLogger(JNIObjectArray.class);
	Timer timer;
	
	static {
		try {
			System.loadLibrary("j9ben");
		} catch (UnsatisfiedLinkError e) {}
	}

	public JNIObjectArray() {
		timer = new Timer ();
	}

	static final int loopCount = 100;
	static final int arraySize = 1000;
	
	public native void getObjectArrayElement(Object[] array, Object[] blankArray, int arraySize, int loopCount);

	
	public void fillArray(Object[] array)
	{
		for (int i = 0; i < array.length; i++)
		{
			array[i] = new Integer(0);
		}
	}
	
	@Test(groups = { "level.sanity","component.jit" })
	public void testJNIObjectArray()
	{
		Object[] array = new Object[arraySize];
		Object[] blankArray = new Object[arraySize];
		fillArray(array);
		
		
		try
		{
			getObjectArrayElement(array, blankArray, arraySize, 1);
			
		} catch (UnsatisfiedLinkError e) {
			Assert.fail("No natives for JNI tests");
		}
		
		timer.reset();
		getObjectArrayElement(array, blankArray, arraySize, loopCount);
		timer.mark();
		logger.info(loopCount + " GetObjectArrayElements calls (size " + arraySize + ") = " + timer.delta());
		
		
		
	}
}
