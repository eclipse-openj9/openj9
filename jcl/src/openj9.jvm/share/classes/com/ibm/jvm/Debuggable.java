/*[INCLUDE-IF Sidecar16]*/

package com.ibm.jvm;

/*******************************************************************************
 * Copyright (c) 2008, 2014 IBM Corp. and others
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

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;


/** 
 * The Debuggable annotation applies to either classes or methods and provides
 * a hint to the VM that decorated entities must remain debuggable.  This facility
 * is intended for use by languages implemented in Java where portions of the program
 * must support debugging (e.g. breakpoints, variable access) but where whole-program
 * optimization is critical.
 */

// Ensure that Debuggable annotations are visible at runtime
@Retention(RetentionPolicy.RUNTIME)

// Constrain Debuggable annotations to types and methods
@Target({ElementType.TYPE, ElementType.METHOD})

public @interface Debuggable {

}
