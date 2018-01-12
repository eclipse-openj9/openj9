/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
package com.ibm.jvm.format;

import java.io.IOException;

/** 
 * This exception is thrown when the beginning of a buffer (including spanned
 * record if any) is not the start of a record.
 * <p> An InvalidSpannedRecordException is thrown whenever an invalid spanned
 *     record is encountered.  It is thrown at two possible points in
 *     TraceRecordExternal.prime()and caught in TraceFormat.readAndFormat()
 *
 * @author John Barfield
 */
public class InvalidSpannedRecordException extends IOException
{
    public InvalidSpannedRecordException() {
        super();
    }

    public InvalidSpannedRecordException(String s) {
        super(s);
    }
}
