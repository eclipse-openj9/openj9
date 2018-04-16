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
package defect.cmvc198986;

import java.lang.reflect.*;
import java.security.AccessControlException;

public class ProxyFieldAccess {
	static final Class<?> signalClass = sun.misc.Signal.class;

	public static void main(String[] argv) throws Exception {
		new ProxyFieldAccess().test();
	}

	void test() {
		try {
			// create a proxy instance in a non-restricted package by
			// implementing a non-public interface
			Class<?> nonPublicIntf = Class.forName("java.util.Formatter$FormatString", false, null);
			Class<?> restrictedIntf = sun.misc.SignalHandler.class;
			Class<?>[] intfs = new Class<?>[] { nonPublicIntf, restrictedIntf };
			Object proxy = Proxy.newProxyInstance(null, intfs, new InvocationHandler() {
				public Object invoke(Object o, Method m, Object[] args) {
					return null;
				}
			});

			Method m = ClassLoader.class.getDeclaredMethod("findLoadedClass", String.class);
			m.setAccessible(true);
			ClassLoader cl = getClass().getClassLoader();
			System.out.println(cl);
			Object o = m.invoke(cl, "java.util.$Proxy0");
			if (o != null) {
				throw new Error("Test failed due to java.util.$Proxy0 already loaded.");
			}
			System.out.println(o);

			System.setSecurityManager(new SecurityManager());

			ProxyFieldAccess pfa = new ProxyFieldAccess();
			pfa.fieldAccess(proxy);
			pfa.methodAccess(proxy);

			//	        System.setSecurityManager(null);
			//	        m = ClassLoader.class.getDeclaredMethod("findLoadedClass", String.class);
			//	        m.setAccessible(true);
			o = m.invoke(getClass().getClassLoader(), "java.util.$Proxy0");
			if (o == null) {
				throw new Error("Test failed due to java.util.$Proxy0 NOT loaded.");
			}
			System.out.println(m.invoke(getClass().getClassLoader(), "java.util.$Proxy0"));

			System.out.println("ProxyFieldAccess test passed");
		} catch (ClassNotFoundException e) {
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
		} catch (SecurityException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	private void fieldAccess(Object proxy) throws Exception {
		try {
			Object o = ((java.util.$Proxy0)proxy).SIG_DFL;
			throw new Error("Proxy field access via static linking test failed");
		} catch (AccessControlException e) {
			/* ok */
		}

		try {
			Class<?> c = proxy.getClass();
			Field f = c.getField("SIG_DFL");
			throw new Error("Proxy field access via Class.getField test failed");
		} catch (AccessControlException e) {
			/* ok */
		}
	}

	private void methodAccess(Object proxy) throws Exception {
		try {
			((java.util.$Proxy0)proxy).handle(null);
			throw new Error("Proxy method access via static linking test failed");
		} catch (AccessControlException e) {
			/* ok */
		}

		try {
			Class<?> c = proxy.getClass();
			Method m = c.getMethod("handle", signalClass);
			throw new Error("Proxy method access via Class.getMethod test failed");
		} catch (AccessControlException e) {
			/* ok */
		}
	}
}
