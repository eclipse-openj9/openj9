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
import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.URI;
import java.text.MessageFormat;
import java.text.ParsePosition;
import java.text.SimpleDateFormat;
import java.util.Date;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageFactory;

/** 
 * @author ajohnson
 */
public class PHDImageFactory implements ImageFactory {

	static final Date earliestDump = new Date((2000-1970)*(365*4+1)/4*24*3600*1000L);
	static final Date latestDump = new Date(Integer.MAX_VALUE*1000L);
	
	public PHDImageFactory() {
	}

	public Image getImage(ImageInputStream in, URI sourceID) throws IOException {
		return new PHDImage(sourceID, in);
	}

	public Image getImage(ImageInputStream in, ImageInputStream meta, URI sourceID) throws IOException {
		ImageFactory metaFactory = getMetaFactory();
		Image metaImage = metaFactory.getImage(meta, sourceID);
		return new PHDImage(sourceID, in, metaImage);
	}

	public Image[] getImagesFromArchive(File archive, boolean extract) throws IOException {
		throw new IOException("Not supported for PHD files");
	}

	private ImageFactory getMetaFactory() throws IOException {
		ImageFactory factory = null;
		try {
			Class<?> c2 = Class.forName("com.ibm.dtfj.image.javacore.JCImageFactory");
			factory = (ImageFactory) c2.newInstance();
		} catch (ClassNotFoundException e) {
			IOException e2 = new IOException("Unable to create javacore image factory");
			e2.initCause(e);
			throw e2;
		} catch (InstantiationException e) {
			IOException e2 = new IOException("Unable to create javacore image factory");
			e2.initCause(e);
			throw e2;
		} catch (IllegalAccessException e) {
			IOException e2 = new IOException("Unable to create javacore image factory");
			e2.initCause(e);
			throw e2;
		}
		return factory;
	}

	public int getDTFJMajorVersion() {
		return DTFJ_MAJOR_VERSION;
	}

	public int getDTFJMinorVersion() {
		return DTFJ_MINOR_VERSION;
	}

	public Image getImage(File file) throws IOException {
		File meta = findMetaFile(file);
		if (meta == null) {
			// Resolve symbolic links and see if metafile can be found that way
			File f2 = file;
			try {
				f2 = file.getCanonicalFile();
			} catch (IOException e) {
				// Ignore
			}
			if (!file.equals(f2))
				meta = findMetaFile(f2);
		}
		if (meta != null) {
			try {
				Image i2 = getMetaImage(file, meta);
				return new PHDImage(file, i2);
			} catch (IOException e) {
				// Swallow an IOException from an unasked-for metadata file
			}
		}
		return new PHDImage(file);
	}

	/**
	 * Use some heuristics to find the metafile
	 * @param file
	 * @return Associated meta file (javacore) or null if not found
	 */
	private File findMetaFile(File file) {
		String name = file.getName();
		String prefix = "heapdump";
		// Allow extra stuff in front of "heapdump"
		int p = Math.max(0, name.indexOf(prefix));
		String extraPrefix = name.substring(0, p);
		name = name.substring(p);
		if (name.startsWith(prefix)) {
			// Name may be of form heapdump.20081016.125646.7376.0001.phd
			// with metafile javacore.20081016.125646.7376.0002.txt
			// or heapdump.20081016.125646.7376.phd
			// with metafile javacore.20081016.125646.7376.txt  
			// or heapdump.20081106.111729.675904.phd 
			// with metafile javacore.20081106.111729.675904.txt
			// or heapdump.20081016.125646.7376.0001.phd.gz
			// with metafile javacore.20081016.125646.7376.0002.txt
			// i.e. filename.date.time.pid.seq.ext
			// Also allow anything before the heapdump, e.g.
			// myFailingTest.123.heapdump.20081016.125646.7376.phd
			String s[] = name.split("\\.");
			s[0] = extraPrefix + "javacore" + s[0].substring(prefix.length());
			int sequence[];
			if (s.length >= 6) {
				sequence = new int[]{0,1,-1};
			} else {
				sequence = new int[]{0};
			}
			for (int j : sequence) {
				String metafn = genJavacoreName(s, j, 4);
				File meta = new File(file.getParent(), metafn);
				if (meta.exists()) {
					return meta;
				}
			}
			// or for Java 1.4.2
			// heapdump.20090907.222334.4420.phd
			// heapdump.20090907.222334.4420.phd.gz
			// javacore.20090907.222357.4420.txt
			if (s.length == 5 || s.length == 6 && s[5].equals("gz")) {
				// See if javacore is created less than 2 minutes after the heap dump
				String dateTime = "yyyyMMdd.HHmmss";
				SimpleDateFormat sdf = new SimpleDateFormat(dateTime);
				ParsePosition pp = new ParsePosition(0);
				for (int i = prefix.length(); i < name.length(); ++i) {
					pp.setIndex(i);
					Date d = sdf.parse(name, pp);
					if (d != null && !d.before(earliestDump)&& d.before(latestDump)) {
						/* Valid date */
						String namePrefix = name.substring(0, i).replace("heapdump", "javacore");
						String nameSuffix = name.substring(pp.getIndex()).replaceAll("\\.phd$", ".txt").replaceAll("\\.phd\\.gz$", ".txt");
						// Try incrementing date by 1000ms = 1s for 2 minutes */
						for (long dl = d.getTime(); dl < d.getTime() + 2*60*1000; dl += 1000) {
							String newDate = sdf.format(new Date(dl));
							String metafn = extraPrefix + namePrefix + newDate + nameSuffix;
							File meta = new File(file.getParent(), metafn);
							if (meta.exists()) {
								return meta;
							}
						}
					}
				}
			}			
			// or for AIX 1.4.2 filenamePID.EPOCHTIME.ext
			// heapdump454808.1244656860.phd
			// javacore454808.1244656979.txt
			if (s.length == 3 || s.length == 4 && s[3].equals("gz")) {
				// See if javacore is created less than 5 minutes after the heap dump
				for (int j = 1; j < 5 * 60; ++j) {
					String metafn = genJavacoreName(s, j, 1);
					File meta = new File(file.getParent(), metafn);
					if (meta.exists()) {
						return meta;
					}
				}
			}
		}
		return null;
	}

	private String genJavacoreName(String[] s, int inc, int componentToInc) {
		StringBuffer sb = new StringBuffer(s[0]);
		for (int i = 1; i < s.length - 1; ++i) {
			String ss = s[i];
			// Increment the dump id if required
			if (i == componentToInc) {
				try {
					int iv = Integer.parseInt(ss);
					String si = Integer.toString(iv+inc);
					ss = ss.substring(0, Math.max(0, ss.length() - si.length())) + si;
				} catch (NumberFormatException e) {
				}
			} else if (i == s.length - 2 && s[i + 1].equals("gz")) {
				// Skip adding .phd for file ending .phd.gz
				break;
			}
			sb.append('.');
			sb.append(ss);
		}
		sb.append(".txt");
		return sb.toString();
	}

	public Image getImage(File file, File metaFile) throws IOException {
		Image i2 = getMetaImage(file, metaFile);
		return new PHDImage(file, i2);
	}

	private Image getMetaImage(File file, File metaFile) throws IOException,
			FileNotFoundException {
		ImageFactory metaFactory = getMetaFactory();
		Image i2;
		try {
			i2 = metaFactory.getImage(metaFile);
		} catch (RuntimeException e) {
			// javacore reader isn't that robust
			FileNotFoundException ex = new FileNotFoundException(MessageFormat.format("Problem opening dump {0} metafile {1}", file, metaFile));
			ex.initCause(e);
			throw ex;
		}
		return i2;
	}

	public int getDTFJModificationLevel() {
		// modification level is VM stream plus build version - historically the tag from RAS_Auto-Build/projects.psf - now just a number
		return 28002;
	}
	
}
