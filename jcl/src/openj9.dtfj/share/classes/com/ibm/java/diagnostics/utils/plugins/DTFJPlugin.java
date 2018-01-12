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

package com.ibm.java.diagnostics.utils.plugins;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Retention(RetentionPolicy.RUNTIME)
public @interface DTFJPlugin {
	/**
	 * Version is of the formx.y where x.y is the minimum level of the DTFJ API 
	 * required in order for the plugin to work with x being the major version 
	 * and y the minor version. The wildcard * can be used to specify any match 
	 * e.g. 1.* would match any implementation of version 1 of the DTFJ API.
	 * 
	 * @return the minimum version of the API for which this plugin is valid
	 */
	public String version();
	
	/**
	 * Specifies that the DTFJ plugin does or does not require the presence of
	 * a JavaRuntime.
	 * 
	 * @return
	 */
	public boolean runtime() default true;
	
	/**
	 * Specifies that the DTFJ does or does not require the presence of 
	 * an Image
	 * 
	 * @return
	 */
	public boolean image() default true;
	
	/**
	 * Specifies that the output from this plugin can be cached.
	 * Default is true.
	 * 
	 * @return true if output can be cached, false if not
	 */
	public boolean cacheOutput() default false;
}
