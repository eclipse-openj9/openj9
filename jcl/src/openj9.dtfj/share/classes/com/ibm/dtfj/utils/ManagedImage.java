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
package com.ibm.dtfj.utils;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.utils.file.ManagedImageSource;

/**
 * Wrapper class to allow management functionality to be added to DTFJ Images
 * returned to the client. It is done as an extension to the Image interface
 * rather than a wrapper class so as to preserve to original class implementing
 * image in case that is being checked for.
 * 
 * This needs to move to the ras.jar.
 * 
 * @author adam
 *
 */
public interface ManagedImage extends Image {
	
	public void setImageSource(ManagedImageSource source);
	
	public ManagedImageSource getImageSource();
}
