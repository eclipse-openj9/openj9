/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

package org.openj9.test.attachAPI;

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.lang.reflect.Method;
import java.util.Properties;

import javax.management.remote.JMXConnector;
import javax.management.remote.JMXConnectorFactory;
import javax.management.remote.JMXServiceURL;

import org.openj9.test.util.StringPrintStream;
import org.testng.Assert;
import org.testng.annotations.AfterMethod;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;

import com.sun.tools.attach.VirtualMachine;

@Test(groups = { "level.extended" })
@SuppressWarnings("nls")
public class TestManagementAgent extends AttachApiTest {
	private String myProcessId;
	final boolean dumpLogs = TestUtil.getDumpLogs();
	private File commonDir;

	@SuppressWarnings("boxing")
	@BeforeMethod
	protected void setUp(Method testMethod) {
		testName = testMethod.getName();
		/*
		 * Wait for attachAPI to initialize otherwise these tests may start
		 * before they should.
		 *
		 * waitForAttachApiInitialization() will return false when a default
		 * timeout is reached.
		 */
		if (null == commonDir) { /* lazy initialization */
			commonDir = new File(System.getProperty("java.io.tmpdir"),
					".com_ibm_tools_attach");
		}
		if (!TargetManager.waitForAttachApiInitialization()) {
			TargetManager.dumpLogs(true);
			myProcessId = Long.toString(TargetManager.getProcessId());
			File myAdvert = new File(commonDir, myProcessId);
			if (myAdvert.exists()) {
				logger.error(myAdvert.getAbsolutePath()
						+ " already exists and cannot be deleted");
			}
			Assert.fail("main process did not initialize attach API, PID = "
					+ myProcessId);
		}
		logger.debug("\n------------------------------------------------------------\ntime = ");
		logger.debug(System.currentTimeMillis());
		String myId = TargetManager.getVmId();
		if ((null == myId) || (myId.length() == 0)) {
			logger.error("attach API failed to initialize");
			if (!commonDir.exists()) {
				TargetManager.dumpLogs();
				Assert.fail("Could not create common directory "
						+ commonDir.getAbsolutePath());
			} else if (!commonDir.canWrite()) {
				TargetManager.dumpLogs();
				Assert.fail("Could not write common directory "
						+ commonDir.getAbsolutePath());
			} else {
				TargetManager.dumpLogs();
				Assert.fail("Could not initialize attach API");
			}
		}
	}

	@AfterMethod
	protected void tearDown() {
		if (dumpLogs) {
			TargetManager.dumpLogs();
		}
		TargetManager.setLogging(false);
	}

	@Test
	public void test_startLocalManagementAgent() {
		logger.debug("starting " + getName());
		TargetManager target = launchTarget();
		String targetPid = target.getTargetPid();
		logger.debug("launched " + targetPid);
		try {
			VirtualMachine vm = VirtualMachine.attach(targetPid);
			String response = vm.startLocalManagementAgent();
			logger.debug("response="+response);
			logger.debug("Connecting");
			JMXServiceURL agentURL = new JMXServiceURL(response);
			verifyManagementAgent(vm, agentURL);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("unexpected exception " + e.getMessage() + " in " + targetPid);
		} finally {
			target.terminateTarget();
		}
		logger.debug("ending " + getName());
	}

	@Test
	public void test_startManagementAgent() {
		logger.debug("starting " + getName());
		TargetManager target = launchTarget();
		String targetPid = target.getTargetPid();
	logger.debug("launched " + targetPid);
		try {
			VirtualMachine vm = VirtualMachine.attach(targetPid);
			Properties localAgentProperties = new Properties();
			final String portNum = "5678";
			localAgentProperties.put("com.sun.management.jmxremote.port", portNum);
			localAgentProperties.put("com.sun.management.jmxremote.authenticate", "false");
			localAgentProperties.put("com.sun.management.jmxremote.local.only", "false");
			localAgentProperties.put("com.sun.management.jmxremote.ssl", "false");

			vm.startManagementAgent(localAgentProperties);
			JMXServiceURL agentURL = new JMXServiceURL("service:jmx:rmi:///jndi/rmi://:"+portNum+"/jmxrmi");
			verifyManagementAgent(vm, agentURL);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("unexpected exception " + e.getMessage() + " in " + targetPid);
		} finally {
			target.terminateTarget();
		}
		logger.debug("ending " + getName());
	}

	private void verifyManagementAgent(VirtualMachine vm, JMXServiceURL agentURL)
			throws IOException {
		JMXConnector agentConnection = JMXConnectorFactory.connect(agentURL, null);
		agentConnection.connect();
		String connId = agentConnection.getConnectionId();
		logger.debug("getConnectionId="+connId);
		Properties remoteAgentProperties = vm.getAgentProperties();
		logger.debug("remoteAgentProperties=");
		PrintStream buff = StringPrintStream.factory();
		remoteAgentProperties.list(buff);
		logger.debug(buff.toString());
	}

	private String getName() {
		return testName;
	}

}
