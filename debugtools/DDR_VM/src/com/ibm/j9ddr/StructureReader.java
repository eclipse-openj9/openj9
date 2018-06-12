/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.io.Reader;
import java.io.StringWriter;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.logging.Logger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static java.util.logging.Level.*;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.logging.LoggerNames;

// TODO: Lazy initializing has been removed.  Need to decide if it stays out.
public class StructureReader {
	public static final int VERSION = 1;
	public static final int J9_STRUCTURES_EYECATCHER = 0xFACEDEB8;	//eyecatcher / magic identifier for a J9 structure file
	private HashMap<String, StructureDescriptor> structures = null;
	private String[] packages;

	private static final Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
	private StructureHeader header;

	@SuppressWarnings("unused")
	private byte unused;

//	public static final String STRUCTURE_PACKAGE_SLASH_NAME = "com/ibm/j9ddr/vm/structure/";
//	public static final String STRUCTURE_PACKAGE_DOT_NAME = "com.ibm.j9ddr.vm.structure";
//	public static final String POINTER_PACKAGE_DOT_NAME = "com.ibm.j9ddr.vm.pointer.generated";
	public static final Class<?>[] STRUCTURE_CONSTRUCTOR_SIGNATURE = new Class[] {Long.TYPE};
	public static final byte BIT_FIELD_FORMAT_LITTLE_ENDIAN = 1;
	public static final byte BIT_FIELD_FORMAT_BIG_ENDIAN = 2;
	public static final int BIT_FIELD_CELL_SIZE = 32;

	public static final String ALIAS_MAP_RESOURCE = "com/ibm/j9ddr/StructureAliases%d.dat";

	private static final Pattern MULTI_LINE_COMMENT_PATTERN = Pattern.compile(Pattern.quote("/*") + ".*?" + Pattern.quote("*/") , Pattern.DOTALL);
	private static final Pattern SINGLE_LINE_COMMENT_PATTERN = Pattern.compile(Pattern.quote("//") + ".*$", Pattern.MULTILINE);
	private static final Pattern MAP_PATTERN = Pattern.compile("(.*?)=(.*?)$", Pattern.MULTILINE);

	private Map<String, String> aliasMap;

	private long packageVersion;
	private String basePackage = DDR_VERSIONED_PACKAGE_PREFIX;

	/* Patterns for cleaning types */
	/* Pattern that matches a 1 or more characters not including ']' that occur after [ */
	private static final Pattern CONTENTS_OF_ARRAY_PATTERN = Pattern.compile("(?<=\\[).*?(?=\\])");

	private static final Pattern SPACES_AFTER_SQUARE_BRACKETS_PATTERN = Pattern.compile("(?<=\\])\\s+");

	private static final Pattern SPACES_BEFORE_SQUARE_BRACKETS_PATTERN = Pattern.compile("\\s+(?=\\[)");

	private static final Pattern SPACES_AFTER_ASTERISKS_PATTERN = Pattern.compile("(?<=\\*)\\s+");

	private static final Pattern SPACES_BEFORE_ASTERISKS_PATTERN = Pattern.compile("\\s+(?=\\*)");

	// VM Version constants
	private static final String VM_MAJOR_VERSION = "VM_MAJOR_VERSION";
	private static final String VM_MINOR_VERSION = "VM_MINOR_VERSION";
	private static final String ARM_SPLIT_DDR_HACK = "ARM_SPLIT_DDR_HACK";
	private static final String DDRALGORITHM_STRUCTURE_NAME = "DDRAlgorithmVersions";

	public static final String DDR_VERSIONED_PACKAGE_PREFIX = "com.ibm.j9ddr.vm";

	public static enum PackageNameType {
		STRUCTURE_PACKAGE_SLASH_NAME,
		STRUCTURE_PACKAGE_DOT_NAME,
		PACKAGE_DOT_BASE_NAME,
		POINTER_PACKAGE_DOT_NAME
	}

	/**
	 * Initialize this reader from the supplied data.
	 * The ImageInputStream must be validated and already be positioned to point to the 1st byte of
	 * the DDR Structure Data.
	 * The ByteOrder of the ImageInputStream must already be set to the ByteOrder of the file being read.
	 * The DDR Structure data is written in the ByteOrder of the platform that created the core file.
	 * TODO: Pick a ByteOrder for the in service VM data we will create
	 */
	public StructureReader(ImageInputStream in) throws IOException {
		parse(in);
		setStream();
		applyAliases();
	}

	/**
	 * Read a stream in the J9DDRStructStore superset format
	 */
	public StructureReader(InputStream in) throws IOException {
		parse(in);
		setStream();
	}

	public StructureHeader getHeader() {
		return header;
	}

	/**
	 * Sets the based on the DDRAlgorithmVersions field in the blob.
	 */
	private void setStream() {
		StructureDescriptor version = structures.get(DDRALGORITHM_STRUCTURE_NAME);
		long vmMajorVersion = 2;		//default values of 2
		long vmMinorVersion = 30;

		/* JAZZ 103906 : A hack was introduced when we split jvm.29 for ARM development. 
		 * The goal was to keep using the same values of VM_MAJOR_VERSION and 
		 * VM_MINOR_VERSION as in jvm.29 but now support ARM in this separate stream. 
		 * Therefore, we needed a new way to differentiate the special ARM jvm.29 stream
		 * and this is the reason why the ARM_SPLIT_DDR_HACK field was added.
		 */
		boolean armHack = false;
		if(version != null) {
			for(ConstantDescriptor constant : version.getConstants()) {
				if(constant.getName().equals(VM_MAJOR_VERSION)) {
					vmMajorVersion = constant.getValue();
				}
				if(constant.getName().equals(VM_MINOR_VERSION)) {
					vmMinorVersion = constant.getValue();
				}
				if(constant.getName().equals(ARM_SPLIT_DDR_HACK)) {
					armHack = (1 == constant.getValue());
				}
			}
		}
		packageVersion = (vmMajorVersion * 10) + (vmMinorVersion / 10);		//the stream is a 2 digit value
		packages = new String[4];
		if (!armHack) {
			packages[PackageNameType.STRUCTURE_PACKAGE_SLASH_NAME.ordinal()] = String.format("com/ibm/j9ddr/vm%2d/structure/", packageVersion);
			packages[PackageNameType.STRUCTURE_PACKAGE_DOT_NAME.ordinal()] = String.format(DDR_VERSIONED_PACKAGE_PREFIX + "%2d.structure", packageVersion);
			packages[PackageNameType.PACKAGE_DOT_BASE_NAME.ordinal()] = String.format(DDR_VERSIONED_PACKAGE_PREFIX + "%2d", packageVersion);
			packages[PackageNameType.POINTER_PACKAGE_DOT_NAME.ordinal()] = String.format(DDR_VERSIONED_PACKAGE_PREFIX + "%2d.pointer.generated", packageVersion);
		} else {
			packages[PackageNameType.STRUCTURE_PACKAGE_SLASH_NAME.ordinal()] = String.format("com/ibm/j9ddr/vm%2d_00/structure/", packageVersion);
			packages[PackageNameType.STRUCTURE_PACKAGE_DOT_NAME.ordinal()] = String.format(DDR_VERSIONED_PACKAGE_PREFIX + "%2d_00.structure", packageVersion);
			packages[PackageNameType.PACKAGE_DOT_BASE_NAME.ordinal()] = String.format(DDR_VERSIONED_PACKAGE_PREFIX + "%2d_00", packageVersion);
			packages[PackageNameType.POINTER_PACKAGE_DOT_NAME.ordinal()] = String.format(DDR_VERSIONED_PACKAGE_PREFIX + "%2d_00.pointer.generated", packageVersion);
		}
	}

	public String getBasePackage() {
		return basePackage;
	}

	/**
	 * Get the package name that should be used including the version information
	 * @param type
	 * @return
	 */
	public String getPackageName(PackageNameType type) {
		if(packages == null) {
			throw new IllegalStateException("The DDR version information is not yet available");
		}
		return packages[type.ordinal()];
	}

	private void applyAliases() throws IOException
	{
		aliasMap = loadAliasMap(this.packageVersion);

		for (StructureDescriptor thisStruct : structures.values()) {
			for (FieldDescriptor thisField : thisStruct.fields) {
				thisField.applyAliases(aliasMap);
			}
		}
	}

	public static Map<String, String> loadAliasMap(long version) throws IOException
	{
		String mapData = loadAliasMapData(version);

		mapData = stripComments(mapData);

		Map<String, String> aliasMap = new HashMap<String, String>();

		Matcher mapMatcher = MAP_PATTERN.matcher(mapData);

		while (mapMatcher.find()) {
			String from = mapMatcher.group(1);
			String to = mapMatcher.group(2);

			aliasMap.put(from.trim(), to.trim());
		}

		return Collections.unmodifiableMap(aliasMap);
	}

	private static String stripComments(String mapData)
	{
		mapData = MULTI_LINE_COMMENT_PATTERN.matcher(mapData).replaceAll("");
		mapData = SINGLE_LINE_COMMENT_PATTERN.matcher(mapData).replaceAll("");
		return mapData;
	}

	private static String loadAliasMapData(long version) throws IOException
	{
		String streamAliasMapResource = String.format(ALIAS_MAP_RESOURCE, version);
		InputStream is = StructureReader.class.getResourceAsStream('/' + streamAliasMapResource);
		if (null == is) {
			throw new RuntimeException("Failed to load alias map from resource: " + streamAliasMapResource + " - cannot continue");
		}

		Reader reader = null;
		try {
			reader = new InputStreamReader(is, "UTF-8");
		} catch (UnsupportedEncodingException e) {
			//Should be impossible
			throw new RuntimeException(e);
		}

		try {
			StringBuilder builder = new StringBuilder();

			char[] buffer = new char[4096];

			int read;

			while ( (read = reader.read(buffer)) != -1 ) {
				builder.append(buffer, 0, read);
			}

			return builder.toString();
		} finally {
			reader.close();
		}
	}

	/**
	 * Get a list of the structures defined by this reader
	 * @return Set interface for the structure names
	 */
	public Set<String> getStructureNames() {
		return structures.keySet();
	}

	/**
	 * Test to see if a structure has been read in and hence avaialble
	 * @param name name of the structure to look for
	 * @return true if the structure exists, false if not
	 */
	public boolean hasStructure(String name) {
		return structures.containsKey(name);
	}


	public int getStructureSizeOf(String structureName) {
		if(!hasStructure(structureName)) {
			return 0;
		}
		return structures.get(structureName).getSizeOf();
	}

	public Collection<StructureDescriptor> getStructures() {
		return structures.values();
	}

	/**
	 * Get a map of the fields for a particular structure and their offsets.
	 * @param structureName name of the structure
	 * @return map of the fields as FieldName, Offset pair
	 */
	public ArrayList<FieldDescriptor> getFields(String structureName) {

		StructureDescriptor structure = structures.get(structureName);
		if (structure == null) {
			throw new IllegalArgumentException("The structure [" + structureName + "] was not found");
		}

		return structure.fields;
	}

	public ArrayList<ConstantDescriptor> getConstants(String structureName) {

		StructureDescriptor structure = structures.get(structureName);
		if (structure == null) {
			throw new IllegalArgumentException("The structure [" + structureName + "] was not found");
		}

		return structure.constants;
	}

	private void parse(InputStream inputStream) throws IOException {
		header = new StructureHeader(inputStream);
		structures = new HashMap<String, StructureDescriptor>();
		BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));

		String line = reader.readLine();
		StructureDescriptor structure = null;
		while (line != null) {
			char type = line.charAt(0);
			switch (type) {
			case 'S':
				structure = new StructureDescriptor(line);
				structures.put(structure.getName(), structure);
				break;
			case 'F':
				Collection<FieldDescriptor> fields = FieldDescriptor.inflate(line);
				structure.fields.addAll(fields);
				break;
			case 'C':
				ConstantDescriptor constant = new ConstantDescriptor(line);
				structure.constants.add(constant);
				break;
			default:
				throw new IllegalArgumentException(String.format("Superset stream contained unknown line: %s", line));
			}
			line = reader.readLine();
		}
	}

	/**
	 * Add to the existing set of structures already contained in this reader. Note that
	 * this will replace any definitions already contained within the reader.
	 *
	 * @param ddrStream stream to add the structures from
	 * @throws IOException
	 */
	public void addStructures(ImageInputStream ddrStream) throws IOException {
		StructureHeader fragmentHeader = new StructureHeader(ddrStream);
		checkBlobVersion();
		if(header.getSizeofBool() != fragmentHeader.getSizeofBool()) {
			throw new IOException("Invalid fragment definition : size of boolean is not the same");
		}
		if(header.getSizeofUDATA() != header.getSizeofUDATA()) {
			throw new IOException("Invalid fragment definition : size of UDATA is not the same");
		}
		parseStructures(ddrStream, fragmentHeader);
	}

	/**
	 * Parse the supplied data and extract the structure information.
	 * It is expected that the ImageInputStream already points to the start
	 * of the DDR structure data and the ByteOrder of the ImageInputStream is
	 * set to the ByteOrder of the core file.  (The DDR Structure data is written in
	 * the ByteOrder of the platform that created the core file.)
	 *
	 * This parse is called when the blob is initially loaded.
	 *
	 * TODO: Pick a ByteOrder for the in service VM data we will create
	 * The structures are lazily loaded in that the
	 * name of the structure is identified but not processed until a subsequent
	 * call requests it.
	 * @param type
	 *
	 * @param data valid J9 structure data
	 * @throws IOException re-throws any exceptions from the ImageInputStream
	 */
	private void parse(ImageInputStream ddrStream) throws IOException {
		logger.logp(FINE,null,null,"Parsing structures. Start address = {0}",Long.toHexString(ddrStream.getStreamPosition()));
		header = new StructureHeader(ddrStream);
		checkBlobVersion();
		parseStructures(ddrStream, header);
	}

	/**
	 * Checks that the blob version is supported and correctly sets the ByteOrder.
	 *
	 * @param ddrStream blob stream to check
	 * @throws IOException thrown if the version is not supported
	 */
	private void checkBlobVersion() throws IOException {

		logger.logp(FINE, null, null, "Stream core structure version = {0}", header.getCoreVersion());

		if (header.getCoreVersion() > VERSION) {
			throw new IOException("Core structure version " + header.getCoreVersion() + " != StructureReader version " + VERSION);
		}

	}

	/**
	 * Parse the structures from a supplied stream.
	 *
	 * @param structureCount number of structures to parse
	 * @param ddrStream stream to read from, assumes that the stream is correctly positioned
	 * @throws IOException
	 */
	private void parseStructures(ImageInputStream ddrStream, StructureHeader header) throws IOException {
		logger.logp(FINER, null, null, "structDataSize={0}, stringTableDataSize={1}, structureCount={2}",
				new Object[]{header.getStructDataSize(), header.getStringTableDataSize(), header.getStructureCount()});

		// This line must come after the header reads above or the offsets will be wrong.
		long ddrStringTableStart = ddrStream.getStreamPosition() + header.getStructDataSize();

		logger.logp(FINER,null,null,"ddrStringTableStart=0x{0}",Long.toHexString(ddrStringTableStart));

		if(structures == null) {
			//initialize the structure map with a sensible initial capacity
			structures = new HashMap<String, StructureDescriptor>(header.getStructureCount());
		}

		for(int i = 0; i < header.getStructureCount(); i++) {
			logger.logp(FINER, null, null, "Reading structure on iteration {0}",i);
			StructureDescriptor structure = new StructureDescriptor();
			structure.name = readString(ddrStream, ddrStringTableStart);
			if (structure.name == "") {
				logger.logp(FINE, null, null, "Structure name was blank for structure {0}",i);
				throw new IllegalArgumentException(String.format("No structure name found for structure %d", i));
			}
			if (structure.name == null) {
				logger.logp(FINE, null, null, "Structure name was null for structure {0}",i);
				throw new IllegalArgumentException(String.format("Structure name was null for structure %d", i));
			}
			structure.name = structure.name.replace("__", "$");
			logger.logp(FINE,null,null,"Reading structure {0}",structure.name);

			structure.pointerName = structure.name + "Pointer";
			structure.superName = readString(ddrStream, ddrStringTableStart);
			structure.superName = structure.superName.replace("__", "$");
			structure.sizeOf = ddrStream.readInt();
			int numberOfFields = ddrStream.readInt();
			structure.fields = new ArrayList<FieldDescriptor>(numberOfFields);
			int numberOfConstants = ddrStream.readInt();
			structure.constants = new ArrayList<ConstantDescriptor>(numberOfConstants);

			logger.logp(FINER,null,null,"{0} super {1} sizeOf {2}",new Object[]{structure.name,structure.superName,structure.sizeOf});

			for (int j = 0; j < numberOfFields; j++) {
				String declaredName = readString(ddrStream, ddrStringTableStart);

				//Inline anonymous structures are handled by stacking the fields, separated by ".".
				String name = declaredName.replace(".", "$");

				String declaredType = readString(ddrStream, ddrStringTableStart);
				if (name.equals("hashCode")) {
					name = "_hashCode";
				}

				//Type is unaliased later
				int offset = ddrStream.readInt();
				FieldDescriptor field = new FieldDescriptor(offset, declaredType, declaredType, name, declaredName);
				structure.fields.add(field);
				logger.logp(FINEST,null,null,"Field: {0}.{1} offset {2}, declaredType {3}",new Object[]{structure.name,name,offset,declaredType,declaredType});
			}

			for (int j = 0; j < numberOfConstants; j++) {
				String name = readString(ddrStream, ddrStringTableStart);
				long value = ddrStream.readLong();
				ConstantDescriptor constant = new ConstantDescriptor(name, value);
				structure.constants.add(constant);
				logger.logp(FINEST,null,null,"Constant: {0}.{1}={2}",new Object[]{structure.name,name,value});
			}

			structures.put(structure.name, structure);
		}

		logger.logp(FINE,null,null,"Finished parsing structures");
	}

	private String readString(ImageInputStream ddrStream, long ddrStringTableStart) {
		try {
			int stringOffset = ddrStream.readInt();
			if (stringOffset == -1) {
				return "";
			}

			long pos = ddrStream.getStreamPosition();
			long seekPos = ddrStringTableStart + stringOffset;
			ddrStream.seek(seekPos);
			int length = ddrStream.readUnsignedShort();
			if (length > 200) {
				throw new IOException(String.format("Improbable string length: %d", length));
			}
			// TODO: Reuse buffer
			byte[] buffer = new byte[length];
			int read = ddrStream.read(buffer);

			if (read != length) {
				throw new IOException("StructureReader readString() Failed to read " + length + " at " + Long.toHexString(seekPos) + ". Result: " + read);
			}

			String result = new String(buffer, "UTF-8");
			ddrStream.seek(pos);
			return result;
		} catch (IOException e) {
			// put the stack trace to the log
			StringWriter sw = new StringWriter();
			PrintWriter pw = new PrintWriter(sw);
			e.printStackTrace(pw);
			logger.logp(FINE, null, null, sw.toString());
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return null;
	}

	public static class StructureDescriptor {
		String name;
		String superName;
		String pointerName;
		int sizeOf;
		ArrayList<FieldDescriptor> fields;
		ArrayList<ConstantDescriptor> constants;

		public StructureDescriptor() {
			super();
		}

		public StructureDescriptor(String line) {
			super();
			inflate(line);
		}

		public String toString() {
			return name + " extends " + superName;
		}

		public String getPointerName() {
			return name + "Pointer";
		}

		public String getName() {
			return name;
		}

		public String getSuperName() {
			return superName;
		}

		public ArrayList<FieldDescriptor> getFields() {
			return fields;
		}

		public ArrayList<ConstantDescriptor> getConstants() {
			return constants;
		}

		public int getSizeOf() {
			return sizeOf;
		}

		private void inflate(String line) {
			String[] parts = line.split("\\|");
			if (parts.length < 3 || parts.length > 4) {
				throw new IllegalArgumentException(String.format("Superset file line is invalid: %s", line));
			}
			constants = new ArrayList<ConstantDescriptor>();
			fields = new ArrayList<FieldDescriptor>();
			name = parts[1];
			pointerName = parts[2];

			if (parts.length == 4) {
				superName = parts[3];
			} else {
				superName = "";
			}
		}

		public String deflate() {
			StringBuffer result = new StringBuffer();
			result.append("S|");				// 0
			result.append(getName());			// 1
			result.append("|");
			result.append(getPointerName());	// 2
			result.append("|");
			result.append(getSuperName());		// 3
			return result.toString();
		}

	}

	public static class ConstantDescriptor implements Comparable<ConstantDescriptor>{
		String name;
		long value;  // U_64
		// TODO: what can hold a U_64 in Java

		public ConstantDescriptor(String name, long value) {
			super();
			this.name = name;
			this.value = value;
		}

		public ConstantDescriptor(String line) {
			super();
			inflate(line);
		}

		public String toString() {
			return name + " = " + value;
		}

		public String getName() {
			return name;
		}

		public long getValue() {
			return value;
		}

		public int compareTo(ConstantDescriptor o) {
			return getName().compareTo(o.getName());
		}

		private void inflate(String line) {
			String[] parts = line.split("\\|");
			if (parts.length != 2) {
				throw new IllegalArgumentException(String.format("Superset file line is invalid: %s", line));
			}
			name = parts[1];
		}

		public String deflate() {
			return "C|" + getName();	//0 & 1
		}

		@Override
		public boolean equals(Object obj) {
			if((obj == null) || !(obj instanceof ConstantDescriptor)) {
				return false;
			}
			ConstantDescriptor compareTo = (ConstantDescriptor) obj;
			return name.equals(compareTo.name) && (value == compareTo.value);
		}

		@Override
		public int hashCode() {
			return name.hashCode();
		}


	}

	public static class FieldDescriptor implements Comparable<FieldDescriptor>{
		String type;		  // Type as declared in Java
		String declaredType;  // Type as declared in C or C++
		String name;		  // Name as declared in Java
		String declaredName;  // Name as declared in C or C++
		int offset;

		public FieldDescriptor(int offset, String type, String declaredType, String name, String declaredName) {
			super();
			this.type = type;
			this.declaredType = declaredType;
			this.name = name;
			this.offset = offset;
			this.declaredName = declaredName;
		}

		public void applyAliases(Map<String, String> aliasMap)
		{
			type = unalias(declaredType,aliasMap);
			cleanUpTypes();
		}


		/**
		 * Cleans up this type by mapping U_32 -> U32 , removing any const declaration etc.
		 */
		public void cleanUpTypes() {
			type = stripUnderscore(type);
			type = stripTypeQualifiers(type);
			declaredType = stripUnderscore(declaredType);
		}

		private String stripTypeQualifiers(String type)
		{
			String working = type.replaceAll("const", "");
			working = working.replaceAll("volatile", "");
			return working.trim();
		}

		//removes underscore to map onto a single definition of J9 types e.g. U_8 -> U8 or I_DATA -> IDATA
		private String stripUnderscore(String type) {
			return type.replaceAll("U_(?=\\d+|DATA)", "U").replaceAll("I_(?=\\d+|DATA)", "I");
		}

		/*
		 * Check the type name against the known type aliases.
		 * Probably want a better solution for this.
		 */
		private String unalias(String type, Map<String, String> aliasMap)
		{
			CTypeParser parser = new CTypeParser(type);

			String result = parser.getCoreType();

			/* Unalias the type */
			if(aliasMap.containsKey(result)) {
				result = aliasMap.get(result);
			}

			return parser.getPrefix() + result + parser.getSuffix();
		}

		public String getName() {
			return name;
		}

		public String getDeclaredName() {
			return declaredName;
		}

		public String getType() {
			return type;
		}

		public String getDeclaredType() {
			return declaredType;
		}

		public int getOffset() {
			return offset;
		}

		public String toString() {
			return type + " " + name + " Offset: " + offset;
		}

		public int compareTo(FieldDescriptor o) {
			return getName().compareTo(o.getName());
		}

		public static Collection<FieldDescriptor> inflate(String line) {
			String[] parts = line.split("\\|");
			if (parts.length < 5 || ((parts.length - 3) % 2) != 0) {
				throw new IllegalArgumentException(String.format("Superset file line is invalid: %s", line));
			}

			int count = (parts.length - 3) / 2;
			Collection<FieldDescriptor> result = new ArrayList<FieldDescriptor>(count);

			final String declaredName = parts[2];
			for (int i = 0; i < count; i++) {
				String fieldName = parts[1];
				if (i >0) {
					fieldName = fieldName + "_v" + i;
				}

				FieldDescriptor fd = new FieldDescriptor(0, parts[3 + i * 2], parts[4 + i * 2], fieldName, declaredName);
				result.add(fd);
			}
			return result;
		}

		public String deflate() {
			StringBuffer result = new StringBuffer();
			result.append("F|");				// 0
			result.append(getName());			// 1
			result.append("|");
			result.append(getDeclaredName());			// 2
			result.append("|");
			result.append(StructureReader.simplifyType(getType()));			// 3
			result.append("|");
			result.append(StructureReader.simplifyType(getDeclaredType()));	// 4
			return result.toString();
		}

		@Override
		public boolean equals(Object obj) {
			if((obj == null) || !(obj instanceof FieldDescriptor)) {
				return false;
			}
			FieldDescriptor compareTo = (FieldDescriptor) obj;
			return compareTo.deflate().equals(deflate());
		}

		@Override
		public int hashCode() {
			return name.hashCode();
		}


	}

	public byte[] getClassBytes(String binaryName) throws ClassNotFoundException {
		//parse the binary format name (i.e. it is in dot notation e.g. java.lang.String)
		int pos = binaryName.lastIndexOf('.');		
		String clazzName = binaryName.substring(pos + 1);
		
		// The className we are trying to load is FooOffsets.
		// The structure name stored in the reader is Foo
		String fullClassName = getPackageName(PackageNameType.STRUCTURE_PACKAGE_SLASH_NAME) + clazzName;
		StructureDescriptor structure = structures.get(clazzName);
		if (structure == null) {
			throw new ClassNotFoundException(String.format("%s is not in core file.", clazzName));
		}

		return BytecodeGenerator.getClassBytes(structure, fullClassName);
	}

	// TODO: Make this more efficient.  Probably change representation of fields and constants in Structure
	public long getConstantValue(String structureName, String constantName, long defaultValue) {
		if (constantName.equals("SIZEOF")) {
			return getStructureSizeOf(structureName);
		}

		for (ConstantDescriptor constant : getConstants(structureName)) {
			if (constant.getName().equals(constantName)) {
				return constant.getValue();
			}
		}
		return defaultValue;
	}

	public boolean getBuildFlagValue(String structureName, String constantName, boolean defaultValue) {
		long defaultLongValue = 0;
		if (defaultValue) {
			defaultLongValue = 1;
		}

		long value = getConstantValue(structureName, constantName, defaultLongValue);
		return value != 0;
	}

	public byte getSizeOfBool() {
		return header.getSizeofBool();
	}

	public byte getSizeOfUDATA() {
		return header.getSizeofUDATA();
	}

	public byte getBitFieldFormat() {
		return header.getBitfieldFormat();
	}


	public static String simplifyType(String type)
	{
		String working = type;

		/* Strip out the contents of array declarations */
		working = filterOutPattern(working, CONTENTS_OF_ARRAY_PATTERN);
		working = filterOutPattern(working, SPACES_BEFORE_SQUARE_BRACKETS_PATTERN);
		working = filterOutPattern(working, SPACES_AFTER_SQUARE_BRACKETS_PATTERN);
		working = filterOutPattern(working, SPACES_BEFORE_ASTERISKS_PATTERN);
		working = filterOutPattern(working, SPACES_AFTER_ASTERISKS_PATTERN);

		return working;
	}

	private static String filterOutPattern(String input, Pattern p)
	{
		Matcher m = p.matcher(input);

		if (m.find()) {
			return m.replaceAll("");
		}

		return input;
	}
}
