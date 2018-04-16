package org.openj9.test.java.lang.invoke.helpers;

/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

import java.lang.invoke.*;

public class Helper_MethodHandleInfoOtherPackagePublic {
	public static MethodHandles.Lookup lookup() { return MethodHandles.lookup(); }

	private String privateMethod() { return "privateMethod"; }
	protected String protectedMethod() { return "protectedMethod"; }
	String packageMethod() { return "packageMethod"; }
	public String publicMethod() { return "publicMethod"; }

	public static MethodHandle privateClassMethodHandle() throws Throwable {
		return lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePrivate.class, "publicMethod", MethodType.methodType(String.class));
	}

	public static MethodHandle publicClassMethodHandle() throws Throwable {
		return lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePublic.class, "publicMethod", MethodType.methodType(String.class));
	}
}
