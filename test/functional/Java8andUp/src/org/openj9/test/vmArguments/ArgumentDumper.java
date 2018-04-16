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
package org.openj9.test.vmArguments;

import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.util.Properties;
import java.util.Set;

@SuppressWarnings("nls")
public class ArgumentDumper {
	public static final String USERARG_TAG = "2CIUSERARG";
	public static void main(String[] args) {

		System.out.println(USERARG_TAG);
		RuntimeMXBean bean = ManagementFactory.getRuntimeMXBean();
		for (String a: bean.getInputArguments()) {
			System.out.println(a);
		}
		System.out.print("\n===============\n\nSystem properties\n\n");
		Properties props = System.getProperties();
		Set<String> pns = props.stringPropertyNames();
		for (String pn: pns) {
			String p = props.getProperty(pn).replaceAll("\n", "\\n");
			System.out.print(pn);
			System.out.print("=");
			System.out.println(p);
		}
	}

}
