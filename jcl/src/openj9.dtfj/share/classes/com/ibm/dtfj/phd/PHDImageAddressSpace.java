/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2020 IBM Corp. and others
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
package com.ibm.dtfj.phd;

import java.io.File;
import java.io.IOException;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.runtime.ManagedRuntime;

/** 
 * @author ajohnson
 */
class PHDImageAddressSpace implements ImageAddressSpace {

	private final List<ImageProcess> processList;
	private final ImageAddressSpace metaImageAddressSpace;
	
	PHDImageAddressSpace(File file, PHDImage parentImage, ImageAddressSpace metaImageAddressSpace) throws IOException {
		this.metaImageAddressSpace = metaImageAddressSpace;
		processList = new ArrayList<>();
		ImageProcess metaProc = metaImageAddressSpace != null ? metaImageAddressSpace.getCurrentProcess() : null;
		processList.add(new PHDImageProcess(file, parentImage,this, metaProc));
	}
	
	PHDImageAddressSpace(ImageInputStream stream, PHDImage parentImage, ImageAddressSpace metaImageAddressSpace) throws IOException {
		this.metaImageAddressSpace = metaImageAddressSpace;
		processList = new ArrayList<>();
		ImageProcess metaProc = metaImageAddressSpace != null ? metaImageAddressSpace.getCurrentProcess() : null;
		processList.add(new PHDImageProcess(stream, parentImage, this, metaProc));
	}

	@Override
	public ImageProcess getCurrentProcess() {
		return processList.get(0);
	}

	@Override
	public ByteOrder getByteOrder() {
		if (metaImageAddressSpace != null) {
			return metaImageAddressSpace.getByteOrder();
		}

		return ByteOrder.BIG_ENDIAN; // FIXME
	}

	@Override
	public Iterator<ImageSection> getImageSections() {
		List<ImageSection> list = new ArrayList<ImageSection>();
		for (Iterator<ImageProcess>ip = getProcesses(); ip.hasNext(); ) {
			ImageProcess p = ip.next();
			if (p instanceof CorruptData) continue;
			for (Iterator<ManagedRuntime> jr = p.getRuntimes(); jr.hasNext(); ) {
				ManagedRuntime mr = jr.next();
				if (mr instanceof CorruptData) continue;
				if (mr instanceof JavaRuntime) {
					for (Iterator<JavaHeap> jh = ((JavaRuntime)mr).getHeaps(); jh.hasNext(); ) {
						JavaHeap hp = jh.next();
						if (hp instanceof CorruptData) continue;
						for (Iterator<ImageSection> is = hp.getSections(); is.hasNext(); ) {
							// Add the corrupt sections too
							list.add(is.next());
						}
					}
				}
			}
		}
		if (metaImageAddressSpace != null) {
			for (Iterator it = metaImageAddressSpace.getImageSections(); it.hasNext(); ) {
				Object next = it.next();
				if (next instanceof ImageSection) {
					ImageSection section = (ImageSection)next;
					ImageSection newSection = new PHDImageSection(section.getName(), getPointer(section.getBaseAddress().getAddress()), section.getSize());
					list.add(newSection);
				}
			}
		}
		return list.iterator();
	}

	public ImagePointer getPointer(long arg0) {
		return new PHDImagePointer(this,arg0);
	}

	public Iterator<ImageProcess> getProcesses() {
		return processList.iterator();
	}

	public String getID() throws DataUnavailable, CorruptDataException {
		return "0";
	}

	public Properties getProperties() {
		return new Properties();		//not supported for this reader
	}

}
