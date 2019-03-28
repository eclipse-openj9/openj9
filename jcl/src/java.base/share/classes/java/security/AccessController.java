/*[INCLUDE-IF Sidecar16]*/
/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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
package java.security;

import java.security.AccessControlContext.AccessCache;

/*[IF Sidecar19-SE]
import jdk.internal.reflect.CallerSensitive;
/*[ELSE]*/
import sun.reflect.CallerSensitive;
/*[ENDIF]*/

/**
 * Checks access to system resources. Supports marking of code
 * as privileged. Makes context snapshots to allow checking from
 * other contexts.
 *
 * @author      OTI
 * @version     initial
 */
public final class AccessController {
	static {
		// Initialize vm-internal caches
		initializeInternal();
	}

	static final int OBJS_INDEX_ACC = 0;
	static final int OBJS_INDEX_PDS = 1;
	static final int OBJS_ARRAY_SIZE = 3;
	static final int OBJS_INDEX_PERMS_OR_CACHECHECKED = 2;

private static native void initializeInternal();
/*[PR CMVC 197399] Improve checking order */
private static final SecurityPermission createAccessControlContext = new SecurityPermission("createAccessControlContext"); //$NON-NLS-1$

/* [PR CMVC 188787] Enabling -Djava.security.debug option within WAS keeps JVM busy */
static final class DebugRecursionDetection {
	private static ThreadLocal<String> tlDebug = new ThreadLocal<>();
	static ThreadLocal<String> getTlDebug() {
		return tlDebug;
	}
}

/*[PR 1FDIC6B] J9JCL:WIN95 - AccessController missing private no-arg constructor */
/**
 * Prevents this class from being instantiated.
 */
private AccessController() {
	super();
}

/**
 * The object array returned has following format:
 *
 * Pre-JEP140 format: AccessControlContext/ProtectionDomain..., and the length of the object array is NOT divisible by OBJS_ARRAY_SIZE
 *  First element is an AccessControlContext object which might be null, either from a full permission privileged frame or from current thread if there is no such privileged frames
 *  ProtectionDomain elements after AccessControlContext object could be in one of following two formats:
 *  For doPrivileged methods - flag forDoPrivilegedWithCombiner is false:
 *      the ProtectionDomain element might be null, first ProtectionDomain element is a duplicate of the ProtectionDomain of the caller of doPrivileged
 *      rest of ProtectionDomain elements are from the callers discovered during stack walking
 *      the start index of the actual ProtectionDomain element is 2 of the object array returned
 *  For doPrivilegedWithCombiner methods - flag forDoPrivilegedWithCombiner is true:
 *      there are only two ProtectionDomain elements, first one is the ProtectionDomain of the caller of doPrivileged
 *      and the other is the ProtectionDomain of the caller of doPrivilegedWithCombiner
 *      the fourth element of the object array is NULL for padding to ensure that the length of the object array is NOT divisible by OBJS_ARRAY_SIZE either
 *
 * JEP 140 format: AccessControlContext/ProtectionDomain[]/Permission[]
 *  The length of the object array is always divisible by OBJS_ARRAY_SIZE.
 *  Depends on number of limited permission privileged frames, the result are in following format:
 *      First element is an AccessControlContext object
 *      Second element could be in one of following two formats:
 *          For doPrivileged methods - flag forDoPrivilegedWithCombiner is false:
 *              an array of ProtectionDomain objects in which first ProtectionDomain element is a duplicate of the ProtectionDomain of the caller of doPrivileged
 *              the start index of the actual ProtectionDomain element is 1 of this ProtectionDomain objects array
 *          For doPrivilegedWithCombiner methods - flag forDoPrivilegedWithCombiner is true:
 *              an array of ProtectionDomain objects with only two elements
 *              first one is the ProtectionDomain of the caller of doPrivileged
 *              and the other is the ProtectionDomain of the caller of doPrivilegedWithCombiner
 *      Third element is an array of Limited Permission objects
 *      Repeating this format:
 *          AccessControlContext object,
 *          ProtectionDomain objects array with same format above when flag forDoPrivilegedWithCombiner is false
 *           or just the ProtectionDomain of the caller of doPrivileged in case of flag forDoPrivilegedWithCombiner is true
 *          Permission object array
 *       Until a full permission privileged frame or the end of the stack reached.
 *
 * Note: 1. The reason to have Pre-JEP140 and JEP 140 format is to keep similar format and processing logic
 *          when there is no limited doPrivileged method (JEP 140 implementation) involved.
 *          This helped to address performance issue raised by JAZZ 66091: Perf work for LIR 28261 (Limited doPrivileged / JEP 140)
 *       2. The reason to duplicate the ProtectionDomain object of the caller of doPrivileged is to avoid creating a new object array
 *          without NULL and duplicate ProtectionDomain objects discovered during stack walking while still keeping same order of
 *          those objects.
 *
 * A full permission privileged frame is any frame running one of the following methods:
 *
 * <code><ul>
 * <li>java/security/AccessController.doPrivileged(Ljava/security/PrivilegedAction;)Ljava/lang/Object;</li>
 * <li>java/security/AccessController.doPrivileged(Ljava/security/PrivilegedExceptionAction;)Ljava/lang/Object;</li>
 * <li>java/security/AccessController.doPrivileged(Ljava/security/PrivilegedAction;Ljava/security/AccessControlContext;)Ljava/lang/Object;</li>
 * <li>java/security/AccessController.doPrivileged(Ljava/security/PrivilegedExceptionAction;Ljava/security/AccessControlContext;)Ljava/lang/Object;</li>
 * </ul></code>
 *
 * A limited permission privileged frame is any frame running one of the following methods:
 *
 * <code><ul>
 * <li>java/security/AccessController.doPrivileged(Ljava/security/PrivilegedAction;Ljava/security/AccessControlContext;[Ljava/security/Permission;)Ljava/lang/Object;</li>
 * <li>java/security/AccessController.doPrivileged(Ljava/security/PrivilegedExceptionAction;Ljava/security/AccessControlContext;[Ljava/security/Permission;)Ljava/lang/Object;</li>
 * </ul></code>
 *
 * @param depth The stack depth at which to start. Depth 0 is the current frame (the caller of this native).
 * @param forDoPrivilegedWithCombiner The flag to indicate if it is for doPrivilegedWithCombiner method
 *
 * @return an Object[] as description above
 */
private static native Object[] getAccSnapshot(int depth, boolean forDoPrivilegedWithCombiner);

/**
 * This native retrieves the ProtectionDomain object of the non-reflection/MethodHandleInvoke caller as per depth specified.
 * For example, when depth is set to 1 for method doPrivilegedWithCombiner, the ProtectionDomain object of the caller of doPrivilegedWithCombiner is returned
 * Note: it is only for limited doPrivilegedWithCombiner methods for now.
 *
 * @param depth The stack depth at which to start.
 *
 * @return a ProtectionDomain object as per description above
 */
private static native ProtectionDomain getCallerPD(int depth);

/**
 * provide debug info according to debug settings before throwing AccessControlException
 *
 * @param debug     overall debug flag returned from DebugRecursionDetection.getTlDebug()
 * @param perm      the permission to check
 * @param pDomain   the pDomain to check
 * @param createACCdenied   if true, actual cause of this ACE was SecurityPermission("createAccessControlContext") denied
 * @exception   AccessControlException  always throw an AccessControlException
 */
private static void throwACE(boolean debug, Permission perm, ProtectionDomain pDomain, boolean createACCdenied) {
	if (debug) {
		DebugRecursionDetection.getTlDebug().set(""); //$NON-NLS-1$
		AccessControlContext.debugPrintAccess();
		if (createACCdenied) {
			System.err.println("access denied " + perm + " due to untrusted AccessControlContext since " + createAccessControlContext + " is denied."); //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
		} else {
			System.err.println("access denied " + perm); //$NON-NLS-1$
		}
		DebugRecursionDetection.getTlDebug().remove();
	}
	if (debug && ((AccessControlContext.debugSetting() & AccessControlContext.DEBUG_ACCESS_FAILURE) != 0)) {
		DebugRecursionDetection.getTlDebug().set(""); //$NON-NLS-1$
		new Exception("Stack trace").printStackTrace(); //$NON-NLS-1$
		if (createACCdenied) {
			System.err.println("domain that failed " + createAccessControlContext + " check " + pDomain); //$NON-NLS-1$ //$NON-NLS-2$
		} else {
			System.err.println("domain that failed " + pDomain); //$NON-NLS-1$
		}
		DebugRecursionDetection.getTlDebug().remove();
	}
	if (createACCdenied) {
		/*[MSG "K002d", "Access denied {0} due to untrusted AccessControlContext since {1} is denied"]*/
		throw new AccessControlException(com.ibm.oti.util.Msg.getString("K002d", perm, createAccessControlContext), perm); //$NON-NLS-1$
	} else {
		/*[MSG "K002c", "Access denied {0}"]*/
		throw new AccessControlException(com.ibm.oti.util.Msg.getString("K002c", perm), perm); //$NON-NLS-1$
	}
}

/**
 * Helper method to check whether the running program is allowed to access the resource
 * being guarded by the given Permission argument
 * Assuming perm is not null.
 *
 * @param perm the permission to be checked
 * @param acc the AccessControlContext to be checked
 * @param objects the object array returned from native getAccSnapshot
 * @param frame the doPrivileged frame are being checked
 * @param checked the cached check result
 * @param objPDomains the object array containing ProtectionDomain objects
 * @param debug the debug flag
 * @param startPos the start index of the actual ProtectionDomain objects
 *          1 is JEP140 format, 2 is Pre-JEP140 format
 *
 * @return true if access is granted by a limited permission, otherwise return false
 */
private static boolean checkPermissionHelper(Permission perm, AccessControlContext acc, Object[] objects, int frame, AccessCache checked, Object[] objPDomains, int debug, int startPos) {
	boolean limitedPermImplied = false;
	boolean debugEnabled = (debug & AccessControlContext.DEBUG_ENABLED) != 0;
	ProtectionDomain[] pDomains = generatePDarray(acc, objPDomains, debugEnabled, startPos);
	if (debugEnabled && (0 != (AccessControlContext.debugSetting() & AccessControlContext.DEBUG_ACCESS_DOMAIN))) {
		DebugRecursionDetection.getTlDebug().set(""); //$NON-NLS-1$
		AccessControlContext.debugPrintAccess();
		if (null == pDomains || 0 == pDomains.length) {
			System.err.println("domain (context is null)"); //$NON-NLS-1$
		} else {
			/*[PR 121690] -Djava.security.debug=access:domain should print domains in checkPermission() */
			for (int i = 0; i < pDomains.length; ++i) {
				System.err.println("domain " + i + " " + pDomains[i]); //$NON-NLS-1$ //$NON-NLS-2$
			}
		}
		DebugRecursionDetection.getTlDebug().remove();
	}
	int length = pDomains == null ? 0 : pDomains.length;
	if ((null != acc)
		&& (null != acc.context)
		&& (AccessControlContext.STATE_AUTHORIZED != acc.authorizeState)
		&& (null != System.getSecurityManager())
	) {
		/*[PR JAZZ 72492] PMR 24367,001,866: Unexpected Security Permission "createAccessControlContext" exception thrown from 1.6SR14 onwards */
		// startPos: 1 is JEP140 format, 2 is Pre-JEP140 format
		ProtectionDomain callerPD = (ProtectionDomain) objPDomains[startPos - 1];
		if (null != callerPD && !callerPD.implies(createAccessControlContext)) {
			/*[PR CMVC 197399] Improve checking order */
			// new behavior introduced by this fix
			// an ACE is thrown if there is a untrusted PD but without SecurityPermission createAccessControlContext
			throwACE((debug & AccessControlContext.DEBUG_ACCESS_DENIED) != 0, perm, callerPD, true);
		}
	}

	if (2 == startPos) { // Pre-JEP140 format
		if (null != acc && (null != acc.doPrivilegedAcc || null != acc.nextStackAcc || acc.isLimitedContext)) {
			checked = new AccessCache(); /* checked was null initially when Pre-JEP140 format */
			return AccessControlContext.checkPermissionWithCache(perm, pDomains, debug, acc, false, null, null, checked);
		} else {
			if (pDomains != null) {
				for (int i = 0; i < length ; ++i) {
					// invoke PD within acc.context first
					/*[IF Sidecar19-SE]*/
					if ((pDomains[length - i - 1] != null) && !pDomains[length - i - 1].impliesWithAltFilePerm(perm)) {
					/*[ELSE]*/
					if ((pDomains[length - i - 1] != null) && !pDomains[length - i - 1].implies(perm)) {
					/*[ENDIF] Sidecar19-SE*/
						throwACE((debug & AccessControlContext.DEBUG_ACCESS_DENIED) != 0, perm, pDomains[length - i - 1], false);
					}
				}
			}
		}
	} else {
		if (AccessControlContext.checkPermissionWithCache(perm,
				pDomains, debug, (AccessControlContext)objects[frame * OBJS_ARRAY_SIZE],
				(null != objects[frame * OBJS_ARRAY_SIZE + OBJS_INDEX_PERMS_OR_CACHECHECKED]),
				(Permission[])objects[frame * OBJS_ARRAY_SIZE + OBJS_INDEX_PERMS_OR_CACHECHECKED],
				null, checked)
		) {
			limitedPermImplied = true;
		}
	}
	return limitedPermImplied;
}

/**
 * Helper to print debug stack information for checkPermission().
 *
 * @param debug the debug flags in effect
 * @param perm the permission to check
 */
private static void debugPrintStack(boolean debug, Permission perm) {
	if (debug && ((AccessControlContext.debugSetting() & AccessControlContext.DEBUG_ACCESS_STACK) != 0)) {
		DebugRecursionDetection.getTlDebug().set(""); //$NON-NLS-1$
		new Exception("Stack trace for " + perm).printStackTrace(); //$NON-NLS-1$
		DebugRecursionDetection.getTlDebug().remove();
	}
}

/**
 * Helper to print debug information for checkPermission() when in the pre JEP 140 format.
 *
 * @param objects the AccessControlContext and ProtectionDomain array obtained from the stack
 * @param perm the permission to check
 * @return if debugging is enabled
 */
private static boolean debugHelperPreJEP140(Object[] objects, Permission perm) {
	boolean debug = true;
	if (AccessControlContext.debugCodeBaseArray != null) {
		debug = false;
		for (int i = 2; i < objects.length; ++i) {
			Object pd = objects[i];
			if (pd != null) {
				CodeSource cs = ((ProtectionDomain) pd).getCodeSource();
				if ((cs != null) && AccessControlContext.debugCodeBase(cs.getLocation())) {
					debug = true;
					break;
				}
			}
		}
		AccessControlContext acc = (AccessControlContext) objects[0];
		if (!debug) {
			debug = (acc != null) && acc.hasDebugCodeBase();
		}
	}

	if (debug && !AccessControlContext.debugPermission(perm)) {
		debug = false;
	}

	debugPrintStack(debug, perm);
	return debug;
}

/**
 * Helper to print debug information for checkPermission() when in the JEP 140 format.
 *
 * @param objects the AccessControlContext and ProtectionDomain array obtained from the stack
 * @param perm the permission to check
 * @return if debugging is enabled
 */
private static boolean debugHelperJEP140(Object[] objects, Permission perm) {
	boolean debug = true;
	if (AccessControlContext.debugCodeBaseArray != null) {
		debug = false;
		done:
		for (int j = 0; j < objects.length / OBJS_ARRAY_SIZE; ++j) {
			AccessControlContext acc = (AccessControlContext) objects[j * OBJS_ARRAY_SIZE];
			Object[] objPDomains = (Object[]) objects[j * OBJS_ARRAY_SIZE + OBJS_INDEX_PDS];
			for (int i = 1; i < objPDomains.length; ++i) {
				Object pd = objPDomains[i];
				if (pd != null) {
					CodeSource cs = ((ProtectionDomain) pd).getCodeSource();
					if ((cs != null) && AccessControlContext.debugCodeBase(cs.getLocation())) {
						debug = true;
						break done;
					}
				}
			}
			if ((acc != null) && acc.hasDebugCodeBase()) {
				debug = true;
				break;
			}
		}
	}

	if (debug) {
		debug = AccessControlContext.debugPermission(perm);
	}

	debugPrintStack(debug, perm);
	return debug;
}

/**
 * Checks whether the running program is allowed to
 * access the resource being guarded by the given
 * Permission argument.
 *
 * @param       perm                    the permission to check
 * @exception   AccessControlException  if access is not allowed.
 *              NullPointerException if perm is null
 */
public static void checkPermission(Permission perm) throws AccessControlException {
	if (perm == null) {
		throw new NullPointerException();
	}
	// DEBUG_ENABLED is set if there is nothing to match, or if there is a matching codebase or permission object
	int debug = AccessControlContext.DEBUG_DISABLED;
	if (AccessControlContext.debugSetting() != 0) {
		if (null == DebugRecursionDetection.getTlDebug().get()) {
			debug = AccessControlContext.DEBUG_ACCESS_DENIED | AccessControlContext.DEBUG_ENABLED;
		}
	}

	Object[] objects = getAccSnapshot(1, false);
	boolean isPreJEP140Format = (0 == objects.length % OBJS_ARRAY_SIZE) ? false : true;

	if (isPreJEP140Format) {
		if ((debug != AccessControlContext.DEBUG_DISABLED) && !debugHelperPreJEP140(objects, perm)) {
			debug = AccessControlContext.DEBUG_ACCESS_DENIED; // Disable DEBUG_ENABLED
		}

		checkPermissionHelper(perm, (AccessControlContext) objects[0], null, 0, null, objects, debug, 2); // the actual ProtectionDomain element starts at index 2
	} else {
		int frameNbr = objects.length / OBJS_ARRAY_SIZE;

		if ((debug != AccessControlContext.DEBUG_DISABLED) && !debugHelperJEP140(objects, perm)) {
			debug = AccessControlContext.DEBUG_ACCESS_DENIED; // Disable DEBUG_ENABLED
		}

		AccessCache checked = new AccessCache();
		for (int j = 0; j < frameNbr; ++j) {
			AccessControlContext acc = (AccessControlContext) objects[j * OBJS_ARRAY_SIZE];
			Object[] objPDomains = (Object[]) objects[j * OBJS_ARRAY_SIZE + OBJS_INDEX_PDS];
			if (checkPermissionHelper(perm, acc, objects, j, checked, objPDomains, debug, 1)) { // the actual ProtectionDomain element starts at index 1
				break;
			}
		}
	}
	if ((debug & AccessControlContext.DEBUG_ENABLED) != 0) {
		DebugRecursionDetection.getTlDebug().set(""); //$NON-NLS-1$
		AccessControlContext.debugPrintAccess();
		System.err.println("access allowed " + perm); //$NON-NLS-1$
		DebugRecursionDetection.getTlDebug().remove();
	}
}

/**
 * Used to keep the context live during doPrivileged().
 *
 * @param context  the context to retain
 *
 * @see         #doPrivileged(PrivilegedAction, AccessControlContext)
 */
private static void keepalive(AccessControlContext context) {
	return;
}

/**
 * @param perms  the permissions to retain
 */
private static void keepalive(Permission... perms) {
	return;
}

/**
 * Answers the access controller context of the current thread,
 * including the inherited ones. It basically retrieves all the
 * protection domains from the calling stack and creates an
 * <code>AccessControlContext</code> with them.
 *
 * @return an AccessControlContext which captures the current state
 *
 * @see         AccessControlContext
 */
public static AccessControlContext getContext() {
	return getContextHelper(false);
}

/**
 * This is a helper method for getContext() and doPrivilegedWithCombiner methods.
 * Answers the access controller context of the current thread including the inherited ones.
 * Refer native getAccSnapshot() for return data format accordingly to the flag forDoPrivilegedWithCombiner.
 *
 * @param forDoPrivilegedWithCombiner The flag to indicate if it is for doPrivilegedWithCombiner method
 * @return an AccessControlContext which captures the current state
 */
@CallerSensitive
private static AccessControlContext getContextHelper(boolean forDoPrivilegedWithCombiner) {
	Object[] domains = getAccSnapshot(2, forDoPrivilegedWithCombiner);
	boolean isPreJEP140Format = (0 == domains.length % OBJS_ARRAY_SIZE) ? false : true;
	if (isPreJEP140Format) {
		AccessControlContext acc = (AccessControlContext) domains[0];
		/*[PR JAZZ 66930] j.s.AccessControlContext.checkPermission() invoke untrusted ProtectionDomain.implies */
		// The actual ProtectionDomain element starts at index 2
		ProtectionDomain[] pDomains = generatePDarray(acc, domains, false, 2);
		AccessControlContext accTmp;
		int newAuthorizedState = getNewAuthorizedState(acc, (ProtectionDomain) domains[1]);
		if (null != acc && (null != acc.doPrivilegedAcc || null != acc.nextStackAcc || acc.isLimitedContext)) {
			accTmp = new AccessControlContext(acc, pDomains, newAuthorizedState);
		} else {
			accTmp = new AccessControlContext(pDomains, newAuthorizedState);
		}
		if (null != acc && null != acc.domainCombiner) {
			accTmp.domainCombiner = acc.domainCombiner;
		}
		return accTmp;
	}
	int frameNbr = domains.length / OBJS_ARRAY_SIZE;
	AccessControlContext accContext = null;
	AccessControlContext accLower = null;
	DomainCombiner dc = null; // store a non-null domainCombiner from a closest frame or null
	for (int j = 0; j < frameNbr; ++j) {
		AccessControlContext acc = (AccessControlContext) domains[j * OBJS_ARRAY_SIZE];
		/*[PR JAZZ 66930] j.s.AccessControlContext.checkPermission() invoke untrusted ProtectionDomain.implies */
		// the actual ProtectionDomain element starts at index 1
		ProtectionDomain[] pDomains = generatePDarray(acc, (Object[]) domains[j * OBJS_ARRAY_SIZE + OBJS_INDEX_PDS], false, 1);
		AccessControlContext accTmp;
		int newAuthorizedState = getNewAuthorizedState(acc, (ProtectionDomain)((Object[]) domains[j * OBJS_ARRAY_SIZE + OBJS_INDEX_PDS])[0]);
		if (((null != acc) && acc.isLimitedContext) || (1 < frameNbr)) {
			// there is a limited doPrivilege frame
			accTmp = new AccessControlContext(acc, pDomains, newAuthorizedState);
		} else {
			// not set doPrivilegedAcc for non limited doPrivilege
			// all ProtectionDomains already included within pDomains
			accTmp = new AccessControlContext(pDomains, newAuthorizedState);
		}
		if (null != acc && null != acc.domainCombiner) {
			accTmp.domainCombiner = acc.domainCombiner;
			if (dc == null) {
				// this dc will be set to accContext.domainCombiner
				dc = acc.domainCombiner;
			}
		}
		if (null != domains[j * OBJS_ARRAY_SIZE + OBJS_INDEX_PERMS_OR_CACHECHECKED]) {
			// this is frame with limited permissions
			accTmp.isLimitedContext = true;
			accTmp.limitedPerms = (Permission[]) domains[j * OBJS_ARRAY_SIZE + OBJS_INDEX_PERMS_OR_CACHECHECKED];
		}

		if (null == accLower) {
			accContext = accTmp;
		} else {
			accLower.nextStackAcc = accTmp;
		}
		accLower = accTmp;
	}
	if ((accContext != null) && (accContext.domainCombiner == null) && (dc != null)) {
		accContext.domainCombiner = dc;
	}
	return accContext;
}

/**
 * Helper method to generate ProtectionDomain array for a new AccessControlContext object
 *
 * @param acc the AccessControlContext object to be checked
 * @param domains the incoming ProtectionDomain object array
 * @param debug the debug flag
 * @param startPos the start index of the actual ProtectionDomain object
 *
 * @return a ProtectionDomain object array
 */
private static ProtectionDomain[] generatePDarray(AccessControlContext acc, Object[] domains, boolean debug, int startPos) {
	ProtectionDomain[] pDomains = null;
	/*[PR JAZZ 66930] j.s.AccessControlContext.checkPermission() invoke untrusted ProtectionDomain.implies */
	if (null != acc
		&& null != acc.domainCombiner
		&& ((AccessControlContext.STATE_AUTHORIZED == acc.authorizeState) || null == System.getSecurityManager())
	) {
		if (debug) {
			DebugRecursionDetection.getTlDebug().set(""); //$NON-NLS-1$
			AccessControlContext.debugPrintAccess();
			System.err.println("AccessController invoking the Combiner"); //$NON-NLS-1$
			DebugRecursionDetection.getTlDebug().remove();
		}
		pDomains = acc.domainCombiner.combine(toArrayOfProtectionDomains(domains, null, startPos), acc.context);
	} else {
		pDomains = toArrayOfProtectionDomains(domains, acc, startPos);
	}
	if (null != pDomains && 0 == pDomains.length) {
		pDomains = null;
	}
	return pDomains;
}

/**
 * Helper method to determine the newAuthorizedState according to incoming acc and callerPD.
 *
 * @param   acc the AccessControlContext object to be checked
 * @param   callerPD the caller's ProtectionDomain object
 *
 * @return   AccessControlContext.STATE_AUTHORIZED or STATE_NOT_AUTHORIZED (can't be STATE_UNKNOWN)
 */
private static int getNewAuthorizedState(AccessControlContext acc, ProtectionDomain callerPD) {
	int newAuthorizedState;
	/*[PR JAZZ 87596] PMR 18839,756,000 - Need to trust AccessControlContext created without active SecurityManager */
	if ((null != acc) && (null != System.getSecurityManager())) {
		newAuthorizedState = acc.authorizeState;
		if (AccessControlContext.STATE_UNKNOWN == newAuthorizedState) {
			// only change AccessControlContext.authorizeState when it is unknown initially
			if (null == callerPD || callerPD.implies(createAccessControlContext)) {
				newAuthorizedState = AccessControlContext.STATE_AUTHORIZED;
			} else {
				newAuthorizedState = AccessControlContext.STATE_NOT_AUTHORIZED;
			}
		}
	} else {
		newAuthorizedState = AccessControlContext.STATE_AUTHORIZED;
	}
	return newAuthorizedState;
 }

/**
 * Helper method to combine the ProtectionDomain objects
 *
 * @param domains the incoming ProtectionDomain object array
 * @param acc the AccessControlContext whose context is an ProtectionDomain object array
 * @param startPos the start index of the domains
 *
 * @return an ProtectionDomain object array
 */
static ProtectionDomain[] toArrayOfProtectionDomains(Object[] domains, AccessControlContext acc, int startPos) {
	final ProtectionDomain[] accDomains = acc == null ? null : acc.context;
	ProtectionDomain[] answer = null;

	if (domains == null) {
		if (accDomains != null && accDomains.length != 0) {
			answer = accDomains;
		}
	} else if (accDomains == null) {
		int domainCount = domains.length - startPos;
		for (int i = 0; i < domainCount; ++i) {
			if (domains[startPos + i] == null) {
				domainCount = i;
				break;
			}
		}
		if (domainCount > 0) {
			answer = new ProtectionDomain[domainCount];
			System.arraycopy(domains, startPos, answer, 0, domainCount);
		}
	} else {
		final int domainCount = domains.length;
		final int accDomainCount = accDomains.length;
		int newDomainCount = 0;
		answer = new ProtectionDomain[domainCount + accDomainCount];
		for (int i = startPos; i < domainCount; ++i) {
			Object domain = domains[i];
			if (domain == null) {
				break;
			}
			for (int j = accDomainCount;;) {
				j -= 1;
				if (j < 0) {
					answer[newDomainCount] = (ProtectionDomain) domain;
					newDomainCount += 1;
					break;
				} else if (accDomains[j] == domain) {
					break;
				}
			}
		}
		if (newDomainCount + accDomainCount == 0) {
			answer = null;
		} else if (newDomainCount == 0) {
			answer = accDomains;
		} else {
			if (newDomainCount < domainCount) {
				ProtectionDomain[] copy = new ProtectionDomain[newDomainCount + accDomainCount];
				System.arraycopy(answer, 0, copy, 0, newDomainCount);
				answer = copy;
			}
			System.arraycopy(accDomains, 0, answer, newDomainCount, accDomainCount);
		}
	}

	return answer;
}

/**
 * Performs the privileged action specified by <code>action</code>.
 * <p>
 * When permission checks are made, if the permission has been granted by all
 * frames below and including the one representing the call to this method,
 * then the permission is granted. In other words, the check stops here.
 *
 * Any unchecked exception generated by this method will propagate up the chain.
 * @param <T> the type of value returned by PrivilegedAction.run
 *
 * @param action The PrivilegedAction to performed
 *
 * @return the result of the PrivilegedAction
 *
 * @exception   NullPointerException if action is null
 *
 * @see         #doPrivileged(PrivilegedAction)
 */
/*[PR 1GO8C5O] required for initializeInternal */
@CallerSensitive
public static <T> T doPrivileged(PrivilegedAction<T> action) {
	return action.run();
}

/**
 * Performs the privileged action specified by <code>action</code>.
 * <p>
 * When permission checks are made, if the permission has been granted by all
 * frames below and including the one representing the call to this method,
 * then the permission is granted iff it is granted by the AccessControlContext
 * <code>context</code>. In other words, no more checking of the current stack
 * is performed. Instead, the passed in context is checked.
 *
 * Any unchecked exception generated by this method will propagate up the chain.
 * @param <T> the type of value returned by PrivilegedAction.run
 *
 * @param action The PrivilegedAction to performed
 * @param context The AccessControlContext to check
 *
 * @return the result of the PrivilegedAction
 *
 * @exception   NullPointerException if action is null
 *
 * @see         #doPrivileged(PrivilegedAction)
 */
@CallerSensitive
public static <T> T doPrivileged(PrivilegedAction<T> action, AccessControlContext context) {
	T result = action.run();
	/*[PR 108112] context is not kept alive*/
	keepalive(context);
	return result;
}

/**
 * Performs the privileged action specified by <code>action</code>.
 * <p>
 * When permission checks are made, if the permission has been granted by all
 * frames below and including the one representing the call to this method,
 * then the permission is granted. In other words, the check stops here.
 *
 * Any unchecked exception generated by this method will propagate up the chain.
 * However, checked exceptions will be caught an re-thrown as PrivilegedActionExceptions
 * @param <T> the type of value returned by PrivilegedExceptionAction.run
 *
 * @param action The PrivilegedExceptionAction to performed
 *
 * @return the result of the PrivilegedExceptionAction
 *
 * @throws PrivilegedActionException when a checked exception occurs when performing the action
 *          NullPointerException if action is null
 *
 * @see         #doPrivileged(PrivilegedAction)
 */
@CallerSensitive
public static <T> T doPrivileged(PrivilegedExceptionAction<T> action)
	throws PrivilegedActionException
{
	try {
		return action.run();
	} catch (RuntimeException ex) {
		throw ex;
	} catch (Exception ex) {
		throw new PrivilegedActionException(ex);
	}
}

/**
 * Performs the privileged action specified by <code>action</code>.
 * <p>
 * When permission checks are made, if the permission has been granted by all
 * frames below and including the one representing the call to this method,
 * then the permission is granted iff it is granted by the AccessControlContext
 * <code>context</code>. In other words, no more checking of the current stack
 * is performed. Instead, the passed in context is checked.
 *
 * Any unchecked exception generated by this method will propagate up the chain.
 * However, checked exceptions will be caught an re-thrown as PrivilegedActionExceptions
 * @param <T> the type of value returned by PrivilegedExceptionAction.run
 *
 * @param action The PrivilegedExceptionAction to performed
 * @param context The AccessControlContext to check
 *
 * @return the result of the PrivilegedExceptionAction
 *
 * @throws PrivilegedActionException when a checked exception occurs when performing the action
 *          NullPointerException if action is null
 *
 * @see         #doPrivileged(PrivilegedAction)
 */
@CallerSensitive
public static <T> T doPrivileged (PrivilegedExceptionAction<T> action, AccessControlContext context)
	throws PrivilegedActionException
{
	try {
		T result = action.run();
		/*[PR 108112] context is not kept alive*/
		keepalive(context);
		return result;
	} catch (RuntimeException ex) {
		throw ex;
	} catch (Exception ex) {
		throw new PrivilegedActionException(ex);
	}
}

/**
 * Performs the privileged action specified by <code>action</code>, retaining
 * any current DomainCombiner.
 * <p>
 * When permission checks are made, if the permission has been granted by all
 * frames below and including the one representing the call to this method,
 * then the permission is granted. In other words, the check stops here.
 *
 * Any unchecked exception generated by this method will propagate up the chain.
 * @param <T> the type of value returned by PrivilegedAction.run
 *
 * @param action The PrivilegedAction to performed
 *
 * @return the result of the PrivilegedAction
 *
 * @see         #doPrivileged(PrivilegedAction)
 */
@CallerSensitive
public static <T> T doPrivilegedWithCombiner(PrivilegedAction<T> action) {
	return doPrivileged(action, getContextHelper(true));
}

/**
 * Performs the privileged action specified by <code>action</code>, retaining
 * any current DomainCombiner.
 * <p>
 * When permission checks are made, if the permission has been granted by all
 * frames below and including the one representing the call to this method,
 * then the permission is granted. In other words, the check stops here.
 *
 * Any unchecked exception generated by this method will propagate up the chain.
 * However, checked exceptions will be caught an re-thrown as PrivilegedActionExceptions
 * @param <T> the type of value returned by PrivilegedExceptionAction.run
 *
 * @param action The PrivilegedExceptionAction to performed
 *
 * @return the result of the PrivilegedExceptionAction
 *
 * @throws PrivilegedActionException when a checked exception occurs when performing the action
 *
 * @see         #doPrivileged(PrivilegedAction)
 */
@CallerSensitive
public static <T> T doPrivilegedWithCombiner(PrivilegedExceptionAction<T> action)
	throws PrivilegedActionException
{
	return doPrivileged(action, getContextHelper(true));
}

/**
 * Helper method to check if any permission is null
 *
 * @param perms The Permission arguments to limit the scope of the caller's privileges.
 *
 * @exception   NullPointerException if any perm is null
 *
 */
private static void checkPermsNPE(Permission... perms) {
	for (int i = 0; i < perms.length; ++i) {
		if (null == perms[i]) {
			throw new NullPointerException();
		}
	}
}

/**
 * Performs the privileged action specified by <code>action</code>.
 * <p>
 * When permission checks are made, if the permission has been granted by all
 * frames below and including the one representing the call to this method,
 * then the permission is granted iff it is granted by the AccessControlContext
 * <code>context</code> and also granted by one of the permissions arguments.
 *
 * Any unchecked exception generated by this method will propagate up the chain.
 * @param <T> the type of value returned by PrivilegedAction.run
 *
 * @param action The PrivilegedAction to performed
 * @param context The AccessControlContext to check
 * @param perms The Permission arguments to limit the scope of the caller's privileges.
 *
 * @return the result of the PrivilegedAction
 * @since 1.8
 *
 * @exception   NullPointerException if action is null
 *
 * @see         #doPrivileged(PrivilegedAction)
 * @see         #doPrivileged(PrivilegedAction, AccessControlContext)
 */
@CallerSensitive
public static <T> T doPrivileged(PrivilegedAction<T> action,
		AccessControlContext context, Permission... perms)
{
	checkPermsNPE(perms);
	T result = action.run();
	keepalive(context);
	keepalive(perms);
	return result;
}

/**
 * Performs the privileged action specified by <code>action</code>, retaining
 * any current DomainCombiner.
 * <p>
 * When permission checks are made, if the permission has been granted by all
 * frames below and including the one representing the call to this method,
 * then the permission is granted iff it is granted by one of the permissions arguments.
 *
 * Any unchecked exception generated by this method will propagate up the chain.
 * @param <T> the type of value returned by PrivilegedAction.run
 *
 * @param action The PrivilegedAction to performed
 * @param context The AccessControlContext to check
 * @param perms The Permission arguments to limit the scope of the caller's privileges.
 *
 * @return the result of the PrivilegedAction
 * @since 1.8
 *
 * @see         #doPrivileged(PrivilegedAction)
 * @see         #doPrivileged(PrivilegedAction, AccessControlContext)
 */
@CallerSensitive
public static <T> T doPrivilegedWithCombiner(PrivilegedAction<T> action,
		AccessControlContext context, Permission... perms)
{
	checkPermsNPE(perms);
	ProtectionDomain domain = getCallerPD(1);
	ProtectionDomain[] pdArray = (domain == null) ? null : new ProtectionDomain[] { domain };
	return doPrivileged(action, new AccessControlContext(context, pdArray, getNewAuthorizedState(context, domain)), perms);
}

/**
 * Performs the privileged action specified by <code>action</code>.
 * <p>
 * When permission checks are made, if the permission has been granted by all
 * frames below and including the one representing the call to this method,
 * then the permission is granted iff it is granted by the AccessControlContext
 * <code>context</code> and also granted by one of the permissions arguments.
 *
 * Any unchecked exception generated by this method will propagate up the chain.
 * However, checked exceptions will be caught an re-thrown as PrivilegedActionExceptions
 * @param <T> the type of value returned by PrivilegedExceptionAction.run
 *
 * @param action The PrivilegedExceptionAction to performed
 * @param context The AccessControlContext to check
 * @param perms The Permission arguments to limit the scope of the caller's privileges.
 *
 * @return the result of the PrivilegedExceptionAction
 * @since 1.8
 *
 * @throws PrivilegedActionException when a checked exception occurs when performing the action
 *          NullPointerException if action is null
 *
 * @see         #doPrivileged(PrivilegedAction)
 * @see         #doPrivileged(PrivilegedAction, AccessControlContext)
 */
@CallerSensitive
public static <T> T doPrivileged(PrivilegedExceptionAction<T> action,
		AccessControlContext context, Permission... perms)
	throws PrivilegedActionException
{
	try {
		checkPermsNPE(perms);
		T result = action.run();
		keepalive(context);
		keepalive(perms);
		return result;
	} catch (RuntimeException ex) {
		throw ex;
	} catch (Exception ex) {
		throw new PrivilegedActionException(ex);
	}
}

/**
 * Performs the privileged action specified by <code>action</code>, retaining
 * any current DomainCombiner.
 * <p>
 * When permission checks are made, if the permission has been granted by all
 * frames below and including the one representing the call to this method,
 * then the permission is granted and also granted by one of the permissions arguments.
 *
 * Any unchecked exception generated by this method will propagate up the chain.
 * However, checked exceptions will be caught an re-thrown as PrivilegedActionExceptions
 * @param <T> the type of value returned by PrivilegedExceptionAction.run
 *
 * @param action The PrivilegedExceptionAction to performed
 * @param context The AccessControlContext to check
 * @param perms The Permission arguments to limit the scope of the caller's privileges.
 *
 * @return the result of the PrivilegedExceptionAction
 * @since 1.8
 *
 * @throws PrivilegedActionException when a checked exception occurs when performing the action
 *
 * @see         #doPrivileged(PrivilegedAction)
 * @see         #doPrivileged(PrivilegedAction, AccessControlContext)
 */
public static <T> T doPrivilegedWithCombiner(PrivilegedExceptionAction<T> action,
		AccessControlContext context, Permission... perms)
	throws PrivilegedActionException
{
	checkPermsNPE(perms);
	ProtectionDomain domain = getCallerPD(1);
	ProtectionDomain[] pdArray = (domain == null) ? null : new ProtectionDomain[] { domain };
	return doPrivileged(action, new AccessControlContext(context, pdArray, getNewAuthorizedState(context, domain)), perms);
}

}
