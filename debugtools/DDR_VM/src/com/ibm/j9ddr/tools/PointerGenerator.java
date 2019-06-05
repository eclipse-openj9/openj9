/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.tools;

import static com.ibm.j9ddr.StructureTypeManager.TYPE_ARRAY;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_BITFIELD;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_BOOL;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_DOUBLE;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_ENUM;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_ENUM_POINTER;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_FJ9OBJECT;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_FJ9OBJECT_POINTER;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_FLOAT;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_J9OBJECTCLASS;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_J9OBJECTCLASS_POINTER;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_J9OBJECTMONITOR;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_J9OBJECTMONITOR_POINTER;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_J9SRP;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_J9SRP_POINTER;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_J9WSRP;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_J9WSRP_POINTER;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_POINTER;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_SIMPLE_MAX;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_SIMPLE_MIN;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_STRUCTURE;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_STRUCTURE_POINTER;
import static com.ibm.j9ddr.StructureTypeManager.TYPE_VOID;
import static com.ibm.j9ddr.StructureTypeManager.simpleTypeAccessorMap;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.j9ddr.CTypeParser;
import com.ibm.j9ddr.StructureReader;
import com.ibm.j9ddr.StructureReader.ConstantDescriptor;
import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;
import com.ibm.j9ddr.StructureTypeManager;
import com.ibm.j9ddr.tools.store.J9DDRStructureStore;

/**
 * Utility to generate concrete implementation classes based on that
 * structure data found in a binary file of the same format as the new
 * core file offset data.
 */
public class PointerGenerator {

	private static final String BEGIN_USER_IMPORTS = "[BEGIN USER IMPORTS]";
	private static final String END_USER_IMPORTS = "[END USER IMPORTS]";

	private static final String BEGIN_USER_CODE = "[BEGIN USER CODE]";
	private static final String END_USER_CODE = "[END USER CODE]";

	private final Map<String, String> opts = new HashMap<String, String>();
	StructureReader structureReader;
	File outputDir;
	File outputDirHelpers;
	private boolean cacheClass = false;
	private boolean cacheFields = false;
	private Properties cacheProperties = null;

	private StructureTypeManager typeManager;

	public PointerGenerator() {
		super();
		opts.put("-p", null);
		opts.put("-o", null);
		opts.put("-f", null);
		opts.put("-v", null);
		opts.put("-s", null);  		// superset filename
		opts.put("-h", null);		// helper class location - optional
		opts.put("-u", "true");		// flag to control if user code is supported or not, default is true
		opts.put("-c", "");			// optional value to provide a cache properties file
		opts.put("-l", "false");	// flag to determine if legacy DDR is used, default is false
	}

	public static void main(String[] args) throws Exception {
		PointerGenerator app = new PointerGenerator();
		app.parseArgs(args);
		app.generateClasses();
		if (app.errorCount == 0) {
			System.out.println("Processing complete");
		} else {
			System.out.println("Processing failed");
			System.exit(1);
		}
	}

	private void generateClasses() {
		String fileName = opts.get("-f");
		String supersetFileName = opts.get("-s");

		try {
			J9DDRStructureStore store = new J9DDRStructureStore(fileName, supersetFileName);
			System.out.println("superset directory name : " + fileName);
			System.out.println("superset file name : " + store.getSuperSetFileName());
			InputStream inputStream = store.getSuperset();
			structureReader = new StructureReader(inputStream);
			inputStream.close();
		} catch (IOException e) {
			errorCount += 1;
			System.out.println("Problem with file: " + fileName);
			e.printStackTrace();
			return;
		}

		outputDir = getOutputDir("-p");
		if (opts.get("-h") != null) {
			// where to write the helpers to if the option is set
			outputDirHelpers = getOutputDir("-h");
		}

		typeManager = new StructureTypeManager(structureReader.getStructures());

		for (StructureDescriptor structure : structureReader.getStructures()) {
			try {
				if (FlagStructureList.isFlagsStructure(structure.getName())) {
					generateBuildFlags(structure);
				} else {
					generateClass(structure);
				}
			} catch (FileNotFoundException e) {
				errorCount += 1;
				String error = String.format("File not found processing: %s: %s", structure.getPointerName(), e.getMessage());
				System.out.println(error);
			} catch (IOException e) {
				errorCount += 1;
				String error = String.format("IOException processing: %s: %s", structure.getPointerName(), e.getMessage());
				System.out.println(error);
			}
		}
	}

	private void generateBuildFlags(StructureDescriptor structure) throws IOException {
		File javaFile = new File(outputDir, structure.getName() + ".java");
		List<String> userImports = new ArrayList<String>();
		List<String> userCode = new ArrayList<String>();
		collectMergeData(javaFile, userImports, userCode, structure);

		byte[] original = null;
		int length = 0;
		if (javaFile.exists()) {
			length = (int) javaFile.length();
			original = new byte[length];
			FileInputStream fis = new FileInputStream(javaFile);
			fis.read(original);
			fis.close();
		}

		ByteArrayOutputStream newContents = new ByteArrayOutputStream(length);
		PrintWriter writer = new PrintWriter(newContents);
		writeCopyright(writer);
		writer.format("package %s;%n", opts.get("-p"));
		writeBuildFlagImports(writer);
		writer.println();
		writeClassComment(writer, structure.getName());
		writer.format("public final class %s {%n", structure.getName());
		writer.println();
		writer.println("\t// Do not instantiate constant classes");
		writer.format("\tprivate %s() {%n", structure.getName());
		writer.format("\t}%n");
		writer.println();
		writeBuildFlags(writer, structure);
		writer.println();
		writeBuildFlagsStaticInitializer(writer, structure);
		writer.println("}");
		writer.close();

		byte[] newContentsBytes = newContents.toByteArray();
		if (null == original || !Arrays.equals(original, newContentsBytes)) {
			FileOutputStream fos = new FileOutputStream(javaFile);
			fos.write(newContentsBytes);
			fos.close();
		}
	}

	private void writeBuildFlagsStaticInitializer(PrintWriter writer, StructureDescriptor structure) {
		Collections.sort(structure.getConstants());

		writer.println("\tstatic {");
		writer.println("\t\tHashMap<String, Boolean> defaultValues = new HashMap<String, Boolean>();");
		writer.println();
		writer.println("\t\t// Edit default values here");

		for (ConstantDescriptor constant : structure.getConstants()) {
			writer.format("\t\tdefaultValues.put(\"%s\", Boolean.FALSE);%n", constant.getName());
		}

		writer.println();
		writer.println("\t\ttry {");
		writer.println("\t\t\tClassLoader loader = " + structure.getName() + ".class.getClassLoader();");
		writer.println("\t\t\tif (!(loader instanceof com.ibm.j9ddr.J9DDRClassLoader)) {");
		writer.println("\t\t\t\tthrow new IllegalArgumentException(\"Cannot determine the runtime loader\");");
		writer.println("\t\t\t}");
		writer.println("\t\t\tClass<?> runtimeClass = ((com.ibm.j9ddr.J9DDRClassLoader) loader).loadClassRelativeToStream(\"structure." + structure.getName() + "\", false);");
		writer.println("\t\t\tField[] fields = runtimeClass.getFields();");
		writer.println("\t\t\tfor (int i = 0; i < fields.length; i++) {");
		writer.println("\t\t\t\tField field = fields[i];");
		writer.println("\t\t\t\t// Overwrite default value with real value if it exists.");
		writer.println("\t\t\t\tdefaultValues.put(field.getName(), field.getLong(runtimeClass) != 0);");
		writer.println("\t\t\t}");
		writer.println("\t\t} catch (ClassNotFoundException e) {");
		writer.println("\t\t\tthrow new IllegalArgumentException(String.format(\"Can not initialize flags from core file.%n%s\", e.getMessage()));");
		writer.println("\t\t} catch (IllegalAccessException e) {");
		writer.println("\t\t\tthrow new IllegalArgumentException(String.format(\"Can not initialize flags from core file.%n%s\", e.getMessage()));");
		writer.println("\t\t}");
		writer.println();

		for (ConstantDescriptor constant : structure.getConstants()) {
			writer.format("\t\t%s = defaultValues.get(\"%s\");%n", constant.getName(), constant.getName());
		}
		writer.println("\t}");
	}

	private void generateClass(StructureDescriptor structure) throws IOException {
		File javaFile = new File(outputDir, structure.getPointerName() + ".java");
		List<String> userImports = new ArrayList<String>();
		List<String> userCode = new ArrayList<String>();
		if (opts.get("-u").equals("true")) {
			// if user code is supported then preserve it between generations
			collectMergeData(javaFile, userImports, userCode, structure);
		} else {
			// caching can be set from an optional properties file
			setCacheStatusFromPropertyFile(structure);
		}

		if (cacheClass || cacheFields) {
			System.out.println("Caching enabled for " + structure.getName() + "=" + cacheClass + "," + cacheFields);
		}

		int length = 0;
		byte[] original = null;
		if (javaFile.exists()) {
			length = (int) javaFile.length();
			original = new byte[length];
			FileInputStream fis = new FileInputStream(javaFile);
			fis.read(original);
			fis.close();
		}

		ByteArrayOutputStream newContents = new ByteArrayOutputStream(length);
		PrintWriter writer = new PrintWriter(newContents);
		writeCopyright(writer);
		writer.println();
		writeGeneratedWarning(writer);
		writer.format("package %s;%n", opts.get("-p"));
		writer.println();
		if (opts.get("-u").equals("true")) {
			writerUserData(BEGIN_USER_IMPORTS, END_USER_IMPORTS, userImports, writer);
		}
		writer.println();
		writeImports(writer, structure);
		writer.println();
		writeClassComment(writer, structure.getPointerName());
		writer.format("@com.ibm.j9ddr.GeneratedPointerClass(structureClass=%s.class)", structure.getName());
		writer.println();

		String superName = structure.getSuperName();
		if (superName.isEmpty()) {
			superName = "Structure";
		}
		writer.format("public class %s extends %sPointer {%n", structure.getPointerName(), superName);
		writer.println();
		writer.println("\t// NULL");
		writer.format("\tpublic static final %s NULL = new %s(0);%n", structure.getPointerName(), structure.getPointerName());
		writer.println();
		if (cacheClass) {
			writer.println("\t// Class Cache");
			if (opts.get("-u").equals("false")) {
				writer.println("\tprivate static final boolean CACHE_CLASS = true;");
			}
			writer.format("\tprivate static HashMap<Long, %s> CLASS_CACHE = new HashMap<Long, %s>();%n", structure.getPointerName(), structure.getPointerName());
			writer.println();
		}
		if (cacheFields) {
			if (opts.get("-u").equals("false")) {
				writer.println("\tprivate static final boolean CACHE_FIELDS = true;");
			}
		}
		if (opts.get("-u").equals("true")) {
			writerUserData(BEGIN_USER_CODE, END_USER_CODE, userCode, writer);
		}
		writer.println();
		writeConstructor(writer, structure);
		if (cacheClass) {
			writer.println("\t// Caching support methods");
			writer.println();
			generateCacheSupportMethods(writer, structure);
		}
		writer.println("\t// Implementation methods");
		writer.println();
		generateImplementationMethods(writer, structure);
		writer.println("}");
		writer.close();

		byte[] newContentsBytes = newContents.toByteArray();
		if (null == original || !Arrays.equals(original, newContentsBytes)) {
			FileOutputStream fos = new FileOutputStream(javaFile);
			fos.write(newContentsBytes);
			fos.close();
		}

		if ((outputDirHelpers != null) && (userCode.size() > 4)) {
			File helperFile = new File(outputDirHelpers, structure.getPointerName() + ".java");
			PrintWriter helperWriter = new PrintWriter(helperFile);
			for (String line : userImports) {
				helperWriter.println(line);
			}
			for (String line : userCode) {
				helperWriter.println(line);
			}
			helperWriter.close();
		}
	}

	/**
	 * Sets the class and field cache flags from the optional properties file
	 * specified at startup by the
	 * @param structure
	 */
	private void setCacheStatusFromPropertyFile(StructureDescriptor structure) {
		String opt = opts.get("-c");
		if ((opt == null) || (opt.length() == 0)) {
			// no caching is set so set the flags to false
			cacheClass = false;
			cacheFields = false;
		} else {
			if (cacheProperties == null) {
				// haven't loaded the properties yet
				cacheProperties = new Properties();
				File file = new File(opt);
				if (file.exists()) {
					try {
						FileInputStream in = new FileInputStream(file);
						cacheProperties.load(in);
					} catch (Exception e) {
						String msg = String.format("The cache properties file [%s] specified by the -c option could not be read", file.getAbsolutePath());
						throw new IllegalArgumentException(msg, e);
					}
				} else {
					String msg = String.format("The cache properties file [%s] specified by the -c option does not exist", file.getAbsolutePath());
					throw new IllegalArgumentException(msg);
				}
			}
			// get the caching flags for this structure, defaults to no caching if the structure name is not found
			String values = cacheProperties.getProperty(structure.getName(), "false,false");
			String[] parts = values.split(",");
			if (parts.length != 2) {
				String msg = String.format("The cache properties file [%s] contains an invalid setting for the key [%s]." +
						" The value should be in the format <boolean>,<boolean>", opt);
				throw new IllegalArgumentException(msg);
			}
			cacheClass = Boolean.parseBoolean(parts[0]);
			cacheFields = Boolean.parseBoolean(parts[1]);
		}
	}

	private void generateCacheSupportMethods(PrintWriter writer, StructureDescriptor structure) {
		if (cacheClass) {
			writer.format("\tprivate static void setCache(Long address, %s clazz) {%n", structure.getPointerName());
			writer.format("\t\tCLASS_CACHE.put(address, clazz);%n");
			writer.format("\t}%n");
			writer.println();
			writer.format("\tprivate static %s checkCache(Long address) {%n", structure.getPointerName());
			writer.format("\t\treturn CLASS_CACHE.get(address);%n");
			writer.format("\t}%n");
			writer.println();
		}
	}

	private void writeBuildFlags(PrintWriter writer, StructureDescriptor structure) {
		writer.println("\t// Build Flags");
		Collections.sort(structure.getConstants());
		for (ConstantDescriptor constant : structure.getConstants()) {
			writer.format("\tpublic static final boolean %s;%n", constant.getName());
		}
	}

	private void collectMergeData(File javaFile, List<String> userImports, List<String> userCode, StructureDescriptor structure) throws IOException {
		if (javaFile.exists()) {
			BufferedReader reader = new BufferedReader(new FileReader(javaFile));
			String aLine;

			while ((aLine = reader.readLine()) != null) {
				if (aLine.contains(BEGIN_USER_IMPORTS)) {
					collectUserData(userImports, reader, END_USER_IMPORTS);
				} else if (aLine.contains(BEGIN_USER_CODE)) {
					collectUserData(userCode, reader, END_USER_CODE);
				}
			}
			reader.close();
		}

		cacheClass = false;
		cacheFields = false;

		// Add cache variables the 1st time only.

		for (String line : userCode) {
			int pos = 0;

			if ((pos = line.indexOf("private static final boolean CACHE_FIELDS")) != -1) {
				cacheFields |= getFlagValue(line, pos);
			}

			if ((pos = line.indexOf("private static final boolean CACHE_CLASS")) != -1) {
				cacheClass |= getFlagValue(line, pos);
			}
		}
	}

	// gets the value of a cache flag by parsing the boolean at the end of the line
	private static boolean getFlagValue(String line, int start) {
		int equals = line.indexOf("=", start);
		if (equals != -1) {
			int semicolon = line.indexOf(";", equals);
			if (semicolon != -1) {
				String value = line.substring(equals + 1, semicolon).trim();
				return Boolean.parseBoolean(value);
			}
		}
		return false;
	}

	private static void collectUserData(List<String> userData, BufferedReader reader, String endTag)
			throws IOException {
		String aLine;
		while ((aLine = reader.readLine()) != null) {
			if (aLine.contains(endTag)) {
				break;
			} else {
				userData.add(aLine);
			}
		}
	}

	private static void writerUserData(String beginTag, String endTag, List<String> data, PrintWriter writer) {
		writer.print("/*");
		writer.print(beginTag);
		writer.println("*/");
		for (String line : data) {
			writer.println(line);
		}
		writer.print("/*");
		writer.print(endTag);
		writer.println("*/");
	}

	private void writeGeneratedWarning(PrintWriter writer) {
		writer.println("/**");
		writer.println(" * WARNING!!! GENERATED FILE");
		writer.println(" *");
		writer.println(" * This class is generated.");
		writer.println(" * Do not use the Eclipse \"Organize Imports\" feature on this class.");

		if (opts.get("-u").equals("true")) {
			writer.println(" *");
			writer.println(" * It can contain user content, but that content must be delimited with the");
			writer.println(" * the tags");
			writer.println(" * [BEGIN USER IMPORTS]");
			writer.println(" * [END USER IMPORTS]");
			writer.println(" *");
			writer.println(" * or");
			writer.println(" *");
			writer.println(" * [BEGIN USER CODE]");
			writer.println(" * [END USER CODE]");
			writer.println(" *");
			writer.println(" * These tags are entered as comments.  Characters before [ and after ] are ignored.");
			writer.println(" * Lines between the tags are inserted into the newly generated file.");
			writer.println(" *");
			writer.println(" * IMPORTS are combined and inserted above newly generated imports.  CODE is combined");
			writer.println(" * and inserted immediately after the class declaration");
			writer.println(" *");
			writer.println(" * All lines outside these tags are lost and replaced with newly generated code.");
		}
		writer.println(" */");
	}

	private void writeClassComment(PrintWriter writer, String name) {
		writer.println("/**");
		writer.format(" * Structure: %s%n", name);
		writer.println(" *");
		writer.println(" * A generated implementation of a VM structure");
		writer.println(" *");

		if (opts.get("-u").equals("true")) {
			writer.println(" * This class contains generated code and MAY contain hand written user code.");
			writer.println(" *");
			writer.println(" * Hand written user code must be contained at the top of");
			writer.println(" * the class file, specifically above");
			writer.println(" * the comment line containing WARNING!!! GENERATED CODE");
			writer.println(" *");
			writer.println(" * ALL code below the GENERATED warning will be replaced with new generated code");
			writer.println(" * each time the PointerGenerator utility is run.");
			writer.println(" *");
			writer.format(" * The generated code will provide getters for all elements in the %s%n", name);
			writer.println(" * structure.  Where possible, meaningful return types are inferred.");
			writer.println(" *");
			writer.println(" * The user may add methods to provide meaningful return types where only pointers");
			writer.println(" * could be automatically inferred.");
		} else {
			writer.println(" * Do not place hand written user code in this class as it will be overwritten.");
		}
		writer.println(" */");
	}

	private void generateImplementationMethods(PrintWriter writer, StructureDescriptor structure) {
		Collections.sort(structure.getFields());
		for (FieldDescriptor fieldDescriptor : structure.getFields()) {
			if (omitFieldImplementation(structure, fieldDescriptor)) {
				continue;
			}

			String typeName = fieldDescriptor.getType();
			int type = typeManager.getType(typeName);
			switch (type) {
			case TYPE_STRUCTURE:
				writeStructureMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_STRUCTURE_POINTER:
				writeStructurePointerMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_POINTER:
				writePointerMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_ARRAY:
				writeArrayMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_J9SRP:
				writeSRPMethod(writer, structure, fieldDescriptor, false);
				break;
			case TYPE_J9WSRP:
				writeSRPMethod(writer, structure, fieldDescriptor, true);
				break;
			case TYPE_J9SRP_POINTER:
				writeSRPPointerMethod(writer, structure, fieldDescriptor, false);
				break;
			case TYPE_J9WSRP_POINTER:
				writeSRPPointerMethod(writer, structure, fieldDescriptor, true);
				break;
			case TYPE_FJ9OBJECT:
				writeFJ9ObjectMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_FJ9OBJECT_POINTER:
				writeFJ9ObjectPointerMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_J9OBJECTCLASS:
				writeJ9ObjectClassMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_J9OBJECTCLASS_POINTER:
				writeJ9ObjectClassPointerMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_J9OBJECTMONITOR:
				writeJ9ObjectMonitorMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_J9OBJECTMONITOR_POINTER:
				writeJ9ObjectMonitorPointerMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_VOID:
				writeStructureMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_BOOL:
				writeBoolMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_DOUBLE:
				writeDoubleMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_FLOAT:
				writeFloatMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_ENUM:
				writeEnumMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_ENUM_POINTER:
				writeEnumPointerMethod(writer, structure, fieldDescriptor);
				break;
			case TYPE_BITFIELD:
				int colonIndex = typeName.indexOf(':');
				if (colonIndex == -1) {
					throw new IllegalArgumentException(String.format("%s is not a bitfield", fieldDescriptor));
				}
				writeBitFieldMethod(writer, structure, fieldDescriptor, type, fieldDescriptor.getName());
				break;
			default:
				if ((TYPE_SIMPLE_MIN <= type) && (type <= TYPE_SIMPLE_MAX)) {
					writeSimpleTypeMethod(writer, structure, fieldDescriptor, type);
				} else {
					String error = String.format("Unhandled structure type: %s->%s %s", structure.getPointerName(), fieldDescriptor.getName(), typeName);
					System.out.println(error);
				}
				break;
			}
		}
	}

	private void writeSRPPointerMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor, boolean wide) {
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		String pointerType = wide ? "WideSelfRelativePointer" : "SelfRelativePointer";
		if (cacheFields) {
			writer.format("\tprivate %s %s_cache;%n", pointerType, getter);
		}
		writeMethodSignature(writer, pointerType, getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\t%s_cache = %s.cast(getPointerAtOffset(%s._%sOffset_));%n", getter, pointerType, structure.getName(), offsetConstant);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn %s.cast(getPointerAtOffset(%s._%sOffset_));%n", pointerType, structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		writeEAMethod(writer, "PointerPointer", structure, fieldDescriptor);
	}

	private void writeBitFieldMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor, int type, String getter) {
		CTypeParser parser = new CTypeParser(fieldDescriptor.getType());
		String typeString = parser.getCoreType();

		if (getter.length() == 0) {
			writer.format("\t// %s %s%n", fieldDescriptor.getDeclaredType(), fieldDescriptor.getName());
			writer.println();
			return;
		}

		if (cacheFields) {
			writer.format("\tprivate %s %s_cache;%n", typeString, getter);
		}
		writeMethodSignature(writer, typeString, getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\t%s_cache = get%sBitfield(%s._%s_s_, %s._%s_b_);%n", getter, typeString, structure.getName(), getter, structure.getName(), getter);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn get%sBitfield(%s._%s_s_, %s._%s_b_);%n", typeString, structure.getName(), getter, structure.getName(), getter);
		if (cacheFields) {
			writer.println("\t\t}");
		}
		writeMethodClose(writer);
	}

	/**
	 * Quick hack to remove specific generated field implementations because
	 * they will interfere with changes to the implementation made my the user.
	 *
	 * You only have to add methods that would be generated but need to be altered
	 * by the user to contain a different return type.
	 *
	 * Also removes anonymous fields generated on AIX which start with illegal
	 * characters (#)
	 *
	 * There is likely a MUCH better way to do this.
	 *
	 * @param structure
	 * @param fieldDescriptor
	 * @return boolean indicating if the field should be omitted
	 */
	static boolean omitFieldImplementation(StructureDescriptor structure, FieldDescriptor field) {
		String name = structure.getPointerName() + "." + field.getName();
		return name.contains("#");
	}

	private void writeMethodSignature(PrintWriter writer, String returnType, String getter, FieldDescriptor field, boolean fieldAccessor) {
		writer.format("\t// %s %s%n", field.getDeclaredType(), field.getDeclaredName());
		if (fieldAccessor) {
			writer.format("\t@com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName=\"_%sOffset_\", declaredType=\"%s\")", getOffsetConstant(field), field.getDeclaredType());
			writer.println();
		}
		writer.format("\tpublic %s %s() throws CorruptDataException {%n", returnType, getter);
	}

	private void writeEAMethod(PrintWriter writer, String returnType, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		if (cacheFields) {
			writer.format("\tprivate %s %sEA_cache;%n", returnType, getter);
		}
		writeMethodSignature(writer, returnType, getter + "EA", fieldDescriptor, false);
		writeZeroCheck(writer);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%sEA_cache == null) {%n", getter);
			writer.format("\t\t\t\t%sEA_cache = %s.cast(address + %s._%sOffset_);%n", getter, returnType, structure.getName(), offsetConstant);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %sEA_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn %s.cast(address + %s._%sOffset_);%n", returnType, structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);
	}

	private void writeZeroCheck(PrintWriter writer) {
		writer.format("\t\tif (address == 0) {%n");
		writer.format("\t\t\tthrow new NullPointerDereference();%n");
		writer.format("\t\t}%n");
		writer.println();
	}

	private void writeEnumEAMethod(PrintWriter writer, String returnType, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		String enumType = getEnumType(fieldDescriptor.getType());

		if (cacheFields) {
			writer.format("\tprivate %s %sEA_cache;%n", returnType, getter);
		}
		writeMethodSignature(writer, returnType, getter + "EA", fieldDescriptor, false);
		writeZeroCheck(writer);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%sEA_cache == null) {%n", getter);
			writer.format("\t\t\t\t%sEA_cache = %s.cast(address + %s._%sOffset_, %s.class);%n", getter, returnType, structure.getName(), offsetConstant, enumType);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %sEA_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn %s.cast(address + %s._%sOffset_, %s.class);%n", returnType, structure.getName(), offsetConstant, enumType);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);
	}

	private String getOffsetConstant(FieldDescriptor fieldDescriptor) {
		String fieldName = fieldDescriptor.getName();
		if (opts.get("-l").equals("true")) {
			return getOffsetConstant(fieldName);
		}
		return fieldName;
	}

	private final static Pattern offsetPattern = Pattern.compile("^(.+)_v\\d+$");

	public static String getOffsetConstant(String fieldName) {
		Matcher matcher = offsetPattern.matcher(fieldName);
		if (matcher.matches()) {
			return matcher.group(1);
		}
		return fieldName;
	}

	private void writeSRPEAMethod(PrintWriter writer, String returnType, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		if (cacheFields) {
			writer.format("\tprivate %s %sEA_cache;%n", returnType, getter);
		}
		writeMethodSignature(writer, returnType, getter + "EA", fieldDescriptor, false);
		writeZeroCheck(writer);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%sEA_cache == null) {%n", getter);
			writer.format("\t\t\t\t%sEA_cache = %s.cast(address + (%s._%sOffset_ + getIntAtOffset(%s._%sOffset_)));%n", getter, returnType, structure.getName(), offsetConstant, structure.getName(), offsetConstant);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %sEA_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn %s.cast(address + %s._%sOffset_);%n", returnType, structure.getName(), offsetConstant, structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);
	}

	private void writeMethodClose(PrintWriter writer) {
		writer.println("\t}");
		writer.println();
	}

	private void writePointerMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		String pointerType = getPointerType(fieldDescriptor.getType());
		String returnType = generalizeSimplePointer(pointerType);

		if (cacheFields) {
			writer.format("\tprivate %s %s_cache;%n", pointerType, getter);
		}
		writeMethodSignature(writer, returnType, getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\t%s_cache = %s.cast(getPointerAtOffset(%s._%sOffset_));%n", getter, pointerType, structure.getName(), offsetConstant);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn %s.cast(getPointerAtOffset(%s._%sOffset_));%n", pointerType, structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		writeEAMethod(writer, "PointerPointer", structure, fieldDescriptor);
	}

	private static String getPointerType(String pointerType) {
		String type = ConstPattern.matcher(pointerType).replaceAll("").trim();
		int firstStar = type.indexOf('*');

		if (type.indexOf('*', firstStar + 1) > 0) {
			// two or more levels of indirection
			return "PointerPointer";
		}

		type = type.substring(0, firstStar).trim();

		if (type.equals("bool")) {
			return "BoolPointer";
		} else if (type.equals("double")) {
			return "DoublePointer";
		} else if (type.equals("float")) {
			return "FloatPointer";
		} else if (type.equals("void")) {
			return "VoidPointer";
		} else {
			return type + "Pointer";
		}
	}

	private int errorCount = 0;

	private void writeArrayMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		try {
			String returnType = getArrayType(fieldDescriptor.getType());
			if (returnType.equals("EnumPointer")) {
				writeEnumEAMethod(writer, returnType, structure, fieldDescriptor);
			} else {
				writeEAMethod(writer, returnType, structure, fieldDescriptor);
			}
		} catch (RuntimeException e) {
			// If there are fewer than 100 errors, print a stacktrace (instead
			// of terminating the program) to allow more than one problem to be
			// fixed per iteration.
			errorCount += 1;
			if (errorCount < 100) {
				e.printStackTrace();
			} else {
				throw e;
			}
		}
	}

	private static String getEnumType(String enumDeclaration) {
		int start = 0;
		int end = enumDeclaration.length();

		if (enumDeclaration.startsWith("enum ")) {
			start = "enum ".length();
		}

		if (enumDeclaration.endsWith("[]")) {
			end = end - 2;
		}

		return enumDeclaration.substring(start, end);
	}

	private String getArrayType(String arrayDeclaration) {
		String componentType = arrayDeclaration.substring(0, arrayDeclaration.lastIndexOf('[')).trim();
		int arrayType = typeManager.getType(componentType);

		switch (arrayType) {
		case TYPE_BOOL:
			return "BoolPointer";

		case TYPE_ENUM:
			return "EnumPointer";

		case TYPE_DOUBLE:
			return "DoublePointer";

		case TYPE_FLOAT:
			return "FloatPointer";

		case TYPE_BITFIELD:
			// Is a bitfield array even legal?
			break;

		case TYPE_POINTER:
		case TYPE_ARRAY:
		case TYPE_STRUCTURE_POINTER:
		case TYPE_FJ9OBJECT_POINTER:
		case TYPE_J9OBJECTCLASS_POINTER:
		case TYPE_J9OBJECTMONITOR_POINTER:
			// All you pointer types look the same to me
			return "PointerPointer";

		case TYPE_J9SRP:
		case TYPE_J9WSRP:
			// Not implemented
			break;

		case TYPE_STRUCTURE:
			return removeTypeTags(componentType) + "Pointer";

		case TYPE_FJ9OBJECT:
			return "ObjectReferencePointer";

		case TYPE_J9OBJECTCLASS:
			return "ObjectClassReferencePointer";

		case TYPE_J9OBJECTMONITOR:
			return "ObjectMonitorReferencePointer";

		default:
			if ((TYPE_SIMPLE_MIN <= arrayType) && (arrayType <= TYPE_SIMPLE_MAX)) {
				return componentType + "Pointer";
			}
		}

		throw new IllegalArgumentException("Type is not a recognized array: " + arrayDeclaration);
	}

	private void writeBoolMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		if (cacheFields) {
			writer.format("\tprivate Boolean %s_cache;%n", getter);
		}
		writeMethodSignature(writer, "boolean", getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\t%s_cache = new Boolean(getBoolAtOffset(%s._%sOffset_));%n", getter, structure.getName(), offsetConstant);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache.booleanValue();%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn getBoolAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		/* Now write an EA method to return the address of the slot */
		writeEAMethod(writer, "BoolPointer", structure, fieldDescriptor);
	}

	private void writeDoubleMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		if (cacheFields) {
			writer.format("\tprivate Double %s_cache;%n", getter);
		}
		writeMethodSignature(writer, "double", getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\t%s_cache = new Double(getDoubleAtOffset(%s._%sOffset_));%n", getter, structure.getName(), offsetConstant);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache.doubleValue();%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn getDoubleAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		/* Now write an EA method to return the address of the slot */
		writeEAMethod(writer, "DoublePointer", structure, fieldDescriptor);
	}

	private void writeEnumMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		String enumType = getEnumType(fieldDescriptor.getType());

		if (cacheFields) {
			writer.format("\tprivate Long %s_cache;%n", getter);
		}
		writeMethodSignature(writer, "long", getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\tif (%s.SIZEOF == 1) {%n", enumType);
			writer.format("\t\t\t\t\t%s_cache = new Long(getByteAtOffset(%s._%sOffset_));%n", getter, structure.getName(), offsetConstant);
			writer.format("\t\t\t\t} else if (%s.SIZEOF == 2) {%n", enumType);
			writer.format("\t\t\t\t\t%s_cache = new Long(getShortAtOffset(%s._%sOffset_));%n", getter, structure.getName(), offsetConstant);
			writer.format("\t\t\t\t} else if (%s.SIZEOF == 4) {%n", enumType);
			writer.format("\t\t\t\t\t%s_cache = new Long(getIntAtOffset(%s._%sOffset_));%n", getter, structure.getName(), offsetConstant);
			writer.format("\t\t\t\t} else if (%s.SIZEOF == 8) {%n", enumType);
			writer.format("\t\t\t\t\t%s_cache = new Long(getLongAtOffset(%s._%sOffset_));%n", getter, structure.getName(), offsetConstant);
			writer.format("\t\t\t\t} else {%n");
			writer.format("\t\t\t\t\tthrow new IllegalArgumentException(\"Unexpected ENUM size in core file\");%n");
			writer.format("\t\t\t\t}%n");
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache.longValue();%n", getter);
			writer.format("\t\t} else {%n");
		}
		writer.format("\t\t\tif (%s.SIZEOF == 1) {%n", enumType);
		writer.format("\t\t\t\treturn getByteAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
		writer.format("\t\t\t} else if (%s.SIZEOF == 2) {%n", enumType);
		writer.format("\t\t\t\treturn getShortAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
		writer.format("\t\t\t} else if (%s.SIZEOF == 4) {%n", enumType);
		writer.format("\t\t\t\treturn getIntAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
		writer.format("\t\t\t} else if (%s.SIZEOF == 8) {%n", enumType);
		writer.format("\t\t\t\treturn getLongAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
		writer.format("\t\t\t} else {%n");
		writer.format("\t\t\t\tthrow new IllegalArgumentException(\"Unexpected ENUM size in core file\");%n");
		writer.format("\t\t\t}%n");
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		/* Now write an EA method to return the address of the slot */
		writeEnumEAMethod(writer, "EnumPointer", structure, fieldDescriptor);
	}

	private void writeEnumPointerMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		String pointerType = "EnumPointer";
		String type = fieldDescriptor.getType();
		String enumType = getEnumType(type.substring(0, type.indexOf('*')));

		if (cacheFields) {
			writer.format("\tprivate %s %s_cache;%n", pointerType, getter);
		}
		writeMethodSignature(writer, pointerType, getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\t%s_cache = %s.cast(getPointerAtOffset(%s._%sOffset_), %s.class);%n", getter, pointerType, structure.getName(), offsetConstant, enumType);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn %s.cast(getPointerAtOffset(%s._%sOffset_), %s.class);%n", pointerType, structure.getName(), offsetConstant, enumType);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		writeEAMethod(writer, "PointerPointer", structure, fieldDescriptor);
	}

	private void writeFloatMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		if (cacheFields) {
			writer.format("\tprivate Float %s_cache;%n", getter);
		}
		writeMethodSignature(writer, "float", getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\t%s_cache = new Float(getFloatAtOffset(%s._%sOffset_));%n", getter, structure.getName(), offsetConstant);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache.floatValue();%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn getFloatAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		/* Now write an EA method to return the address of the slot */
		writeEAMethod(writer, "FloatPointer", structure, fieldDescriptor);
	}

	/*
	 * The new DDR tooling doesn't always distinguish between IDATA and I32 or I64
	 * or between UDATA and U32 or U64. Thus the return types of accessor methods
	 * must be more general to be compatible with the pointer types derived from
	 * both 32-bit and 64-bit core files. This generalization must occur even for
	 * the pointer stubs generated from this code so that incompatibilities will
	 * be discovered at build time, rather than at run time.
	 */
	private static String generalizeSimpleType(String type) {
		if ("I32".equals(type) || "I64".equals(type)) {
			return "IDATA";
		} else if ("U32".equals(type) || "U64".equals(type)) {
			return "UDATA";
		} else {
			return type;
		}
	}

	/*
	 * Like generalizeSimpleType() above, but for pointer types.
	 */
	private static String generalizeSimplePointer(String type) {
		if ("I32Pointer".equals(type) || "I64Pointer".equals(type)) {
			return "IDATAPointer";
		} else if ("U32Pointer".equals(type) || "U64Pointer".equals(type)) {
			return "UDATAPointer";
		} else {
			return type;
		}
	}

	private void writeSimpleTypeMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor, int type) {
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		String typeString = fieldDescriptor.getType();
		String returnType = generalizeSimpleType(typeString);

		if (cacheFields) {
			writer.format("\tprivate %s %s_cache;%n", typeString, getter);
		}
		writeMethodSignature(writer, returnType, getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\t%s_cache = new %s(%s(%s._%sOffset_));%n", getter, typeString, simpleTypeAccessorMap.get(type), structure.getName(), offsetConstant);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn new %s(%s(%s._%sOffset_));%n", typeString, simpleTypeAccessorMap.get(type), structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		/* Now write an EA method to return the address of the slot */
		writeEAMethod(writer, returnType + "Pointer", structure, fieldDescriptor);
	}

	private static final Pattern ConstPattern = Pattern.compile("\\s*\\bconst\\s+");

	private static final Pattern TypeTagPattern = Pattern.compile("\\s*\\b(class|enum|struct)\\s+");

	private static String removeTypeTags(String type) {
		return TypeTagPattern.matcher(type).replaceAll("").trim();
	}

	private void writeSRPMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor, boolean isWide) {
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		String typeString = fieldDescriptor.getType();
		final String prefix = isWide ? "J9WSRP" : "J9SRP";
		final int prefixLength = prefix.length();
		final String getAtOffsetFunction = isWide ? "getPointerAtOffset" : "getIntAtOffset";
		String referencedTypeString = null;

		if (typeString.startsWith(prefix) && typeString.startsWith("(", prefixLength)) {
			referencedTypeString = typeString.substring(prefixLength + 1, typeString.length() - 1).trim();
		} else {
			referencedTypeString = "void";
		}

		int type = typeManager.getType(referencedTypeString);

		String pointerType = null;
		switch (type) {
		case TYPE_STRUCTURE:
			pointerType = removeTypeTags(referencedTypeString) + "Pointer";
			break;
		case TYPE_VOID:
			pointerType = "VoidPointer";
			break;
		case TYPE_J9SRP:
			pointerType = "SelfRelativePointer";
			break;
		case TYPE_J9WSRP:
			pointerType = "WideSelfRelativePointer";
			break;
		default:
			if ((TYPE_SIMPLE_MIN <= type) && (type <= TYPE_SIMPLE_MAX)) {
				pointerType = referencedTypeString + "Pointer";
			} else {
				throw new RuntimeException("Unexpected SRP reference type: " + type + " from " + typeString);
			}
			break;
		}

		if (cacheFields) {
			writer.format("\tprivate %s %s_cache;%n", pointerType, getter);
		}
		writeMethodSignature(writer, pointerType, getter, fieldDescriptor, true);
		writeZeroCheck(writer);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\tlong nextAddress = %s(%s._%sOffset_);%n", getAtOffsetFunction, structure.getName(), offsetConstant);
			writer.format("\t\t\t\tif (nextAddress == 0) {%n");
			writer.format("\t\t\t\t\t%s_cache = %s.NULL;%n", getter, pointerType);
			writer.format("\t\t\t\t} else {%n");
			writer.format("\t\t\t\t\t%s_cache = %s.cast(address + (%s._%sOffset_ + nextAddress));%n", getter, pointerType, structure.getName(), offsetConstant);
			writer.format("\t\t\t\t}%n");
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache;%n", getter);
			writer.format("\t\t}%n");
		}
		writer.format("\t\tlong nextAddress = %s(%s._%sOffset_);%n", getAtOffsetFunction, structure.getName(), offsetConstant);
		writer.format("\t\tif (nextAddress == 0) {%n");
		writer.format("\t\t\treturn %s.NULL;%n", pointerType);
		writer.format("\t\t}%n");
		writer.format("\t\treturn %s.cast(address + (%s._%sOffset_ + nextAddress));%n", pointerType, structure.getName(), offsetConstant);

		writeMethodClose(writer);

		writeSRPEAMethod(writer, isWide ? "WideSelfRelativePointer" : "SelfRelativePointer", structure, fieldDescriptor);
	}

	private void writeStructurePointerMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String type = fieldDescriptor.getType();
		String targetType = type.substring(0, type.indexOf('*'));
		String returnType = removeTypeTags(targetType) + "Pointer";
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		if (cacheFields) {
			writer.format("\tprivate %s %s_cache;%n", returnType, getter);
		}
		writeMethodSignature(writer, returnType, getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\tlong pointer = getPointerAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
			writer.format("\t\t\t\t%s_cache = %s.cast(pointer);%n", getter, returnType);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\tlong pointer = getPointerAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writer.format("\t\treturn %s.cast(pointer);%n", returnType);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		writeEAMethod(writer, "PointerPointer", structure, fieldDescriptor);
	}

	private void writeFJ9ObjectMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String returnType = "J9ObjectPointer";
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		if (cacheFields) {
			writer.format("\tprivate %s %s_cache;%n", returnType, getter);
		}
		writeMethodSignature(writer, returnType, getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\t%s_cache = getObjectReferenceAtOffset(%s._%sOffset_);%n", getter, structure.getName(), offsetConstant);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn getObjectReferenceAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		writeEAMethod(writer, "ObjectReferencePointer", structure, fieldDescriptor);
	}

	private void writeFJ9ObjectPointerMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String returnType = "ObjectReferencePointer";
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		if (cacheFields) {
			writer.format("\tprivate %s %s_cache;%n", returnType, getter);
		}
		writeMethodSignature(writer, returnType, getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\tlong pointer = getPointerAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
			writer.format("\t\t\t\t%s_cache = %s.cast(pointer);%n", getter, returnType);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\tlong pointer = getPointerAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
		writer.format("\t\t\treturn %s.cast(pointer);%n", returnType);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		writeEAMethod(writer, "PointerPointer", structure, fieldDescriptor);
	}

	private void writeJ9ObjectClassMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String returnType = "J9ClassPointer";
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		if (cacheFields) {
			writer.format("\tprivate %s %s_cache;%n", returnType, getter);
		}
		writeMethodSignature(writer, returnType, getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\t%s_cache = getObjectClassAtOffset(%s._%sOffset_);%n", getter, structure.getName(), offsetConstant);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn getObjectClassAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		writeEAMethod(writer, "ObjectClassReferencePointer", structure, fieldDescriptor);
	}

	private void writeJ9ObjectClassPointerMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String returnType = "ObjectClassReferencePointer";
		String getter = fieldDescriptor.getName();
		writeMethodSignature(writer, returnType, getter, fieldDescriptor, true);
		writer.format("\t\t// j9objectclass_t* method goes here%n");
		writer.format("\t\treturn null;%n");
		writeMethodClose(writer);

		writeEAMethod(writer, "PointerPointer", structure, fieldDescriptor);
	}

	private void writeJ9ObjectMonitorMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String returnType = "J9ObjectMonitorPointer";
		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		if (cacheFields) {
			writer.format("\tprivate %s %s_cache;%n", returnType, getter);
		}
		writeMethodSignature(writer, returnType, getter, fieldDescriptor, true);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\t%s_cache = getObjectMonitorAtOffset(%s._%sOffset_);%n", getter, structure.getName(), offsetConstant);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn getObjectMonitorAtOffset(%s._%sOffset_);%n", structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		writeEAMethod(writer, "ObjectMonitorReferencePointer", structure, fieldDescriptor);
	}

	private void writeJ9ObjectMonitorPointerMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String returnType = "ObjectMonitorReferencePointer";
		String getter = fieldDescriptor.getName();
		writeMethodSignature(writer, returnType, getter, fieldDescriptor, true);
		writer.format("\t\t// j9objectmonitor_t* method goes here%n");
		writer.format("\t\treturn null;%n");
		writeMethodClose(writer);

		writeEAMethod(writer, "PointerPointer", structure, fieldDescriptor);
	}

	private void writeStructureMethod(PrintWriter writer, StructureDescriptor structure, FieldDescriptor fieldDescriptor) {
		String type = fieldDescriptor.getType();
		String returnType;

		if (type.equals("void")) {
			returnType = "VoidPointer";
		} else {
			returnType = removeTypeTags(type) + "Pointer";
		}

		String getter = fieldDescriptor.getName();
		String offsetConstant = getOffsetConstant(fieldDescriptor);
		if (cacheFields) {
			writer.format("\tprivate %s %s_cache;%n", returnType, getter);
		}
		writeMethodSignature(writer, returnType, getter, fieldDescriptor, true);
		writeZeroCheck(writer);
		if (cacheFields) {
			writer.format("\t\tif (CACHE_FIELDS) {%n");
			writer.format("\t\t\tif (%s_cache == null) {%n", getter);
			writer.format("\t\t\t\t%s_cache = %s.cast(address + %s._%sOffset_);%n", getter, returnType, structure.getName(), offsetConstant);
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn %s_cache;%n", getter);
			writer.format("\t\t} else {%n\t");
		}
		writer.format("\t\treturn %s.cast(address + %s._%sOffset_);%n", returnType, structure.getName(), offsetConstant);
		if (cacheFields) {
			writer.format("\t\t}%n");
		}
		writeMethodClose(writer);

		writeEAMethod(writer, "PointerPointer", structure, fieldDescriptor);
	}

	private void writeConstructor(PrintWriter writer, StructureDescriptor structure) {
		String name = structure.getPointerName();
		String structureName = structure.getName();

		writer.println("\t// Do not call this constructor.  Use static method cast instead.");
		writer.format("\tprotected %s(long address) {%n", name);
		writer.println("\t\tsuper(address);");
		writer.println("\t}");
		writer.println();
		writer.format("\tpublic static %s cast(AbstractPointer structure) {%n", name);
		writer.format("\t\treturn cast(structure.getAddress());%n");
		writer.println("\t}");
		writer.println();

		writer.format("\tpublic static %s cast(UDATA udata) {%n", name);
		writer.format("\t\treturn cast(udata.longValue());%n");
		writer.println("\t}");
		writer.println();

		writer.format("\tpublic static %s cast(long address) {%n", name);
		writer.format("\t\tif (address == 0) {%n");
		writer.format("\t\t\treturn NULL;%n");
		writer.format("\t\t}%n");
		writer.println();
		if (cacheClass) {
			writer.format("\t\tif (CACHE_CLASS) {%n");
			writer.format("\t\t\t%s clazz = checkCache(address);%n", name);
			writer.format("\t\t\tif (null == clazz) {%n");
			writer.format("\t\t\t\tclazz = new %s(address);%n", name, name);
			writer.format("\t\t\t\tsetCache(address, clazz);%n");
			writer.format("\t\t\t}%n");
			writer.format("\t\t\treturn clazz;%n");
			writer.format("\t\t}%n");
		}
		writer.format("\t\treturn new %s(address);%n", name, name);
		writer.println("\t}");
		writer.println();

		writer.format("\tpublic %s add(long count) {%n", name);
		writer.format("\t\treturn %s.cast(address + (%s.SIZEOF * count));%n", name, structureName);
		writer.format("\t}%n");
		writer.println();

		writer.format("\tpublic %s add(Scalar count) {%n", name);
		writer.format("\t\treturn add(count.longValue());%n", name);
		writer.format("\t}%n");
		writer.println();

		writer.format("\tpublic %s addOffset(long offset) {%n", name);
		writer.format("\t\treturn %s.cast(address + offset);%n", name);
		writer.format("\t}%n");
		writer.println();

		writer.format("\tpublic %s addOffset(Scalar offset) {%n", name);
		writer.format("\t\treturn addOffset(offset.longValue());%n", name);
		writer.format("\t}%n");
		writer.println();

		writer.format("\tpublic %s sub(long count) {%n", name);
		writer.format("\t\treturn %s.cast(address - (%s.SIZEOF * count));%n", name, structureName);
		writer.format("\t}%n");
		writer.println();

		writer.format("\tpublic %s sub(Scalar count) {%n", name);
		writer.format("\t\treturn sub(count.longValue());%n", name);
		writer.format("\t}%n");
		writer.println();

		writer.format("\tpublic %s subOffset(long offset) {%n", name);
		writer.format("\t\treturn %s.cast(address - offset);%n", name);
		writer.format("\t}%n");
		writer.println();

		writer.format("\tpublic %s subOffset(Scalar offset) {%n", name);
		writer.format("\t\treturn subOffset(offset.longValue());%n", name);
		writer.format("\t}%n");
		writer.println();

		writer.format("\tpublic %s untag(long mask) {%n", name);
		writer.format("\t\treturn %s.cast(address & ~mask);%n", name);
		writer.format("\t}%n");
		writer.println();

		writer.format("\tpublic %s untag() {%n", name);
		writer.format("\t\treturn untag(UDATA.SIZEOF - 1);%n");
		writer.format("\t}%n");
		writer.println();

		writer.format("\tprotected long sizeOfBaseType() {%n");
		writer.format("\t\treturn %s.SIZEOF;%n",structureName);
		writer.format("\t}%n");
		writer.println();
	}

	private void writeImports(PrintWriter writer, StructureDescriptor structure) {
		if (structure.getFields().size() > 0) {
			writer.println("import com.ibm.j9ddr.CorruptDataException;");
			writer.println("import com.ibm.j9ddr.NullPointerDereference;");
		}
		String version = opts.get("-v");
		writer.println(String.format("import com.ibm.j9ddr.vm%s.pointer.*;", version));
		writer.println(String.format("import com.ibm.j9ddr.vm%s.structure.*;", version));
		writer.println(String.format("import com.ibm.j9ddr.vm%s.types.*;", version));
		if (cacheClass) {
			writer.println("import java.util.HashMap;");
		}
	}

	private void writeBuildFlagImports(PrintWriter writer) {
		writer.println("import java.util.HashMap;");
		writer.println("import java.lang.reflect.Field;");
	}

	private void parseArgs(String[] args) {
		if ((args.length == 0) || ((args.length % 2) != 0)) {
			printHelp();
			System.exit(1);
		}

		for (int i = 0; i < args.length; i += 2) {
			if (opts.containsKey(args[i])) {
				opts.put(args[i], args[i + 1]);
			} else {
				System.out.println("Invalid option : " + args[i]);
				printHelp();
				System.exit(1);
			}
		}

		for (String key : opts.keySet()) {
			String value = opts.get(key);
			if (value == null && !key.equals("-s") && !key.equals("-h")) {
				System.err.println("The option " + key + " has not been set.\n");
				printHelp();
				System.exit(1);
			}
		}
	}

	/**
	 * Print usage help to stdout
	 */
	private static void printHelp() {
		System.out.println("Usage :\n\njava PointerGenerator -p <package name> -o <output path> -f <path to structure file> -v <vm version> [-s <superset file name> -h <helper class package> -u <user code support> -c <cache properties> -l <legacy mode>]\n");
		System.out.println("<package name>           : the package name for all the generated classes e.g. com.ibm.j9ddr.vm.pointer.generated");
		System.out.println("<relative output path>   : where to write out the class files.  Full path to base of package hierarchy e.g. c:\\src\\");
		System.out.println("<path to structure file> : full path to the J9 structure file");
		System.out.println("<vm version>             : the version of the VM for which the pointers are generated e.g. 23 and corresponds to the stub package name");
		System.out.println("<superset file name>     : optional filename of the superset to be used as input / output");
		System.out.println("<helper class package>   : optional package for pointer helper files to be generated in from user code");
		System.out.println("<user code support>      : optional set to true or false to enable or disable user code support in the generated pointers, default if not specified is true");
		System.out.println("<cache properties>       : optional properties file which controls the class and field caching of generated pointers");
		System.out.println("<legacy mode>            : optional flag set to true or false indicating if legacy DDR is used");
	}

	/**
	 * Determine the output directory for the classes, create it if required
	 * @return File pointing to the output directory
	 */
	private File getOutputDir(String option) {
		String outputPath = opts.get("-o").replace('\\', '/');
		if (!outputPath.endsWith("/")) {
			outputPath += "/";
		}
		outputPath += opts.get(option).replace('.', '/');
		System.out.println("Writing generated classes to " + outputPath);
		File output = new File(outputPath);
		output.mkdirs();
		return output;
	}

	private static void writeCopyright(PrintWriter writer) {
		int year = Calendar.getInstance().get(Calendar.YEAR);

		writer.println("/*******************************************************************************");
		writer.println(" * Copyright (c) 1991, " + year + " IBM Corp. and others");
		writer.println(" *");
		writer.println(" * This program and the accompanying materials are made available under");
		writer.println(" * the terms of the Eclipse Public License 2.0 which accompanies this");
		writer.println(" * distribution and is available at https://www.eclipse.org/legal/epl-2.0/");
		writer.println(" * or the Apache License, Version 2.0 which accompanies this distribution");
		writer.println(" * and is available at https://www.apache.org/licenses/LICENSE-2.0.");
		writer.println(" *");
		writer.println(" * This Source Code may also be made available under the following");
		writer.println(" * Secondary Licenses when the conditions for such availability set");
		writer.println(" * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU");
		writer.println(" * General Public License, version 2 with the GNU Classpath");
		writer.println(" * Exception [1] and GNU General Public License, version 2 with the");
		writer.println(" * OpenJDK Assembly Exception [2].");
		writer.println(" *");
		writer.println(" * [1] https://www.gnu.org/software/classpath/license.html");
		writer.println(" * [2] http://openjdk.java.net/legal/assembly-exception.html");
		writer.println(" *");
		writer.println(" *  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception");
		writer.println(" *******************************************************************************/");
	}

}
