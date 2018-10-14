/*[INCLUDE-IF Sidecar16]*/

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

/*
 *
 * Change activity:
 *
 * Reason  Date   Origin  Description
 * ------  ----   ------  ----------------------------------------------------
 * 075648  080704 kates:  Trace Engine Separation
 * 094077  170805 kilnerm Rework AppTrace mechanism
 *
 * ===========================================================================
 * Module Information:
 *
 * DESCRIPTION: Trace class for JVM trace facility manipulation
 * ===========================================================================
 */

package com.ibm.jvm;

import java.util.*;

/**
 * The <code>Trace</code> class contains methods for controlling trace and using application trace.
 * This class cannot be instantiated.
 * <p>
 * The methods {@link #set(String cmd)}, {@link #suspend()}, {@link #resume()}, {@link #suspendThis()}, {@link #resumeThis()} and {@link #snap()} are used
 * to control trace by changing the current trace settings, suspending or resuming tracing globally or just for the current thread and by
 * triggering a snap dump of the current trace buffers.
 * <p>
 * The {@link #registerApplication(String name, String[] templates)} method allows a new trace component and trace points
 * to be registered with trace.
 * <p>
 * The {@code trace(int handle, int traceId, ...)} methods are used take the application
 * trace points registered by {@code registerApplication(name, formats)}. The handle
 * value is the integer returned by the  {@code registerApplication(name, formats)}.
 * The traceId is an index into the formats array and the {@code trace(int handle, int traceId, ...)}
 * method called must match the types required by the format String {@code formats[traceId]}.
 * <p>
 * If the number or types of the parameters passed to {@code trace(int handle, int traceId, ...)}
 * do not match the arguments in {@code formats[traceId]} a java/lang/IllegalArgumentException
 * will be thrown.
 */
public final class Trace {

	public static final String EVENT = "0 "; //$NON-NLS-1$
	public static final String EXCEPTION = "1 "; //$NON-NLS-1$
	public static final String ENTRY = "2 "; //$NON-NLS-1$
	public static final String EXIT = "4 "; //$NON-NLS-1$
	public static final String EXCEPTION_EXIT = "5 "; //$NON-NLS-1$

	private static final String LEGACY_TRACE_PERMISSION_PROPERTY = "com.ibm.jvm.enableLegacyTraceSecurity"; //$NON-NLS-1$

	private static final TracePermission TRACE_PERMISSION = new TracePermission();

	/**
	 * Initialize the class.
	 */
	static {
		initTraceImpl();
	}

	/** Don't let anyone instantiate this class. */
	private Trace() {
		super();
	}

	/**
	 * This method does nothing.
	 * 
	 * @deprecated this method does nothing
	 */
	/*[IF Sidecar19-SE]
	@Deprecated(forRemoval = true, since = "1.8")
	/*[ELSE] Sidecar19-SE */
	@Deprecated
	/*[ENDIF] Sidecar19-SE */
	public static void initializeTrace() {
		// initialization has already been done 
	}

	/*
	 * Natives
	 */

	// Trace initialization
	private static native void initTraceImpl();

	/**
	 * Sets options for the trace subsystem.
	 * <p>
	 * The trace option is passed in as a String.
	 * <p>
	 * Use the same syntax as the -Xtrace command-line option, with the initial
	 * -Xtrace: omitted.
	 * <p>
	 * See Using the -Xtrace option as described in the section on trace options
	 * in the documentation for the IBM JVM. Note that some options can only be
	 * set at startup on the command line not at runtime using this method.
	 * <p>
	 * This method returns zero on success and non-zero otherwise.
	 * <p>
	 * A security manager check will be made only if the system property
	 * com.ibm.jvm.enableLegacyTraceSecurity is set to "true" in which case a
	 * check will be made for com.ibm.jvm.TracePermission
	 * 
	 * @param cmd
	 *            the trace options string to set
	 * @return 0 on success, non-zero otherwise
	 * 
	 * @throws SecurityException
	 *             if there is a security manager and it doesn't allow the
	 *             checks required to change the dump settings
	 */
	public static int set(String cmd) {
		checkLegacySecurityPermssion();
		return setImpl(cmd);
	}

	/**
     * Trigger a snap dump. The snap dump format is not human-readable
     * and must be processed using the trace formatting tool supplied
     * with the IBM JVM.
	 * 
	 * A security manager check will be made only if the system property
	 * com.ibm.jvm.enableLegacyTraceSecurity is set to "true" in which case
	 * a check will be made for com.ibm.jvm.TracePermission
	 * 
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to take this dump
	 */
	public static void snap() {
		checkLegacySecurityPermssion();
		snapImpl();
	}
	
	/**
	 * Suspends tracing for all the threads in the JVM.
	 * 
	 * A security manager check will be made only if the system property
	 * com.ibm.jvm.enableLegacyTraceSecurity is set to "true" in which case
	 * a check will be made for com.ibm.jvm.TracePermission
	 * 
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to take this dump
	 */
	public static void suspend() {
		checkLegacySecurityPermssion();
		suspendImpl();	
	}

	/**
	 * Resumes tracing for all the threads in the JVM.
	 * 
	 * A security manager check will be made only if the system property
	 * com.ibm.jvm.enableLegacyTraceSecurity is set to "true" in which case
	 * a check will be made for com.ibm.jvm.TracePermission
	 * 
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to take this dump
	 */
	public static void resume() {
		checkLegacySecurityPermssion();
		resumeImpl();
	}

	/**
	 * Decrements the suspend and resume count for the current thread and suspends
	 * tracing the thread if the result is negative.
	 * 
	 * A security manager check will be made only if the system property
	 * com.ibm.jvm.enableLegacyTraceSecurity is set to "true" in which case
	 * a check will be made for com.ibm.jvm.TracePermission
	 * 
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to take this dump
	 */
	public static void suspendThis() {
		checkLegacySecurityPermssion();
		suspendThisImpl();
	}

	/**
	 * Increments the suspend and resume count for the current thread and resumes
	 * tracing the thread if the result is not negative.
	 * 
	 * A security manager check will be made only if the system property
	 * com.ibm.jvm.enableLegacyTraceSecurity is set to "true" in which case
	 * a check will be made for com.ibm.jvm.TracePermission
	 * 
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to take this dump
	 */
	public static void resumeThis() {
		checkLegacySecurityPermssion();
		resumeThisImpl();
	}

	// Application trace registration method
	/**
	 * Registers a new application and trace points for that application with
	 * trace. See Registering for trace as described in the section on
	 * Application Trace in the documentation for the IBM JVM.
	 * <p>
	 * The registerApplication() method returns an integer value. Use this value
	 * as the handle parameter in subsequent trace() calls. If the
	 * registerApplication() method call fails for any reason the value returned
	 * is -1.
	 * <p>
	 * When enabling or disabling trace points registered for this application
	 * either on the command line or using Trace.set() the trace point id's will
	 * be the application name and the index in the template array.
	 * 
	 * For example if you register an application called HelloWorld with three
	 * Strings in the template array the trace id's will be HelloWorld.0,
	 * HelloWorld.1 and HelloWorld.2
	 * 
	 * To start printing the third trace point you could call Trace.set() like
	 * this: {@code Trace.set("print=HelloWorld.2"); }
	 * <p>
	 * A security manager check will be made only if the system property
	 * com.ibm.jvm.enableLegacyTraceSecurity is set to "true" in which case a
	 * check will be made for com.ibm.jvm.TracePermission
	 * 
	 * @param name
	 *            the name of the application you want to trace
	 * @param templates
	 *            an array of format strings like the strings used by the C
	 *            printf method
	 * 
	 * @throws SecurityException
	 *             if there is a security manager and it doesn't allow the
	 *             checks required to take this dump
	 * @return an int that should be passed to trace() calls as the handle value
	 *         or -1 on error.
	 */
	public synchronized static int registerApplication(String name,
			String[] templates) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(name, "name"); //$NON-NLS-1$
		Objects.requireNonNull(templates, "templates"); //$NON-NLS-1$
		return registerApplicationImpl(name, templates);
	}  /* ibm@94077 */

	// Trace control API natives
	private static native int setImpl(String cmd);

	private static native void snapImpl();

	private static native void suspendImpl();

	private static native void resumeImpl();

	private static native void suspendThisImpl();

	private static native void resumeThisImpl();

	// Application trace registration method
	private synchronized static native int registerApplicationImpl(String name,
			String[] templates); /* ibm@94077 */

	/**
	 * Check the caller has permission to use the Trace API for calls that existed pre-Java 8
	 * when security was added. Public API added after Java 8 should call checkTraceSecurityPermssion()
	 * directly.
	 * @throws SecurityException
	 */
    private static void checkLegacySecurityPermssion() throws SecurityException {
    	if (!("false".equalsIgnoreCase(com.ibm.oti.vm.VM.getVMLangAccess()	//$NON-NLS-1$
    		.internalGetProperties().getProperty(LEGACY_TRACE_PERMISSION_PROPERTY)))) {
    		checkTraceSecurityPermssion();
    	}
    }
	
    private static void checkTraceSecurityPermssion() throws SecurityException {
		/* Check the caller has TracePermission. */
		SecurityManager manager = System.getSecurityManager();
		if (manager != null) {
			manager.checkPermission(TRACE_PERMISSION);
		}
	}

	// Application trace tracing methods
	public static void trace(int handle, int traceId) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId);
	}

	public static void trace(int handle, int traceId, String s1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1);
	}

	public static void trace(int handle, int traceId, String s1, String s2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		Objects.requireNonNull(s2, "s2"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, s2);
	}

	public static void trace(int handle, int traceId, String s1, String s2, String s3) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		Objects.requireNonNull(s2, "s2"); //$NON-NLS-1$
		Objects.requireNonNull(s3, "s3"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, s2, s3);
	}

	public static void trace(int handle, int traceId, String s1, Object o1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		Objects.requireNonNull(o1, "o1"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, o1);
	}

	public static void trace(int handle, int traceId, Object o1, String s1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(o1, "o1"); //$NON-NLS-1$
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, o1, s1);
	}

	public static void trace(int handle, int traceId, String s1, int i1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, i1);
	}

	public static void trace(int handle, int traceId, int i1, String s1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, i1, s1);
	}

	public static void trace(int handle, int traceId, String s1, long l1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, l1);
	}

	public static void trace(int handle, int traceId, long l1, String s1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, l1, s1);
	}

	public static void trace(int handle, int traceId, String s1, byte b1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, b1);
	}

	public static void trace(int handle, int traceId, byte b1, String s1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, b1, s1);
	}

	public static void trace(int handle, int traceId, String s1, char c1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, c1);
	}

	public static void trace(int handle, int traceId, char c1, String s1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, c1, s1);
	}

	public static void trace(int handle, int traceId, String s1, float f1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, f1);
	}

	public static void trace(int handle, int traceId, float f1, String s1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, f1, s1);
	}

	public static void trace(int handle, int traceId, String s1, double d1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, d1);
	}

	public static void trace(int handle, int traceId, double d1, String s1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, d1, s1);
	}

	public static void trace(int handle, int traceId, Object o1) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(o1, "o1"); //$NON-NLS-1$
		traceImpl(handle, traceId, o1);
	}

	public static void trace(int handle, int traceId, Object o1, Object o2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(o1, "o1"); //$NON-NLS-1$
		Objects.requireNonNull(o2, "o2"); //$NON-NLS-1$
		traceImpl(handle, traceId, o1, o2);
	}

	public static void trace(int handle, int traceId, int i1) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, i1);
	}

	public static void trace(int handle, int traceId, int i1, int i2) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, i1, i2);
	}

	public static void trace(int handle, int traceId, int i1, int i2, int i3) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, i1, i2, i3);
	}

	public static void trace(int handle, int traceId, long l1) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, l1);
	}

	public static void trace(int handle, int traceId, long l1, long l2) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, l1, l2);
	}

	public static void trace(int handle, int traceId, long l1, long l2, long i3) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, l1, l2, i3);
	}

	public static void trace(int handle, int traceId, byte b1) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, b1);
	}

	public static void trace(int handle, int traceId, byte b1, byte b2) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, b1, b2);
	}

	public static void trace(int handle, int traceId, byte b1, byte b2, byte b3) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, b1, b2, b3);
	}

	public static void trace(int handle, int traceId, char c1) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, c1);
	}

	public static void trace(int handle, int traceId, char c1, char c2) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, c1, c2);
	}

	public static void trace(int handle, int traceId, char c1, char c2, char c3) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, c1, c2, c3);
	}

	public static void trace(int handle, int traceId, float f1) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, f1);
	}

	public static void trace(int handle, int traceId, float f1, float f2) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, f1, f2);
	}

	public static void trace(int handle, int traceId, float f1, float f2, float f3) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, f1, f2, f3);
	}

	public static void trace(int handle, int traceId, double d1) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, d1);
	}

	public static void trace(int handle, int traceId, double d1, double d2) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, d1, d2);
	}

	public static void trace(int handle, int traceId, double d1, double d2, double d3) {
		checkLegacySecurityPermssion();
		traceImpl(handle, traceId, d1, d2, d3);
	}

	public static void trace(int handle, int traceId, String s1, Object o1, String s2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		Objects.requireNonNull(o1, "o1"); //$NON-NLS-1$
		Objects.requireNonNull(s2, "s2"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, o1, s2);
	}

	public static void trace(int handle, int traceId, Object o1, String s1, Object o2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(o1, "o1"); //$NON-NLS-1$
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		Objects.requireNonNull(o2, "o2"); //$NON-NLS-1$
		traceImpl(handle, traceId, o1, s1, o2);
	}

	public static void trace(int handle, int traceId, String s1, int i1, String s2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		Objects.requireNonNull(s2, "s2"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, i1, s2);
	}

	public static void trace(int handle, int traceId, int i1, String s1, int i2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, i1, s1, i2);
	}

	public static void trace(int handle, int traceId, String s1, long l1, String s2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		Objects.requireNonNull(s2, "s2"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, l1, s2);
	}

	public static void trace(int handle, int traceId, long l1, String s1, long l2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, l1, s1, l2);
	}

	public static void trace(int handle, int traceId, String s1, byte b1, String s2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		Objects.requireNonNull(s2, "s2"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, b1, s2);
	}

	public static void trace(int handle, int traceId, byte b1, String s1, byte b2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, b1, s1, b2);
	}

	public static void trace(int handle, int traceId, String s1, char c1, String s2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		Objects.requireNonNull(s2, "s2"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, c1, s2);
	}

	public static void trace(int handle, int traceId, char c1, String s1, char c2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, c1, s1, c2);
	}

	public static void trace(int handle, int traceId, String s1, float f1, String s2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		Objects.requireNonNull(s2, "s2"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, f1, s2);
	}

	public static void trace(int handle, int traceId, float f1, String s1, float f2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, f1, s1, f2);
	}

	public static void trace(int handle, int traceId, String s1, double d1, String s2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		Objects.requireNonNull(s2, "s2"); //$NON-NLS-1$
		traceImpl(handle, traceId, s1, d1, s2);
	}

	public static void trace(int handle, int traceId, double d1, String s1, double d2) {
		checkLegacySecurityPermssion();
		Objects.requireNonNull(s1, "s1"); //$NON-NLS-1$
		traceImpl(handle, traceId, d1, s1, d2);
	}

	// Application trace tracing natives
	private static native void traceImpl(int handle, int traceId);

	private static native void traceImpl(int handle, int traceId, String s1);

	private static native void traceImpl(int handle, int traceId, String s1, String s2);

	private static native void traceImpl(int handle, int traceId, String s1, String s2, String s3);

	private static native void traceImpl(int handle, int traceId, String s1, Object o1);

	private static native void traceImpl(int handle, int traceId, Object o1, String s1);

	private static native void traceImpl(int handle, int traceId, String s1, int i1);

	private static native void traceImpl(int handle, int traceId, int i1, String s1);

	private static native void traceImpl(int handle, int traceId, String s1, long l1);

	private static native void traceImpl(int handle, int traceId, long l1, String s1);

	private static native void traceImpl(int handle, int traceId, String s1, byte b1);

	private static native void traceImpl(int handle, int traceId, byte b1, String s1);

	private static native void traceImpl(int handle, int traceId, String s1, char c1);

	private static native void traceImpl(int handle, int traceId, char c1, String s1);

	private static native void traceImpl(int handle, int traceId, String s1, float f1);

	private static native void traceImpl(int handle, int traceId, float f1, String s1);

	private static native void traceImpl(int handle, int traceId, String s1, double d1);

	private static native void traceImpl(int handle, int traceId, double d1, String s1);

	private static native void traceImpl(int handle, int traceId, Object o1);

	private static native void traceImpl(int handle, int traceId, Object o1, Object o2);

	private static native void traceImpl(int handle, int traceId, int i1);

	private static native void traceImpl(int handle, int traceId, int i1, int i2);

	private static native void traceImpl(int handle, int traceId, int i1, int i2, int i3);

	private static native void traceImpl(int handle, int traceId, long l1);

	private static native void traceImpl(int handle, int traceId, long l1, long l2);

	private static native void traceImpl(int handle, int traceId, long l1, long l2, long i3);

	private static native void traceImpl(int handle, int traceId, byte b1);

	private static native void traceImpl(int handle, int traceId, byte b1, byte b2);

	private static native void traceImpl(int handle, int traceId, byte b1, byte b2, byte b3);

	private static native void traceImpl(int handle, int traceId, char c1);

	private static native void traceImpl(int handle, int traceId, char c1, char c2);

	private static native void traceImpl(int handle, int traceId, char c1, char c2, char c3);

	private static native void traceImpl(int handle, int traceId, float f1);

	private static native void traceImpl(int handle, int traceId, float f1, float f2);

	private static native void traceImpl(int handle, int traceId, float f1, float f2, float f3);

	private static native void traceImpl(int handle, int traceId, double d1);

	private static native void traceImpl(int handle, int traceId, double d1, double d2);

	private static native void traceImpl(int handle, int traceId, double d1, double d2, double d3);

	private static native void traceImpl(int handle, int traceId, String s1, Object o1, String s2);

	private static native void traceImpl(int handle, int traceId, Object o1, String s1, Object o2);

	private static native void traceImpl(int handle, int traceId, String s1, int i1, String s2);

	private static native void traceImpl(int handle, int traceId, int i1, String s1, int i2);

	private static native void traceImpl(int handle, int traceId, String s1, long l1, String s2);

	private static native void traceImpl(int handle, int traceId, long l1, String s1, long l2);

	private static native void traceImpl(int handle, int traceId, String s1, byte b1, String s2);

	private static native void traceImpl(int handle, int traceId, byte b1, String s1, byte b2);

	private static native void traceImpl(int handle, int traceId, String s1, char c1, String s2);

	private static native void traceImpl(int handle, int traceId, char c1, String s1, char c2);

	private static native void traceImpl(int handle, int traceId, String s1, float f1, String s2);

	private static native void traceImpl(int handle, int traceId, float f1, String s1, float f2);

	private static native void traceImpl(int handle, int traceId, String s1, double d1, String s2);

	private static native void traceImpl(int handle, int traceId, double d1, String s1, double d2);

	/**
	 * Returns the microsecond time stamp. The accuracy of the time returned
	 * depends on the level of support provided by the particular chip the JVM
	 * is running on. In particular the time may not exactly match the time
	 * returned by the system clock, so times from this method should only be
	 * compared with other times returned from this method.
	 * 
	 * @deprecated Use {@code System.nanoTime()} instead as this provides as
	 *             good or better resolution and is more portable.
	 * @return the current time in microseconds or 0 if a high-resolution timer
	 *         is not supported on the platform.
	 */
	@Deprecated
	public static native long getMicros();

}
