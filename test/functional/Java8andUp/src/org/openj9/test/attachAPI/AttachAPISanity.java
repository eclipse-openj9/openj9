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
package org.openj9.test.attachAPI;

import java.io.IOException;
import java.util.List;
import java.util.Properties;

import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;

import com.sun.tools.attach.VirtualMachine;
import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.spi.AttachProvider;

@SuppressWarnings({"nls"})
@Test(groups = { "level.sanity" })
public class AttachAPISanity extends AttachApiTest {

	private final String VENDOR_NAME_PROP = "vendor.name";
	private String myVmid;

	@Test(groups = { "level.sanity" })
	public void testAttachToSelf() {
		logger.debug("testAttachToSelf");

		try {
			VirtualMachine selfVm = VirtualMachine.attach(myVmid);
			Properties vmProps = selfVm.getSystemProperties();
			String vendor = vmProps.getProperty(VENDOR_NAME_PROP);
			String myVendor = System.getProperty(VENDOR_NAME_PROP);
			AssertJUnit.assertEquals("my vendor: " + myVmid + " vendor from properties:"
					+ vendor, myVendor, vendor);
			selfVm.detach();
		} catch (AttachNotSupportedException | IOException e) {
			logExceptionInfoAndFail(e);
		}
	}

	@Test(groups = { "level.sanity" })
	public void testAttachToOtherProcess() {
		logger.debug("testAttachToOtherProcess");
		launchAndTestTargets(1);
	}

	/**
	 * Test attaching to this VM when there are other VMs running
	 */
	@Test(groups = { "level.sanity" })
	public void testAttachToSelfAndOthers() {
		final int numberOfTargets = 4;
		logger.debug("starting " + testName);
		TargetManager tgts[] = new TargetManager[numberOfTargets];
		for (int i = 0; i < numberOfTargets; ++i) {
			tgts[i] = new TargetManager(TestConstants.TARGET_VM_CLASS, null);
			tgts[i].syncWithTarget();
		}
		List<AttachProvider> providers = AttachProvider.providers();
		AttachProvider myProvider = providers.get(0); /*
		 * there is only one
		 * provider currently
		 */
		try {
			for (TargetManager t : tgts) {
				logger.debug("attach to other VM: " + t.targetId);
				VirtualMachine vm = myProvider.attachVirtualMachine(t.targetId);
				vm.detach();
				logger.debug("attach to self: " + myVmid);
				vm = myProvider.attachVirtualMachine(myVmid);
				vm.detach();
			}
		} catch (AttachNotSupportedException | IOException e) {
			logExceptionInfoAndFail(e);
		}
	}

	@BeforeMethod
	protected void setUp() {
		if (!TargetManager.waitForAttachApiInitialization()) {
			TargetManager.dumpLogs(true);
			Assert.fail("main process did not initialize attach API");
		}
		myVmid = org.openj9.test.attachAPI.TargetManager.getVmId();
	}
}
