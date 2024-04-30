/*
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.access;

import java.lang.reflect.Field;
import java.security.ProtectionDomain;

import jdk.internal.misc.Unsafe;

/**
 * Helper for creating anonymous/hidden classes.
 */
public final class UnsafeClasses {

	private static final Unsafe unsafe = Unsafe.getUnsafe();

	public static Class<?> defineAnonClass(Class<?> hostClass, byte[] bytes) throws IllegalAccessException {
		return unsafe.defineAnonymousClass(hostClass, bytes, null);
	}

	public static Class<?> defineAnonOrHiddenClass(Class<?> hostClass, byte[] bytes) throws IllegalAccessException {
		return unsafe.defineAnonymousClass(hostClass, bytes, null);
	}

	public static Class<?> defineClass(String name, byte[] bytes, int offset, int length,
			ClassLoader loader, ProtectionDomain domain) {
		return unsafe.defineClass(name, bytes, offset, length, loader, domain);
	}

}
