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
 * Interface which lists all of the property names which are part of the DTFJ context property bag.
 * 
 * @author adam
 *
 */
public interface SessionProperties {
	/**
	 * If jdmpview was started with -verbose then this property is present and has the value "true".
	 */
	public static final String VERBOSE_MODE_PROPERTY = "verbose.mode";
	
	/**
	 * Controls if files are extracted from archives or not
	 */
	public static final String EXTRACT_PROPERTY = "extract.mode";
	
	/**
	 * The path to the core file which is providing the current context
	 */
	public static final String CORE_FILE_PATH_PROPERTY = "core_file_path";
		
	/**
	 * Contains the DTFJ image factory
	 */
	public static final String IMAGE_FACTORY_PROPERTY = "image.factory";
	
	/**
	 * Name of the logger to use
	 */
	public static final String LOGGER_PROPERTY = "com.ibm.jvm.dtfjview.logger";
	
	/**
	 * Holds the value of the users current working directory
	 */
	public static final String PWD_PROPERTY = "pwd";
	
	/**
	 * A reference to the jdmpview session which is currently being executed
	 */
	public static final String SESSION_PROPERTY = "session";
	
	/**
	 * Set if zip files are to be treated as in legacy implementations 
	 * which will call ImageFactory.getImage(File)
	 */
	public static final String LEGACY_ZIP_MODE_PROPERTY = "zip.mode.legacy";
}

