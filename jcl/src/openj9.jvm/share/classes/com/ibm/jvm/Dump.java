/*[INCLUDE-IF Sidecar16]*/
package com.ibm.jvm;

/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

/**
 * This class is used to trigger and configure the options used to produce different
 * types of diagnostic dumps available from the OpenJ9 JVM.
 * <p>
 * -Xdump must be enabled on the command line or the functions that attempt to cause
 * dumps to be created or set options will fail with a java.lang.RuntimeException.
 * <p>
 * The methods on this class can be used to trigger dumps, configure dump options and
 * query those options.
 * <p>
 * The {@link #JavaDump()}, {@link #SystemDump()}, {@link #HeapDump()} and {@link #SnapDump()}
 * methods trigger dumps of the given type with no options and no return value.
 * Although they are not configurable they do provide an easy API to use via reflection
 * if your code is likely to run on both OpenJ9 and non-OpenJ9 JVMs and you only need the most
 * basic ability to create a dump.
 * <p>
 * The {@link #javaDumpToFile()}, {@link #systemDumpToFile()}, {@link #heapDumpToFile()} and
 * {@link #snapDumpToFile()} methods allow a destination file to be optionally specified and
 * will return the full path of the file that is created.
 * <br>
 * The recommended usage of the {@link #javaDumpToFile()}, {@link #systemDumpToFile()},
 * {@link #heapDumpToFile()} and {@link #snapDumpToFile()}
 * methods is to call the no argument versions  of these calls rather than specifying a file
 * name as this will trigger a dump to the default location. Your dump file will go to the
 * default location specified by any -Xdump options given to the JVM at startup time following
 * the user or administrators preferences.
 * The location the dump file was written to will be returned as a String so the generated
 * file can be located.
 * <p>
 * The {@link #triggerDump(String)} method offers similar functionality as the DumpToFile() methods
 * but with the ability to specify any dump options that are meaningful for a dump that occurs
 * immediately. The options are passed as a String that follows the same format as the option
 * strings passed to -Xdump on the command line.<br>
 * For example:
 * <ul>
 * <li>triggerDump("java") is equivalent to javaDumpToFile() or javaDumpToFile(null) all three
 * will cause a javadump to be generated to the default location.</li>
 * <li>triggerDump("heap:file=heapdump.phd") is equivalent to heapDumpToFile("heapdump.phd")</li>
 * <li>triggerDump("heap:file=heapdump.txt,opts=CLASSIC") allows you to specify the CLASSIC
 * option to triggerDump and produce a text format heap dump which is not possible through
 * the *DumpToFile(String filename) or *Dump() methods.</li>
 * <li>triggerDump("java:request=exclusive") will trigger a java dump with the request option set
 * to "exclusive" and any other options, including the file name, taken from the default options
 * for java dumps</li>
 * </ul>
 * <p>
 * The {@link #setDumpOptions(String)} method allows dump options that will cause or change how
 * a dump occurs for an event in the future to be specified. The options are specified in the
 * format expected by the -Xdump command line. Not all options can be configured at runtime and
 * this method will throw an InvalidDumpOption exception if it is passed an option that cannot be set.<p>
 * For example:
 * <ul>
 * <li>setDumpOptions("java") - enable java dumps with the default settings.</li>
 * <li>setDumpOptions("java:events=vmstop") - enable java dumps on the vmstop event (this will
 * occur once when the JVM exits).</li>
 * <li>setDumpOptions("none") - disable all dump agents on all events.</li>
 * <li>setDumpOptions("heap:none") - disable all heap dump agents on all events.</li>
 * <li>setDumpOptions("system:none:events=systhrow,filter=java/lang/OutOfMemoryError") - disable
 * system dumps on systhrow events for OutOfMemory errors only.</li>
 * </ul>
 * For full details of dump options see the section on dump agents in the documentation for the OpenJ9 JVM.
 * <p>
 * The {@link #queryDumpOptions()} method returns a String array containing a snapshot of the currently
 * configured dump options. Each String is in the format expected by the -Xdump command line
 * option and setDumpOptions. The Strings can be passed back to setDumpOptions to recreate
 * the current dump agent configuration at a later time.
 * <p>
 * The {@link #resetDumpOptions()} method resets the dump options to the settings specified when the
 * JVM was started removing any additional configuration done since then.<br>
 * If you wish to change the dump configuration at runtime and then reset it to an earlier
 * state that included additional runtime configuration done through this API or JVMTI you should
 * consider saving the result of queryDumpOptions and then later use {@link #setDumpOptions(String)}
 * to restore that configuration after a call to setDumpOptions("none") to clear all dump agent
 * configuration.
 */
public class Dump {
	private static final String SystemRequestPrefix = "z/OS".equalsIgnoreCase(System.getProperty("os.name")) ? "system:dsn=" : "system:file=";
  
	/**
	 * Trigger a java dump. A java dump is in a human-readable format, and
	 * summarizes the state of the JVM.
	 * 
	 * A security manager check will be made only if the system property
	 * com.ibm.jvm.enableLegacyDumpSecurity is set to "true" in which case
	 * a check will be made for com.ibm.jvm.DumpPermission
	 * 
	 * @throws RuntimeException if the vm does not contain RAS dump support
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 */
	public static void JavaDump() {
		checkLegacySecurityPermssion();
		JavaDumpImpl();
	}

	/**
	 * Trigger a heap dump. The default heap dump format (a phd file) is not
	 * human-readable.
	 * 
	 * A security manager check will be made only if the system property
	 * com.ibm.jvm.enableLegacyDumpSecurity is set to "true" in which case
	 * a check will be made for com.ibm.jvm.DumpPermission
	 * 
	 * @throws RuntimeException if the vm does not contain RAS dump support
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 */
	public static void HeapDump() {
		checkLegacySecurityPermssion();
		HeapDumpImpl();
	}

	/**
	 * Trigger a system dump. A system dump is a platform-specific
	 * file that contains information about the active processes, threads, and
	 * system memory. System dumps are usually large.
	 * 
	 * A security manager check will be made only if the system property
	 * com.ibm.jvm.enableLegacyDumpSecurity is set to "true" in which case
	 * a check will be made for com.ibm.jvm.DumpPermission
	 * 
	 * @throws RuntimeException if the vm does not contain RAS dump support
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 */
	public static void SystemDump() {
		checkLegacySecurityPermssion();
		SystemDumpImpl();
	}

	/*
	 * Dump should not be instantiated.
	 */
	private Dump() {	
	}
	
	private static native int JavaDumpImpl();
	private static native int HeapDumpImpl();
	private static native int SystemDumpImpl();

	/**
     * Trigger a snap dump. The snap dump format is not human-readable
     * and must be processed using the trace formatting tool supplied
     * with the OpenJ9 JVM.
	 * 
	 * A security manager check will be made only if the system property
	 * com.ibm.jvm.enableLegacyDumpSecurity is set to "true" in which case
	 * a check will be made for com.ibm.jvm.DumpPermission
	 * 
	 * @throws RuntimeException if the vm does not contain RAS dump support
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 */
    public static void SnapDump() {
    	checkLegacySecurityPermssion();
    	SnapDumpImpl();
    }
    
	private static native int SnapDumpImpl();
    
	private static final DumpPermission DUMP_PERMISSION = new DumpPermission();
	private static final ToolDumpPermission TOOL_DUMP_PERMISSION = new ToolDumpPermission();
	private static final String LEGACY_DUMP_PERMISSION_PROPERTY = "com.ibm.jvm.enableLegacyDumpSecurity"; //$NON-NLS-1$
	
    private static final class DumpOptionsLock {}
    private static final DumpOptionsLock dumpLock = new DumpOptionsLock();
	
	/**
	 * Trigger a java dump. A java dump is in a human-readable format, and
	 * summarizes the state of the JVM.
	 * 
	 * The JVM will attempt to write the file to the specified file name. This may
	 * include replacement tokens as documented in the section on dump agents
	 * in the documentation for the OpenJ9 JVM.
	 * 
	 * A string containing the actual file name written to is returned. This may not
	 * be the same as the requested filename for several reasons:
	 * <ul>
	 * <li>null or the empty string were specified, this will cause the JVM to write the
	 *  dump to the default location based on the current dump settings and return that
	 *  path.</li>
	 * <li>Replacement (%) tokens were specified in the file name. These will have been
	 *  expanded.</li>
	 * <li>The full path is returned, if only a file name with no directory was specified
	 *  the full path with the directory the dump was written to will be returned.</li>
	 * <li>The JVM couldn't write to the specified location. In this case it will attempt
	 *  to write the dump to another location, unless -Xdump:nofailover was specified on
	 *  the command line.</li>
	 * </ul>
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown.
	 * 
	 * @param fileNamePattern the file name to write to, which may be null, empty or include replacement tokens
	 * @return the file name that the dump was actually written to
	 * @throws InvalidDumpOptionException if the filename was invalid
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 */
    public static String javaDumpToFile(String fileNamePattern ) throws InvalidDumpOptionException {
    	String request = null;
    	if( "".equals(fileNamePattern) ) { //$NON-NLS-1$
    		fileNamePattern = null;
    	}
    	if( fileNamePattern != null ) {
    		// Check no-one has tried to sneak options onto the end.
    		checkForExtraOptions(fileNamePattern);
    		request = "java:file=" + fileNamePattern; //$NON-NLS-1$
    	} else {
    		// This is equivalent to the JavaDump() call.
    		request = "java"; //$NON-NLS-1$
    	}
    	String dump = null;
    	dump = triggerDump(request, "javaDumpToFile"); //$NON-NLS-1$
     	return dump;
    }

	/**
	 * Trigger a java dump. A java dump is in a human-readable format, and
	 * summarizes the state of the JVM.
	 * 
	 * The JVM will attempt to write the file to the default location.
	 * 
	 * A string containing the actual file name written to is returned.
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown.
	 * 
	 * @return the file name that the dump was actually written to
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump 
	 */
    public static String javaDumpToFile() {
    	try {
    		return triggerDump("java", "javaDumpToFile"); //$NON-NLS-1$ //$NON-NLS-2$
    	} catch (InvalidDumpOptionException e) {
    		// Cannot actually be thrown, since we aren't specifying anything other than a known dump type.
    		// Prevent the user having to add a catch block to their call.
    		return null;
    	}
    }
    
	/**
	 * Trigger a heap dump. The default heap dump format (a phd file) is not
	 * human-readable.
	 * 
	 * The JVM will attempt to write the file to the specified file name. This may
	 * include replacement tokens as documented in the section on dump agents
	 * in the documentation for the OpenJ9 JVM.
	 * 
	 * A string containing the actual file name written to is returned. This may not
	 * be the same as the requested filename for several reasons:
	 * <ul>
	 * <li>null or the empty string were specified, this will cause the JVM to write the
	 *  dump to the default location based on the current dump settings and return that
	 *  path.</li>
	 * <li>Replacement (%) tokens were specified in the file name. These will have been
	 *  expanded.</li>
	 * <li>The full path is returned, if only a file name with no directory was specified
	 *  the full path with the directory the dump was written to will be returned.</li>
	 * <li>The JVM couldn't write to the specified location. In this case it will attempt
	 *  to write the dump to another location, unless -Xdump:nofailover was specified on
	 *  the command line.</li>
	 * </ul>
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown.
	 * 
	 * @param fileNamePattern the file name to write to, which may be null, empty or include replacement tokens
	 * @return the file name that the dump was actually written to
	 * @throws InvalidDumpOptionException if the filename was invalid
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 */
    public static String heapDumpToFile(String fileNamePattern ) throws InvalidDumpOptionException {
     	String request = null;
    	if( "".equals(fileNamePattern) ) { //$NON-NLS-1$
    		fileNamePattern = null;
    	}
    	if( fileNamePattern != null ) {
    		// Check no-one has tried to sneak options onto the end.
    		checkForExtraOptions(fileNamePattern);
    		request = "heap:file=" + fileNamePattern + ","; //$NON-NLS-1$ //$NON-NLS-2$
    	} else {
    		// This is equivalent to the HeapDump() call.
    		request = "heap:"; //$NON-NLS-1$
    	}
   		request += "opts=PHD"; //$NON-NLS-1$
   		String dump = null;
   		dump = triggerDump(request, "heapDumpToFile"); //$NON-NLS-1$
    	return dump;
    }
    
	/**
	 * Trigger a heap dump. The default heap dump format (a phd file) is not
	 * human-readable.
	 * 
	 * The JVM will attempt to write the file to the default location.
	 * 
	 * A string containing the actual file name written to is returned.
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown.
	 * 
	 * @return the file name that the dump was actually written to 
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 */
    public static String heapDumpToFile() {
    	try {
    		return triggerDump("heap:opts=PHD", "heapDumpToFile"); //$NON-NLS-1$ //$NON-NLS-2$
    	} catch (InvalidDumpOptionException e) {
    		// Cannot actually be thrown, since we aren't specifying anything other than a known dump type.
    		// Prevent the user having to add a catch block to their call.
    		return null;
    	}
    }
    
	/**
	 * Trigger a system dump. A system dump is a platform-specific
	 * file that contains information about the active processes, threads, and
	 * system memory. System dumps are usually large.
	 * 
	 * The JVM will attempt to write the file to the specified file name. This may
	 * include replacement tokens as documented in the section on dump agents
	 * in the documentation for the OpenJ9 JVM.
	 * 
	 * A string containing the actual file name written to is returned. This may not
	 * be the same as the requested filename for several reasons:
	 * <ul>
	 * <li>null or the empty string were specified, this will cause the JVM to write the
	 *  dump to the default location based on the current dump settings and return that
	 *  path.</li>
	 * <li>Replacement (%) tokens were specified in the file name. These will have been
	 *  expanded.</li>
	 * <li>The full path is returned, if only a file name with no directory was specified
	 *  the full path with the directory the dump was written to will be returned.</li>
	 * <li>The JVM couldn't write to the specified location. In this case it will attempt
	 *  to write the dump to another location, unless -Xdump:nofailover was specified on
	 *  the command line.</li>
	 * </ul>
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown.
	 * 
	 * @param fileNamePattern the file name to write to, which may be null, empty or include replacement tokens
	 * @return the file name that the dump was actually written to
	 * @throws InvalidDumpOptionException if the filename was invalid
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 */
    public static String systemDumpToFile(String fileNamePattern) throws InvalidDumpOptionException {
    	String request = null;
    	if( "".equals(fileNamePattern) ) { //$NON-NLS-1$
    		fileNamePattern = null;
    	}
    	if( fileNamePattern != null ) {
    		// Check no-one has tried to sneak options onto the end.
    		checkForExtraOptions(fileNamePattern);
    		request = SystemRequestPrefix + fileNamePattern; //$NON-NLS-1$
    	} else {
    		// This is equivalent the to SystemDump() call.
    		request = "system"; //$NON-NLS-1$
    	}
    	String dump = null;
       	dump = triggerDump(request, "systemDumpToFile"); //$NON-NLS-1$
       	return dump;
    }
    
	/**
	 * Trigger a system dump. A system dump is a platform-specific
	 * file that contains information about the active processes, threads, and
	 * system memory. System dumps are usually large.
	 * 
	 * The JVM will attempt to write the file to the default location.
	 * 
	 * A string containing the actual file name written to is returned.
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown.
	 * 
	 * @return the file name that the dump was actually written to
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 */
    public static String systemDumpToFile() {
     	try {
    		return triggerDump("system", "systemDumpToFile"); //$NON-NLS-1$ //$NON-NLS-2$
    	} catch (InvalidDumpOptionException e) {
    		// Cannot actually be thrown, since we aren't specifying anything other than a known dump type.
    		// Prevent the user having to add a catch block to their call.
    		return null;
    	}
    }
    
    /**
     * Trigger a snap dump. The snap dump format is not human-readable
     * and must be processed using the trace formatting tool supplied
     * with the OpenJ9 JVM.
	 * 
	 * The JVM will attempt to write the file to the specified file name. This may
	 * include replacement tokens as documented in the section on dump agents
	 * in the documentation for the OpenJ9 JVM.
	 * 
	 * A string containing the actual file name written to is returned. This may not
	 * be the same as the requested filename for several reasons:
	 * <ul>
	 * <li>null or the empty string were specified, this will cause the JVM to write the
	 *  dump to the default location based on the current dump settings and return that
	 *  path.</li>
	 * <li>Replacement (%) tokens were specified in the file name. These will have been
	 *  expanded.</li>
	 * <li>The full path is returned, if only a file name with no directory was specified
	 *  the full path with the directory the dump was written to will be returned.</li>
	 * <li>The JVM couldn't write to the specified location. In this case it will attempt
	 *  to write the dump to another location, unless -Xdump:nofailover was specified on
	 *  the command line.</li>
	 * </ul>
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown.
	 * 
	 * @param fileNamePattern the file name to write to, which may be null, empty or include replacement tokens
	 * @return the file name that the dump was actually written to
     * @throws InvalidDumpOptionException if the filename was invalid
     * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 */
    public static String snapDumpToFile(String fileNamePattern) throws InvalidDumpOptionException {
    	String request = null;
    	if( "".equals(fileNamePattern) ) { //$NON-NLS-1$
    		fileNamePattern = null;
    	}
    	if( fileNamePattern != null ) {
    		// Check no-one has tried to sneak options onto the end.
    		checkForExtraOptions(fileNamePattern);
    		request = "snap:file=" + fileNamePattern; //$NON-NLS-1$
    	} else {
    		// This is equivalent to the SnapDump() call.
    		request = "snap"; //$NON-NLS-1$
    	}
    	String dump = null;
    	dump = triggerDump(request, "snapDumpToFile"); //$NON-NLS-1$
    	return dump;
    }
    
	/**
     * Trigger a snap dump. The snap dump format is not human-readable
     * and must be processed using the trace formatting tool supplied
     * with the OpenJ9 JVM.
	 * 
	 * The JVM will attempt to write the file to the default location.
	 * 
	 * A string containing the actual file name written to is returned.
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown.
	 * 
	 * @return the file name that the dump was actually written to
	 * 
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 */
    public static String snapDumpToFile() {
    	try {
    		return triggerDump("snap", "snapDumpToFile"); //$NON-NLS-1$ //$NON-NLS-2$
    	} catch (InvalidDumpOptionException e) {
    		// Cannot actually be thrown, since we aren't specifying anything other than a known dump type.
    		// Prevent the user having to add a catch block to their call.
    		return null;
    	}
    }

    private static void checkForExtraOptions(String fileNamePattern) throws InvalidDumpOptionException {
		// Check no-one has tried to sneak options onto the end of a filename.
    	if( fileNamePattern.contains(",")) { //$NON-NLS-1$
    		throw new InvalidDumpOptionException("Invalid dump filename specified."); //$NON-NLS-1$
    	}
	}
    
    private static void checkDumpSecurityPermssion() throws SecurityException {
		/* Check the caller has DumpPermission. */
		SecurityManager manager = System.getSecurityManager();
		if( manager != null ) {
			manager.checkPermission(DUMP_PERMISSION);
		}
    }
    
    private static void checkToolSecurityPermssion() throws SecurityException {
		/* Check the caller has DumpPermission. */
		SecurityManager manager = System.getSecurityManager();
		if( manager != null ) {
			manager.checkPermission(TOOL_DUMP_PERMISSION);
		}
    }
    
    private static void checkLegacySecurityPermssion() throws SecurityException {
    	if (!("false".equalsIgnoreCase(com.ibm.oti.vm.VM.getVMLangAccess()	//$NON-NLS-1$
    		.internalGetProperties().getProperty(LEGACY_DUMP_PERMISSION_PROPERTY)))) {
    		checkDumpSecurityPermssion();
    	}
    }
   
    /**
	 * Trigger a dump with the specified options.
	 * This method will trigger a dump of the specified type,
	 * with the specified options, immediately. The dump type and
	 * options are specified using the same string parameters
	 * as the -Xdump flag as described in the section on dump agents
	 * in the documentation for the OpenJ9 JVM.
	 * 
	 * Settings that do not apply to dumps that occur immediately
	 * ("range=", "priority=", "filter=", "events=", "none" and "defaults")
	 * will be ignored.
	 * 
	 * The "opts=" setting is supported if an option is used that causes two
	 * dumps to occur only the filename for the first will be returned.
	 * 
	 * If a filename is specified for the dump it may contain replacement strings
	 * as specified in the documentation. In addition if a dump cannot be created
	 * with the specified filename the JVM may attempt to write it to another location.
	 * For these reasons you should always use the file name that is returned from this function 
	 * when looking for the dump rather than the name you supplied.
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown. If a "tool" dump is requested an
	 * additional check for com.ibm.jvm.ToolDumpPermission will also be made.
	 * 
	 * @param dumpOptions a dump settings string
	 * 
	 * @return The file name of the dump that was created. The String "-" means the dump was written to stderr. 
	 * 
	 * @throws RuntimeException if the vm does not contain RAS dump support
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 * @throws InvalidDumpOptionException If the dump options are invalid or the dump operation fails
	 * @throws NullPointerException if dumpSettings is null
	 */
	public static String triggerDump(String dumpOptions) throws InvalidDumpOptionException {
		if( dumpOptions == null ) {
			throw new NullPointerException();
		}
		// All the other permissions will be checked in triggerDump(dumpSettings, event);
		if( isToolDump(dumpOptions) ) {
			checkToolSecurityPermssion();
		}
		return triggerDump(dumpOptions, "triggerDump"); //$NON-NLS-1$
	}

	private static String triggerDump(String dumpSettings, String event) throws InvalidDumpOptionException {

		/* Check the caller is allowed to trigger a dump. */
		checkDumpSecurityPermssion();
	
		return triggerDumpsImpl(dumpSettings, event);
	}

	/**
     * Sets options for the dump subsystem.
     * The dump option is passed in as an String.
     * Use the same syntax as the -Xdump command-line option, with the initial -Xdump: omitted.
     * See Using the -Xdump option as described in the section on dump agents
	 * in the documentation for the OpenJ9 JVM.
	 * 
	 * This method may throw a DumpConfigurationUnavailableException if the dump configuration
	 * cannot be altered. If this occurs it will usually be because a dump event is currently 
	 * being handled. As this can take some time depending on the dumps being generated an
	 * exception is thrown rather than this call blocking the calling thread potentially for
	 * minutes. 
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown. If a "tool" dump is specified an
	 * additional check for com.ibm.jvm.ToolDumpPermission will also be made.
	 * 
     * @param dumpOptions the options string to set 
     * @throws InvalidDumpOptionException if the specified option cannot be set or is incorrect
     * @throws DumpConfigurationUnavailableException If the dump configuration cannot be changed because a dump is currently in progress
     * @throws SecurityException if there is a security manager and it doesn't allow the checks required to change the dump settings
     * @throws NullPointerException if options is null
     */
    public static void setDumpOptions(String dumpOptions) throws InvalidDumpOptionException, DumpConfigurationUnavailableException {	

		/* Check the caller is allowed to trigger a dump. */
    	checkDumpSecurityPermssion();
    	
		if( isToolDump(dumpOptions) ) {
			checkToolSecurityPermssion();
		}
		
		if( dumpOptions == null ) {
			throw new NullPointerException();
		}
		
		/* Synchronised to prevent two Java threads trying to set/reset dump settings at once.
		 * resetDumpOptionsImpl is also synchronised in this way.
		 * A DumpConfigurationUnavailableException can still be thrown if a dump was in 
		 * progress and the dump configuration could not be updated.
		 */
		synchronized (dumpLock) {
			setDumpOptionsImpl(dumpOptions);
		}
    }
    
    /**
     * Returns the current dump configuration as an array of Strings.
     * The syntax of the option Strings is the same as the -Xdump command-line option,
     * with the initial -Xdump: omitted. See Using the -Xdump option
     * in the section on dump agents in the documentation for the OpenJ9 JVM.
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown.
	 * 
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to read the dump settings
     * @return the options strings
     */
    public static String[] queryDumpOptions() {
		/* Check the caller is allowed to query dump settings. */
    	checkDumpSecurityPermssion();
		
    	String options = queryDumpOptionsImpl();
    	if( options != null ) {
    		if( "".equals(options) ) { //$NON-NLS-1$
    			String[] empty = {};
    			return empty;
    		} else {
    			return options.split("\n"); //$NON-NLS-1$
    		}
    	}
    	return null;
    }
    
    /**
     * Reset the JVM dump options to the settings specified when the JVM
     * was started removing any additional configuration done since then.
     * 
     * This method may throw a DumpConfigurationUnavailableException if the dump configuration
	 * cannot be altered. If this occurs it will usually be because a dump event is currently 
	 * being handled. As this can take some time depending on the dumps being generated an
	 * exception is thrown rather than this call blocking the calling thread potentially for
	 * minutes. 
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown.
	 * 
     * @throws com.ibm.jvm.DumpConfigurationUnavailableException if the dump configuration cannot be changed because a dump is currently in progress
     * @throws SecurityException if there is a security manager and it doesn't allow the checks required to change the dump settings
     */
    public static void resetDumpOptions() throws DumpConfigurationUnavailableException {
    	
		/* Check the caller is allowed to reset dump settings. */
    	checkDumpSecurityPermssion();
		
    	/* Synchronised to prevent two Java threads trying to update dump settings at once.
    	 * setDumpOptions is also synchronised in this way.
    	 * A DumpConfigurationUnavailableException can still be thrown if a dump was in 
    	 * progress and the dump configuration could not be updated. */
    	synchronized (dumpLock) {
    		resetDumpOptionsImpl();
    	}
    }

	private static native void setDumpOptionsImpl(String options) throws InvalidDumpOptionException, DumpConfigurationUnavailableException;
	private static native String queryDumpOptionsImpl();
	private static native void resetDumpOptionsImpl() throws DumpConfigurationUnavailableException;
	private static native String triggerDumpsImpl(String dumpOptions, String event) throws InvalidDumpOptionException;
	private static native boolean isToolDump(String dumpOptions);
}
