/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package openj9.tools.attach.diagnostics.base;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Collections;
import java.util.List;
import java.util.stream.Collectors;

import com.ibm.tools.attach.target.IPC;

/**
 * Common methods for the diagnostics tools
 *
 */
public class DiagnosticUtils {

	/**
	 * Command strings for executeDiagnosticCommand()
	 */
	/**
	 * Get the thread stack traces
	 */
	public static final String DIAGNOSTICS_THREAD_PRINT = "Thread.print"; //$NON-NLS-1$
	/**
	 * Run System.gc();
	 */
	public static final String DIAGNOSTICS_GC_RUN = "GC.run"; //$NON-NLS-1$
	/**
	 * Get the heap object statistics.
	 */
	public static final String DIAGNOSTICS_GC_CLASS_HISTOGRAM = "GC.class_histogram"; //$NON-NLS-1$

	/**
	 * Get the heap configuration.
	 */
	public static final String DIAGNOSTICS_GC_CONFIG = "GC.config"; //$NON-NLS-1$

	/**
	 * Properties names for executeDiagnosticCommand() results
	 */
	/**
	 * Number of items in a list
	 */
	public static final String NUM_COMMANDS = "num_items"; //$NON-NLS-1$
	public static final String COMMAND_PREFIX = "command."; //$NON-NLS-1$

	/**
	 * Key for the command sent to executeDiagnosticCommand()
	 */
	public static final String COMMAND_STRING = "command_string"; //$NON-NLS-1$
	
	/**
	 * Use this to separate arguments in a diagnostic command string.
	 */
	public static final String DIAGNOSTICS_OPTION_SEPARATOR = ","; //$NON-NLS-1$
	/**
	 * Include ownable synchronizers in thread info.
	 */
	public static final String LOCKS_OPTION = "locks"; //$NON-NLS-1$
	/**
	 * Report only live heap objects.
	 */
	public static final String LIVE_OPTION = "live"; //$NON-NLS-1$

	/**
	 * Append an integer to a prefix to create a key for a list item
	 * @param n item number
	 * @param prefix base prefix
	 * @return key string
	 */
	public static String itemNKey(String prefix, int n) {
		return prefix + Integer.toString(n);
	}

	/**
	 * Encode a list of Strings to a properties object, maintaining order
	 * 
	 * @param stringList list of strings
	 * @param keyPrefix  base string for the keys
	 * @return properties object
	 */
	public static DiagnosticProperties stringListToProperties(List<String> stringList, String keyPrefix) {
		DiagnosticProperties props = new DiagnosticProperties();
		props.put(IPC.PROPERTY_DIAGNOSTICS_ERROR, false);
		props.put(NUM_COMMANDS, stringList.size());
		int n = 0;
		for (String cmd : stringList) {
			String prefix = itemNKey(keyPrefix, n);
			props.put(prefix, cmd);
		}
		DiagnosticProperties.dumpPropertiesIfDebug("Commands:", props.toProperties()); //$NON-NLS-1$
		return props;
	}

	/**
	 * @return contents of standard in as a list of strings, one per line.
	 */
	public static List<String> stdinToStringList() {
		List<String> stdinList;
		try (BufferedReader jpsOutReader = new BufferedReader(new InputStreamReader(System.in))) {
			stdinList = jpsOutReader.lines()
					.map(s -> s.trim())
					.filter(s -> !s.isEmpty())
					.collect(Collectors.toList());
		} catch (IOException e) {
			stdinList = Collections.emptyList();
		}
		return stdinList;
	}

	public static String makeHeapHistoCommand(boolean liveObjects) {
		String cmd = DIAGNOSTICS_GC_CLASS_HISTOGRAM;
		if (liveObjects) {
			cmd += DIAGNOSTICS_OPTION_SEPARATOR + LIVE_OPTION;
		}
		return cmd;
	}

	public static final String EMPTY_STRING = "";

}
