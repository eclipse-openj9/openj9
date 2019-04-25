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

import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.function.Function;
import java.util.function.Predicate;

import openj9.tools.attach.diagnostics.base.DiagnosticProperties;
import openj9.tools.attach.diagnostics.base.DiagnosticUtils;
import openj9.tools.attach.diagnostics.info.HeapConfigInfo;
import openj9.tools.attach.diagnostics.info.JvmInfo;
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
		commandTable.put(openj9.tools.attach.diagnostics.base.DiagnosticUtils.DIAGNOSTICS_GC_CLASS_HISTOGRAM, s -> getHeapStatistics(s));
		commandTable.put(openj9.tools.attach.diagnostics.base.DiagnosticUtils.DIAGNOSTICS_GC_RUN, s -> runGC());
		commandTable.put(openj9.tools.attach.diagnostics.base.DiagnosticUtils.DIAGNOSTICS_GC_CONFIG, s -> getHeapConfig());
		commandTable.put(openj9.tools.attach.diagnostics.base.DiagnosticUtils.DIAGNOSTICS_GC_FINALIZABLE, s -> getFinalizable());
	}

	@Override
	public DiagnosticProperties getThreadGroupInfo() throws IOException {
		DiagnosticProperties threadInfoProperties = ThreadGroupInfo.getThreadInfoProperties();
		DiagnosticProperties.dumpPropertiesIfDebug("Thread group properties", threadInfoProperties); //$NON-NLS-1$
		return threadInfoProperties;
	}

	@Override
	public DiagnosticProperties executeDiagnosticCommand(String diagnosticCommand) {
		if (DiagnosticProperties.isDebug) {
			System.err.println("executeDiagnosticCommand: "+diagnosticCommand); //$NON-NLS-1$
		}

		DiagnosticProperties result;
		String[] commandRoot = diagnosticCommand.split(DiagnosticUtils.DIAGNOSTICS_OPTION_SEPARATOR);
		Function<String, DiagnosticProperties> cmd = commandTable.get(commandRoot[0]);
		if (null == cmd) {
			result = DiagnosticProperties.makeStatusProperties(true,
					"Command " + diagnosticCommand + "not recognized"); //$NON-NLS-1$ //$NON-NLS-2$
		} else {
			result = cmd.apply(diagnosticCommand);
			result.put(DiagnosticUtils.COMMAND_STRING, diagnosticCommand);
		}
		return result;
	}

	private static DiagnosticProperties getHeapStatistics(String diagnosticCommand) {
		boolean doLive = Arrays.stream(diagnosticCommand
				.split(DiagnosticUtils.DIAGNOSTICS_OPTION_SEPARATOR))
				.anyMatch(Predicate.isEqual(DiagnosticUtils.LIVE_OPTION));
		if (doLive) {
			runGC();
		}
		String result = JvmInfo.getMemoryMXBean().getHeapClassStatistics();
		return DiagnosticProperties.makeStringResult(result);
	}

	private static DiagnosticProperties getHeapConfig() {
		String result = HeapConfigInfo.getHeapConfigString();
		return DiagnosticProperties.makeStringResult(result);
	}

	private static DiagnosticProperties runGC() {
		System.gc();
		return DiagnosticProperties.makeCommandSucceeded();
	}

	private static DiagnosticProperties getFinalizable() {
		int finalCount = JvmInfo.getMemoryMXBean().getObjectPendingFinalizationCount();
		return DiagnosticProperties.makeStringResult(String.format("Approximate number of finalizable objects: %d%n", //$NON-NLS-1$
				Integer.valueOf(finalCount)));
	}

}
