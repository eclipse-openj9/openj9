/*[INCLUDE-IF Sidecar16]*/

package java.lang;

/*******************************************************************************
 * Copyright (c) 1998, 2014 IBM Corp. and others
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
 
/**
 * This exception is thrown when a program attempts
 * to access a constructor which is not accessible 
 * from the location where the reference is made.
 *
 * @author		OTI
 * @version		initial
 */
public class InstantiationException extends ReflectiveOperationException { 
	private static final long serialVersionUID = -8441929162975509110L;
	
/**
 * Constructs a new instance of this class with its 
 * walkback filled in.
 *
 * @author		OTI
 * @version		initial
 */
public InstantiationException () {
	super();
}
/**
 * Constructs a new instance of this class with its 
 * walkback and message filled in.
 *
 * @author		OTI
 * @version		initial
 *
 * @param		detailMessage String
 *				The detail message for the exception.
 */
public InstantiationException (String detailMessage) {
	super(detailMessage);
}

/**
 * Constructs a new instance of this class with its 
 * walkback and message filled in.
 *
 * @author		OTI
 * @version		initial
 *
 * @param		clazz Class
 *				The class which cannot be instantiated.
 */
InstantiationException (Class clazz) {
	super(clazz.getName());
}
}
