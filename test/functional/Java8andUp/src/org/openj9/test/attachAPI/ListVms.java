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

import java.util.List;

import com.sun.tools.attach.VirtualMachineDescriptor;
import com.sun.tools.attach.spi.AttachProvider;

@SuppressWarnings("nls")
public class ListVms {

	public static final String LIST_VMS_EXIT = "listVms:exit";

	public static void main(String[] args) {
		if (!TargetManager.waitForAttachApiInitialization()) {
			System.err.println("ListVms: attach API initialization failed");
		}
		listVms();
	}

	private static void listVms() {
		List<AttachProvider> providers = AttachProvider.providers();
		AttachProvider ap = providers.get(0);
		if (null == ap) {
			System.err.println("no attach providers available");
		}
		@SuppressWarnings("null")
		List<VirtualMachineDescriptor> vmds = ap.listVirtualMachines();
		System.out.println("looking for attach targets");
		for (VirtualMachineDescriptor vmd : vmds) {
			System.out.println("id: " + vmd.id() + " name: "
					+ vmd.displayName());
		}
		System.out.println(LIST_VMS_EXIT);
		System.out.flush();
		System.out.close();
		System.err.close();
	}

}
