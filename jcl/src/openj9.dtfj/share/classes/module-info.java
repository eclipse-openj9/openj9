/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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

/*[REM] This file must not use tabs because the dependency recognition code in openjdk does not support them. */

/**
 * Diagnostic Tool Framework for Java&trade; (DTFJ)
 *
 * The Diagnostic Tool Framework for Java&trade; (DTFJ) is a Java application
 * programming interface (API) used to support the building of Java diagnostic
 * tools. DTFJ works with data from a system dump or a Javadump.
 */
module openj9.dtfj {
  requires transitive java.desktop;
  requires transitive java.logging;
  requires java.xml;
  requires openj9.traceformat;
  /*[IF PLATFORM-mz31 | PLATFORM-mz64]*/
  requires com.ibm.jzos;
  /*[ENDIF]*/
  exports com.ibm.dtfj.image;
  exports com.ibm.dtfj.image.j9 to openj9.dtfjview;
  exports com.ibm.dtfj.java;
  exports com.ibm.dtfj.runtime;
  exports com.ibm.dtfj.utils.file to openj9.dtfjview;
  exports com.ibm.java.diagnostics.utils to openj9.dtfjview;
  exports com.ibm.java.diagnostics.utils.commands to openj9.dtfjview;
  exports com.ibm.java.diagnostics.utils.plugins to openj9.dtfjview;
}
