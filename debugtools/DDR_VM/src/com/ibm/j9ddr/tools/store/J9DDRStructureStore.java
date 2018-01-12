/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.store;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.security.DigestInputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipOutputStream;

import javax.imageio.stream.ImageInputStream;
import javax.imageio.stream.MemoryCacheImageInputStream;

import com.ibm.j9ddr.StructureReader;
import com.ibm.j9ddr.StructureReader.ConstantDescriptor;
import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;

public class J9DDRStructureStore {

	private File directory;
	private HashMap<StructureKey, String> keyMap = new HashMap<StructureKey, String>();
	HashMap<String, HashSet<String>> superset = new HashMap<String, HashSet<String>>();
	private byte[] structureBytes;
	private String supersetFilename;
	
	// Constants
	
	private static final String INDEX_FILE_NAME = "keymap.idx";
	public  static final String DEFAULT_SUPERSET_FILE_NAME = "superset.dat";
	private static final byte[] SUPERSET_ID;		
	private static final byte[] SUPERSET_ID_EBCDIC;		//EBCDIC version of the superset id
	private static final String CHARSET_EBCDIC = "Cp037";

	static {
		try {
			SUPERSET_ID = "S|".getBytes("ASCII");
			SUPERSET_ID_EBCDIC = "S|".getBytes(CHARSET_EBCDIC);
		} catch(Exception e) {
			throw new RuntimeException("Could not create SUPERSET identifier", e);
		}
	}

	/**
	 * Add newBlob to the database associated to key.
	 * 
	 * If key exists and the associated blob is identical to newBlob no action is taken
	 * and no exception is thrown.
	 * 
	 * If key exists but the associated blob is not identical to newBlob throw StructureMismatchError.
	 * This is to detect the situation where the structure has changed for a DDR enabled VM but the
	 * VM's DDR DDRStructKey has not been changed.
	 * 
	 * If an identical blob with a different key already exists in the database the key is added
	 * with a reference to the existing blob.  This will save space but will be transparent to the user.
	 * 
	 * QUESTION: Do we want to store a time stamp with  the key?
	 * 
	 * On the order of 5000 keys will be added to the database annually.  Ideally a
	 * partitioning scheme for DDRStructKey name space can be devised such that new keys need only be stored
	 * when structure data changes.
	 * 
	 * Parameters may not be null
	 * 
	 * @param key - Key identifying the VM the structure belongs to.
	 * @param structureFileName - Full path to a structure file
	 * @param inService - True if the structure is for an in-service (non-DDR enabled VM), false otherwise
	 * @throws StructureMismatchError - If key exists in the database but refers to a blob that is NOT identical to blob.
	 * @throws IOException - If there is a problem storing the the blob
	 * @throws  
	 */
	public void add(StructureKey key, String structureFileName, boolean inService) throws IOException, StructureMismatchError 
	{
		String digest = digestStructure(structureFileName);
	
		if (inService == false) {
			return;
		}
		
		String existingDigest = keyMap.get(key);
		
		if (existingDigest != null) {
			if (existingDigest.equals(digest)) {
				// Adding identical key and structure.  Do nothing
				return;
			} else {
				// Adding an existing build ID but structure is different.  Warn.
				throw new StructureMismatchError(String.format("%s exists in database but is not the same as the structure being added from %s", key, structureFileName));
			}
		}
		
		// Key does not exist in the database
		
		boolean structureAlreadyStored = keyMap.containsValue(digest);
		keyMap.put(key, digest);
		writeIndex();
		if (!structureAlreadyStored) {
			// Structure was not stored so physically store it
			physicallyStore(key, digest);
		}
	}
	
	public void updateSuperset() throws IOException {
		StructureReader reader = null;
		if((structureBytes.length > 2) && (structureBytes[0] == SUPERSET_ID[0]) && (structureBytes[1] == SUPERSET_ID[1])) {
			//creating a reader using an input stream treats it as a superset input i.e. text
			InputStream in = new ByteArrayInputStream(structureBytes);
			reader = new StructureReader(in);
		} else {
			//using an image input stream to the reader indicates that is in binary blob format
			ImageInputStream inputStream = new MemoryCacheImageInputStream(new ByteArrayInputStream(structureBytes));
			reader = new StructureReader(inputStream);
		}
		
		for (StructureDescriptor structure : reader.getStructures()) {
			String structureHeader = structure.deflate();
			HashSet<String> structureContents = superset.get(structureHeader);
			if (structureContents == null) {
				structureContents = new HashSet<String>();
				superset.put(structureHeader, structureContents);
			}
			
			for (FieldDescriptor field : structure.getFields()) {
				addFieldToSuperset(structureContents, field);
			}
			
			for (ConstantDescriptor constant : structure.getConstants()) {
				String constantHeader = constant.deflate();
				structureContents.add(constantHeader);
			}
		}
		writeSuperset();
	}

	private boolean addFieldToSuperset(HashSet<String> structureContents, FieldDescriptor field) {
		if(field.getDeclaredName().startsWith("_j9padding")) {
			// Discard all padding fields
			return false;
		}
		
		String newLine = null;		
		Iterator<String> contents = structureContents.iterator();
		while (contents.hasNext()) {
			String line = contents.next();
			if (line.startsWith("F|" + field.getName() + "|")) {
				// Found matching field name.  Check return types
				String fieldType = StructureReader.simplifyType(field.getType());
				String fieldDeclaredType = StructureReader.simplifyType(field.getDeclaredType());
				String[] parts = line.split("\\|");
				for (int i = 3; i < parts.length; i+=2) {
					String supersetType = StructureReader.simplifyType(parts[i]);
					String supersetDeclaredType = StructureReader.simplifyType(parts[i+1]);
					if (supersetType.equals(fieldType) && supersetDeclaredType.equals(fieldDeclaredType)) {
						// Found matching return type... Ignore
						return false;
					}
				}
				
				// existing field has new return type.  Modify entry
				contents.remove();
				newLine = line + "|" + fieldType + "|" + fieldDeclaredType;
				break;
			}
		}
		
		if (newLine == null) {
			// Field did not exist.  Add it.
			structureContents.add(field.deflate());
		} else {
			// Add new return type to existing field
			structureContents.add(newLine);
		}
		
		return true;
	}

	private void writeSuperset() throws IOException {
		File supersetFile = new File(directory, supersetFilename);
		BufferedWriter bw = null;
		try {
			FileWriter fw = new FileWriter(supersetFile);
			bw = new BufferedWriter(fw);

			ArrayList<String> keys = new ArrayList<String>(superset.size());
			keys.addAll(superset.keySet());
			Collections.sort(keys);
			
			for (String key : keys) {
				bw.write(key);
				bw.newLine();
				String[] values = superset.get(key).toArray(new String[0]);
				Arrays.sort(values);
				for (String line : values) {
					bw.write(line);
					bw.newLine();
				}
			}
		} finally {
			if (bw != null) {
				bw.close();
			}
		}
	}

	private void readSuperset() throws IOException {
		superset = new HashMap<String, HashSet<String>>();
		File supersetFile = new File(directory, supersetFilename);
		
		if (! supersetFile.exists()) {
			return;
		}
		
		BufferedReader br = null;
		try {
			FileReader fr = new FileReader(supersetFile);
			br = new BufferedReader(fr);
			String line = br.readLine();
			HashSet<String> structureContents = null;
			while (line != null) {
				if (line.charAt(0) == 'S') {
					structureContents = new HashSet<String>();
					superset.put(line, structureContents);
				} else {
					structureContents.add(line);
				}
				line = br.readLine();
			}
		} finally {
			if (br != null) {
				br.close();
			}
		}
	}

	private void readIndex() throws IOException {
		File indexFile = new File(directory, INDEX_FILE_NAME);
		
		if (! indexFile.exists()) {
			return;
		}
		
		BufferedReader br = null;
		try {
			FileReader fr = new FileReader(indexFile);
			br = new BufferedReader(fr);
			String line = br.readLine();
			keyMap = new HashMap<StructureKey, String>();
			while (line != null) {
				int index = line.indexOf(" ");
				if (index > -1) {
					StructureKey key = new StructureKey(line.substring(0, index));
					String digest = line.substring(index + 1);
					keyMap.put(key, digest);
				}
				line = br.readLine();
			}
		} finally {
			if (br != null) {
				br.close();
			}
		}
	}
	
	private void writeIndex() throws IOException {
		File indexFile = new File(directory, INDEX_FILE_NAME);
		BufferedWriter bw = null;
		try {
			FileWriter fw = new FileWriter(indexFile);
			bw = new BufferedWriter(fw);
			Iterator<java.util.Map.Entry<StructureKey, String>> entries = keyMap.entrySet().iterator();
			while (entries.hasNext()) {
				java.util.Map.Entry<StructureKey, String> entry = entries.next();
				bw.write(entry.getKey().toString());
				bw.write(" ");
				bw.write(entry.getValue());
				bw.write(System.getProperty("line.separator"));
			}
		} finally {
			if (bw != null) {
				bw.close();
			}
		}
	}

	private void physicallyStore(StructureKey key, String digest) throws IOException {
		
		File zipFile = getZipFile(key, digest);
		
		if (zipFile.exists()) {
			throw new IOException(String.format("Attempting to over write existing structure data %s with key %s", zipFile.getAbsolutePath(), key));
		}
		
		ZipOutputStream zipOutputStream = new ZipOutputStream(new FileOutputStream(zipFile));
		ZipEntry zipEntry = getZipEntry(key, digest);
	
		zipOutputStream.putNextEntry(zipEntry);
		zipOutputStream.write(structureBytes);
		zipOutputStream.closeEntry();
		zipOutputStream.close();
	}

	private File getZipFile(StructureKey key, String digest) {
		return new File(directory, key.getPlatform() + "." + digest + ".zip");
	}

	private ZipEntry getZipEntry(StructureKey key, String digest) {
		return new ZipEntry(key.getPlatform() + "." + digest + ".ddr");
	}

	private String digestStructure(String structureFileName) throws IOException {
		MessageDigest md = null;
		byte[] buffer = new byte[1024];
		
		try {
			md = MessageDigest.getInstance("MD5");
		} catch (NoSuchAlgorithmException e) {
			// This only happens if the JRE configuration is really messed up.
			throw new IllegalStateException(e.getMessage());
		}

		DigestInputStream dis = null;
		FileInputStream structure = new FileInputStream(structureFileName);
		dis = new DigestInputStream(structure, md);
		ByteArrayOutputStream structureStream = new ByteArrayOutputStream();
		
		try {
			while (true) {
				int bytesRead = dis.read(buffer);
				if (bytesRead == -1) {
					break;
				}
				structureStream.write(buffer, 0, bytesRead);
			}
			dis.close();
		} finally {
			structure.close();
		}
		
		structureBytes = structureStream.toByteArray();
		
		if((structureBytes.length > 2) && (structureBytes[0] == SUPERSET_ID_EBCDIC[0]) && (structureBytes[1] == SUPERSET_ID_EBCDIC[1])) {
			convertFromEBCDIC();
		}
		
		byte[] digestBytes = dis.getMessageDigest().digest();
		return byteArrayHexString(digestBytes);
	}
	
	//converts the structure data from ebcdic into ASCII 
	private void convertFromEBCDIC() throws IOException {
		try {
			System.out.println("Converting input file from EBCDIC to UTF-8");
			String data = new String(structureBytes, CHARSET_EBCDIC);
			structureBytes = data.getBytes("UTF-8");
		} catch (Exception e) {
			IOException ioe = new IOException("Failed to convert data from EBCDIC");
			ioe.initCause(e);
			throw ioe;
		}
	}
	
	private String byteArrayHexString(byte[] digest) {
		StringWriter result = new StringWriter();
		for (int i = 0; i < digest.length; i++) {
			result.write(Integer.toHexString(0x100 + digest[i] & 0x00FF));
		}
		return result.toString();
	}

	/**
	 * Remove the element key from the database.  Naturally the blob data itself is only removed once all keys
	 * that refer to it have been removed
	 * 
	 * @param key
	 * @return true if the key was found and removed or did not exist, false otherwise
	 * @throws IOException - if there was a problem removing the blob
	 */
	public boolean remove(StructureKey key) throws IOException {
		String digest = keyMap.remove(key);
		if (digest == null) {
			return false;
		}
		
		if (!keyMap.containsValue(digest)) {
			File zipFile = getZipFile(key, digest);
			zipFile.delete();
		}
		
		writeIndex();
		
		return true;
	}
	
	/**
	 * @param key
	 * @return An InputStream open and positioned at the start of the J9DDR structure the blob associated with key.  null if key is not contained in the database
	 * @throws IOException - if there is a problem retrieving the blob
	 */
	public ImageInputStream get(StructureKey key) throws IOException {
		String digest = keyMap.get(key);
		if (digest == null) {
			return null;
		}
		
		ZipFile zipFile = new ZipFile(getZipFile(key, digest));
		ZipEntry entry = zipFile.getEntry(getZipEntry(key, digest).getName());
		InputStream entryStream = zipFile.getInputStream(entry);
		
		byte[] buffer = new byte[1024];
		ByteArrayOutputStream output = new ByteArrayOutputStream((int) entry.getSize());
		
		while (true) {
			int bytesRead = entryStream.read(buffer);
			if (bytesRead == -1) {
				break;
			}
			output.write(buffer, 0, bytesRead);
		}
		zipFile.close();
		
		return new MemoryCacheImageInputStream(new ByteArrayInputStream(output.toByteArray()));
	}
	
	/**
	 * Return a blob that is contains structures and fields that are a superset of all blobs stored
	 * in the database.  
	 * 
	 * Field offsets are ignored for this operation and the field offsets returned in the blob have no
	 * meaning.  Only the structure and field names are useful.
	 * 
	 * This API is used for the DDR Structure and Pointer Java code generator
	 * 
	 * @return an ImageInputStream opened and positioned at the start of a J9DDR structure blob
	 * @throws FileNotFoundException 
	 */
	public InputStream getSuperset() throws FileNotFoundException {
		return new FileInputStream(new File(directory, supersetFilename));
	}
	
	protected J9DDRStructureStore() {
		super();
	}
	
	/**
	 * Create and open the database on the databaseDirectory.
	 * Store the current contents of the receiver in the database.
	 * Throw an IOException if the directory already exists and does not
	 * contain a valid database.
	 * @param backingDirectory - Full path to directory 
	 * @throws IOException if backingDirectory does not exist or can not be accessed
	 */
	public J9DDRStructureStore(String databaseDirectory, String supersetFileName) throws IOException {
		super();
		
		if (supersetFileName == null) {
			this.supersetFilename = DEFAULT_SUPERSET_FILE_NAME;
		} else {
			this.supersetFilename = supersetFileName;
		}
		
		initialize(databaseDirectory);
	}
	
	private void initialize(String databaseDirectory) throws IOException {
		if (databaseDirectory == null) {
			throw new IllegalArgumentException("databaseDirectory may not be null");
		}
		
		directory = new File(databaseDirectory);
		if (!directory.exists()) {
			create();
		} else {
			open();
		}
	}

	private void create() throws IOException {
		if (!directory.mkdir()) {
			throw new IOException(String.format("Unable to create directory %s", directory.getAbsolutePath()));
		}
		writeIndex();
		writeSuperset();
	}

	/**
	 * Open the database on the backingDirectory.
	 * Will not create existing directories
	 * @param backingDirectory - Full path to directory 
	 * @throws IOException if backingDirectory does not exist or can not be accessed
	 */
	private void open() throws IOException {
		readIndex();
		readSuperset();
	}
	
	public String getSuperSetFileName(){
		return this.supersetFilename;
	}

	@Deprecated
	public void close() throws IOException {
//		if (zipOutputStream != null) {
//			zipOutputStream.close();
//			zipOutputStream = null;
//		}
		
//		if (zipFile != null) {
//			zipFile.close();
//			zipFile = null;
//		}
	}
}
