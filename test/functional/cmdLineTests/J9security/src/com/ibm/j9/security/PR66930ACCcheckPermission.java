/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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
package com.ibm.j9.security;

import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.security.AccessControlContext;
import java.security.AccessControlException;
import java.security.AccessController;
import java.security.CodeSource;
import java.security.DomainCombiner;
import java.security.Permission;
import java.security.PermissionCollection;
import java.security.PrivilegedAction;
import java.security.ProtectionDomain;
import java.security.SecurityPermission;
import java.util.PropertyPermission;

public class PR66930ACCcheckPermission {

	private static boolean	untrustedPDimpliesCalled = false;
	private final static PropertyPermission PERM_JAVA_VERSION_READ = new PropertyPermission("java.version", "read");
	private final static SecurityPermission PERM_CREATE_ACC = new SecurityPermission("createAccessControlContext");

	public static void main(String[] args) {
		new PR66930ACCcheckPermission().test();
	}

	void test() {
		final String	baseName = this.getClass().getName();
		ClassLoader	privilegedCL = new URLClassLoader(new URL[]{this.getClass().getProtectionDomain().getCodeSource().getLocation()}, null) {
			public PermissionCollection getPermissions(CodeSource cs) {
				PermissionCollection pc = super.getPermissions(cs);
				pc.add(PERM_CREATE_ACC);
				return pc;
			}
		};
		
		try {
			Class<?>	cls = Class.forName(baseName + "$PrivilegedClass", true, privilegedCL);
			Object	obj = cls.newInstance();
			Method	mt = cls.getMethod("test", AccessControlContext.class);
			ProtectionDomain pd1 = new ProtectionDomain(null, null) {
				public boolean implies(Permission perm) {
					untrustedPDimpliesCalled = true;
					System.out.println("Untrusted ProtectionDomain.implies() has been called");
					return true;
				}
			};

			System.setSecurityManager(new SecurityManager());
			
			AccessControlContext	accSimple = new AccessControlContext(new ProtectionDomain[]{pd1});
			untrustedPDimpliesCalled = false;
			accSimple.checkPermission(PERM_JAVA_VERSION_READ);
			if (!untrustedPDimpliesCalled) {
				System.out.println("FAILED: untrusted ProtectionDomain.implies() should have been called");
			}
			
			AccessControlContext	accInjected = AccessController.doPrivileged(new PrivilegedAction<AccessControlContext>() {
				public AccessControlContext run() {
					return AccessController.getContext();
				}
			}, accSimple);
			untrustedPDimpliesCalled = false;
			try {
				accInjected.checkPermission(PERM_JAVA_VERSION_READ);
				System.out.println("FAILED: AccessControlException NOT thrown");
			} catch (AccessControlException ace) {
				System.out.println("GOOD: AccessControlException is expected");
			}                                   
			if (untrustedPDimpliesCalled) {
				System.out.println("FAILED: untrusted ProtectionDomain.implies() should NOT be called");
			}
			
			mt.invoke(obj, accInjected);
			
			System.out.println("ALL TESTS FINISHED");
		} catch (Exception e) {
			e.printStackTrace();
			System.out.println("FAIL: TEST FAILED, probably setup issue.");
		}
	}
	
	public static class PrivilegedClass {
		public void test(final AccessControlContext accIncoming) {
			/*
			 * The stack looks like following:
				PR66930ACCcheckPermission$PrivilegedClass.test(AccessControlContext)	---> With SecurityPermission("createAccessControlContext") 	
				NativeMethodAccessorImpl.invoke0(Method, Object, Object[])	
				NativeMethodAccessorImpl.invoke(Object, Object[])	
				DelegatingMethodAccessorImpl.invoke(Object, Object[])	
				Method.invoke(Object, Object...)	
				PR66930ACCcheckPermission.test()	---> NO SecurityPermission("createAccessControlContext")	
				PR66930ACCcheckPermission.main(String[])	
			 * 
			 */
			AccessController.doPrivileged(new PrivilegedAction<Void>() {
				public Void run() {
					try {
						// following actions are allowed because the caller has the required SecurityPermission("createAccessControlContext")
						new AccessControlContext(accIncoming, new DomainCombiner() {
							public ProtectionDomain[] combine(ProtectionDomain[] arg0, ProtectionDomain[] arg1) {
								return null;
							}
						});
						AccessController.checkPermission(PERM_CREATE_ACC);
						AccessController.getContext().checkPermission(PERM_JAVA_VERSION_READ);
					} catch (AccessControlException ace) {
						System.out.println("FAILED: AccessControlException is NOT expected");
					}
					try {
						// The AccessControlContext captured earlier is not affected by current caller stack
						accIncoming.checkPermission(PERM_JAVA_VERSION_READ);
						System.out.println("FAILED: AccessControlException NOT thrown");
					} catch (AccessControlException ace) {
						System.out.println("GOOD: AccessControlException is expected");
					}
					return null;
				}
			});
			// following actions are NOT allowed because some caller in the stack don't have the required SecurityPermission("createAccessControlContext")
			// i.e., a full stack walk was performed in following security checks.
			try {
				new AccessControlContext(accIncoming, new DomainCombiner() {
					public ProtectionDomain[] combine(ProtectionDomain[] arg0, ProtectionDomain[] arg1) {
						return null;
					}
				});
				System.out.println("FAILED: AccessControlException NOT thrown");
			} catch (AccessControlException ace) {
				System.out.println("GOOD: AccessControlException is expected");
			}
			try {
				AccessController.checkPermission(PERM_CREATE_ACC);
				System.out.println("FAILED: AccessControlException NOT thrown");
			} catch (AccessControlException ace) {
				System.out.println("GOOD: AccessControlException is expected");
			}
			try {
				// The AccessControlContext captured earlier is not affected by current caller stack
				accIncoming.checkPermission(PERM_JAVA_VERSION_READ);
				System.out.println("FAILED: AccessControlException NOT thrown");
			} catch (AccessControlException ace) {
				System.out.println("GOOD: AccessControlException is expected");
			}
		}
	}
}
