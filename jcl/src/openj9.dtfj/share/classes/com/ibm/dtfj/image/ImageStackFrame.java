/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image;

/**
 * Represents a native stack frame.
 */
public interface ImageStackFrame {

    /**
     * Get the address of the current instruction within
     * the procedure being executed.
     * @return the address of the current instruction within
     * the procedure being executed, or null if not available.
     * <p>
     * Use this address with caution, as it is provided only 
     * as a best guess. It may not be correct, or even within
     * readable memory
     * @throws CorruptDataException 
     */
    public ImagePointer getProcedureAddress() throws CorruptDataException;
    
    /**
     * Get the base pointer of the stack frame.
     * @return the base pointer of the stack frame
     * @throws CorruptDataException 
     */
    public ImagePointer getBasePointer() throws CorruptDataException;
    
    /**
     * Returns a string describing the procedure at this stack
     * frame. Implementations should use the following template
     * so that procedure names are reported consistently:
     * <p>
     public * <pre>libname(sourcefile)::entrypoint&#177;<!-- +/- -->offset</pre>
     * <p>
     * Any portion of the template may be omitted if it is not available 
     * <dl>
     * <dt>e.g.</dt>
     * 		<dd><pre>system32(source.c)::WaitForSingleObject+14</pre></dd>
     * 		<dd><pre>system32::WaitForSingleObject-4</pre></dd>
     * 		<dd><pre>(source.c)::WaitForSingleObject</pre></dd>
     * 		<dd><pre>::WaitForSingleObject+14</pre></dd>
     * 		<dd><pre>system32+1404</pre></dd>
     * 		<dd><pre>system32::TWindow::open(int,void*)+14</pre></dd>
     * </dl>
     * 
     * @return a string naming the function executing in this
     * stack frame.  If the name is not known for legitimate
     * reasons, DTFJ will return a synthetic name.
     * @throws CorruptDataException 
     */
    public String getProcedureName() throws CorruptDataException;
    
}
