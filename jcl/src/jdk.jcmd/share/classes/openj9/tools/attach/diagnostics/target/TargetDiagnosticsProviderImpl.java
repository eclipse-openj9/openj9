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

import openj9.tools.attach.diagnostics.base.DiagnosticProperties;
import openj9.tools.attach.diagnostics.info.ThreadGroupInfo;
import openj9.tools.attach.diagnostics.spi.TargetDiagnosticsProvider;

/**
 * This class allows a Attach API attacher to query a target JVM about
 * diagnostic information such as JIT compilation, threads, classes, etc.
 * Instances must be created using the getDiagnostics() factory method.
 *
 */
public class TargetDiagnosticsProviderImpl implements TargetDiagnosticsProvider {

	@Override
	public DiagnosticProperties getThreadGroupInfo() throws IOException {
		DiagnosticProperties threadInfoProperties = ThreadGroupInfo.getThreadInfoProperties();
		DiagnosticProperties.dumpPropertiesIfDebug("Thread group properties", threadInfoProperties); //$NON-NLS-1$
		return threadInfoProperties;
	}

}
