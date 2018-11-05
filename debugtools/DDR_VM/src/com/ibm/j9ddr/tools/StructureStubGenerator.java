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
package com.ibm.j9ddr.tools;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.ibm.j9ddr.CTypeParser;
import com.ibm.j9ddr.StructureReader;
import com.ibm.j9ddr.StructureReader.ConstantDescriptor;
import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;
import com.ibm.j9ddr.tools.store.J9DDRStructureStore;

/**
 * Utility to generate concrete implementation classes based on that
 * structure data found in a binary file of the same format as the new
 * core file offset data.
 */
public class StructureStubGenerator {

	final Map<String, String> opts;
	StructureReader structureReader;
	File outputDir;

	public StructureStubGenerator() {
		super();
		opts = new HashMap<String, String>();
		opts.put("-p", null);
		opts.put("-o", null);
		opts.put("-f", null);
		opts.put("-s", null);
		opts.put("-l", "false");
	}

	public static void main(String[] args) throws Exception {
		StructureStubGenerator app = new StructureStubGenerator();
		app.parseArgs(args);
		app.generateClasses();
		System.out.println("Processing complete");
	}

	private void generateClasses() {
		String supersetDirectoryName = opts.get("-f");
		String supersetFileName = opts.get("-s");

		try {
			J9DDRStructureStore store = new J9DDRStructureStore(supersetDirectoryName, supersetFileName);

			System.out.println("superset directory name : " + supersetDirectoryName);
			System.out.println("superset file name : " + store.getSuperSetFileName());

			InputStream inputStream = store.getSuperset();
			structureReader = new StructureReader(inputStream);
			inputStream.close();
		} catch (IOException e) {
			System.out.println("Could not find file: " + supersetDirectoryName + "/" + supersetFileName);
			return;
		}

		outputDir = getOutputDir();

		for (StructureDescriptor structure : structureReader.getStructures()) {
			try {
				if (FlagStructureList.isFlagsStructure(structure.getName())) {
					// generateBuildFlags(structure);
				} else {
					generateClass(structure);
				}
			} catch (FileNotFoundException e) {
				String error = String.format("File Not Found processing: %s: %s", structure.getName(), e.getMessage());
				System.out.println(error);
			} catch (IOException e) {
				String error = String.format("IOException processing: %s: %s", structure.getName(), e.getMessage());
				System.out.println(error);
			}
		}
	}

	private void generateClass(StructureDescriptor structure) throws IOException {
		File javaFile = new File(outputDir, structure.getName() + ".java");
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
		writer.println();
		writeClassComment(writer, structure.getName());
		writer.format("public final class %s {%n", structure.getName());
		writer.println();
		writeConstants(writer, structure);
		writer.println();
		writeOffsetStubs(writer, structure);
		writer.println();
		writeStaticInitializer(writer, structure);
		writer.println("}");
		writer.close();

		byte[] newContentsBytes = newContents.toByteArray();
		if (null == original || !Arrays.equals(original, newContentsBytes)) {
			FileOutputStream fos = new FileOutputStream(javaFile);
			fos.write(newContentsBytes);
			fos.close();
		}
	}

	private static void writeConstants(PrintWriter writer, StructureDescriptor structure) {
		writer.println("\t// VM Constants");
		writer.println();
		writer.format("\tpublic static final long %s;%n", "SIZEOF");
		Collections.sort(structure.getConstants());
		for (ConstantDescriptor constant : structure.getConstants()) {
			writer.format("\tpublic static final long %s;%n", constant.getName());
		}
	}

	private static void writeConstantInitializer(PrintWriter writer, StructureDescriptor structure) {
		writer.format("\t\t%s = 0;%n", "SIZEOF");
		Collections.sort(structure.getConstants());
		for (ConstantDescriptor constant : structure.getConstants()) {
			writer.format("\t\t%s = 0;%n", constant.getName());
		}
	}

	private void writeStaticInitializer(PrintWriter writer, StructureDescriptor structure) {
		writer.println("\t// Static Initializer");
		writer.println();
		writer.println("\tprivate static final boolean RUNTIME = false;");
		writer.println();
		writer.println("\tstatic {");
		writer.println("\t\tif (!RUNTIME) {");
		writer.println("\t\t\tthrow new IllegalArgumentException(\"This stub class should not be on your classpath\");");
		writer.println("\t\t}");
		writer.println();
		writeConstantInitializer(writer, structure);
		writeOffsetInitializer(writer, structure);
		writer.println("\t}");
		writer.println();
	}

	private static void writeClassComment(PrintWriter writer, String name) {
		writer.println("/**");
		writer.format(" * Structure: %s%n", name);
		writer.println(" *");
		writer.println(" * This stub class represents a class that can return in memory offsets");
		writer.println(" * to VM C and C++ structures.");
		writer.println(" *");
		writer.println(" * This particular implementation exists only to allow StructurePointer code to");
		writer.println(" * compile at development time.  This is never loaded at run time.");
		writer.println(" *");
		writer.println(" * At runtime generated byte codes returning actual offset values from the core file");
		writer.println(" * will be loaded by the StructureClassLoader.");
		writer.println(" */");
	}

	private String getOffsetConstant(FieldDescriptor fieldDescriptor) {
		String fieldName = fieldDescriptor.getName();
		if (opts.get("-l").equals("true")) {
			return PointerGenerator.getOffsetConstant(fieldName);
		}
		return fieldName;
	}

	private void writeOffsetStubs(PrintWriter writer, StructureDescriptor structure) {
		List<FieldDescriptor> fields = structure.getFields();

		if (fields.isEmpty()) {
			return;
		}

		writer.println("\t// Offsets");
		writer.println();
		Collections.sort(fields);
		for (FieldDescriptor fieldDescriptor : fields) {
			if (getOffsetConstant(fieldDescriptor).equals(fieldDescriptor.getName())
					&& !PointerGenerator.omitFieldImplementation(structure, fieldDescriptor)) {
				writeOffsetStub(writer, fieldDescriptor, structure.getName(), "Offset");
			}
		}
	}

	private static void writeOffsetStub(PrintWriter writer, FieldDescriptor fieldDescriptor, String className, String postFix) {
		String getter = fieldDescriptor.getName();
		CTypeParser parser = new CTypeParser(fieldDescriptor.getType());

		if (parser.getSuffix().contains(":")) {
			writer.format("\tpublic static final int _%s_s_;%n", getter);
			writer.format("\tpublic static final int _%s_b_;%n", getter);
		} else {
			writer.format("\tpublic static final int _%s%s_;%n", getter, postFix);
		}
	}

	private void writeOffsetInitializer(PrintWriter writer, StructureDescriptor structure) {
		Collections.sort(structure.getFields());
		for (FieldDescriptor fieldDescriptor : structure.getFields()) {
			String fieldName = fieldDescriptor.getName();
			if (getOffsetConstant(fieldDescriptor).equals(fieldName)
					&& !PointerGenerator.omitFieldImplementation(structure, fieldDescriptor)) {
				CTypeParser parser = new CTypeParser(fieldDescriptor.getType());

				if (parser.getSuffix().contains(":")) {
					String bitfieldName = fieldName;
					writer.format("\t\t_%s_s_ = 0;%n", bitfieldName);
					writer.format("\t\t_%s_b_ = 0;%n", bitfieldName);
				} else {
					writer.format("\t\t_%sOffset_ = 0;%n", fieldName);
				}
			}
		}
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
			if (value == null && !key.equals("-s")) {
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
		System.out.println("Usage :\n\njava PointerGenerator -p <package name> -o <output path> -f <path to structure file> [-s <superset file name> -l <legacy mode>]\n");
		System.out.println("<package name>           : the package name for all the generated classes e.g. com.ibm.dtfj.j9.structures");
		System.out.println("<relative output path>   : where to write out the class files.  Full path to base of package hierarchy e.g. c:\\src\\");
		System.out.println("<path to structure file> : full path to the J9 structure file");
		System.out.println("<superset file name>     : optional file name of the superset to be used for input / output");
		System.out.println("<legacy mode>            : optional flag set to true or false indicating if legacy DDR is used");
	}

	/**
	 * Determine the output directory for the classes, create it if required
	 * @return File pointing to the output directory
	 */
	private File getOutputDir() {
		String outputPath = opts.get("-o").replace('\\', '/');
		if (!outputPath.endsWith("/")) {
			outputPath += "/";
		}
		outputPath += opts.get("-p").replace('.', '/');
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
		writer.println(" * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception");
		writer.println(" *******************************************************************************/");
	}

}
