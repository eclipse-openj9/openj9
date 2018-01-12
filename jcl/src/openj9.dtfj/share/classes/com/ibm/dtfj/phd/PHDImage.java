/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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
import java.net.InetAddress;
import java.net.URI;
import java.text.MessageFormat;
import java.text.ParsePosition;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Properties;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.phd.parser.HeapdumpReader;
import com.ibm.dtfj.utils.ManagedImage;
import com.ibm.dtfj.utils.file.ManagedImageSource;

/** 
 * @author ajohnson
 */
public class PHDImage implements ManagedImage {
	
	private final File file;
	private final ArrayList<ImageAddressSpace> addressList;
	private final Image meta;
	private final List<HeapdumpReader> closeList = new LinkedList<HeapdumpReader>();
	private final URI source;
	private ManagedImageSource imageSource = null;

	/** 
	 * Build the image from the PHD file
	 * @param file The phd file.
	 */
	PHDImage(File file) throws IOException {
		this(file, null);
	}

	/** 
	 * Build the image from the PHD file and an associated Javacore file
	 * @param file The phd file.
	 * @param meta The DTFJ image for the metafile data
	 */
	PHDImage(File file, Image meta) throws IOException {
		this.file = file;
		this.meta = meta;
		source = null;
		try {
			ImageAddressSpace metaSpace = null;
			if (meta != null) {
				Iterator<?> i2 = meta.getAddressSpaces();
				if (i2.hasNext()) {
					Object o2 = i2.next();
					if (!(o2 instanceof CorruptData)) {
						metaSpace = (ImageAddressSpace)o2;
					}
				}
			}
			PHDImageAddressSpace imageAddress = new PHDImageAddressSpace(file, this, metaSpace);
			addressList = new ArrayList<ImageAddressSpace>();
			addressList.add(imageAddress);
		} catch (Error e) {
			if (e.getClass() == java.lang.Error.class) {
				// Probably just thrown by the PHD reader
				IOException e1 = new IOException(MessageFormat.format("Problem opening dump {0} metafile {1}", file, meta));
				e1.initCause(e);
				throw e1;
			}
			throw e;
		}
	}
	
	/** 
	 * Build the image from the PHD file
	 * @param file The phd file.
	 */
	PHDImage(URI source, ImageInputStream stream) throws IOException {
		this(source, stream, null);
	}
	
	/** 
	 * Build the image from the PHD file and an associated Javacore file
	 * @param file The stream containing the PHD
	 * @param meta The DTFJ image for the metafile data
	 */
	PHDImage(URI source, ImageInputStream stream, Image meta) throws IOException {
		this.source = source;
		this.file = null;
		this.meta = meta;
		try {
			ImageAddressSpace metaSpace = null;
			if (meta != null) {
				Iterator<?> i2 = meta.getAddressSpaces();
				if (i2.hasNext()) {
					Object o2 = i2.next();
					if (!(o2 instanceof CorruptData)) {
						metaSpace = (ImageAddressSpace)o2;
					}
				}
			}
			PHDImageAddressSpace imageAddress = new PHDImageAddressSpace(stream, this, metaSpace);
			addressList = new ArrayList<ImageAddressSpace>();
			addressList.add(imageAddress);
		} catch (Error e) {
			if (e.getClass() == java.lang.Error.class) {
				// Probably just thrown by the PHD reader
				IOException e1 = new IOException(MessageFormat.format("Problem opening dump {0} metafile {1}", file, meta));
				e1.initCause(e);
				throw e1;
			}
			throw e;
		}
	}
	
	
	public URI getSource() {
		if(source == null) {
			return file.toURI();
		} else {
			return source;
		}
	}

	public Iterator<ImageAddressSpace> getAddressSpaces() {
		return addressList.iterator();
	}

	/**
	 * Use the filename if of the form heapdump.yyyyMMdd.HHmmss.pid.seq.phd
	 * else the file date
	 */
	public long getCreationTime() throws DataUnavailable {
		
		if( file == null ) {
			throw new DataUnavailable("File creation time not available.");
		}
		
		String name = file.getName();
		String prefix = "heapdump";
		// Allow extra stuff in front of "heapdump"
		int p = Math.max(0, name.indexOf(prefix));
		name = name.substring(p);
		if (name.startsWith(prefix)) {
			String dateTime = "yyyyMMdd.HHmmss";
			SimpleDateFormat sdf = new SimpleDateFormat(dateTime);
			ParsePosition pp = new ParsePosition(0);
			for (int i = prefix.length(); i < name.length(); ++i) {
				pp.setIndex(i);
				Date d = sdf.parse(name, pp);
				if (d != null && !d.before(PHDImageFactory.earliestDump)&& d.before(PHDImageFactory.latestDump)) {
					return d.getTime();
				}
			}
		}
		
		// or for AIX 1.4.2 heapdumpPID.EPOCHTIME.phd
		// heapdump454808.1244656860.phd
		String s[] = name.split("\\.");
		prefix = "heapdump";
		if ((s.length == 3  || s.length == 4 && s[3].equals("gz")) && s[0].startsWith(prefix)) {
			try {
				// Check the first part is also a number (PID)
				Integer.parseInt(s[0].substring(prefix.length()));
				// Check the second part is also a number
				long l2 = Long.parseLong(s[1]);
				// The second number is the number of seconds since the epoch
				// Simple validation - since circa 2000 ?
				Date dF = new Date(l2 * 1000L);
				if (!dF.before(PHDImageFactory.earliestDump) && dF.before(PHDImageFactory.latestDump)) {
					return dF.getTime();
				}
			} catch (NumberFormatException e) {
			}
		}
		
		try {
			if (meta != null) return meta.getCreationTime();
		} catch (DataUnavailable e) {
		}
		return file.lastModified();
	}

	public String getHostName() throws DataUnavailable, CorruptDataException {
		if (meta != null) return meta.getHostName();
		throw new DataUnavailable();
	}

	public Iterator<InetAddress> getIPAddresses() throws DataUnavailable {
		// Javacore doesn't have IP addresses either
		throw new DataUnavailable();
	}

	public long getInstalledMemory() throws DataUnavailable {
		if (meta != null) return meta.getInstalledMemory();
		throw new DataUnavailable();
	}

	public int getProcessorCount() throws DataUnavailable {
		if (meta != null) return meta.getProcessorCount();
		throw new DataUnavailable();
	}

	public String getProcessorSubType() throws DataUnavailable,
			CorruptDataException {
		if (meta != null) return meta.getProcessorSubType();
		throw new DataUnavailable();
	}

	public String getProcessorType() throws DataUnavailable,
			CorruptDataException {
		if (meta != null) return meta.getProcessorType();
		throw new DataUnavailable();
	}

	public String getSystemSubType() throws DataUnavailable,
			CorruptDataException {
		if (meta != null) return meta.getSystemSubType();
		throw new DataUnavailable();
	}

	public String getSystemType() throws DataUnavailable, CorruptDataException {
		if (meta != null) return meta.getSystemType();
		throw new DataUnavailable();
	}

	public void close() {
		// Ideally we would explicitly close the various streams in PHDJavaHeap
		if (addressList != null) {
			addressList.clear();
		}
		if (meta != null) {
			meta.close();
		}
		for( HeapdumpReader r: closeList.toArray(new HeapdumpReader[]{})) {
			// This will also call back and remove from list.
			if( r!= null ) {
				r.releaseResources();
			}
		}
		//if the Image Source has been set, then see if an extracted file needs to be deleted
		if((imageSource != null) && (imageSource.getExtractedTo() != null)) {
			imageSource.getExtractedTo().delete();		//attempt to delete the file
		}
	}
	
	protected void finalize() throws Throwable {
		close();
		super.finalize();
	}
	
	// Methods to maintain a list of HeapdumpReaders that have
	// opened the heap dump file. Should only be accessed by
	// HeapdumpReader(File, Image) and HeapdumpReader.close();
	
	/**
	 * Register a HeapdumpReader as needing to be closed when
	 * Image.close() is called on this Image.
	 */
	public void registerReader(HeapdumpReader reader) {
		closeList.add(reader);
	}

	/**
	 * Unregister a HeapdumpReader so it no longer needs to be
	 * closed. This should only be called by HeapdumpReader.close()
	 * to make sure that we don't leak readers.
	 */
	public void unregisterReader(HeapdumpReader reader) {
		closeList.remove(reader);
	}

	public Properties getProperties() {
		return new Properties();		//not supported for this reader
	}

	public ManagedImageSource getImageSource() {
		return imageSource;
	}

	public void setImageSource(ManagedImageSource source) {
		imageSource = source;
	}

	public long getCreationTimeNanos() throws DataUnavailable, CorruptDataException {
		// Not supported in DTFJ for PHD dumps
		throw new DataUnavailable();
	}
}
