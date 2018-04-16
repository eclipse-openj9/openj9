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
package org.openj9.test.attachAPI;

import java.io.IOException;

import org.testng.Assert;
import org.testng.annotations.Test;

import com.ibm.tools.attach.target.AttachHandler;
import com.sun.tools.attach.AgentInitializationException;
import com.sun.tools.attach.AgentLoadException;
import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;

@Test(groups = { "level.extended" })
@SuppressWarnings({"nls"})
public class TestAttachExceptions extends AttachApiTest  {
	private static final String JVMTITST = "jvmtitst";

	@Test
	public void testAttachSelf() {
		if (!AttachHandler.waitForAttachApiInitialization()) {
			TargetManager.dumpLogs(true);
			Assert.fail("main process did not initialize attach API");
		}
		String myId = com.ibm.tools.attach.target.AttachHandler.getVmId();
		VirtualMachine vm;
		try {
			vm = VirtualMachine.attach(myId);
			vm.loadAgentLibrary(JVMTITST);
			vm.detach();
		} catch (AttachNotSupportedException | IOException | AgentLoadException | AgentInitializationException e) {
			logExceptionInfoAndFail(e);
		}
	}

	public static void main() {
		TestAttachExceptions to = new TestAttachExceptions();
		to.testAttachSelf();
	}
}
