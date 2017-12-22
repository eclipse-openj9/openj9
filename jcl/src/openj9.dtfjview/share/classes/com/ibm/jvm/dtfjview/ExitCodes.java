/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview;

/**
 * Exit codes which can be returned by jdmpview.
 * see Jazz Design 46749
 * 
 * @author adam
 *
 */
public interface ExitCodes {
	/**
	 *	Processing completed normally without error
	 */
	public static final int JDMPVIEW_SUCCESS = 0;
	
	/**
	 * One of the supplied commands could not be executed, or had a syntax error, processing continued afterwards.
	 */
	public static final int JDMPVIEW_SYNTAX_ERROR = 1;
	
	/**
	 * The core file could not be found
	 */
	public static final int JDMPVIEW_FILE_ERROR	= 2;
	
	/**
	 * 	There were no jvm's found in the core file
	 */
	public static final int JDMPVIEW_NOJVM_ERROR = 3;
	
	
	/**
	 * An internal error occurred and processing was aborted 
	 */
	public static final int JDMPVIEW_INTERNAL_ERROR	= 4;
	
	/**
	 * 	An output file already exists and the overwrite option was not set
	 */
	public static final int JDMPVIEW_OUTFILE_EXISTS	= 5;
	
	/**
	 * Option -append and Option -overwrite can not co-exist.
	 */
	public static final int JDMPVIEW_APPEND_OVERWRITE_COEXIST	= 6;
}
