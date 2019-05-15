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

/**
 * Container for information about a JVM's threads
 *
 */
public interface DiagnosticsInfo {
	String OPENJ9_DIAGNOSTICS_PREFIX = "openj9_diagnostics."; //$NON-NLS-1$
	String JAVA_INFO = OPENJ9_DIAGNOSTICS_PREFIX + "java_info"; //$NON-NLS-1$
	/**
	 * Use for commands which return a single string
	 */
	String DIAGNOSTICS_STRING_RESULT = OPENJ9_DIAGNOSTICS_PREFIX + "string_result"; //$NON-NLS-1$

	@Override
	String toString();

	/**
	 * Print the information about the remote Java VM.
	 * 
	 * @return contents of system property "java.vm.info"
	 */
	String getJavaInfo();

}
