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
 * Indicates that an attempt was made to access memory which is not included within the image.
 */
public class MemoryAccessException extends DTFJException {
    
	private static final long serialVersionUID = 8883829684484518525L;

	private ImagePointer badPointer;
    
    /**
     * Build exception for the given location and description 
     * @param badPointer		the location which caused the access exception
     * @param description		text description
     */
    public MemoryAccessException(ImagePointer badPointer, String description) {
        super(description);
        this.badPointer = badPointer;
    }
    
    /**
     * Build exception for the given location and description 
     * @param badPointer		the location which caused the access exception
     */
    public MemoryAccessException(ImagePointer badPointer) {
        super();
        this.badPointer = badPointer;
    }
    
    /**
     * Get a pointer into the image where the access failed.
     * @return The pointer into the image where the access failed
     */
    public ImagePointer getPointer() {
    	return badPointer;
    }
}
