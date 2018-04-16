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
import java.security.AccessControlException;
import java.security.Permission;
import java.security.Permissions;
import java.security.ProtectionDomain;
import java.util.PropertyPermission;

public class JEP140Tests {

	final static String	PROP_JAVA_HOME = "java.home";
	final static String	PROP_USER_DIR = "user.dir";
	final static PropertyPermission	READ_PROP_JAVA_HOME = new PropertyPermission(PROP_JAVA_HOME, "read");
	final static PropertyPermission	READ_PROP_USER_DIR = new PropertyPermission(PROP_USER_DIR, "read");
	final static Permissions PERMS_READ_PROP_JAVA_HOME = new Permissions();
	final static Permissions PERMS_READ_PROP_USER_DIR = new Permissions();
	final static AccessControlContext ACC_READ_PROP_JAVA_HOME;
	final static AccessControlContext ACC_READ_PROP_USER_DIR;
	final static Permission[] EMPTY_PERMISSION = new Permission[]{};
	final static Permission[] PERMISSION_WITH_NULL = new Permission[]{READ_PROP_JAVA_HOME, READ_PROP_USER_DIR, null};

	static {
		PERMS_READ_PROP_JAVA_HOME.add(READ_PROP_JAVA_HOME);
		ACC_READ_PROP_JAVA_HOME = new AccessControlContext(new ProtectionDomain[] {new ProtectionDomain(null, PERMS_READ_PROP_JAVA_HOME)});
		
		PERMS_READ_PROP_USER_DIR.add(READ_PROP_USER_DIR);
		ACC_READ_PROP_USER_DIR = new AccessControlContext(new ProtectionDomain[] {new ProtectionDomain(null, PERMS_READ_PROP_USER_DIR)});
	}
	
	public static void main(String[] args) {
		new JEP140Tests().testAll();
	}
	
	void testAll() {
		System.setSecurityManager(new SecurityManager());
		
		final CodeTrusted		ct = new CodeTrusted();

		testPreJEP140(ct);
		testLimiteddoPrivileged(ct);
		testLimitedPrivilegedNested(ct);
		testLimitedPrivilegedMixed(ct);
		testLimitedPrivilegedMixedRevertOrder(ct);
		
		System.out.println("All tests finished");
	}

	
	void testPreJEP140(final CodeTrusted ct) {
		try {
			ct.getPropertyNodoPrivileged(PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try { 
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try { 
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, ct.GetLastContext());
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivileged(PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try { 
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try { 
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, ct.GetLastContext());
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}

		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try { 
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try { 
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, ct.GetLastContext());
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, ct.GetLastContext());
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}

		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, null);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, ct.GetLastContext());
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		
		try {
			String propValue = ct.getPropertyPrivDC(PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned: " + propValue);
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");			
			ace.printStackTrace();
		}			
	}
	
	void testLimiteddoPrivileged(final CodeTrusted ct) {
		AccessControlContext	lastContext = null;
		
		System.out.println("Test: getPropertydoPrivilegedLimited " +
				"doPrivileged(PrivilegedAction<T> action, AccessControlContext context, Permission... perms)");
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, (Permission[])null);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): NPE expected!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		} catch (NullPointerException npe) {
			System.out.println("	PASS: NPE thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, PERMISSION_WITH_NULL);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): NPE expected!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		} catch (NullPointerException npe) {
			System.out.println("	PASS: NPE thrown as expected!");
		}
		
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
/*		
		try {
			ct.getPropertydoPrivilegedLimited(PROP_USER_DIR, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_USER_DIR, lastContext, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
*/
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, null, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, null);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimited(PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
	}
	
	void testLimitedPrivilegedNested(final CodeTrusted ct) {
		AccessControlContext	lastContext = null;
		
		System.out.println("Test: getPropertydoPrivilegedLimitedNested " +
				"Inner doPrivileged(PrivilegedAction<T> action, AccessControlContext context, Permission perms)" +
				"Outer doPrivileged(PrivilegedAction<T> action, AccessControlContext context, Permission... perms)");
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, READ_PROP_USER_DIR, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, READ_PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, null, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, null, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, null, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, null, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, null, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, null, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR, ACC_READ_PROP_USER_DIR, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, ACC_READ_PROP_USER_DIR, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, null, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, ACC_READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, null, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR, lastContext, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, null, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}		
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}		
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, null, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, null, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, null, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}

		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		lastContext = ct.GetLastContext();		
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedNested(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
	}

	void testLimitedPrivilegedMixed(final CodeTrusted ct) {
		AccessControlContext	lastContext = null;
		
		System.out.println("Test: getPropertydoPrivilegedLimitedMixed " +
				"Outer doPrivileged(PrivilegedAction<T> action, AccessControlContext context)" + 
				"Inner doPrivileged(PrivilegedAction<T> action, AccessControlContext context, Permission... perms)");
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_USER_DIR, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_USER_DIR, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, lastContext, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, null, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, null, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, lastContext, null);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}

		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, null, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, null, lastContext, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}

		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
		try {
			ct.getPropertydoPrivilegedLimitedMixed(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
		}
	}
	
	void testLimitedPrivilegedMixedRevertOrder(final CodeTrusted ct) {
		AccessControlContext	lastContext = null;
		
		System.out.println("Test: getProppertydoPrivilegedLimitedMixedRevertOrder " +
				"Inner doPrivileged(PrivilegedAction<T> action, AccessControlContext context)" +
				"Outer doPrivileged(PrivilegedAction<T> action, AccessControlContext context, Permission... perms)");
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}

		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception expected! ");
		} catch (AccessControlException ace) {
			System.out.println("	PASS: Security Exception thrown as expected!");
		}
		
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_USER_DIR, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, ACC_READ_PROP_USER_DIR, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_USER_DIR, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_JAVA_HOME, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, lastContext, READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}

		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_JAVA_HOME);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		lastContext = ct.GetLastContext();
		try {
			ct.checkLastContext(READ_PROP_JAVA_HOME);
			System.out.println("	PASS: checkPermission READ_PROP_JAVA_HOME succeed!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getPropertydoPrivilegedWithContext(PROP_JAVA_HOME, lastContext);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, lastContext, ACC_READ_PROP_JAVA_HOME, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
		try {
			ct.getProppertydoPrivilegedLimitedMixedRevertOrder(PROP_JAVA_HOME, ACC_READ_PROP_JAVA_HOME, lastContext, READ_PROP_USER_DIR);
			System.out.println("	PASS: property value returned!");
		} catch (AccessControlException ace) {
			System.out.println("	FAILED at line (" + currentLineNumber() + "): Security Exception NOT expected! ");
			ace.printStackTrace();
		}
	}

	int currentLineNumber() {
//		return Thread.currentThread().getStackTrace()[2].getLineNumber();	// oracle
		return Thread.currentThread().getStackTrace()[3].getLineNumber();	// j9
	}
}
