package com.ibm.j9.offload.tests.runlocal;

/*******************************************************************************
 * Copyright (c) 2009, 2012 IBM Corp. and others
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

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import junit.framework.Test;
import junit.framework.TestSuite;
import junit.framework.TestCase;
import java.io.ObjectInputStream;
import java.security.AccessControlException;
import java.lang.reflect.Member;
import javax.xml.parsers.*;

/**
 * Unit tests to validate our JVMService 
 */
public class RunNativesLocallyTest extends TestCase {
	
	static String[] args = null;
	static String testOutput;
	
	class ProcessOutputThread extends Thread {
		InputStream theInput;
		
		ProcessOutputThread(InputStream theOutput){
			this.theInput = theOutput;
		}
		
		public void run() {
			try {
				byte buffer[] = new byte[1000];
				while(true){
					int length = theInput.read(buffer, 0,1000);
					if (length >0){
						String receivedData = new String(buffer,0,length);
						testOutput = testOutput + receivedData;
					} else {
						break;
					}
				}
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
	}
	
	public int accessibleField;
	
	/**
	 * method that can be used to run the test by itself
	 * 
	 * @param args no valid arguments
	 */
	public static void main (String[] args) {
		RunNativesLocallyTest.args = args;
		junit.textui.TestRunner.run(suite());
	}
	
	public static Test suite(){
		return new TestSuite(RunNativesLocallyTest.class);
	}
	
	
	/* tests JVM_GetThreadInterruptEvent */
	public void testNativeRunLocally(){
		try {
			String javaExecuteCommand = System.getProperty("user.dir") + File.separator + "java -Xint -Xproxy:auto -Dcom.ibm.proxy.verboseIsolation -cp " +  System.getProperty("java.class.path") + " com.ibm.j9.offload.tests.runlocal.TriggerMathNative";
			System.out.println("javaExecuteCommand:"+ javaExecuteCommand);
			Process p1 = Runtime.getRuntime().exec(javaExecuteCommand);
			ProcessOutputThread t = new ProcessOutputThread(p1.getInputStream());
			t.start();
			assertTrue("Did not get expected 0 response code", p1.waitFor()==0);
			// validate method that should run locally
			assertTrue("Output did not confirm native was remoted",testOutput.indexOf("JNI Intercept: Dispatching method=java/lang/StrictMath.acos") != -1);
			// validate method that should have been remoted
			assertTrue("Output did not confirm native was remoted",testOutput.indexOf("Remote Invocation: Dispatching method=java/lang/System.getEncoding") != -1);
		} catch (Exception e) {
			fail("Unexpected Exception:" + e);
		}
	}
	
}
