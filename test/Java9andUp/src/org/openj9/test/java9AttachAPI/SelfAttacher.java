/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *******************************************************************************/
package org.openj9.test.java9AttachAPI;

import java.io.IOException;
import java.util.Properties;

import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;
import com.sun.tools.attach.VirtualMachineDescriptor;

public class SelfAttacher {

	static final int ATTACH_ERROR_CODE = 10;
	static final int ATTACH_NOT_SUPPORTED_CODE = 11;
	static final int ATTACH_SELF_IOEXCEPTION_CODE = 12;
	static final int ATTACH_SELF_API_SUCCEEDED_CODE = 13;
	public static void main(String[] args) {
		try {
			String myId = VmIdGetter.getVmId();
			System.err.println("myId="+myId);
			boolean found = false;
			for (int i = 0; i <10 && !found; ++i) {
				Thread.sleep(100);
				for (VirtualMachineDescriptor v: VirtualMachine.list()) {
					if (v.id().equals(myId)) {
						found = true;
						break;
					}
				}
			}
			VirtualMachine vm = VirtualMachine.attach(myId);
			Properties props = vm.getSystemProperties();
			props.list(System.out);
		} catch (AttachNotSupportedException e) {
			e.printStackTrace();
			System.exit(ATTACH_NOT_SUPPORTED_CODE);
		} catch (IOException e) {
			e.printStackTrace();
			System.exit(ATTACH_SELF_IOEXCEPTION_CODE);
		} catch (InterruptedException e) {
			e.printStackTrace();
			System.exit(ATTACH_ERROR_CODE);
		}
		System.exit(ATTACH_SELF_API_SUCCEEDED_CODE);
	}

}
