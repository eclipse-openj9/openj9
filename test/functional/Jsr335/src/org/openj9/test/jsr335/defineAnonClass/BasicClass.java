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
package org.openj9.test.jsr335.defineAnonClass;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import org.testng.log4testng.Logger;

public class BasicClass {
	static int staticField = -1;
	int defaultField = -1;
	private static int privateStaticField = -1;
	private int privateField = -1;
	protected int protectedField = -1;
	final int finalField = -1;
	public int publicField = -1;
	private static Logger logger = Logger.getLogger(BasicClass.class);
		
	static int callStaticFunction() {
		return TestUnsafeDefineAnonClass.CORRECT_ANSWER;
	}
	
	int callFunction() {
		return TestUnsafeDefineAnonClass.CORRECT_ANSWER;
	}
	
	private int callPrivateFunction() {
		return TestUnsafeDefineAnonClass.CORRECT_ANSWER;
	}
	
	private static int callPrivateStaticFunction() {
		return TestUnsafeDefineAnonClass.CORRECT_ANSWER;
	}
	
	protected static int callProtectedStaticFunction() {
		return TestUnsafeDefineAnonClass.CORRECT_ANSWER;
	}
	
	protected int callProtectedFunction() {
		return TestUnsafeDefineAnonClass.CORRECT_ANSWER;
	}
	
	class BasicInnerClass {

		public int field8;
		private int field9;
		
		public BasicInnerClass() {
			logger.debug("constructed");
		}
		
		private void callFunction5() {
			logger.debug("private void callFunction5()");
		}
		
		public void callFunction6() {
			logger.debug("public void callFunction6()");
		}
	}
	
	static class BasicInnerClass2 {

		public int field8;
		private int field9;
		
		public BasicInnerClass2() {
			logger.debug("constructed");
		}
		
		private void callFunction5() {
			logger.debug("private void callFunction5()");
		}
		
		public void callFunction6() {
			logger.debug("public void callFunction6()");
		}
	}
	
	private class BasicInnerClass3 {

		public int field8;
		private int field9;
		
		public BasicInnerClass3() {
			logger.debug("constructed");
		}
		
		private void callFunction5() {
			logger.debug("private void callFunction5()");
		}
		
		public void callFunction6() {
			logger.debug("public void callFunction6()");
		}
	}
	
	protected class BasicInnerClass4 {

		public int field8;
		private int field9;
		
		public BasicInnerClass4() {
			logger.debug("constructed");
		}
		
		private void callFunction5() {
			logger.debug("private void callFunction5()");
		}
		
		public void callFunction6() {
			logger.debug("public void callFunction6()");
		}
	}
	
	public class BasicInnerClass5 {

		public int field8;
		private int field9;
		
		public BasicInnerClass5() {
			logger.debug("constructed");
		}
		
		private void callFunction5() {
			logger.debug("private void callFunction5()");
		}
		
		public void callFunction6() {
			logger.debug("public void callFunction6()");
		}
	}
	
	
	static private class BasicInnerClass6 {

		public int field8;
		private int field9;
		
		public BasicInnerClass6() {
			logger.debug("constructed");
		}
		
		private void callFunction5() {
			logger.debug("private void callFunction5()");
		}
		
		public void callFunction6() {
			logger.debug("public void callFunction6()");
		}
	}
}
