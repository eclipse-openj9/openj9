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

package com.ibm.j9.tests.jeptests;

import java.io.IOException;
import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;

/**
 * @file StaticAgents.java
 * @brief Tests J9's JVMTI native agent static linking capability, specifically, JEP178.
 * The class is provides programmatic way to load agents, thereby testing out the Live
 * phase agent attach mechanism.  Also, this same class is executed (skipping attaching
 * the agent) when testing agents during the OnLoad phase.
 */
public class StaticAgents {
	public static void main(String[] args) {
		if (args.length > 0 && args[0].equals("--attach")) {
			com.sun.tools.attach.VirtualMachine vm = null;
			RuntimeMXBean bean = ManagementFactory.getRuntimeMXBean();
			String jvmName = bean.getName();
			String agentA = new String("testjvmtiA");
			String agentB = new String("testjvmtiB");

			try {
				/* Fetch the PID of the current VM; use this to attach agents to. */
				vm = com.sun.tools.attach.VirtualMachine.attach(jvmName.split("@")[0]);
				System.out.println("[MSG] Attaching native agent testjvmtiA");
				vm.loadAgentLibrary(agentA, null);
				System.out.println("[MSG] Attaching native agent testjvmtiB");
				vm.loadAgentLibrary(agentB, null);

				System.out.println("[MSG] Testing jep178 for native agents during Live phase (OnAttach)");

				vm.detach();
			} catch(com.sun.tools.attach.AttachNotSupportedException except) {
			} catch(com.sun.tools.attach.AgentInitializationException except) {
			} catch(com.sun.tools.attach.AgentLoadException except) {
			} catch(IOException except) {
			}
		} else {
			System.out.println("[MSG] Testing jep178 for native agents at JVM startup (OnLoad)");
		}
	}
}
