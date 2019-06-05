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
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;
import com.sun.tools.attach.VirtualMachineDescriptor;
import com.sun.tools.attach.spi.AttachProvider;

@SuppressWarnings("nls")
public class Props {

	private static boolean attachAll;
	private static boolean noProps;
	public static void main(String[] args) {
		ArrayList<String> vmidBuffer = new ArrayList<String>();
		String[] vmidList;
		int reps = 1;
		boolean getIterations = false;
		TargetManager.waitForAttachApiInitialization();

		for (String arg : args) {
			if (getIterations) {
				reps = Integer.parseInt(arg);
				getIterations = false;
				continue;
			}
			if (arg.startsWith("-")) {
				if (arg.equals("-a")) {
					attachAll = true;
				} else if (arg.equals("-i")) {
					getIterations = true;
				} else if (arg.equals("-p")) {
					noProps = true;
				}
			} else {
				vmidBuffer.add(arg);
			}
		}
		if (vmidBuffer.size() == 0) {
			if (attachAll) {
				vmidList = listVms();
			} else {
				System.err.println("no target specified");
				return;
			}
		} else {
			vmidList = vmidBuffer.toArray(new String[vmidBuffer.size()]);
		}
		for (int i = 0; i < reps; ++i) {
			getProps(vmidList);
		}
	}

	private static String[] listVms() {
		List<AttachProvider> providers = AttachProvider.providers();
		AttachProvider ap = providers.get(0);
		if (null == ap) {
			System.err.println("no attach providers available");
			return(null);
		}
		List<VirtualMachineDescriptor> vmds = ap.listVirtualMachines();
		ArrayList<String> vmidBuffer = new ArrayList<String>();
		for (VirtualMachineDescriptor vmd : vmds) {
			vmidBuffer.add(vmd.id());
		}
		return vmidBuffer.toArray(new String[vmidBuffer.size()]);
	}

	private static void getProps(String[] vmids) {
		for (String vmid : vmids) {
			System.out.println("#########################\nAttach to " + vmid);
			try {
				VirtualMachine vm = VirtualMachine.attach(vmid);
				if (!noProps) {
					Properties props = vm.getSystemProperties();
					System.out.println("System properties");
					for (String s : props.stringPropertyNames()) {
						String p = props.getProperty(s);
						System.out.println(s + "=" + p);
						System.out
								.println("----------------------------------------");
					}
				}
				vm.detach();
			} catch (AttachNotSupportedException e) {
				e.printStackTrace();
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
	}

}
