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
public class JNILocalRef {
	
	private static Logger logger = Logger.getLogger(JNILocalRef.class);
	Timer timer;

	static {
		try {
			System.loadLibrary("j9ben");
		} catch (UnsatisfiedLinkError e) {}
	}

	public JNILocalRef() {
		timer = new Timer ();
	}

	static final int loopCount = 100000;
	
	public native void localReference8(Object o1, Object o2, Object o3, Object o4, Object o5, Object o6, Object o7, Object o8, int n);
	public native void localReference32(Object o1, Object o2, Object o3, Object o4, Object o5, Object o6, Object o7, Object o8, Object o9, Object o10, Object o11, Object o12, Object o13, Object o14, Object o15, Object o16, Object o17, Object o18, Object o19, Object o20, Object o21, Object o22, Object o23, Object o24, Object o25, Object o26, Object o27, Object o28, Object o29, Object o30, Object o31, Object o32, int loopCount);
	
	@Test(groups = { "level.sanity","component.jit" })
	public void testJNILocalRef()
	{
		Object o1 = new Integer(0);
		Object o2 = new Integer(0);
		Object o3 = new Integer(0);
		Object o4 = new Integer(0);
		Object o5 = new Integer(0);
		Object o6 = new Integer(0);
		Object o7 = new Integer(0);
		Object o8 = new Integer(0);
		Object o9 = new Integer(0);
		Object o10 = new Integer(0);
		Object o11 = new Integer(0);
		Object o12 = new Integer(0);
		Object o13 = new Integer(0);
		Object o14 = new Integer(0);
		Object o15 = new Integer(0);
		Object o16 = new Integer(0);
		Object o17 = new Integer(0);
		Object o18 = new Integer(0);
		Object o19 = new Integer(0);
		Object o20 = new Integer(0);
		Object o21 = new Integer(0);
		Object o22 = new Integer(0);
		Object o23 = new Integer(0);
		Object o24 = new Integer(0);
		Object o25 = new Integer(0);
		Object o26 = new Integer(0);
		Object o27 = new Integer(0);
		Object o28 = new Integer(0);
		Object o29 = new Integer(0);
		Object o30 = new Integer(0);
		Object o31 = new Integer(0);
		Object o32 = new Integer(0);
		
		try
		{
			localReference8(o1, o2, o3, o4, o5, o6, o7, o8, 1);
			localReference32(o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12, o13, o14, o15, o16, o17, o18, o19, o20, o21, o22, o23, o24, o25, o26, o27, o28, o29, o30, o31, o32, 1);
		} catch (UnsatisfiedLinkError e) {
			Assert.fail("No natives for JNI tests");
		}
		
		timer.reset();
		localReference8(o1, o2, o3, o4, o5, o6, o7, o8, loopCount);
		timer.mark();
		logger.info(loopCount + " New/DeleteLocalRef calls (on 3x8 objects) = " + timer.delta());
		
		timer.reset();
		localReference32(o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12, o13, o14, o15, o16, o17, o18, o19, o20, o21, o22, o23, o24, o25, o26, o27, o28, o29, o30, o31, o32, loopCount);
		timer.mark();
		logger.info(loopCount + " New/DeleteLocalRef calls (on 2x32 objects) = " + timer.delta());
	}
}
