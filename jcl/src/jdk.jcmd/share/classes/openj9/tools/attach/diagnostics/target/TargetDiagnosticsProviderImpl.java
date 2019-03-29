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

package openj9.tools.attach.diagnostics.target;

import static openj9.tools.attach.diagnostics.base.DiagnosticUtils.COMMAND_STRING;
import static openj9.tools.attach.diagnostics.base.DiagnosticUtils.DIAGNOSTICS_GC_CLASS_HISTOGRAM;
import static openj9.tools.attach.diagnostics.base.DiagnosticUtils.DIAGNOSTICS_GC_CONFIG;
import static openj9.tools.attach.diagnostics.base.DiagnosticUtils.DIAGNOSTICS_OPTION_SEPARATOR;
import static openj9.tools.attach.diagnostics.base.DiagnosticUtils.DIAGNOSTICS_THREAD_PRINT;
import static openj9.tools.attach.diagnostics.base.DiagnosticUtils.LIVE_OPTION;

import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.function.Function;
import java.util.function.Predicate;

import openj9.tools.attach.diagnostics.base.DiagnosticProperties;
import openj9.tools.attach.diagnostics.info.HeapClassInfo;
import openj9.tools.attach.diagnostics.info.HeapConfigInfo;
import openj9.tools.attach.diagnostics.info.ThreadGroupInfo;
import openj9.tools.attach.diagnostics.spi.TargetDiagnosticsProvider;

/**
 * This class allows a Attach API attacher to query a target JVM about
 * diagnostic information such as JIT compilation, threads, classes, etc.
 * Instances must be created using the getDiagnostics() factory method.
 *
 */
public class TargetDiagnosticsProviderImpl implements TargetDiagnosticsProvider {

	static final Map<String, Function<String, DiagnosticProperties>> commandTable;
	static {
		commandTable = new HashMap<>();
		commandTable.put(DIAGNOSTICS_GC_CLASS_HISTOGRAM, s -> getHeapStatistics(s));
		commandTable.put(DIAGNOSTICS_GC_CONFIG, s -> getHeapConfig(s));
		commandTable.put(DIAGNOSTICS_THREAD_PRINT, s -> getThreadGroupInfo(s));
		commandTable.put(DIAGNOSTICS_THREAD_PRINT, s -> runGC(s));
	}

	@Override
	public DiagnosticProperties getThreadGroupInfo() throws IOException {
		return getThreadGroupInfo(null);
	}

	private static DiagnosticProperties runGC(@SuppressWarnings("unused") String unused) {
		System.gc();
		return makeCommandSucceeded();
	}

	private static DiagnosticProperties getThreadGroupInfo(@SuppressWarnings("unused") String ignored) {
		DiagnosticProperties threadInfoProperties = ThreadGroupInfo.getThreadInfoProperties();
		DiagnosticProperties.dumpPropertiesIfDebug("Thread group properties", threadInfoProperties); //$NON-NLS-1$
		return threadInfoProperties;
	}

	/**
	 * Get information about objects on the heap, such as number and size of objects
	 * for each class.
	 * 
	 * @param diagnosticCommand text of the command, with arguments if any
	 * @return statistics encoded as properties
	 */
	private static DiagnosticProperties getHeapStatistics(String diagnosticCommand) {
		boolean doLive = Arrays.stream(diagnosticCommand
				.split(DIAGNOSTICS_OPTION_SEPARATOR))
				.anyMatch(Predicate.isEqual(LIVE_OPTION));
		if (doLive) {
			runGC(diagnosticCommand);
		}
		DiagnosticProperties heapStatsProperties = HeapClassInfo.getHeapClassStatistics();
		DiagnosticProperties.dumpPropertiesIfDebug("Heap statistics properties", heapStatsProperties); //$NON-NLS-1$
		return heapStatsProperties;
	}

	/**
	 * Get information about objects on the heap, such as number and size of objects
	 * for each class.
	 * 
	 * @param diagnosticCommand text of the command, with arguments if any
	 * @return statistics encoded as properties
	 */
	private static DiagnosticProperties getHeapConfig(String diagnosticCommand) {
		DiagnosticProperties heapConfigProperties = HeapConfigInfo.getHeapConfigInfo();
		DiagnosticProperties.dumpPropertiesIfDebug("Heap config properties", heapConfigProperties); //$NON-NLS-1$
		return heapConfigProperties;
	}

	@Override
	public DiagnosticProperties executeDiagnosticCommand(String diagnosticCommand) {
		if (DiagnosticProperties.isDebug) {
			System.err.println("executeDiagnosticCommand: "+diagnosticCommand); //$NON-NLS-1$
		}

		DiagnosticProperties result;
		String[] commandRoot = diagnosticCommand.split(DIAGNOSTICS_OPTION_SEPARATOR);
		Function<String, DiagnosticProperties> cmd = commandTable.get(commandRoot[0]);
		if (null == cmd) {
			result = DiagnosticProperties.makeStatusProperties(true,
					"Command " + diagnosticCommand + "not recognized"); //$NON-NLS-1$ //$NON-NLS-2$
		} else {
			result = cmd.apply(diagnosticCommand);
			result.put(COMMAND_STRING, diagnosticCommand);
		}
		return result;
	}

	private static DiagnosticProperties makeCommandSucceeded() {
		return DiagnosticProperties.makeStatusProperties(false, "Command succeeded"); //$NON-NLS-1$
	}


}
