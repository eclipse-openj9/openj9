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

import com.sun.tools.attach.AgentInitializationException;
import com.sun.tools.attach.AgentLoadException;
import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;

@SuppressWarnings("nls")
public class TestLoadAgentPath {
	/*
	 * A sample AgentLibrary can be created by exporting a jar file of th
	 * 
	 * /**
	 * 
	 * @param args target [agent [options]]
	 */
	public static void main(String[] args) {
		String agent = "";
		String target = "";
		String options = null;
		if (args.length < 1) {
			fail("TestLoadAgent target [agent [options]]");
		} else {
			target = args[0];
		}
		if (args.length > 1) {
			agent = args[1];
		}
		if (args.length > 2) {
			options = args[2];
		}
		try {
			VirtualMachine vm = VirtualMachine.attach(target);
			if (null == options) {
				vm.loadAgentPath(agent);
			} else {
				vm.loadAgentPath(agent, options);
			}
			vm.detach();
		} catch (AttachNotSupportedException e) {
			fail("AttachNotSupportedException");
		} catch (IOException e) {
			fail("IOException");
		} catch (AgentLoadException e) {
			fail(e.toString());
		} catch (AgentInitializationException e) {
			fail(e.toString());
		}
	}

	private static void fail(String msg) {
		System.err.println(msg);
		java.lang.System.exit(1);
	}

}
