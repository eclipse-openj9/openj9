/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.classloader;

public interface IClassLoaderTypes {
	
	public static final String CLASSLOADER_SECTION = "CLASSES";
	public static final String T_1CLTEXTCLLOS = "1CLTEXTCLLOS";
	public static final String T_1CLTEXTCLLSS = "1CLTEXTCLLSS";
	public static final String T_2CLTEXTCLLOADER = "2CLTEXTCLLOADER";
	public static final String T_3CLNMBRLOADEDLIB = "3CLNMBRLOADEDLIB";
	public static final String T_3CLNMBRLOADEDCL  = "3CLNMBRLOADEDCL";
	
	public static final String T_1CLTEXTCLLIB = "1CLTEXTCLLIB";
	public static final String T_2CLTEXTCLLIB = "2CLTEXTCLLIB";
	public static final String T_3CLTEXTLIB = "3CLTEXTLIB";
	
	public static final String T_1CLTEXTCLLOD = "1CLTEXTCLLOD";
	public static final String T_2CLTEXTCLLOAD = "2CLTEXTCLLOAD";
	public static final String T_3CLTEXTCLASS = "3CLTEXTCLASS";
	
	
	/*
	 * Attributes
	 */
	public static final String CL_ATT_ACCESS_PERMISSIONS = "cl_access_permissions";
	public static final String CL_ATT__NAME = "cl_name";
	public static final String CL_ATT_ADDRESS = "cl_address";
	public static final String CL_ATT_SHADOW_ADDRESS = "cl_shadow_address";
	public static final String CL_ATT_PARENT_ADDRESS = "cl_parent_address";
	public static final String CL_ATT_PARENT_NAME = "cl_parent_name";
	public static final String CL_ATT_NMBR__LIB = "cl_nmbr_ld_lb";
	public static final String CL_ATT_NMBR_LOADED_CL = "cl_nmbr_ld_cl";
	public static final String CL_ATT_LIB_NAME = "cl_att_lib_name";
	public static final String CLASS_ATT_NAME = "class_name";
	public static final String CLASS_ATT_ADDRESS = "class_address";
	
	

}
