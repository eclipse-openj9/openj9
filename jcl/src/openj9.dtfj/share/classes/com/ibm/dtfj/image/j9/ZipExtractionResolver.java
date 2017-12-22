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
package com.ibm.dtfj.image.j9;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Vector;
import java.util.logging.Logger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import com.ibm.dtfj.corereaders.ResourceReleaser;

/**
 * @author jmdisher
 *
 */
public class ZipExtractionResolver implements IFileLocationResolver, ResourceReleaser
{
	private ZipFile _container;
	private Map _openFilesByName = new HashMap();
	private List _deletables = new Vector();
	
	public ZipExtractionResolver(ZipFile zip)
	{
		_container = zip;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.j9.IFileLocationResolver#findFileWithFullPath(java.lang.String)
	 */
	public File findFileWithFullPath(String fullPath)
			throws FileNotFoundException
	{
		String tempname = "";

		//take the end of the path and see if we have such an entry
		String name = (new File(fullPath)).getName();
		File knownFile = (File) _openFilesByName.get(fullPath);
		
		if (null == knownFile) {
			//first try to get the entry by its long name.  If that doesn't work, try the short name (an easy way to get some backward compatibility)
			ZipEntry entry = _container.getEntry(fullPath);
			
			if (null == entry) {
				entry = _container.getEntry(name);
			}
			if (null == entry) {
				throw new FileNotFoundException("No ZIP entry with name: \"" + fullPath + "\"");
			} else {
				File temp = null;
				InputStream zipContent = null;
				FileOutputStream output = null;
				int size = 0;
				byte[] buffer = new byte[4096];

				try {
					temp = File.createTempFile(name, ".dtfj");
					tempname = temp.getAbsolutePath();
					//set the file for deletion
					temp.deleteOnExit();
					_deletables.add(temp);
				} catch (IOException e) {
					FileNotFoundException fnf = new FileNotFoundException("Unable to open temporary file: " + e.getMessage());
					fnf.setStackTrace(e.getStackTrace());
					throw fnf;
				}

				try {
					zipContent = _container.getInputStream(entry);
					output = new FileOutputStream(temp);
				} catch (IOException e) {
					FileNotFoundException fnf = new FileNotFoundException("Unable to locate ZIP entry: " + entry.getName() + " : " + e.getMessage());
					fnf.setStackTrace(e.getStackTrace());
					throw fnf;
				}

				try {
					size = zipContent.read(buffer);
				} catch (IOException e) {
					FileNotFoundException fnf = new FileNotFoundException("Unable to read ZIP entry: " + entry.getName() + " : " + e.getMessage());
					fnf.setStackTrace(e.getStackTrace());
					throw fnf;
				}

				boolean deleteAfterClose = false;
				try {
					do {
						try {
							output.write(buffer, 0, size);
						} catch (IOException e) {
							deleteAfterClose = true;
							FileNotFoundException fnf = new FileNotFoundException("Unable to write: " + entry.getName() + " to " + tempname + " : " + e.getMessage());
							fnf.setStackTrace(e.getStackTrace());
							throw fnf;
						}
	
						try {
							size  = zipContent.read(buffer);
						} catch (IOException e) {
							deleteAfterClose = true;
							FileNotFoundException fnf = new FileNotFoundException("Unable to read ZIP entry: " + entry.getName() + " : " + e.getMessage());
							fnf.setStackTrace(e.getStackTrace());
							throw fnf;
						}
					} while (size > 0);
				} finally {
					try {
						output.close();
						if (deleteAfterClose) {
							temp.deleteOnExit();
							temp.delete();
						}
					} catch (IOException e) {
						FileNotFoundException fnf = new FileNotFoundException("Unable to close file: " + e.getMessage());
						fnf.setStackTrace(e.getStackTrace());
						throw fnf;
					} 
				}

				//save this to the collection
				_openFilesByName.put(fullPath, temp);
				knownFile = temp;
				Logger.getLogger(com.ibm.dtfj.image.ImageFactory.DTFJ_LOGGER_NAME).fine("Extracted " + fullPath + " to " + temp.getAbsolutePath());
			}
		}
		return knownFile;
	}

	public File decompressCoreFile() throws FileNotFoundException
	{
		// we are going to try to infer the name of the core file since we know that it will be with an XML file of the same name
		String coreName = _baseCoreName();
		
		if (null != coreName) {
			return findFileWithFullPath(coreName);
		} else {
			throw new FileNotFoundException("Couldn't find a file resembling a core file inside the zip");
		}
	}

	/**
	 * @return
	 */
	private String _baseCoreName()
	{
		String coreName = null;
		Enumeration outer = _container.entries();
		String zipName = _container.getName();
		
		while ((null == coreName) && (outer.hasMoreElements())) {
			ZipEntry outerEntry = (ZipEntry) outer.nextElement();
			String outerName = outerEntry.getName();
			
			if (zipName.equals(outerName  + ".zip")) {
				//outerEntry is the same name as the zip which is the JExtract convention for naming.  Therefore, this is the core file
				coreName = outerName;
			} else {
				ZipEntry innerEntry = _container.getEntry(outerName + ".xml");
				if (null != innerEntry) {
					//there is an XML file corresponding to outerEntry which is a JExtract convention so this is the core file
					coreName = outerName;
				}
			}
		}
		return coreName;
	}

	public InputStream decompressMetaDataStream() throws IOException, FileNotFoundException
	{
		InputStream streamData = null;
		String coreName = _baseCoreName();
		if (null != coreName) {
			streamData = _container.getInputStream(_container.getEntry(coreName + ".xml"));
		} else {
			throw new  FileNotFoundException("Couldn't find a file resembling a JExtract XML index file inside the zip");
		}
		return streamData;
	}

	public void closeOpenFiles() throws IOException 
	{
		if(_container != null) {
			_container.close();
		}
	}
	
	public Iterator getCreatedFiles() 
	{
		return _deletables.iterator();
	}

	public void releaseResources() throws IOException {
		closeOpenFiles();
		File f;
		Iterator i;
		for(i = getCreatedFiles(), f = null; i.hasNext() && ((f=(File) i.next())!=null) ; ) {
			String logMessage = "ZIP resource "+f.getAbsolutePath()+" ";
			if (f.delete()) {
				logMessage += "deleted successfully.";
			} else {
				logMessage += "could not be deleted. Marking for deletion at shutdown.";
				f.deleteOnExit();
			}
			Logger.getLogger(com.ibm.dtfj.image.ImageFactory.DTFJ_LOGGER_NAME).fine(logMessage);
		}
	}

}
