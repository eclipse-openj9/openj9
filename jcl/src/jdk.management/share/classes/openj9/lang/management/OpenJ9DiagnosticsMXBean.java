/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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


package openj9.lang.management;

import java.lang.management.PlatformManagedObject;

/**
 * <p>
 * This interface provides APIs to dynamically trigger dump agents. APIs are also available to
 * configure dump options.
 * This MXBean reuses the methods in com.ibm.jvm.Dump API 
 * <br>
 * <table border="1">
 * <caption><b>Usage example for the {@link OpenJ9DiagnosticsMXBean}</b></caption>
 * <tr> <td>
 * <pre>
 * {@code
 *   ...
 *   try {
 *      mxbeanName = new ObjectName("openj9.lang.management:type=OpenJ9Diagnostics");
 *   } catch (MalformedObjectNameException e) {
 *      // Exception Handling
 *   }
 *   try {
 *      MBeanServer mbeanServer = ManagementFactory.getPlatformMBeanServer();
 *      if (false == mbeanServer.isRegistered(mxbeanName)) {
 *         // OpenJ9DiagnosticsMXBean not registered
 *      }
 *      OpenJ9DiagnosticsMXBean diagBean = JMX.newMXBeanProxy(mbeanServer, mxbeanName, OpenJ9DiagnosticsMXBean.class);
 *   } catch (Exception e) {
 *      // Exception Handling
 *   }
 * }
 * </pre></td></tr>
 * </table>
 */
public interface OpenJ9DiagnosticsMXBean extends PlatformManagedObject {
	/**
	 * Reset the JVM dump options to the settings specified when the JVM was started removing any additional
	 * configuration done since then. This method may throw a ConfigurationUnavailableException if the dump 
	 * configuration cannot be altered. If this occurs it will usually be because a dump event is currently being 
	 * handled.
	 *
	 * @throws ConfigurationUnavailableException if the configuration cannot be changed because a dump is already in progress
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to change the dump settings
	 */
	public void resetDumpOptions() throws ConfigurationUnavailableException;

	/**
	 * This function sets options for the dump subsystem.
	 * The dump option is passed in as a String. Use the same syntax as the -Xdump command-line option, with the 
	 * initial -Xdump: omitted. See Using the -Xdump option as described in the section on dump agents in the 
	 * documentation for the OpenJ9 JVM. This method may throw a ConfigurationUnavailableException if the dump
	 * configuration cannot be altered.
	 *
	 * @param dumpOptions the options string to be set
	 * @throws InvalidOptionException if the specified dumpOptions cannot be set or is incorrect
	 * @throws ConfigurationUnavailableException if the configuration cannot be changed because a dump is already in progress
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to change the dump settings
	 * @throws NullPointerException if dumpOptions is null
	 */
	public void setDumpOptions(String dumpOptions) throws InvalidOptionException, ConfigurationUnavailableException;

	/**
	 * This function triggers the specified dump agent. Dump agents supported - java, snap, system and heap.
	 * A java dump is in a human-readable format, and summarizes the state of the JVM.
	 * The default heap dump format (a phd file) is not human-readable.
	 * A system dump is a platform-specific file that contains information about the active processes, threads, and
	 * system memory. System dumps are usually large. 
	 * The snap dump format is not human-readable and must be processed using the trace formatting tool supplied  with the OpenJ9 JVM.
	 *
	 * @param dumpAgent the dump agent to be triggered
	 * @throws IllegalArgumentException if the specified dump agent is invalid or unsupported by this method
	 * @throws RuntimeException if the vm does not contain RAS dump support
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 * @throws NullPointerException if dumpAgent is null
	 */
	public void triggerDump(String dumpAgent) throws IllegalArgumentException;

	/**
	 * This function triggers the specified dump agent. Dump agents supported - java, snap, system and heap.
	 * The JVM will attempt to write the file to the specified file name. This may
	 * include replacement tokens as documented in the section on dump agents
	 * in the documentation for the OpenJ9 JVM.
	 * 
	 * A string containing the actual filename written to is returned. This may not
	 * be the same as the requested filename for several reasons:
	 * <ul>
	 * <li>null or the empty string was specified, this will cause the JVM to write the
	 *  dump to the default location based on the current dump settings and return that
	 *  path.</li>
	 * <li>Replacement (%) tokens were specified in the file name. These will have been
	 *  expanded.</li>
	 * <li>The full path is returned, if only a filename with no directory was specified
	 *  the full path with the directory the dump was written to will be returned.</li>
	 * <li>The JVM couldn't write to the specified location. In this case it will attempt
	 *  to write the dump to another location, unless -Xdump:nofailover was specified on
	 *  the command line.</li>
	 * </ul>
	 * 
	 * If a security manager exists a permission check for com.ibm.jvm.DumpPermission will be
	 * made, if this fails a SecurityException will be thrown.
	 *
	 * @return the file name that the dump was actually written to 
	 * @param dumpAgent the dump agent to be triggered
	 * @param fileNamePattern the filename to write to, which may be null, empty or include replacement tokens
	 * @throws InvalidOptionException if the fileNamePattern was invalid
	 * @throws IllegalArgumentException if the specified dump agent is invalid or unsupported by this method
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 * @throws NullPointerException if dumpAgent is null
	 */
	public String triggerDumpToFile(String dumpAgent, String fileNamePattern) throws IllegalArgumentException, InvalidOptionException;

	/**
	 * This function triggers the heap dump agent and requests for a heap dump in CLASSIC format.
	 *
	 * @return The file name of the dump that was created
	 * @throws InvalidOptionException if the dump operation fails
	 * @throws RuntimeException if the JVM does not contain RAS dump support
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to trigger this dump
	 */
	public String triggerClassicHeapDump() throws InvalidOptionException;
}
