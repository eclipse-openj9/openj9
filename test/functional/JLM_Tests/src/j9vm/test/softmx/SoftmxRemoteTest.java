/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

package j9vm.test.softmx;


import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.util.*;
import javax.management.*;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.management.*;
import java.net.MalformedURLException;
import java.net.ServerSocket;

import javax.management.remote.*;
import javax.management.ObjectName;
import com.ibm.lang.management.MemoryMXBean;

/*
 * Start the RemoteTestServer with softmx enabled and jmx enabled before run the test.
 */
@Test(groups = { "level.extended" })
public class SoftmxRemoteTest{

	private static final Logger logger = Logger.getLogger(SoftmxRemoteTest.class);

	JMXConnector connector;
	MBeanServerConnection mbeanConnection;
    ObjectName beanName;
    com.ibm.lang.management.MemoryMXBean ibmBean;

    private  static Process remoteServer;
    private  static ChildWatchdog watchdog;


	@BeforeMethod
	protected void setUp() {

		if (remoteServer == null){
			startRemoteServer();
			watchdog = new ChildWatchdog(remoteServer);
			waitRemoteServer();
		}

		JMXServiceURL urlForRemoteMachine;
		MemoryMXBean mxbean = null;

		try {
			urlForRemoteMachine = new JMXServiceURL("service:jmx:rmi:///jndi/rmi://localhost:9999/jmxrmi");
			// connect to the remote host and create a proxy than can be used to get/set the maxHeapSize attribute
			// this is the attribute which adjusts the softmx value
			connector = JMXConnectorFactory.connect(urlForRemoteMachine,null);
			mbeanConnection = connector.getMBeanServerConnection();
			beanName = new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME);
			mxbean = JMX.newMXBeanProxy(mbeanConnection,beanName, MemoryMXBean.class);
		    // cast downwards so that we can use the IBM-specific functions
			ibmBean = (com.ibm.lang.management.MemoryMXBean) mxbean;

		} catch (MalformedURLException e) {
			e.printStackTrace();
			Assert.fail("Please check your JMXServiceURL!");
		} catch (MalformedObjectNameException e) {
			e.printStackTrace();
			Assert.fail("Get MalformedObjectNameException!");
		} catch (IOException e) {
			e.printStackTrace();
			Assert.fail("Get IOException!");
		}
		AssertJUnit.assertNotNull("JMX.newMXBeanProxy() returned null : TEST FAILED IN setUp()", mxbean);
	}

	@Test
	public void testJMXSetHeapSize() {

		long max_size = ibmBean.getMaxHeapSize();
		long max_limit = ibmBean.getMaxHeapSizeLimit();

		if (ibmBean.isSetMaxHeapSizeSupported()) {
			logger.debug("    Current max heap size:  " + max_size + " bytes");
			logger.debug("    Max heap size limit:    " + max_limit + " bytes");
			if (max_size < max_limit) {
				long reset_size = (max_size + max_limit) / 2;
				logger.debug("    Reset heap size to:    " + reset_size + " bytes");
				ibmBean.setMaxHeapSize(reset_size);
				AssertJUnit.assertEquals(reset_size, ibmBean.getMaxHeapSize());
				logger.debug("    Current max heap size is reset to:  " + reset_size + " bytes");
			} else {
				Assert.fail("Warning: current maximum heap size reach maximum heap size limit,it can't be reset!");
			}
		} else {
			Assert.fail("Warning: VM doesn't support runtime reconfiguration of the maximum heap size!");
		}
	}

	@Test
	public void testJMXSetHeapSizeBiggerThanLimit(){

		long max_limit = ibmBean.getMaxHeapSizeLimit();
		if (ibmBean.isSetMaxHeapSizeSupported()){
			logger.debug ( "	Max heap size limit:    " + max_limit + " bytes");
			logger.debug ( "	Current max heap size:  " + ibmBean.getMaxHeapSize() + " bytes");
			logger.debug ( "	Reset max heap size to: " + (max_limit + 1024) + " bytes");
			try {
				ibmBean.setMaxHeapSize(max_limit + 1024);
				Assert.fail("	FAIL: Expected to get an Exception while trying to set max sixe bigger than max heap size limit!");
			} catch (java.lang.IllegalArgumentException e){
				logger.debug ("	PASS: get below expected exception while trying to set max sixe bigger than max heap size limit!");
				logger.debug("Expected exception", e);
			} catch (java.lang.UnsupportedOperationException  e){
				logger.warn ("	java.lang.UnsupportedOperationException: this operation is not supported!");
			} catch (java.lang.SecurityException e){
				logger.warn ("	java.lang.SecurityException: if a SecurityManager is being used and the caller does not have the ManagementPermission value of control");
			} catch (Exception e){
				logger.error("Unexpected exception ", e);
				Assert.fail("Unexpected exception " + e);
			}
		}else{
			Assert.fail("	Warning: VM doesn't support runtime reconfiguration of the maximum heap size!");
		}
	}


	@Test
	public void testJMXSetHeapSizeSmallerThanMin(){

		long min_size = ibmBean.getMinHeapSize();
		if (ibmBean.isSetMaxHeapSizeSupported()){
			logger.debug ( "	Current min heap size:  " + min_size + " bytes");
			logger.debug ( "	Reset min heap size to: " + (min_size - 1024) + " bytes");

			try {
				ibmBean.setMaxHeapSize(min_size - 1024);
				Assert.fail("	FAIL: Expected to get an exception while trying to set max sixe smaller than min heap size!");
			}  catch (java.lang.IllegalArgumentException e){
				logger.debug ("	PASS: get below expected exception while trying to set max sixe bigger than max heap size limit!");
				logger.debug("Expected exception", e);
			} catch (java.lang.UnsupportedOperationException  e){
				logger.warn ("	java.lang.UnsupportedOperationException: this operation is not supported!");
			} catch (java.lang.SecurityException e){
				logger.warn ("	java.lang.SecurityException: if a SecurityManager is being used and the caller does not have the ManagementPermission value of control");
			} catch (Exception e){
				logger.error("Unexpected exception ", e);
				Assert.fail("Unexpected exception " + e);
			}
		}else{
			Assert.fail("	Warning: VM doesn't support runtime reconfiguration of the maximum heap size!");
		}

	}


	/* Steps of the test:
	 * 1. SoftmxRemoteTest sends signal to RemoteTestServer by resetting max heap size to max heap size limit, RemoteTestServer receive the signal,
	 * start allocate objects to force memory used by the heap to 80% of current max heap size;
	 * 2. RemoteTestServer sends signal to SoftmxRemoteTest by decreasing max heap size 1024 bytes when allocation is done;
	 * 3. SoftmxRemoteTest then rest max heap size to 50% of original size, RemoteTestServer trigger gc to shrink heap, verify the heap shrinks to
	 * the reset value within 5 minutes.
	 * 4. If heap shrinks within 5 minutes, RemoteTestServer will try to use 120% of reset max heap size. If RemoteTestServer gets the expected OutOfMemoryError,
	 * it increases max heap size 1024 bytes to notify the SoftmxRemoteTest.*/
	 @Test
	public void testSoftmxHasEffects(){

		Monitor inMonitor = new Monitor(remoteServer.getInputStream());
		Monitor errorMonitor = new Monitor(remoteServer.getErrorStream());
		errorMonitor.start();
		inMonitor.start();

		logger.debug( "	Current max heap size:  " + ibmBean.getMaxHeapSize() + " bytes");
		logger.debug("	Reset max heap size equal to max heap size limit to notify RemoteTestServer to start allocate object.");

		ibmBean.setMaxHeapSize(ibmBean.getMaxHeapSizeLimit());
		logger.debug( "	Current max heap size:  " + ibmBean.getMaxHeapSize() + " bytes");


		/*PrintWriter out = new PrintWriter(new BufferedWriter(
				new OutputStreamWriter(remoteServer.getOutputStream())), true);*/

		logger.debug( "	Current max heap size:  " + ibmBean.getMaxHeapSize() + " bytes");
		logger.debug(" 	Start Allocating New Object to approximately 80% of current max heap size.");


		//RemoteTestServer will decrease max heap size to indicate allocation done, waiting for maximum 2 mins
		long startTime = System.currentTimeMillis();
		boolean isDone = false;
		while((System.currentTimeMillis() - startTime) < 120000 ){
			if (ibmBean.getMaxHeapSizeLimit() - ibmBean.getMaxHeapSize() ==  1024){
				logger.debug( "	RemoteTestServer finish allocating objects!");
				isDone = true;
				break;
			}else{
				try {
					Thread.sleep(100);
					logger.debug( "	Current max heap size  " + ibmBean.getMaxHeapSize() + " bytes");
				} catch (InterruptedException e) {
					e.printStackTrace();
					Assert.fail("	FAIL: Catch InterruptedException");
				}
			}
		}

		AssertJUnit.assertTrue("	RemoteTestServer can't allocate objects within 2 mins!", isDone);

		long new_max_size = (long) (ibmBean.getMaxHeapSize() * 0.5);
		logger.debug("	Reset maximum heap size to 50% of original size: " + new_max_size +
				" to notify RemoteTestServer to force aggressive GC. Wait to check if heap shrinks"
				+ " and cannot grow beyond reset max limit.");
		ibmBean.setMaxHeapSize(new_max_size);
		long actual_max_size = ibmBean.getMaxHeapSize();
		logger.debug("	Actual reset maximum heap size is "+ibmBean.getMaxHeapSize());

		boolean isPassed = false;
		startTime = System.currentTimeMillis();

		/* After heap size shrinks, the RemoteTestServer will try to use 120% of reset max heap size. If it gets OOM as expected,
		 * it will increase max heap size 1024 bytes to notify SoftmxRemoteTest that the test is passed. */
		while((System.currentTimeMillis() - startTime) < 360000 ){
			if (ibmBean.getMaxHeapSize() -  actual_max_size == 1024){
				logger.debug( "	PASS: Heap shrank and cannot grow beyond reset max limit.");
				isPassed = true;
				break;
			}else{
				try {
					Thread.sleep(10);
				} catch (InterruptedException e) {
					e.printStackTrace();
					Assert.fail("	FAIL: Catch InterruptedException");
				}
			}
		}

		AssertJUnit.assertTrue (" 	FAIL: Did not recevie notification of test passing from RemotetestServer in 6 mins.", isPassed);

		inMonitor.stop();
		errorMonitor.stop();

		stopRemoteServer();
	}

	@AfterMethod
	protected void tearDown() throws Exception {
		connector.close();
		watchdog.interrupt();
		remoteServer.destroy();
		try {
			Thread.sleep(10000);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}

	private void startRemoteServer() {

		// check port availability before binding
		AssertJUnit.assertTrue("port 9999 is not available!!!", checkPort(9999));

		logger.debug("Start Remote Server!");

		ArrayList<String> argBuffer = new ArrayList<String>();
		char fs = File.separatorChar;

		String javaExec = System.getProperty("java.home")+fs+"bin"+fs+ "java";
		argBuffer.add(javaExec);

		argBuffer.add("-classpath");
		argBuffer.add(System.getProperty("java.class.path"));

		argBuffer.add(System.getProperty("remote.server.option"));

		argBuffer.add(RemoteTestServer.class.getName());

		String cmdLine[] = new String[argBuffer.size()];
		argBuffer.toArray(cmdLine);

		String cmds = "";
		for (int i = 0; i < cmdLine.length; i++)
			cmds = cmds + cmdLine[i] + " ";


		logger.debug(cmds);

		try {
			remoteServer = Runtime.getRuntime().exec(cmds);
		} catch (IOException e) {
			e.printStackTrace();
		}

		AssertJUnit.assertNotNull("failed to launch child process", remoteServer);

	}

	/*
	 * After RemoteTestserver child process starts, wait until the server is ready.
	 * Check whether the specified port (i.e. 9999) is in use in order to check if the server is ready.
	 * If the server is not ready after 2 mins, max wait time, destroy the child process and fail the test.
	 */
	private void waitRemoteServer() {
		int waitLimit = 120;
		int waitcount = waitLimit;
		while (true == checkPort(9999))
		{
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			if (0 == --waitcount) {
				watchdog.interrupt();
				remoteServer.destroy();
				try {
					Thread.sleep(10000);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
				remoteServer = null;
				Assert.fail("The server is not ready after " + waitLimit + " seconds!!!");
			}
		}
	}

	private void stopRemoteServer(){
		try {
			connector.close();
		} catch (IOException e1) {
			e1.printStackTrace();
		}
		watchdog.interrupt();
		remoteServer.destroy();
		try {
			Thread.sleep(10000);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}


	private class ChildWatchdog extends Thread {
		Process child;

		public ChildWatchdog(Process child) {
			super();
			this.child = child;
		}

		@Override
		public void run() {
			try {
				sleep(300000);
				child.destroy();
				Assert.fail("child hung");
			} catch (InterruptedException e) {
				return;
			}
		}
	}

	private class Monitor extends Thread {

		private BufferedReader inputReader;
		Monitor(InputStream is) {
			this.inputReader = new BufferedReader(new InputStreamReader(is));
		}
		public void run() {
			String inLine;
			try {
				while (null != (inLine = inputReader.readLine())) {
					logger.debug(inLine);
				}
			} catch (IOException e) {
				e.printStackTrace();
				Assert.fail("unexpected IOException");
			}
		}
	}

	protected static boolean checkPort(int port) {
	    ServerSocket ss = null;
	    try {
	        ss = new ServerSocket(port);
	        return true;
	    } catch (IOException e) {
	    } finally {
	        if (ss != null) {
	            try {
	                ss.close();
	            } catch (IOException e) {
	            }
	        }
	    }
		return false;
	}
}
