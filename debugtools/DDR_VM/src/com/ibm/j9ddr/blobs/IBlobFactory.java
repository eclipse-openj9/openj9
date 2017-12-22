/*******************************************************************************
 * Copyright (c) 2013, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.blobs;

import javax.imageio.stream.ImageInputStream;

/**
 * Factory which provides blobs
 * 
 * @author adam
 *
 */
public interface IBlobFactory {
	
	public enum Platforms {
		xi32,			//linux 32
		xa64,			//linux 64
		wi32,			//windows 32
		wa64,			//windows 64
		xp32,			//p/linux 32
		xp64			//p/linux 64
	}

	/**
	 * Gets a blob for a specific platform version etc.
	 * 
	 * @param platform
	 * @param id
	 * @param major
	 * @param minor
	 * @param revision
	 * @return stream if a match is found, null if not
	 */
	public ImageInputStream getBlob(Platforms platform, String id, int major, int minor, int revision);
	
	public ImageInputStream getBlob(String path);
}
