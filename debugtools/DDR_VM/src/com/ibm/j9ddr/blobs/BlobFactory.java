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

import java.io.InputStream;

import javax.imageio.stream.ImageInputStream;
import javax.imageio.stream.MemoryCacheImageInputStream;

public class BlobFactory implements IBlobFactory {
	private static final String BLOB_ROOT = "/data/";
	
	private BlobFactory() {
		//hide constructor
	}
	
	public static IBlobFactory getInstance() {
		return new BlobFactory();
	}
	
	public ImageInputStream getBlob(Platforms platform, String id, int major,
			int minor, int revision) {
		//simple classpath locator, can be enhanced in the future to provide updates over the web
		String path = "/data/" + id + "/" + platform.toString() + "/" + major + "/" + minor + "/" + revision;
		return getBlob(path);
	}

	public ImageInputStream getBlob(String path) {
		if(!path.startsWith(BLOB_ROOT)) {
			path = BLOB_ROOT + path;
		}
		if(!path.endsWith("/j9ddr.dat")) {
			path += "/j9ddr.dat";
		}
		InputStream is = getClass().getResourceAsStream(path);
		if(is != null) {
			MemoryCacheImageInputStream in = new MemoryCacheImageInputStream(is);
			return in;
		}
		return null;
	}

}
