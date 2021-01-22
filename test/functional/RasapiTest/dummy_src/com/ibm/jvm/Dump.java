/*******************************************************************************
 * Copyright (c) 2006, 2021 IBM Corp. and others
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
package com.ibm.jvm;

import java.security.AccessController;
import java.security.PrivilegedAction;

/**
 * This class is used to trigger and configure the options used to produce different
 * types of diagnostic dumps available from the IBM JVM.
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
 * if your code is likely to run on both IBM and non-IBM JVMs and you only need the most
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
 * For full details of dump options see the section on dump agents in the documentation for the IBM JVM.
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

	public static void JavaDump() {
	}

	public static void HeapDump() {
	}

	public static void SystemDump() {
	}

	private Dump() {
	}

    public static void SnapDump() {
    }

    public static String javaDumpToFile(String fileNamePattern ) throws InvalidDumpOptionException {
    	return null;
    }

    public static String javaDumpToFile() {
    	return null;
    }

    public static String heapDumpToFile(String fileNamePattern ) throws InvalidDumpOptionException {
    	return null;
    }

    public static String heapDumpToFile() {
    	return null;
    }


    public static String systemDumpToFile(String fileNamePattern) throws InvalidDumpOptionException {
    	return null;    }

    public static String systemDumpToFile() {
    	return null;
    }
    public static String snapDumpToFile(String fileNamePattern) throws InvalidDumpOptionException {
    	return null;
    }
    public static String snapDumpToFile() {
    	return null;
    }

	public static String triggerDump(String dumpOptions) throws InvalidDumpOptionException {
		return null;
	}

    public static void setDumpOptions(String dumpOptions) throws InvalidDumpOptionException, DumpConfigurationUnavailableException {
    }

    public static String[] queryDumpOptions() {
    	return null;
    }

    public static void resetDumpOptions() throws DumpConfigurationUnavailableException {
    }

}
