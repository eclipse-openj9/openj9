/*******************************************************************************
 * Copyright (c) 2013, 2019 IBM Corp. and others
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
package com.ibm.j9ddr;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteOrder;

import javax.imageio.stream.ImageInputStream;

/**
 * Represents the header for the blob
 *
 * @author adam
 *
 */
public class StructureHeader {

	/**
	 * Identifies the type of blob by its name
	 *
	 * @author adam
	 *
	 */
	public static enum BlobID {
		J9("IBM J9 VM"),
		node("node.js"),
		unknown("unknown");


		private final String name;

		BlobID(String name) {
			this.name = name;
		}

		@Override
		public String toString() {
			return name;
		}


	}

	private static final String DEFAULT_J9_PACKAGE_PREFIX = "vm";

	private int coreVersion;
	private byte sizeofBool;
	private byte sizeofUDATA;
	private byte bitfieldFormat;
	private int structDataSize;
	private int stringTableDataSize;
	private int structureCount;
	private byte configVersion;
	private BlobID blobID = BlobID.J9;
	private int blobVersion;
	private String packageID = DEFAULT_J9_PACKAGE_PREFIX;		//set the default Java package name
	private final long headerSize;								//size of the header based on the number of bytes being read from the stream -1 = unknown

	public StructureHeader(ImageInputStream ddrStream) throws IOException {
		long start = ddrStream.getStreamPosition();
		readCommonData(ddrStream);
		switch(coreVersion) {
			case 2 :
				readBlobVersion(ddrStream);
				break;
		}
		headerSize = ddrStream.getStreamPosition() - start;
	}

	/**
	 * Allow the use of partial header reading by creating a StructureHeader which is
	 * just identified by it's version. Fragments of the header can then be read in.
	 * The primary use for this function is when loading just a blob identifier from the
	 * core file which is then used to match an entry in the blob library.
	 *
	 * @param configVersion
	 */
	public StructureHeader(byte configVersion) {
		this.configVersion = configVersion;
		headerSize = -1;
	}

	/**
	 * Configure a structure header which is just identified by a blob type and version.
	 * This will be used when a blob is determined by heuristics rather than directly
	 * being read from the core. This information is then typically used to retrieve the
	 * blob from the BlobFactory.
	 *
	 * @param id
	 * @param blobVersion
	 * @param packageID
	 */
	public StructureHeader(BlobID id, int blobVersion, String packageID) {
		this.blobID = id;
		this.blobVersion = blobVersion;
		this.packageID = packageID;
		headerSize = -1;
	}

	/**
	 * Use this constructor if the header is coming from a superset file.
	 * Warning : as this file does not contain any header information all the
	 * defaults will be used in this instance.
	 *
	 * @param in
	 */
	public StructureHeader(InputStream in) {
		//do nothing as the header can only be read from a blob image, not a superset file
		headerSize = -1;
	}

	private void readCommonData(ImageInputStream ddrStream) throws IOException {
		ddrStream.mark();
		coreVersion = ddrStream.readInt();
		if (coreVersion > 0xFFFF) {
			ddrStream.setByteOrder(ByteOrder.LITTLE_ENDIAN);
			ddrStream.reset();
			coreVersion = ddrStream.readInt();
		}
		sizeofBool = ddrStream.readByte();
		sizeofUDATA = ddrStream.readByte();
		bitfieldFormat = ddrStream.readByte();
		configVersion = ddrStream.readByte();	//unused byte alignment field
		structDataSize = ddrStream.readInt();
		stringTableDataSize = ddrStream.readInt();
		structureCount = ddrStream.readInt();
	}

	public void readBlobVersion(ImageInputStream ddrStream) throws IOException {
		switch(configVersion) {
			case 1 :
				int id = ddrStream.readInt();
				if(id >= BlobID.unknown.ordinal()) {
					blobID = BlobID.unknown;
				} else {
					blobID = BlobID.values()[id];
				}
				blobVersion = ddrStream.readInt();
				StringBuilder builder = new StringBuilder();
				boolean terminated = false;
				for(int i = 0; i < 32; i++) {
					char c = (char)ddrStream.readByte();
					if(c == 0) {
						terminated |= terminated;
					} else {
						if(!terminated) {
							builder.append(c);
						}
					}
				}
				packageID = builder.toString();
				break;
			default :
				throw new IOException("Blob config version " + configVersion + " is not supported");
		}
	}

	public int getCoreVersion() {
		return coreVersion;
	}

	public byte getSizeofBool() {
		return sizeofBool;
	}

	public byte getSizeofUDATA() {
		return sizeofUDATA;
	}

	public byte getBitfieldFormat() {
		return bitfieldFormat;
	}

	public int getStructDataSize() {
		return structDataSize;
	}

	public int getStringTableDataSize() {
		return stringTableDataSize;
	}

	public int getStructureCount() {
		return structureCount;
	}

	public byte getConfigVersion() {
		return configVersion;
	}

	public BlobID getBlobID() {
		return blobID;
	}

	public int getBlobVersion() {
		return blobVersion;
	}

	public int[] getBlobVersionArray() {
		int[] data = new int[4];
		data[0] = blobVersion & 0xFF;
		data[1] = (blobVersion >> 8) & 0xFF;
		data[2] = (blobVersion >> 16) & 0xFF;
		data[3] = (blobVersion >> 24) & 0xFF;
		return data;
	}

	public String getPackageID() {
		return packageID;
	}

	public long getHeaderSize() {
		return headerSize;
	}
}
