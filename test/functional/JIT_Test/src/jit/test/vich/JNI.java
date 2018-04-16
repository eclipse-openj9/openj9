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

public class JNI {
	private static Logger logger = Logger.getLogger(JNI.class);
	Timer timer;

	static {
		try {
			System.loadLibrary("j9ben");
		} catch (UnsatisfiedLinkError e) {}
	}
	static final int loopCount = 100000;

public JNI() {
	timer = new Timer ();
}

public native void nativeJNI();
public native void nativeJNI(int p1);
public native void nativeJNI(int p1, int p2);
public native void nativeJNI(int p1, int p2, int p3);
public native void nativeJNI(int p1, int p2, int p3, int p4);
public native void nativeJNI(int p1, int p2, int p3, int p4, int p5);
public native void nativeJNI(int p1, int p2, int p3, int p4, int p5, int p6);
public native void nativeJNI(int p1, int p2, int p3, int p4, int p5, int p6, int p7);
public native void nativeJNI(int p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8);
public native void nativeJNI(Object p1);
public native void nativeJNI(Object p1, Object p2);
public native void nativeJNI(Object p1, Object p2, Object p3);
public native void nativeJNI(Object p1, Object p2, Object p3, Object p4);
public native void nativeJNI(Object p1, Object p2, Object p3, Object p4, Object p5);
public native void nativeJNI(Object p1, Object p2, Object p3, Object p4, Object p5, Object p6);
public native void nativeJNI(Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7);
public native void nativeJNI(Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7, Object p8);
@Test(groups = { "level.sanity","component.jit" })
public void testJNI() {
	int i;
	try {
		System.loadLibrary("j9ben");
		nativeJNI(); // try them all first to make sure they exist.
		nativeJNI(1);
		nativeJNI(1, 2);
		nativeJNI(1, 2, 3);
		nativeJNI(1, 2, 3, 4);
		nativeJNI(1, 2, 3, 4, 5);
		nativeJNI(1, 2, 3, 4, 5, 6);
		nativeJNI(1, 2, 3, 4, 5, 6, 7);
		nativeJNI(1, 2, 3, 4, 5, 6, 7, 8);
		nativeJNI();
		nativeJNI("1");
		nativeJNI("1", "2");
		nativeJNI("1", "2", "3");
		nativeJNI("1", "2", "3", "4");
		nativeJNI("1", "2", "3", "4", "5");
		nativeJNI("1", "2", "3", "4", "5", "6");
		nativeJNI("1", "2", "3", "4", "5", "6", "7");
		nativeJNI("1", "2", "3", "4", "5", "6", "7", "8");
	} catch (UnsatisfiedLinkError e) {
		Assert.fail("No natives for JNI tests");
	}
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI();
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI(1);
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(1); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI(1, 2);
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(1, 2); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI(1, 2, 3);
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(1, 2, 3); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI(1, 2, 3, 4);
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(1, 2, 3, 4); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI(1, 2, 3, 4, 5);
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(1, 2, 3, 4, 5); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI(1, 2, 3, 4, 5, 6);
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(1, 2, 3, 4, 5, 6); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI(1, 2, 3, 4, 5, 6, 7);
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(1, 2, 3, 4, 5, 6, 7); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI(1, 2, 3, 4, 5, 6, 7, 8);
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(1, 2, 3, 4, 5, 6, 7, 8); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI("1");
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(\"1\"); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI("1", "2");
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(\"1\", \"2\"); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI("1", "2", "3");
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(\"1\", \"2\", \"3\"); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI("1", "2", "3", "4");
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(\"1\", \"2\", \"3\", \"4\"); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI("1", "2", "3", "4", "5");
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(\"1\", \"2\", \"3\", \"4\", \"5\"); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI("1", "2", "3", "4", "5", "6");
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(\"1\", \"2\", \"3\", \"4\", \"5\", \"6\"); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI("1", "2", "3", "4", "5", "6", "7");
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(\"1\", \"2\", \"3\", \"4\", \"5\", \"6\", \"7\"); = "+ Long.toString(timer.delta()));
	timer.reset();
	for (i = 0; i < loopCount; i++) {
		nativeJNI("1", "2", "3", "4", "5", "6", "7", "8");
	}
	timer.mark();
	logger.info(Integer.toString(loopCount) + " calls to nativeJNI(\"1\", \"2\", \"3\", \"4\", \"5\", \"6\", \"7\", \"8\"); = "+ Long.toString(timer.delta()));
	return;
}
}
