/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.logging;

//central repository for the logger names used in J9DDR

public interface LoggerNames {
	public static final String LOGGER_WALKERS = "j9ddr.walkers";
	public static final String LOGGER_STRUCTURE_READER = "j9ddr.structure_reader";
	public static final String LOGGER_STACKWALKER = "j9ddr.stackwalker";
	public static final String LOGGER_STACKWALKER_LOCALMAP = "j9ddr.stackwalker.localmap";
	public static final String LOGGER_STACKWALKER_STACKMAP = "j9ddr.stackwalker.stackmap";
	public static final String LOGGER_WALKERS_POOL = "j9ddr.walkers.pool";
	public static final String LOGGER_VIEW_DTFJ = "j9ddr.view.dtfj";		//name of the DTFJ logging component
	public static final String LOGGER_INTERACTIVE_CONTEXT = "j9ddr.interactive.context";
}
