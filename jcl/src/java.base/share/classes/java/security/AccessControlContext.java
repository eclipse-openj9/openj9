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

import java.io.IOException;
import java.io.StreamTokenizer;
import java.io.StringReader;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
/*[IF Sidecar19-SE]*/
import sun.security.util.FilePermCompat;
/*[ENDIF] Sidecar19-SE*/

/**
 * An AccessControlContext encapsulates the information which is needed
 * by class AccessController to detect if a Permission would be granted
 * at a particular point in a programs execution.
 *
 * @author  OTI
 * @version initial
 */
public final class AccessControlContext {

	static final int STATE_NOT_AUTHORIZED = 0; // It has been confirmed that the ACC is NOT authorized
	static final int STATE_AUTHORIZED = 1; // It has been confirmed that the ACC is authorized
	static final int STATE_UNKNOWN = 2; // The ACC state is unknown yet.

	private static int debugSetting = -1;
	private static ArrayList<String> debugPermClassArray;
	private static ArrayList<String> debugPermNameArray;
	private static ArrayList<String> debugPermActionsArray;
	static ArrayList<URL> debugCodeBaseArray;

	/* Constants used to set the value of the debugHasCodebase field */
	private static final int DEBUG_UNINITIALIZED_HASCODEBASE = 0;
	private static final int DEBUG_NO_CODEBASE = 1;
	private static final int DEBUG_HAS_CODEBASE = 2;

	static final int DEBUG_DISABLED = 0;
	static final int DEBUG_ACCESS_DENIED = 1; // debug is enabled for access denied, and failure
	static final int DEBUG_ENABLED = 2; // debug is enabled for access allowed, stacks, domains, and threads

	DomainCombiner domainCombiner;
	/*[PR CMVC 111227] JDK6 expects field to be called "context" */
	ProtectionDomain[] context;
	/*[PR CMVC 197399] Improve checking order */
	int authorizeState = STATE_UNKNOWN;
	/*[PR JAZZ 66930] j.s.AccessControlContext.checkPermission() invoke untrusted ProtectionDomain.implies */
	// This flag is to determine if current ACC contains privileged PDs such that
	// createAccessControlContext permission need to be checked before invoking ProtectionDomain.implies()
	private boolean containPrivilegedContext = false;
	// This is the ProtectionDomain of the creator of this ACC
	private ProtectionDomain callerPD;
	AccessControlContext doPrivilegedAcc; // AccessControlContext usually coming from a doPrivileged call
	boolean isLimitedContext = false; // flag to indicate if there are limited permissions
	Permission[] limitedPerms; // the limited permissions when isLimitedContext is true
	AccessControlContext nextStackAcc; // AccessControlContext in next call stack when isLimitedContext is true
	private int debugHasCodebase; // Set to the value of DEBUG_UNINITIALIZED_HASCODEBASE be default. Cache the result of hasDebugCodeBase()

	private static final SecurityPermission createAccessControlContext =
		new SecurityPermission("createAccessControlContext"); //$NON-NLS-1$
	private static final SecurityPermission getDomainCombiner =
		new SecurityPermission("getDomainCombiner"); //$NON-NLS-1$

	static final int DEBUG_ACCESS = 1;
	static final int DEBUG_ACCESS_STACK = 2;
	static final int DEBUG_ACCESS_DOMAIN = 4;
	static final int DEBUG_ACCESS_FAILURE = 8;
	static final int DEBUG_ACCESS_THREAD = 0x10;
	static final int DEBUG_ALL = 0xff;
	
	static final class AccessCache {
		ProtectionDomain[] pdsImplied;
		Permission[] permsImplied;
		Permission[] permsNotImplied;
	}

static int debugSetting() {
	if (debugSetting != -1) return debugSetting;
	debugSetting = 0;
	String value = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty("java.security.debug"); //$NON-NLS-1$
	if (value == null) return debugSetting;
	StreamTokenizer tokenizer = new StreamTokenizer(new StringReader(value));
	tokenizer.resetSyntax();
	tokenizer.wordChars(Character.MIN_CODE_POINT, Character.MAX_CODE_POINT);
	tokenizer.quoteChar('"');
	tokenizer.whitespaceChars(',', ',');

	try {
		while (tokenizer.nextToken() != StreamTokenizer.TT_EOF) {
			String keyword = tokenizer.sval;
			if (keyword.equals("all")) { //$NON-NLS-1$
				debugSetting  = DEBUG_ALL;
				return debugSetting;
			}
			if (keyword.startsWith("access:")) { //$NON-NLS-1$
				debugSetting |= DEBUG_ACCESS;
				keyword = keyword.substring(7);
			}

			if (keyword.equals("access")) { //$NON-NLS-1$
				debugSetting |= DEBUG_ACCESS;
			} else if (keyword.equals("stack")) { //$NON-NLS-1$
				debugSetting |= DEBUG_ACCESS_STACK;
			} else if (keyword.equals("domain")) { //$NON-NLS-1$
				debugSetting |= DEBUG_ACCESS_DOMAIN;
			} else if (keyword.equals("failure")) { //$NON-NLS-1$
				debugSetting |= DEBUG_ACCESS_FAILURE;
			} else if (keyword.equals("thread")) { //$NON-NLS-1$
				debugSetting |= DEBUG_ACCESS_THREAD;
			} else if (keyword.startsWith("permission=")) { //$NON-NLS-1$
				String debugPermClass = keyword.substring(11);
				if (debugPermClass.isEmpty() && tokenizer.nextToken() != StreamTokenizer.TT_EOF) {
					debugPermClass = tokenizer.sval;
				}
				if (null == debugPermClassArray) {
					debugPermClassArray = new ArrayList<>();
				}
				debugPermClassArray.add(debugPermClass);
			} else if (keyword.startsWith("codebase=")) { //$NON-NLS-1$
				String codebase = keyword.substring(9);
				if (codebase.isEmpty() && tokenizer.nextToken() != StreamTokenizer.TT_EOF) {
					codebase = tokenizer.sval;
				}
				URL debugCodeBase = null;
				try {
					debugCodeBase = new URL(codebase);
				} catch (MalformedURLException e) {
					System.err.println("Error setting -Djava.security.debug=access:codebase - " + e); //$NON-NLS-1$
				}
				if (null != debugCodeBase) {
					if (null == debugCodeBaseArray) {
						debugCodeBaseArray = new ArrayList<>();
					}
					debugCodeBaseArray.add(debugCodeBase);
				}
			} else if (keyword.startsWith("permname=")) { //$NON-NLS-1$
				String debugPermName = keyword.substring(9);
				if (debugPermName.isEmpty() && tokenizer.nextToken() != StreamTokenizer.TT_EOF) {
					debugPermName = tokenizer.sval;
				}
				if (null == debugPermNameArray) {
					debugPermNameArray = new ArrayList<>();
				}
				debugPermNameArray.add(debugPermName);
			} else if (keyword.startsWith("permactions=")) { //$NON-NLS-1$
				String debugPermActions = keyword.substring(12);
				if (debugPermActions.isEmpty() && tokenizer.nextToken() != StreamTokenizer.TT_EOF) {
					debugPermActions = tokenizer.sval;
				}
				if (null == debugPermActionsArray) {
					debugPermActionsArray = new ArrayList<>();
				}
				debugPermActionsArray.add(debugPermActions);
			}
		}
	} catch (IOException e) {
		// should not happen with StringReader
	}
	if (0 == (debugSetting & DEBUG_ACCESS)) {
		// If the access keyword is not specified, none of the other keywords have any affect
		debugSetting = 0;
	}
	return debugSetting;
}

/**
 * Return true if the specified Permission is enabled for debug.
 * @param perm a Permission instance
 * @return Return true if the specified Permission is enabled for debug
 */
static boolean debugPermission(Permission perm) {
	boolean result = true;
	if (debugPermClassArray != null) {
		result = false;
		String permClassName = perm.getClass().getName();
		for (String debugPermClass : debugPermClassArray) {
			if (debugPermClass.equals(permClassName)) {
				return true;
			}
		}
	}
	if (debugPermNameArray != null) {
		result = false;
		String permName = perm.getName();
		for (String debugPermName : debugPermNameArray) {
			if (debugPermName.equals(permName)) {
				return true;
			}
		}
	}
	if (debugPermActionsArray != null) {
		result = false;
		String permActions = perm.getActions();
		for (String debugPermActions : debugPermActionsArray) {
			if (debugPermActions.equals(permActions)) {
				return true;
			}
		}
	}
	return result;
}

/**
 * Check if the receiver contains a ProtectionDomain using the debugCodeBase, which
 * was parsed from the java.security.debug system property.
 *
 * @return true if the AccessControlContext contains a ProtectionDomain which
 * matches the debug codebase.
 */
boolean hasDebugCodeBase() {
	if (debugHasCodebase != DEBUG_UNINITIALIZED_HASCODEBASE) {
		return debugHasCodebase == DEBUG_HAS_CODEBASE ? true : false;
	}
	ProtectionDomain[] pds = this.context;
	if (pds != null) {
		for (int i = 0; i < pds.length; ++i) {
			ProtectionDomain pd = this.context[i];
			CodeSource cs = null == pd ? null : pd.getCodeSource();
			if ((cs != null) && debugCodeBase(cs.getLocation()))  {
				debugHasCodebase = DEBUG_HAS_CODEBASE;
				return true;
			}
		}
		if ((this.doPrivilegedAcc != null) && this.doPrivilegedAcc.hasDebugCodeBase()) {
			debugHasCodebase = DEBUG_HAS_CODEBASE;
			return true;
		}
		if ((this.nextStackAcc != null) && this.nextStackAcc.hasDebugCodeBase()) {
			debugHasCodebase = DEBUG_HAS_CODEBASE;
			return true;
		}
	}
	debugHasCodebase = DEBUG_NO_CODEBASE;
	return false;
}

/**
 * Return true if the specified codebase location is enabled for debug.
 * @param location a codebase URL
 * @return Return true if the specified codebase location is enabled for debug
 */
static boolean debugCodeBase(URL location) {
	if (location != null) {
		for (URL debugCodeBase : debugCodeBaseArray) {
			if (debugCodeBase.equals(location)) {
				return true;
			}
		}
	}

	return false;
}

static void debugPrintAccess() {
	System.err.print("access: "); //$NON-NLS-1$
	if ((debugSetting() & DEBUG_ACCESS_THREAD) == DEBUG_ACCESS_THREAD) {
		System.err.print("(" + Thread.currentThread() + ")"); //$NON-NLS-1$ //$NON-NLS-2$
	}
}

/**
 * Constructs a new instance of this class given an array of
 * protection domains.
 *
 * @param fromContext the array of ProtectionDomain
 *
 * @exception NullPointerException if fromContext is null
 */
public AccessControlContext(ProtectionDomain[] fromContext) {
	/*[PR 94884]*/
	int length = fromContext.length;
	if (length == 0) {
		context = null;
	} else {
		int domainIndex = 0;
		context = new ProtectionDomain[length];
		next : for (int i = 0; i < length; ++i) {
			ProtectionDomain current = fromContext[i];
			if (current == null) continue;
			for (int j = 0; j < i; ++j)
				if (current == context[j]) continue next;
			context[domainIndex++] = current;
		}
		if (domainIndex == 0) {
			context = null;
		} else if (domainIndex != length) {
			ProtectionDomain[] copy = new ProtectionDomain[domainIndex];
			System.arraycopy(context, 0, copy, 0, domainIndex);
			context = copy;
		}
	}
	// this.containPrivilegedContext is set to false by default
	// this.authorizeState is STATE_UNKNOWN by default
}

AccessControlContext(ProtectionDomain[] context, int authorizeState) {
	super();
	switch (authorizeState) {
	default:
		// authorizeState can't be STATE_UNKNOWN, callerPD always is NULL
		throw new IllegalArgumentException();
	case STATE_AUTHORIZED:
	case STATE_NOT_AUTHORIZED:
		break;
	}
	this.context = context;
	this.authorizeState = authorizeState;
	this.containPrivilegedContext = true;
}

AccessControlContext(AccessControlContext acc, ProtectionDomain[] context, int authorizeState) {
	super();
	switch (authorizeState) {
	default:
		// authorizeState can't be STATE_UNKNOWN, callerPD always is NULL
		throw new IllegalArgumentException();
	case STATE_AUTHORIZED:
		if (null != acc) {
			// inherit the domain combiner when authorized
			this.domainCombiner = acc.domainCombiner;
		}
		break;
	case STATE_NOT_AUTHORIZED:
		break;
	}
	this.doPrivilegedAcc = acc;
	this.context = context;
	this.authorizeState = authorizeState;
	this.containPrivilegedContext = true;
}

/**
 * Constructs a new instance of this class given a context
 * and a DomainCombiner
 *
 * @param acc the AccessControlContext
 * @param combiner the DomainCombiner
 *
 * @exception java.security.AccessControlException thrown
 * when the caller doesn't have the  "createAccessControlContext" SecurityPermission
 * @exception NullPointerException if the provided context is null.
 */
public AccessControlContext(AccessControlContext acc, DomainCombiner combiner) {
	this(acc, combiner, false);
}

/**
 * Constructs a new instance of this class given a context and a DomainCombiner
 * Skip the Permission "createAccessControlContext" check if preauthorized is true
 *
 * @param acc the AccessControlContext
 * @param combiner the DomainCombiner
 * @param preauthorized the flag to indicate if the permission check can be skipped
 *
 * @exception java.security.AccessControlException thrown
 * when the caller doesn't have the  "createAccessControlContext" SecurityPermission
 * @exception NullPointerException if the provided context is null.
 */
AccessControlContext(AccessControlContext acc, DomainCombiner combiner, boolean preauthorized) {
	if (!preauthorized) {
		SecurityManager security = System.getSecurityManager();
		if (null != security) {
			security.checkPermission(createAccessControlContext);
			/*[PR JAZZ 78139] java.security.AccessController.checkPermission invokes untrusted DomainCombiner.combine method */
		}
	}
	this.authorizeState = STATE_AUTHORIZED;
	this.context = acc.context;
	this.domainCombiner = combiner;
	this.containPrivilegedContext = acc.containPrivilegedContext;
	this.isLimitedContext = acc.isLimitedContext;
	this.limitedPerms = acc.limitedPerms;
	this.nextStackAcc = acc.nextStackAcc;
	this.doPrivilegedAcc = acc.doPrivilegedAcc;
}

/**
 * Combine checked and toBeCombined
 * Assuming:    there are no null or dup elements in checked
 *              there might be nulls or dups in toBeCombined
 *
 * @param objPDs the flag to indicate if it is a ProtectionDomain or Permission object element
 * @param checked the array of objects already checked
 * @param toBeCombined the array of objects to be combined
 * @param start the start position in the toBeCombined array
 * @param len the number of element to be copied
 * @param justCombine the flag to indicate if null/dup check is needed
 *
 * @return the combination of these two array
 */
private static Object[] combineObjs(boolean objPDs, Object[] checked, Object[] toBeCombined, int start, int len, boolean justCombine) {
	if (null == toBeCombined) {
		return checked;
	}
	final int lenToBeCombined = Math.min(len, toBeCombined.length - start);
	if (0 == lenToBeCombined) {
		return checked;
	}
	final int lenChecked = (null == checked) ? 0 : checked.length;
	Object[] answer;
	if (objPDs) {
		answer = new ProtectionDomain[lenChecked + lenToBeCombined];
	} else {
		answer = new Permission[lenChecked + lenToBeCombined];
	}
	if (0 != lenChecked) {
		System.arraycopy(checked, 0, answer, 0, lenChecked);
	}
	if (justCombine) {
		// no null/dup check
		System.arraycopy(toBeCombined, start, answer, lenChecked, lenToBeCombined);
	} else {
		// remove the null & dups
		int answerLength = lenChecked;
		for (int i = 0; i < lenToBeCombined; ++i) {
			Object object = toBeCombined[start + i];
			if (null != object) { // remove null
				// check starts from newly added elements
				for (int j = answerLength;;) {
					j -= 1;
					if (j < 0) {
						answer[answerLength] = object;
						answerLength += 1;
						break;
					} else if (object == answer[j]) {
						break;
					}
				}
			}
		}
		if (answerLength < answer.length) {
			Object[] result;
			if (objPDs) {
				result = new ProtectionDomain[answerLength];
			} else {
				result = new Permission[answerLength];
			}
			System.arraycopy(answer, 0, result, 0, answerLength);
			answer = result;
		}
	}
	return answer;
}

/**
 * Combine checked and toBeCombined ProtectionDomain objects
 * Assuming:    there are no null or dup elements in checked
 *              there might be null or dups in toBeCombined
 *
 * @param checked the array of objects already checked
 * @param toBeCombined the array of objects to be combined
 *
 * @return the combination of these two array
 */
static ProtectionDomain[] combinePDObjs(ProtectionDomain[] checked, Object[] toBeCombined) {
	return (ProtectionDomain[]) combineObjs(true, checked, toBeCombined, 0, (null != toBeCombined) ? toBeCombined.length : 0, false);
}

/**
 * Combine checked and toBeCombined Permission objects
 * Assuming:    there are no null or dup elements in checked
 *              there might be null or dups in toBeCombined
 *
 * @param checked the array of objects already checked
 * @param toBeCombined the array of objects to be combined
 * @param start the start position in the toBeCombined array
 * @param len the number of element to be copied
 * @param justCombine the flag to indicate if null/dup check is needed
 *
 * @return the combination of these two array
 */
static Permission[] combinePermObjs(Permission[] checked, Permission[] toBeCombined, int start, int len, boolean justCombine) {
	return (Permission[]) combineObjs(false, checked, toBeCombined, start, len, justCombine);
}

/**
 * Perform ProtectionDomain.implies(permission) with known ProtectionDomain objects already implied
 *
 * @param perm the permission to be checked
 * @param toCheck the ProtectionDomain to be checked
 * @param cacheChecked the cached check result which is an array with following three elements:
 * ProtectionDomain[] pdsImplied, Permission[] permsImplied, Permission[] permsNotImplied
 *
 * @return -1 if toCheck is null, among pdsImplied or each ProtectionDomain within toCheck implies perm,
 * otherwise the index of ProtectionDomain objects not implied
 */
static int checkPermWithCachedPDsImplied(Permission perm, Object[] toCheck, AccessCache cacheChecked) {
	if (null == toCheck) {
		return -1; // nothing to check, implied
	}
	ProtectionDomain[] pdsImplied = (null == cacheChecked) ? null : cacheChecked.pdsImplied;

	// in reverse order as per RI behavior
	check: for (int i = toCheck.length; i > 0;) {
		i -= 1;
		Object domain = toCheck[i];
		if (null != domain) {
			if (null != pdsImplied) {
				for (int j = 0; j < pdsImplied.length; ++j) {
					if (domain == pdsImplied[j]) {
						continue check;
					}
				}
			}
			/*[IF Sidecar19-SE]*/
			if (!((ProtectionDomain) domain).impliesWithAltFilePerm(perm)) {
			/*[ELSE]*/
			if (!((ProtectionDomain) domain).implies(perm)) {
			/*[ENDIF] Sidecar19-SE*/
				return i; // NOT implied
			}
		}
	}
	if (null != cacheChecked) {
		cacheChecked.pdsImplied = combinePDObjs(pdsImplied, toCheck);
	}
	return -1; // all implied
}

/**
 * Perform Permission.implies(permission) with known Permission objects already implied and NOT implied
 *
 * @param perm the permission to be checked
 * @param permsLimited the limited Permission to be checked
 * @param cacheChecked the cached check result which is an array with following three elements:
 * ProtectionDomain[] pdsImplied, Permission[] permsImplied, Permission[] permsNotImplied
 *
 * @return true if there is a limited permission implied perm, otherwise false
 */
static boolean checkPermWithCachedPermImplied(Permission perm, Permission[] permsLimited, AccessCache cacheChecked) {
	if (null == permsLimited) {
		return false;
	}
	Permission[] permsImplied = null;
	Permission[] permsNotImplied = null;
	if (null != cacheChecked) {
		permsImplied = cacheChecked.permsImplied;
		permsNotImplied = cacheChecked.permsNotImplied;
	}
	boolean success = false;
	int lenNotImplied = permsLimited.length;
	for (int j = 0; j < permsLimited.length; ++j) {
		if (null != permsLimited[j]) { // go through each non-null limited permission
			if (null != permsImplied) {
				for (int k = 0; k < permsImplied.length; ++k) {
					if (permsLimited[j] == permsImplied[k]) {
						success = true; // already implied before
						break;
					}
				}
				if (success) { // already implied
					lenNotImplied = j;
					break;
				}
			}
			boolean notImplied = false;
			if (null != permsNotImplied) {
				for (int k = 0; k < permsNotImplied.length; ++k) {
					if (permsLimited[j] == permsNotImplied[k]) {
						notImplied = true; // already NOT implied before
						lenNotImplied = j;
						break;
					}
				}
			}
			/*[IF Sidecar19-SE]*/
			permsLimited[j] = FilePermCompat.newPermPlusAltPath(permsLimited[j]);
			/*[ENDIF] Sidecar19-SE*/
			if (!notImplied && permsLimited[j].implies(perm)) {
				success = true; // just implied
				if (null != cacheChecked) {
					cacheChecked.permsImplied = combinePermObjs(permsImplied, permsLimited, j, 1, true);
				}
				lenNotImplied = j;
				break;
			}
		}
	}
	if (0 < lenNotImplied && null != cacheChecked) {
		cacheChecked.permsNotImplied = combinePermObjs(permsNotImplied, permsLimited, 0, lenNotImplied, false);
	}
	return success;
}

/**
 * Checks if the permission perm is allowed as per incoming
 *  AccessControlContext/ProtectionDomain[]/isLimited/Permission[]
 * while taking advantage cached
 *  ProtectionDomain[] pdsImplied, Permission[] permsImplied, Permission[] permsNotImplied
 *
 * @param perm the permission to be checked
 * @param accCurrent the current AccessControlContext to be checked
 * @param debug debug flags
 * @param pdsContext the current context to be checked
 * @param isLimited the flag to indicate if there are limited permission(s)
 * @param permsLimited the limited permission(s) to be checked
 * @param accNext the next AccessControlContext to be checked
 * @param cacheChecked the cached check result which is an array with following three elements:
 * ProtectionDomain[] pdsImplied, Permission[] permsImplied, Permission[] permsNotImplied
 *
 * @return true if the access is granted by a limited permission, otherwise an exception is thrown or
 * false is returned to indicate the access was NOT granted by any limited permission.
 */
static boolean checkPermissionWithCache(
		Permission perm,
		Object[] pdsContext,
		int debug,
		AccessControlContext accCurrent,
		boolean isLimited,
		Permission[] permsLimited,
		AccessControlContext accNext,
		AccessCache cacheChecked
) throws AccessControlException {
	if (((debug & DEBUG_ENABLED) != 0) && ((debugSetting() & DEBUG_ACCESS_DOMAIN) != 0)) {
		debugPrintAccess();
		if (pdsContext == null || pdsContext.length == 0) {
			System.err.println("domain (context is null)"); //$NON-NLS-1$
		} else {
			for (int i = 0; i < pdsContext.length; ++i) {
				System.err.println("domain " + i + " " + pdsContext[i]); //$NON-NLS-1$ //$NON-NLS-2$
			}
		}
	}
	if (null != pdsContext) {
		int i = checkPermWithCachedPDsImplied(perm, pdsContext, cacheChecked);
		if (0 <= i) {
			// debug for access denied is not optional
			if ((debug & DEBUG_ACCESS_DENIED) != 0) {
				if ((debugSetting() & DEBUG_ACCESS) != 0) {
					debugPrintAccess();
					System.err.println("access denied " + perm); //$NON-NLS-1$
				}
				if ((debugSetting() & DEBUG_ACCESS_FAILURE) != 0) {
					new Exception("Stack trace").printStackTrace(); //$NON-NLS-1$
					System.err.println("domain that failed " + pdsContext[i]); //$NON-NLS-1$
				}
			}
			/*[MSG "K002c", "Access denied {0}"]*/
			throw new AccessControlException(com.ibm.oti.util.Msg.getString("K002c", perm), perm); //$NON-NLS-1$
		}
	}
	if (null != accCurrent
		&& (null != accCurrent.context || null != accCurrent.doPrivilegedAcc || null != accCurrent.limitedPerms || null != accCurrent.nextStackAcc)
	) {
		// accCurrent check either throwing a security exception (denied) or continue checking (the return value doesn't matter)
		checkPermissionWithCache(perm, accCurrent.context, debug, accCurrent.doPrivilegedAcc, accCurrent.isLimitedContext, accCurrent.limitedPerms, accCurrent.nextStackAcc, cacheChecked);
	}
	if (isLimited && null != permsLimited) {
		if (checkPermWithCachedPermImplied(perm, permsLimited, cacheChecked)) {
			return true; // implied by a limited permission
		}
		if (null != accNext) {
			checkPermissionWithCache(perm, accNext.context, debug, accNext.doPrivilegedAcc, accNext.isLimitedContext, accNext.limitedPerms, accNext.nextStackAcc, cacheChecked);
		}
		return false; // NOT implied by any limited permission
	}
	if ((debug & DEBUG_ENABLED) != 0) {
		debugPrintAccess();
		System.err.println("access allowed " + perm); //$NON-NLS-1$
	}
	return true;
}

/**
 * Helper to print debug information for checkPermission().
 *
 * @param perm the permission to check
 * @return if debugging is enabled
 */
private boolean debugHelper(Permission perm) {
	boolean debug = true;
	if (debugCodeBaseArray != null) {
		debug = hasDebugCodeBase();
	}
	if (debug) {
		debug = debugPermission(perm);
	}
	if (debug && ((debugSetting() & DEBUG_ACCESS_STACK) != 0)) {
		new Exception("Stack trace for " + perm).printStackTrace(); //$NON-NLS-1$
	}
	return debug;
}

/**
 * Checks if the permission <code>perm</code> is allowed in this context.
 * All ProtectionDomains must grant the permission for it to be granted.
 *
 * @param       perm java.security.Permission
 *                  the permission to check
 * @exception   java.security.AccessControlException
 *                  thrown when perm is not granted.
 * @exception   NullPointerException 
 *                  if perm is null
 */
public void checkPermission(Permission perm) throws AccessControlException {
	if (perm == null) throw new NullPointerException();
	if (null != context && (STATE_AUTHORIZED != authorizeState) && containPrivilegedContext && null != System.getSecurityManager()) {
		// only check SecurityPermission "createAccessControlContext" when context is not null, not authorized and containPrivilegedContext.
		if (STATE_UNKNOWN == authorizeState) {
			if (null == callerPD || callerPD.implies(createAccessControlContext)) {
				authorizeState = STATE_AUTHORIZED;
			} else {
				authorizeState = STATE_NOT_AUTHORIZED;
			}
			callerPD = null;
		}
		if (STATE_NOT_AUTHORIZED == authorizeState) {
			/*[MSG "K002d", "Access denied {0} due to untrusted AccessControlContext since {1} is denied"]*/
			throw new AccessControlException(com.ibm.oti.util.Msg.getString("K002d", perm, createAccessControlContext), perm); //$NON-NLS-1$
		}
	}

	boolean debug = (debugSetting() & DEBUG_ACCESS) != 0;
	if (debug) {
		debug = debugHelper(perm);
	}
	checkPermissionWithCache(perm,  this.context, debug ? DEBUG_ENABLED | DEBUG_ACCESS_DENIED : DEBUG_DISABLED, this.doPrivilegedAcc,this.isLimitedContext, this.limitedPerms, this.nextStackAcc, new AccessCache());
}

/**
 * Helper method for equals() checks if the two arrays contain the same sets of objects or nulls.
 * The order of the elements is not significant.
 * If either of the given arrays is null it is treated as an empty array.
 *
 * @param s1  the first set
 * @param s2  the second set
 * @return true if the arrays have the same length and contain the same elements
 */
private static boolean equalSets(Object[] s1, Object[] s2) {
	final int length;

	if (s1 == null || (length = s1.length) == 0) {
		// the first set is empty, the second must be empty as well
		return s2 == null || s2.length == 0;
	}

	// the first set is not empty, the second must be the same size
	if (s2 == null || length != s2.length) {
		return false;
	}

	next: for (int i = 0; i < length; ++i) {
		Object object = s1[i];

		if (object == null) {
			for (int j = 0; j < length; ++j) {
				if (s2[j] == null) {
					continue next;
				}
			}
		} else {
			for (int j = 0; j < length; ++j) {
				if (object.equals(s2[j])) {
					continue next;
				}
			}
		}

		// object was not found in the second set
		return false;
	}

	return true;
}

/**
 * Compares the argument to the receiver, and answers true
 * if they represent the <em>same</em> object using a class
 * specific comparison. In this case, they must both be
 * AccessControlContexts and contain the same protection domains.
 *
 * @param       o       the object to compare with this object
 * @return      <code>true</code>
 *                  if the object is the same as this object
 *              <code>false</code>
 *                  if it is different from this object
 * @see         #hashCode
 */
@Override
public boolean equals(Object o) {
	if (this == o) return true;
	if (o == null || this.getClass() != o.getClass()) return false;
	AccessControlContext otherContext = (AccessControlContext) o;
	/*[PR RTC 66684] j.s.AccessControlContext fields isAuthorized/domainCombiner not used in method equals() & hashCode() */
	// match RI behaviors, i.e., ignore isAuthorized when performing equals
	if (null != this.domainCombiner) {
		if (!this.domainCombiner.equals(otherContext.domainCombiner)) {
			return false;
		}
	} else {
		if (null != otherContext.domainCombiner) {
			return false;
		}
	}
	if (isLimitedContext != otherContext.isLimitedContext) {
		return false;
	}
	if (!equalSets(context, otherContext.context)) {
		return false;
	}
	if (null != doPrivilegedAcc && !doPrivilegedAcc.equals(otherContext.doPrivilegedAcc)) {
		return false;
	}
	if (isLimitedContext) {
		if (!equalSets(limitedPerms, otherContext.limitedPerms)) {
			return false;
		}
		if (null != nextStackAcc) {
			return nextStackAcc.equals(otherContext.nextStackAcc);
		}
	}
	return true;
}

/**
 * Answers an integer hash code for the receiver. Any two
 * objects which answer <code>true</code> when passed to
 * <code>equals</code> must answer the same value for this
 * method.
 *
 * @return the receiver's hash
 *
 * @see #equals
 */
@Override
public int hashCode() {
	int result = 0;
	int i = context == null ? 0 : context.length;
	while ((--i >= 0) && (context[i] != null)) {
		result ^= context[i].hashCode();
	}

	// RI equals not impacted by limited context,
	// JCK still passes with following cause the AccessControlContext in question doesn't have limited context
	// J9 might fail JCK test if JCK hashcode test changes

	if (null != doPrivilegedAcc) {
		result ^= doPrivilegedAcc.hashCode();
	}
	// result = result + (this.isLimitedContext ? 1231 : 1237);
	if (this.isLimitedContext) {
		i = (limitedPerms == null) ? 0 : limitedPerms.length;
		while (--i >= 0) {
			if (null != limitedPerms[i]) {
				result ^= limitedPerms[i].hashCode();
			}
		}
		if (null != nextStackAcc) {
			result ^= nextStackAcc.hashCode();
		}
	}
	return result;
}

/**
 * Answers the DomainCombiner for the receiver.
 *
 * @return the DomainCombiner or null
 *
 * @exception java.security.AccessControlException thrown
 *      when the caller doesn't have the  "getDomainCombiner" SecurityPermission
 */
public DomainCombiner getDomainCombiner() {
	SecurityManager security = System.getSecurityManager();
	if (security != null)
		security.checkPermission(getDomainCombiner);
	return domainCombiner;
}

/**
 * Answers the DomainCombiner for the receiver.
 * This is for internal use without checking "getDomainCombiner" SecurityPermission.
 *
 * @return the DomainCombiner or null
 *
 */
DomainCombiner getCombiner() {
	return domainCombiner;
}

/*
 * Added to resolve: S6907662, CVE-2010-4465: System clipboard should ensure access restrictions
 * Used internally:
 *      java.awt.AWTEvent
 *      java.awt.Component
 *      java.awt.EventQueue
 *      java.awt.MenuComponent
 *      java.awt.TrayIcon
 *      java.security.ProtectionDomain
 *      javax.swing.Timer
 *      javax.swing.TransferHandler
 */
ProtectionDomain[] getContext() {
	return context;
}

/*
 * Added to resolve: S6907662, CVE-2010-4465: System clipboard should ensure access restrictions
 * Called internally from java.security.ProtectionDomain
 */
AccessControlContext(ProtectionDomain[] domains, AccessControlContext acc) {
	this.context = AccessController.toArrayOfProtectionDomains(domains, acc, 0);
	this.authorizeState = STATE_AUTHORIZED;
	this.containPrivilegedContext = true;
	if ((null != acc) && (STATE_AUTHORIZED == acc.authorizeState)) {
		// inherit the domain combiner when authorized
		this.domainCombiner = acc.domainCombiner;
	}
}

/*
 * Added to resolve: S6907662, CVE-2010-4465: System clipboard should ensure access restrictions
 * Called internally from java.security.ProtectionDomain
 */
AccessControlContext optimize() {
	return this;
}

}
