/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
#ifndef j2sever_h
#define j2sever_h

/**
 * Constants for supported J2SE versions.
 * note: J2SE_15 needed for shared classes cache introspection but not supported by JVM.
 */
/*
 * Note: J2SE_LATEST has to be updated to highest Java version supported by VM
 * 		 This allows JVM operates with latest version when classlib.properties doesn't present.
 * Additional note:
 * 		 J2SE_LATEST will be kept as J2SE_19 while Java 18.3 raw build is being built.
 * 		 When a separate Java 18.3 binary is required, a #ifdef flag will be introduced to define
 * 		 J2SE_LATEST to different values accordingly such as following:
 * 		 #if J9VM_JAVA9_BUILD > 9
 * 		 	#define J2SE_LATEST  J2SE_1803
 * 		 #else
 * 		 	#define J2SE_LATEST  J2SE_19
 * 		 #endif
 */
#define J2SE_15  0x1500
#define J2SE_16  0x1600
#define J2SE_17  0x1700
#define J2SE_18  0x1800
#define J2SE_19  0x1900
#define J2SE_LATEST  J2SE_19

/**
 * Masks for extracting major and full versions.
 */
#define J2SE_VERSION_MASK 0xFF00
#define J2SE_RELEASE_MASK 0xFFF0
#define J2SE_SERVICE_RELEASE_MASK 0xFFFF

/**
 * Masks and constants for describing J2SE shapes.
 *  J2SE_SHAPE_SUN = Sun core libraries (aka IBM 'sidecar', Sun code + IBM kernel)
 *  J2SE_SHAPE_RAW = Pure Oracle code without any IBM modifications
 */
/*
 * Note: J2SE_SHAPE_LATEST has to be updated to highest JCL level supported by VM
 * 		 This allows JVM operates with latest level when classlib.properties doesn't present.
 * Additional note:
 * 		 J2SE_SHAPE_LATEST will be kept as J2SE_SHAPE_B165 while Java 18.3 raw build is being built.
 * 		 When a separate Java 18.3 binary is required, a #ifdef flag will be introduced to define
 * 		 J2SE_SHAPE_B165 to different values accordingly such as following:
 * 		 #if J9VM_JAVA9_BUILD > 9
 * 		 	#define J2SE_SHAPE_LATEST  J2SE_SHAPE_B1803
 * 		 #else
 * 		 	#define J2SE_SHAPE_LATEST  J2SE_SHAPE_B165
 * 		 #endif
 */
#define J2SE_SHAPE_LATEST    	J2SE_SHAPE_B165
#define J2SE_SHAPE_SUN     		0x10000
#define J2SE_SHAPE_B136    		0x40000
#define J2SE_SHAPE_B148    		0x50000
#define J2SE_SHAPE_B165    		0x60000
#define J2SE_SHAPE_B1803		0x70000
#define J2SE_SHAPE_RAWPLUSJ9	0x80000
#define J2SE_SHAPE_RAW	 		0x90000
#define J2SE_SHAPE_MASK 		0xF0000
#define J2SE_SHAPE_SHIFT		16

/**
 * Masks and constants for describing J2SE shapes.
 */
#define J2SE_LAYOUT_VM_IN_SUBDIR 0x100000
#define J2SE_LAYOUT_MASK 		0xF00000
#define J2SE_LAYOUT(javaVM) 	((javaVM)->j2seVersion & J2SE_LAYOUT_MASK)

/**
 * Macro to extract the J2SE version given a J9JavaVM.
 */
#define J2SE_VERSION(javaVM) 	((javaVM)->j2seVersion & J2SE_SERVICE_RELEASE_MASK)

/**
 * Macro to extract J2SE version given a JNIEnv.
 */ 
#define J2SE_VERSION_FROM_ENV(env) J2SE_VERSION(((J9VMThread*)env)->javaVM)

/**
 * Macro to extract the shape portion of the J2SE flags.
 */
#define J2SE_SHAPE(javaVM) 	((javaVM)->j2seVersion & J2SE_SHAPE_MASK)

/**
 * Macro to extract J2SE shape given a JNIEnv.
 */ 
#define J2SE_SHAPE_FROM_ENV(env) J2SE_SHAPE(((J9VMThread*)env)->javaVM)

#endif     /* j2sever_h */

