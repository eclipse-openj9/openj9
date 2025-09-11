/*
 * Copyright IBM Corp. and others 2018
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.j9ddr.tools;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Iterator;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.imageio.stream.FileImageInputStream;

import com.ibm.j9ddr.StructureReader;
import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.StructureReader.PackageNameType;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;

/**
 * This class generates the same structure and pointer classes that would
 * be created at runtime from a DDR blob, but only with those fields that
 * are guaranteed to be present. Fields not mentioned in AuxFieldInfo29.dat
 * are omitted to avoid references in hand-written Java code in DDR_VM
 * that might not be present in system dumps.
 */
public final class ClassGenerator {

	private static void checkAndFilterFields(StructureReader reader) {
		boolean problem = false;

		for (StructureDescriptor structure : reader.getStructures()) {
			Iterator<FieldDescriptor> fields = structure.getFields().iterator();

			while (fields.hasNext()) {
				FieldDescriptor field = fields.next();

				if (field.isRequired()) {
					if (!field.isPresent()) {
						System.out.printf("Missing required field: %s.%s%n", structure.getName(), field.getName());
						problem = true;
					}
				} else if (!field.isOptional()) {
					fields.remove();
				}
			}
		}

		if (problem) {
			System.exit(1);
		}
	}

	private static void duplicateOption(String option) {
		System.out.printf("Duplicate option: %s.%n", option);
	}

	public static void main(String... args) {
		ClassGenerator generator = new ClassGenerator();

		generator.configure(args);
		generator.run();
	}

	private static StructureReader readBlob(String fileName) throws IOException {
		// Extend FileImageInputStream to avoid loading the awt shared library.
		final class BlobStream extends FileImageInputStream {
			BlobStream(File file) throws IOException {
				super(file);
			}
		}

		File file = new File(fileName);

		try (BlobStream blob = new BlobStream(file)) {
			return new StructureReader(blob);
		}
	}

	private boolean badOptions;

	private String blobFile;

	private boolean helpRequested;

	private File outDir;

	private boolean verbose;

	private ClassGenerator() {
		super();
		this.badOptions = false;
		this.blobFile = null;
		this.helpRequested = false;
		this.outDir = null;
		this.verbose = false;
	}

	private void configure(String[] args) {
		Pattern blobOption = Pattern.compile("--blob=(.+)");
		Pattern outDirOption = Pattern.compile("--out=(.+)");

		for (String arg : args) {
			Matcher matcher;

			matcher = blobOption.matcher(arg);
			if (matcher.matches()) {
				String fileName = matcher.group(1);

				if (blobFile == null) {
					blobFile = fileName;
				} else {
					duplicateOption(arg);
					badOptions = true;
				}
				continue;
			}

			if ("--debug".equals(arg)) {
				verbose = true;
				continue;
			}

			if ("--help".equals(arg)) {
				helpRequested = true;
				continue;
			}

			matcher = outDirOption.matcher(arg);
			if (matcher.matches()) {
				String fileName = matcher.group(1);

				if (outDir == null) {
					outDir = new File(fileName);
				} else {
					duplicateOption(arg);
					badOptions = true;
				}
				continue;
			}

			System.out.printf("Invalid option: %s.%n", arg);
			badOptions = true;
		}
	}

	private void run() {
		if (badOptions || helpRequested || blobFile == null || outDir == null) {
			System.out.printf("Usage: java %s [options]%n", getClass().getName());
			System.out.printf("  options:%n");
			System.out.printf("    --blob=<blob file>%n");
			System.out.printf("    --debug%n");
			System.out.printf("    --help%n");
			System.out.printf("    --out=<output directory>%n");
		} else {
			try {
				StructureReader reader = readBlob(blobFile);

				checkAndFilterFields(reader);
				writeTo(reader, outDir);
			} catch (IOException e) {
				System.out.printf("Can't read blob: %s%n", e.getLocalizedMessage());
				System.exit(1);
			}
		}
	}

	private void save(byte[] bytes, File directory, String name) throws IOException {
		File file = new File(directory, name + ".class");

		try (FileOutputStream out = new FileOutputStream(file)) {
			out.write(bytes);

			if (verbose) {
				System.out.printf("Wrote: %s%n", file.getPath());
			}
		}
	}

	private void writeTo(StructureReader reader, File rootDir) {
		File pointerDir = new File(rootDir, reader.getPackageName(PackageNameType.POINTER_PACKAGE_SLASH_NAME));
		File structureDir = new File(rootDir, reader.getPackageName(PackageNameType.STRUCTURE_PACKAGE_SLASH_NAME));

		pointerDir.mkdirs();
		structureDir.mkdirs();

		for (StructureDescriptor structure : reader.getStructures()) {
			String name = structure.getName();
			byte[] bytes;

			try {
				if (FlagStructureList.isFlagsStructure(name)) {
					bytes = reader.getPointerClassBytes(name);
					save(bytes, pointerDir, name);
				} else {
					bytes = reader.getStructureClassBytes(name);
					save(bytes, structureDir, name);

					name = structure.getPointerName();
					bytes = reader.getPointerClassBytes(name);
					save(bytes, pointerDir, name);
				}
			} catch (ClassNotFoundException e) {
				System.out.printf("Can't generate %s: %s%n", name, e.getLocalizedMessage());
				System.exit(2);
			} catch (IOException e) {
				System.out.printf("Can't save %s: %s%n", name, e.getLocalizedMessage());
				System.exit(3);
			}
		}
	}

}
