package com.ibm.j9.offload.tests.jvmservice;

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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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
public class JVMServiceTests extends TestCase {
	
	static String[] args = null;
	
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
						System.out.print(receivedData);
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
	
	/* this is used in the JVM_LatestUserDefinedLoader test */
	public class ObjectStreamClass extends ObjectInputStream {
		
		public ObjectStreamClass() throws IOException, SecurityException {
			super();
		}
		
		public Class getProxy(String[] interfaces)throws Exception{
			return(super.resolveProxyClass(interfaces));
		}
	}
	
	/**
	 * method that can be used to run the test by itself
	 * 
	 * @param args no valid arguments
	 */
	public static void main (String[] args) {
		JVMServiceTests.args = args;
		junit.textui.TestRunner.run(suite());
	}
	
	public static Test suite(){
		return new TestSuite(JVMServiceTests.class);
	}
	
	public void testJVM_FreeMemory(){
		Runtime theRuntime = Runtime.getRuntime();
		System.out.println("Free memory:" + theRuntime.freeMemory());
		assertTrue("fre memory should not be 0", theRuntime.freeMemory() != 0);
	}
	
	public void testJVM_TotalMemory(){
		Runtime theRuntime = Runtime.getRuntime();
		System.out.println("Max memory:" + theRuntime.totalMemory());
		assertTrue("total memory should not be 0", theRuntime.totalMemory() != 0);
	}
	
	public void testJVM_MaxMemory(){
		Runtime theRuntime = Runtime.getRuntime();
		System.out.println("Max memory:" + theRuntime.maxMemory());
		assertTrue("max memory should not be 0", theRuntime.maxMemory() != 0);
	}
	
	public void testJVM_GC(){
		Runtime theRuntime = Runtime.getRuntime();
		theRuntime.gc();
	}
	
	public void testJVM_LatestUserDefinedLoader(){
		try {
			ObjectStreamClass temp = new ObjectStreamClass();
			String[] inter = {"java.io.Closeable","java.io.DataInput"};
			temp.getProxy(inter);
		} catch (Exception e){
			fail("Unexpected exception:" + e);
		}
		
	}
	
	public void testJVM_GetClassContext(){
		SecurityManager clss = new SecurityManager();
		
		/* negative  case */
		try{
			clss.checkMemberAccess(java.io.FileOutputStream.class, Member.DECLARED);
			fail("Did not get expected access denied exception");
		} catch(AccessControlException ace){
			/* this is the expected case */
		}
		
		/* positive case */
		try{
			clss.checkMemberAccess(com.ibm.j9.offload.tests.jvmservice.JVMServiceTests.class, Member.DECLARED);
		} catch(AccessControlException ace){
			fail("Got unexpected access control exception");
		}
	}
	
	public void testJVM_GetSystemPackage(){
		assertTrue("failed to get java.lang package", Package.getPackage("java.lang") != null); 
	}
	
	public void testJVM_GetSystemPackages(){
		assertTrue("failed to get system packages", Package.getPackages() != null);
		Package javalang = Package.getPackage("java.lang");
		Package[] packages = Package.getPackages();
		boolean found = false;
		for(int i=0;i<packages.length;i++){
			if (packages[i] == javalang){
				found = true;
				break;
			}
		}
		assertTrue("java.lang package not found in system packages",found);
	}
	
	/* tests JVM_GetThreadInterruptEvent */
	public void testProcessWaitFor(){
		try {
			String javaExecuteCommand = System.getProperty("user.dir") + File.separator + "java -cp " +  System.getProperty("java.class.path") + " com.ibm.j9.offload.tests.jvmservice.Wait1Second";
			System.out.println("javaExecuteCommand:"+ javaExecuteCommand);
			Process p1 = Runtime.getRuntime().exec(javaExecuteCommand);
			ProcessOutputThread t = new ProcessOutputThread(p1.getInputStream());
			t.start();
			assertTrue("Did not get expected 0 response code", p1.waitFor()==0);
		} catch (Exception e) {
			fail("Unexpected Exception:" + e);
		}
	}
	
	public void testProcessWaitForInterrupted(){
		class ProcessInterrupt extends Thread {
			Thread _threadToInterrupt;
			public ProcessInterrupt(Thread threadToInterrupt){
				_threadToInterrupt = threadToInterrupt;
			}
			
			public void run() {
				try {
					System.out.println("Interruptor thread running");
					Thread.sleep(5000);
					System.out.println("Interrupting Thread");
					_threadToInterrupt.interrupt();
				} catch (InterruptedException e) {
					System.out.println("Interrupting thread was interrupted itself !!!!");
				}
			}
		}
		
		
		try {
			ProcessInterrupt interruptor = new ProcessInterrupt(Thread.currentThread());
			String javaExecuteCommand = System.getProperty("user.dir") + File.separator + "java -cp " +  System.getProperty("java.class.path") + " com.ibm.j9.offload.tests.jvmservice.Wait20Seconds";
			System.out.println("javaExecuteCommand:"+ javaExecuteCommand);
			Process p1 = Runtime.getRuntime().exec(javaExecuteCommand);
			ProcessOutputThread t = new ProcessOutputThread(p1.getInputStream());
			t.start();
			try {
				interruptor.start();
				p1.waitFor();
				fail("waitFor was not interrupted");
				
			} catch (InterruptedException e){
				/* this what we expect */
			}
		} catch (Exception e) {
			fail("Unexpected Exception:" + e);
		}
	}
	
	public void testJVM_Halt(){
		try {
			String javaExecuteCommand = System.getProperty("user.dir") + File.separator + "java -Xproxy:auto -cp " +  System.getProperty("java.class.path") + " com.ibm.j9.offload.tests.jvmservice.ExitWithReturnCode78";
			System.out.println("javaExecuteCommand:"+ javaExecuteCommand);
			Process p1 = Runtime.getRuntime().exec(javaExecuteCommand);
			ProcessOutputThread t = new ProcessOutputThread(p1.getInputStream());
			t.start();
			assertTrue("Did not get expected 78 response code", p1.waitFor()==78);
		} catch (Exception e) {
			fail("Unexpected Exception:" + e);
		}
	}
	
	public void testGetResourceFromJar(){
		try {
        	SAXParserFactory saxParserFactory =  SAXParserFactory.newInstance();
        	saxParserFactory.setValidating(true);
        	saxParserFactory.setNamespaceAware(false);
        	javax.xml.parsers.SAXParser parser = saxParserFactory.newSAXParser();
        	parser.parse(new FileInputStream(args[0] + File.separator + "VM" + File.separator + "bin" + File.separator + "offload" + File.separator + "xmlparse-testfile.xml"),new org.xml.sax.helpers.DefaultHandler() );
        	System.out.println("We parsed ok, which means we read a resource from the xml jar ok");
        } catch (Exception e){
        	fail("An exception occured when we tried to parse the xml file and get a resource from the xml jar:" + e);
        }
	}
	
}
