/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

package org.openj9.test.java.lang.management;

import org.testng.annotations.AfterClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeClass;
import org.testng.Assert;
import java.io.File;
import java.io.IOException;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.FileReader;
import java.io.UnsupportedEncodingException;
import java.io.FileNotFoundException;
import java.lang.management.ManagementFactory;
import java.net.MalformedURLException;
import java.util.ArrayList;
import java.util.List;
import javax.management.JMX;
import javax.management.MBeanServer;
import javax.management.MBeanServerConnection;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.remote.JMXConnector;
import javax.management.remote.JMXConnectorFactory;
import javax.management.remote.JMXServiceURL;

import com.ibm.lang.management.*;
import openj9.lang.management.*;
import org.openj9.test.management.ProcessLocking;

/**
 * Class for testing the APIs that are provided as part of OpenJ9DiagnosticsMXBean
 * Interface.
 *
 */

@Test(groups = { "level.extended" })
public class TestOpenJ9DiagnosticsMXBean {
	private static Logger logger = Logger.getLogger(TestOpenJ9DiagnosticsMXBean.class);
	private static String os = System.getProperty("os.name");
	private static Process remoteServer;
	private ObjectName mxbeanName = null;
	private OpenJ9DiagnosticsMXBean diagBean = null;
	private OpenJ9DiagnosticsMXBean diagBeanRemote = null;
	List<String> initialDumpOptions = new ArrayList<String>();
	private JMXConnector connector = null;
	private static ProcessLocking lock;
	private static String tmpFileName;

	@BeforeClass
	public void setUp() throws Exception {
		/* We need the object name in any case - remote as well as local. */
		try {
			mxbeanName = new ObjectName("openj9.lang.management:type=OpenJ9Diagnostics");
		} catch (MalformedObjectNameException e) {
			e.printStackTrace();
			Assert.fail("MalformedObjectNameException!");
		}

		initialDumpOptions = getDumpOptions();

		getLocalMXBean();

		getRemoteMXBean();
	}

	/**
	 * Function to get the dump options. 
	 *
	 */
	private List<String> getDumpOptions() throws IOException {
		List<String> list = new ArrayList<String>();
		String javaExec = System.getProperty("java.home") + File.separator + "bin" + File.separator + "java";
		ProcessBuilder builder = new ProcessBuilder(javaExec, "-Xdump:what", "-version");
		Process process = builder.start();
		builder.redirectErrorStream(true);
	
		BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
		String line;
		while(null != (line = reader.readLine())) {
			logger.info(line);
			list.add(line);
		}
		return list;
	}

	private void getLocalMXBean() {
		MBeanServer mbeanServer = null;
		mbeanServer = ManagementFactory.getPlatformMBeanServer();
		boolean registered = mbeanServer.isRegistered(mxbeanName);
		Assert.assertTrue(registered, "OpenJ9DiagnosticsMXBean is not registered. Cannot Proceed.");

		diagBean = JMX.newMXBeanProxy(mbeanServer, mxbeanName, OpenJ9DiagnosticsMXBean.class);
	} 

	private void getRemoteMXBean() throws IOException, InterruptedException {
		int retryCounter = 0;
		boolean isConnected = false;
		JMXServiceURL urlForRemoteMachine = null;
		MBeanServerConnection mbsc = null;

		/*
		 * A Start the remote server if it is not running yet. Also, attach a
		 * watch dog to this, to bail out, in case it hangs.
		 */
		if (null == remoteServer) {
			startRemoteServer();
		}

		/* Try connecting to the server; retry until limit reached. */
		while (retryCounter < 10 && !isConnected) {
			retryCounter++;
			try {
				urlForRemoteMachine = new JMXServiceURL("service:jmx:rmi:///jndi/rmi://localhost:9999/jmxrmi");

				/*
				 * Connect to remote host and create a proxy to be used to get
				 * an OpenJ9DiagnosticsMXBean instance.
				 */
				connector = JMXConnectorFactory.connect(urlForRemoteMachine, null);
				mbsc = connector.getMBeanServerConnection();

				/*
				 * Obtain an mbean handle using the connection and the object
				 * name for the class OpenJ9DiagnosticsMXBean.
				 */
				diagBeanRemote = JMX.newMXBeanProxy(mbsc, mxbeanName, OpenJ9DiagnosticsMXBean.class);
				boolean registered = mbsc.isRegistered(mxbeanName); 
				Assert.assertTrue(registered, "OpenJ9DiagnosticsMXBean is not registered. Cannot Proceed.");

				/*
				 * If we reached here, it means we are connected (no connection
				 * failure exception thrown).
				 */
				isConnected = true;
			} catch (MalformedURLException e) {
				e.printStackTrace();
				Assert.fail("Please check the url supplied to JMXServiceURL!");
			} catch (IOException e) {
				/*
				 * Waiting 1000 ms before retrying to connect to remote server.
				 */
				logger.error("Failed connecting. Retry " + retryCounter + " after 1000 ms.");

				try {
					Thread.sleep(1000);
				} catch (InterruptedException ie) {
					ie.printStackTrace();
					Assert.fail("Exception occurred while sleeping thread: " + ie.getMessage());
				}
			}
		}
		Assert.assertNotNull(diagBeanRemote, "OpenJ9DiagnosticsMXBean instance on a remote server could not be obtained");
	}
		
	@AfterClass
	public void tearDown() throws Exception {
		try {
			connector.close();
			lock.notifyEvent("closed JMX connection");
		} catch (Throwable t) {
			t.printStackTrace();
			Assert.fail("Failed to close JMX connection");
		}

		stopRemoteServer();

		/* Cleanup the dump directories */
		String[] dirs = { "local", "local_dumps", "remote", "remote_dumps" };
		for (String directory : dirs) {
			File fdir = new File("." + File.separator + directory);
			File[] files = fdir.listFiles();
			for (File file : files) {
				file.delete();
			}
			fdir.delete();
		}

		/* Cleanup the temp file */
		File file = new File(tmpFileName);
		boolean ret = file.delete();
		Assert.assertTrue(ret, "Deleting " + tmpFileName + " Failed!");
	}

	/**
	 * Function to test if the requested dumps are triggered on a local application.
	 *
	 */
	@Test	
	private void testLocal_triggerDump() throws FileNotFoundException, IOException {
		triggerDump(diagBean);
	}

	/**
	 * Function to test if the requested dumps are triggered to the specified file names.
	 *
	 */	
	@Test
	private void testLocal_triggerDumpToFile() throws InvalidOptionException, FileNotFoundException, IOException {
		triggerDumpToFile(diagBean, "local");
	}


	/**
	 * Function to test if the dump options were reset to what it was at JVM initialization on a local application.
	 *
	 */
	@Test	
	private void testLocal_resetDumpOptions() throws IOException, InvalidOptionException, ConfigurationUnavailableException {
		resetDumpOptions(diagBean);
	}

	/**
	 * Function to test if a classic heap dump is created.
	 *
	 */	
	@Test
	private void testLocal_triggerClassicHeapDump() throws InvalidOptionException, FileNotFoundException, IOException {
		triggerClassicHeapDump(diagBean);
	}

	/**
	 * Function to test if dump options can be set dynamically on a local process
	 *
	 */	
	@Test
	private void testLocal_setDumpOptions() throws InvalidOptionException, ConfigurationUnavailableException, FileNotFoundException, IOException {
		boolean found = false;
		String dir = "." + File.separator + "local";
		String dumpFileName = "javacore_alloc.txt";
		String dumpFilePath = dir + File.separator + dumpFileName;
		diagBean.resetDumpOptions();
		diagBean.setDumpOptions("java:events=allocation,filter=#1k,range=1..1,file=" + dumpFilePath);
		int[] a = new int[1024 * 1024];

		dumpFileName = "javacore_unsupported.txt";
		dumpFilePath = dir + File.separator + dumpFileName;
		int counter = 1;
		while(true) {
			try {
				diagBean.setDumpOptions("java:events=catch,filter=java/io/UnsupportedEncodingException,range=1..1,file=" + dumpFilePath);
				break;
			} catch (ConfigurationUnavailableException e) {
				// Ignore exception and try setting options again
				try {
					Thread.sleep(100);
				} catch (InterruptedException ie) {
					// Do nothing - the outer loop will retry if required
				}
				// Only attempt this 20 times before failing the test
				if (counter == 20) {
					e.printStackTrace();
					Assert.fail("Unable to update the dump options: " + e.getMessage());
				}
				counter++;
			}
		}

		try {
			new String("hello").getBytes("Unsupported");
			Assert.fail("Expected Exception - UnsupportedEncodingException did not occur!");
		} catch (UnsupportedEncodingException e) {
			/* Received the expected UnsupportedEncodingException */
		}
		found = findAndDeleteFile(dir, dumpFileName);
		Assert.assertTrue(found, "Fail - Set Dump Options failed as " + dumpFileName + " was not found");

		dumpFileName = "javacore_alloc.txt";
		found = findAndDeleteFile(dir, dumpFileName);
		Assert.assertTrue(found, "Fail - Set Dump Options failed as " + dumpFileName + " was not found");
	}

	/**
	 * Function to test if the requested dumps are triggered on a remote application.
	 *
	 */
	@Test
	private void testRemote_triggerDump() throws FileNotFoundException, IOException {
		triggerDump(diagBeanRemote);
	}

	/**
	 * Function to test if the requested dumps are triggered to the specified file names.
	 *
	 */
	@Test
	private void testRemote_triggerDumpToFile() throws InvalidOptionException, FileNotFoundException, IOException {
		triggerDumpToFile(diagBeanRemote, "remote");
	}

	/**
	 * Function to test if the dump options were reset to what it was at JVM initialization on a remote application.
	 *
	 */
	@Test	
	private void testRemote_resetDumpOptions() throws IOException, InvalidOptionException, ConfigurationUnavailableException, FileNotFoundException {
		resetDumpOptions(diagBeanRemote);			
	}

	/**
	 * Function to test if a classic heap dump is created.
	 *
	 */	
	@Test
	private void testRemote_triggerClassicHeapDump() throws InvalidOptionException, FileNotFoundException, IOException {
		triggerClassicHeapDump(diagBeanRemote);
	}

	/**
	 * Function to test if dump options can be set dynamically on a remote application
	 *
	 */	
	@Test
	private void testRemote_setDumpOptions() throws InvalidOptionException, ConfigurationUnavailableException, InterruptedException, FileNotFoundException, IOException {
		boolean found = false;
		String dir = "." + File.separator + "remote";
		String dumpFileName = "javacore_alloc.txt";
		String dumpFilePath = dir + File.separator + dumpFileName;
		diagBeanRemote.resetDumpOptions();
		diagBeanRemote.setDumpOptions("java:events=allocation,filter=#1k,range=1..1,file=" + dumpFilePath);
		dumpFileName = "javacore_unsupported.txt";
		dumpFilePath = dir + File.separator + dumpFileName;
		diagBeanRemote.setDumpOptions("java:events=catch,filter=java/io/UnsupportedEncodingException,range=1..1,file=" + dumpFilePath);
		lock.notifyEvent("dump settings done");
		lock.waitForEvent("events occurred");

		/* Check for the presence of java core files after the remote server has stopped */
		dumpFileName = "javacore_alloc.txt";

		found = findAndDeleteFile(dir, dumpFileName);
		Assert.assertTrue(found, "Fail - Set Dump Options failed as file " + dumpFileName + " was not found");

		dumpFileName = "javacore_unsupported.txt";
		found = findAndDeleteFile(dir, dumpFileName);
		Assert.assertTrue(found, "Fail - Set Dump Options failed as file " + dumpFileName + " was not found");
	}

	/**
	 * Function to test if the requested dumps are triggered.
	 *
	 * @param diagBean OpenJ9DiagnosticsMXBean instance that has already been initialized.
	 */
	private void triggerDump(OpenJ9DiagnosticsMXBean diagBean) throws FileNotFoundException, IOException {
		boolean found = false;
		String[] dumpAgents = { "java", "heap", "snap", "system", "stack" };
		String[] files = { "javacore", "heapdump", "Snap", "core" };
		int index = 0;

		for (String agent : dumpAgents) {
			try {
				diagBean.triggerDump(agent);
				if (!(agent.equals("stack"))) {
					if (isZOSSystemAgent(agent)) {
						/* checking for dataset presence will be included later */
					} else {
						found = findAndDeleteFile(".", files[index]);
						Assert.assertTrue(found, files[index] + " not found");
					}
				}
			} catch (java.lang.IllegalArgumentException e) {
				/* stack agent is not supported by the trigger method */
				if (agent.equals("stack")) {
					/* Do nothing as the expected exception IllegalArgumentException is received */
				} else {
					Assert.fail("Unexpected IllegalArgumentException : " + e.getMessage());
				}
			}
			index++;
		}
	}

	/**
	 * Function to test if the requested dumps are triggered to the specified file names.
	 *
	 * @param diagBean OpenJ9DiagnosticsMXBean instance that has already been initialized.
	 * @param test indicates if it is a local or remote test
	 */	
	private void triggerDumpToFile(OpenJ9DiagnosticsMXBean diagBean, String test) throws InvalidOptionException, FileNotFoundException, IOException {
		boolean found = false;
		String[] dumpAgents = { "java", "heap", "snap", "system", "stack" };
		String[] files = { "javacore.%Y%m%d.%H%M%S", "heapdump.%H%M%S", "Snap.%pid.trc", "core.%pid.dmp", "stack.%seq.txt" };
		int index = 0;
		String dir = "." + File.separator + test + "_dumps";


		for (String agent : dumpAgents) {
			try {
				String filePath = dir + File.separator + files[index];
				if (isZOSSystemAgent(agent)) {
					filePath = "%uid.MYJVM.%job.D%y%m%d.T%H%M%S";
				}
				String fileName = diagBean.triggerDumpToFile(agent, filePath);
				File dumpFile = new File(fileName);

				if (!(agent.equals("stack"))) {
					if (isZOSSystemAgent(agent)) {
						/* checking for dataset presence will be included later */
					} else {
						found = findAndDeleteFile(dir, dumpFile.getName());
						Assert.assertTrue(found, fileName + " not found");
					}
				} 
			} catch (java.lang.IllegalArgumentException e) {
				/* stack agent is not supported by the trigger method */
				if (agent.equals("stack")) {
					/* Do nothing as the expected exception IllegalArgumentException is received */
				} else {
					Assert.fail("Unexpected IllegalArgumentException : " + e.getMessage());
				}
			}
			index++;
		}
	}

	/**
	 * Function to test if a classic heap dump is created.
	 *
	 * @param diagBean OpenJ9DiagnosticsMXBean instance that has already been initialized.
	 */	
	private void triggerClassicHeapDump(OpenJ9DiagnosticsMXBean diagBean) throws InvalidOptionException, FileNotFoundException, IOException {
		boolean found = false;
		String fileName = diagBean.triggerClassicHeapDump();
		found = findAndDeleteFile(".", new File(fileName).getName());
		Assert.assertTrue(found, fileName + " not found");
	}	

	/**
	 * Function to test if the dump options were reset to what it was at JVM initialization.
	 *
	 * @param diagBean OpenJ9DiagnosticsMXBean instance that has already been initialized.
	 */	
	private void resetDumpOptions(OpenJ9DiagnosticsMXBean diagBean) throws IOException, InvalidOptionException, ConfigurationUnavailableException {
		diagBean.setDumpOptions("java+heap+system:events=vmstop");
		diagBean.resetDumpOptions();

		List<String> newDumpOptions = getDumpOptions();
		boolean res = initialDumpOptions.equals(newDumpOptions);
		
		Assert.assertTrue(res, "Reset Dump Options failed");
	}

	/**
	 * Function to check if the specified file is present in the directory path.
	 *
	 * @param dirPath Directory path where dumps are created.
	 * @param fileName Dump file name.
	 * @return a boolean indicating if the file was found or not.
	 */	
	private static boolean findAndDeleteFile(String dirPath, String fileName) throws FileNotFoundException, IOException {
		BufferedReader reader = null;
		boolean found = false;
		File dir = new File(dirPath);
		File[] files = dir.listFiles();

		for (File f : files) {
			found = f.getName().startsWith(fileName);
			if (found) {
				reader = new BufferedReader(new FileReader(f));
				reader.close();
				boolean res = f.delete();
				Assert.assertTrue(res, "Failed to delete the file");
				break;
			}
		}
		return found;
	}

	/**
	 * Function to check if it is zOS and system dump agent.
	 *
	 * @param agent Indicates the dump agent.
	 * @return a boolean indicating if it is zOS and system dump agent
	 */
	private static boolean isZOSSystemAgent(String agent) {
		if ("z/OS".equalsIgnoreCase(os) && "system".equals(agent)) {
			return true;
		} else {
			return false;
		}
	}

	/**
	 * Internal function: Starts a remote server to connect to.
	 */
	private static void startRemoteServer() throws IOException {
		logger.info("Starting Remote Server to Monitor!");
		char fs = File.separatorChar;
		/* Set up a randomly named file and pass this to child process. Parent keeps checking
		* for its existence on the file-system. When this goes missing, it implies the child
		* process is up and running.
		*/
		File file = File.createTempFile("tmp", ".lock");
		tmpFileName = file.getAbsolutePath();
		String classpath = System.getProperty("java.class.path");
		String jmxremoteOptions = System.getProperty("remote.server.option");
		String javaExec = System.getProperty("java.home") + File.separator + "bin" + File.separator + "java";

		List<String> processArgs = new ArrayList<String>();
		processArgs.add(javaExec);
		processArgs.add("-Xdump:dynamic");
		processArgs.add("-classpath");
		processArgs.add(classpath);

		Assert.assertNotNull(jmxremoteOptions, "Error: -Dremote.server.option system property cannot be null");
		String[] options = jmxremoteOptions.trim().split("\\s+");
		for (String opt : options) {
			processArgs.add(opt);
		}
		/* The lock file that the parent and child processes use for event based synchronization. */
		processArgs.add("-Djava.lock.file=" + tmpFileName);
		processArgs.add(RemoteProcess.class.getName());
		ProcessBuilder builder = new ProcessBuilder(processArgs);
		logger.info(builder.command());
		lock = new ProcessLocking(tmpFileName);
		remoteServer = builder.start();

		lock.waitForEvent("child started");
		logger.info("Staring remote server finished");
	} 

	/**
	 * Internal function: Stops a remote server.
	 */
	private static void stopRemoteServer() throws InterruptedException {
		remoteServer.destroy();
		remoteServer.waitFor();
	} 
}

