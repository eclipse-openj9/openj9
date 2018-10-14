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
package com.ibm.j9.jep140;

import java.security.AccessControlContext;
import java.security.AccessController;
import java.security.DomainCombiner;
import java.security.Permission;
import java.security.Permissions;
import java.security.PrivilegedAction;
import java.security.ProtectionDomain;
import java.util.PropertyPermission;

public class CodeTrusted {
	AccessControlContext	lastGetContext = null;
	
	public AccessControlContext GetLastContext() {
		return lastGetContext;
	}

	public void checkLastContext(Permission perm) {
		if (null == lastGetContext)	{
			throw new NullPointerException();
		}
		lastGetContext.checkPermission(perm);
	}
	
	public void getPropertyNodoPrivileged(final String prop) {
		System.out.println("Codetrusted - getPropertyNodoPrivileged prop = " + prop);
		lastGetContext = AccessController.getContext();
		System.out.println(prop + " is: " + System.getProperty(prop));
	}

	public void getPropertydoPrivileged(final String prop) {
		AccessController.doPrivileged(new PrivilegedAction<Void>() {
			public Void run() {
				System.out.println("Codetrusted - getPropertydoPrivileged prop = " + prop);
				lastGetContext = AccessController.getContext();
				System.out.println(prop + " is: " + System.getProperty(prop));
				return	null;
			}
		});
	}

	public void getPropertydoPrivilegedWithContext(final String prop, final AccessControlContext acc) {
		AccessController.doPrivileged(new PrivilegedAction<Void>() {
			public Void run() {
				System.out.println("Codetrusted - getPropertydoPrivilegedWithContext prop = " + prop);
				lastGetContext = AccessController.getContext();
				System.out.println(prop + " is: " + System.getProperty(prop));
				return null;
			}
		}, acc);
	}
	
	public void getPropertydoPrivilegedLimited(final String prop, final AccessControlContext acc, final Permission... perms) {
		AccessController.doPrivileged(new PrivilegedAction<Void>() {
			public Void run() {
				System.out.println("Codetrusted - getPropertydoPrivilegedLimited prop = " + prop);
				lastGetContext = AccessController.getContext();
				System.out.println(prop + " is: " + System.getProperty(prop));
				return null;
			}
		}, acc, perms);
	}

	public void getPropertydoPrivilegedLimitedNested(final String prop, 
			final AccessControlContext accInner, final Permission permInner,
			final AccessControlContext accOuter, final Permission... permOuter
	) {
		AccessController.doPrivileged(new PrivilegedAction<Void>() {
			public Void run() {
				if (null != permInner) {
					System.out.println("Codetrusted - getPropertydoPrivilegedLimitedNested (null != permInner) prop = " + prop);
					getPropertydoPrivilegedLimited(prop, accInner, permInner);
				} else {
					System.out.println("Codetrusted - getPropertydoPrivilegedLimitedNested (null == permInner) prop = " + prop);
					getPropertydoPrivilegedLimited(prop, accInner);
				}
				return null;
			}
		}, accOuter, permOuter);
	}

	public void getPropertydoPrivilegedLimitedMixed(final String prop, 
			final AccessControlContext accOuter,
			final AccessControlContext accInner, final Permission... permInner 
	) {
		AccessController.doPrivileged(new PrivilegedAction<Void>() {
			public Void run() {
				System.out.println("Codetrusted - getPropertydoPrivilegedLimitedMixed prop = " + prop);
				getPropertydoPrivilegedLimited(prop, accInner, permInner);
				return null;
			}
		}, accOuter);
	}

	public void getProppertydoPrivilegedLimitedMixedRevertOrder(final String prop, 
			final AccessControlContext accInner, 
			final AccessControlContext accOuter, final Permission... permOuter
	) {
		AccessController.doPrivileged(new PrivilegedAction<Void>() {
			public Void run() {
				System.out.println("Codetrusted - getProppertydoPrivilegedLimitedMixedRevertOrder prop = " + prop);
				getPropertydoPrivilegedWithContext(prop, accInner);
				return null;
			}
		}, accOuter, permOuter);
	}
	
	public String getPropertyPrivDC(final String prop) {
		return AccessController.doPrivilegedWithCombiner(new PrivilegedAction<String>() {
			public String run() {
				System.out.println("Codetrusted - getPropertyPrivDC prop = " + prop);
				return System.getProperty(prop);
			}
		});
	}
	
/*	
	public String getPropertyPrivDC(final String prop) {
		String result = AccessController.doPrivilegedWithCombiner(new PrivilegedAction<String>() {
			public String run() {
				String result = System.getProperty(prop);
				System.out.println("Codetrusted - getPropertyPrivDC - getProperty " + prop + " is: " + result);
				return result;
			}
		});
		return result;
	}
*/	
	
	class MyDomainCombinerYes implements DomainCombiner {
		public ProtectionDomain[] combine(ProtectionDomain[] currentDomains,
				ProtectionDomain[] assignedDomains) {
			Permissions perms = new Permissions();
			perms.add(new PropertyPermission("java.home", "read"));
			return new ProtectionDomain[]{ new ProtectionDomain(null, perms)};
		}
	}
	
	class MyDomainCombinerNo implements DomainCombiner {
		public ProtectionDomain[] combine(ProtectionDomain[] currentDomains,
				ProtectionDomain[] assignedDomains) {
			Permissions perms = new Permissions();
			perms.add(new PropertyPermission("java.version", "read"));
			return new ProtectionDomain[]{ new ProtectionDomain(null, perms)};
		}
	}
	
	public String getPropertyPrivWithCombNo(final String prop) {
		AccessControlContext	acc1 = new AccessControlContext(new ProtectionDomain[] {new ProtectionDomain(null, new Permissions())});
		AccessControlContext	acc = new AccessControlContext(acc1, new MyDomainCombinerNo());
		String	result = AccessController.doPrivileged(new PrivilegedAction<String>() {
			public String run() {
				String result = AccessController.doPrivilegedWithCombiner(new PrivilegedAction<String>() {
					public String run() {
						String result = System.getProperty(prop);
						System.out.println("Codetrusted - getPropertyPrivWithCombNo - getProperty " + prop + " is: " + result);
						return result;
					}
				});
				return	result;
			}
		}, acc);
		return result;
	}

	public String getPropertyPrivWithCombYes(final String prop) {
		AccessControlContext	acc1 = new AccessControlContext(new ProtectionDomain[] {new ProtectionDomain(null, new Permissions())});
		AccessControlContext	acc = new AccessControlContext(acc1, new MyDomainCombinerYes());
		String	result = AccessController.doPrivileged(new PrivilegedAction<String>() {
			public String run() {
				String result = AccessController.doPrivilegedWithCombiner(new PrivilegedAction<String>() {
					public String run() {
						String result = System.getProperty(prop);
						System.out.println("Codetrusted - getPropertyPrivWithCombYes - getProperty " + prop + " is: " + result);
						return result;
					}
				});
				return	result;
			}
		}, acc);
		return result;
	}
	
	public String getPropertyNewPrivWithCombNoLimitedNo(final String prop) {
		AccessControlContext	acc1 = new AccessControlContext(new ProtectionDomain[] {new ProtectionDomain(null, new Permissions())});
		AccessControlContext	acc = new AccessControlContext(acc1, new MyDomainCombinerNo());
		String result = AccessController.doPrivilegedWithCombiner(new PrivilegedAction<String>() {
			public String run() {
				String result = System.getProperty(prop);
				System.out.println("Codetrusted - getPropertyNewPrivWithCombNoLimitedNo - getProperty " + prop + " is: " + result);
				return result;
			}
		}, acc);
		return result;
	}

	public String getPropertyNewPrivWithCombNoLimitedYes(final String prop) {
		AccessControlContext	acc1 = new AccessControlContext(new ProtectionDomain[] {new ProtectionDomain(null, new Permissions())});
		AccessControlContext	acc = new AccessControlContext(acc1, new MyDomainCombinerNo());
		String result = AccessController.doPrivilegedWithCombiner(new PrivilegedAction<String>() {
			public String run() {
				String result = System.getProperty(prop);
				System.out.println("Codetrusted - getPropertyNewPrivWithCombNoLimitedYes - getProperty " + prop + " is: " + result);
				return result;
			}
		}, acc, new PropertyPermission("java.home", "read"));
		return result;
	}
	
	public String getPropertyNewPrivWithCombYesLimitedNo(final String prop) {
		AccessControlContext	acc1 = new AccessControlContext(new ProtectionDomain[] {new ProtectionDomain(null, new Permissions())});
		AccessControlContext	acc = new AccessControlContext(acc1, new MyDomainCombinerYes());
		String result = AccessController.doPrivilegedWithCombiner(new PrivilegedAction<String>() {
			public String run() {
				String result = System.getProperty(prop);
				System.out.println("Codetrusted - getPropertyNewPrivWithCombYesLimitedNo - getProperty " + prop + " is: " + result);
				return result;
			}
		}, acc);
		return result;
	}

	public String getPropertyNewPrivWithCombYesLimitedYes(final String prop) {
		AccessControlContext	acc1 = new AccessControlContext(new ProtectionDomain[] {new ProtectionDomain(null, new Permissions())});
		AccessControlContext	acc = new AccessControlContext(acc1, new MyDomainCombinerYes());
		String result = AccessController.doPrivilegedWithCombiner(new PrivilegedAction<String>() {
			public String run() {
				String result = System.getProperty(prop);
				System.out.println("Codetrusted - getPropertyNewPrivWithCombYesLimitedYes - getProperty " + prop + " is: " + result);
				return result;
			}
		}, acc, new PropertyPermission("java.home", "read"));
		return result;
	}
	
}
