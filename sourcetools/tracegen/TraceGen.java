/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.io.Writer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Queue;

/* Process TDF files, generating the required header files and the .dat files. */
@SuppressWarnings("nls")
public class TraceGen {

	private static boolean force = false;

	private static boolean verbose = false;

	static BufferedWriter newWriter(File file) throws IOException {
		Writer writer;

		if (force) {
			writer = new FileWriter(file);
		} else {
			writer = new OutputStreamWriter(new IncrementalFileOutputStream(file));
		}

		return new BufferedWriter(writer);
	}

	static boolean j9buildenv = false;

	static boolean classlibbuildenv = true;

	private static boolean generatecfiles = false;

	private static String traceformatfilename = "TraceFormat.dat";

	static int RAS_MAJOR_VERSION = 5;

	static int RAS_MINOR_VERSION = 0;

	private static int threshold = 1;

	// The tag level that will be used for this version of tracegen.
	private static final String level = "R29-Current-001";

	/* debug options */
	private static boolean writetocurrentdir = false;

	private static boolean debugoutput = false;

	static boolean treatWarningAsError = false;

	static void dp(String s) {
		if (debugoutput) {
			System.out.println(s);
		}
	}

	public static void main(String args[]) {
		System.out.println("Running Trace Gen " + level);

		/* check for debug option first so command line parsing can be traced */
		for (String arg : args) {
			if (arg.equalsIgnoreCase("-debug")) {
				debugoutput = true;
				dp("TraceGen enabling debug output");
			}
		}

		String rootDirectory = ".";

		/* process command line arguments */
		for (int i = 0; i < args.length; ++i) {
			String arg = args[i];
			String nextArg = i < args.length - 1 ? args[i + 1] : null;

			if (arg.equalsIgnoreCase("-j9build")) {
				dp("TraceGen enabling j9build environment");
				j9buildenv = true;
				classlibbuildenv = false;
			} else if (arg.equalsIgnoreCase("-threshold")) {
				if (nextArg == null) {
					System.err.println("TraceGen: threshold specified with no number aborting");
					System.exit(-1);
				}
				dp("TraceGen setting threshold to " + nextArg);
				threshold = Integer.parseInt(nextArg);
				i += 1;
			} else if (arg.equalsIgnoreCase("-j9contract")
					|| arg.equalsIgnoreCase("-contract")) {
				if ((nextArg == null) || nextArg.startsWith("-")) {
					System.err.println("TraceGen: " + arg + " specified with no directory, aborting");
					System.exit(-1);
				}
				dp("TraceGen setting working directory to " + nextArg);
				rootDirectory = nextArg;
				i += 1;
			} else if (arg.equalsIgnoreCase("-majorversion")) {
				i += 1;
				if (nextArg == null) {
					System.err.println("TraceGen: majorversion specified with no number aborting");
					System.exit(-1);
				}
				dp("TraceGen setting major version to " + args[i]);
				RAS_MAJOR_VERSION = Integer.parseInt(args[i]);
			} else if (arg.equalsIgnoreCase("-minorversion")) {
				if (nextArg == null) {
					System.err.println("TraceGen: minorversion specified with no number aborting");
					System.exit(-1);
				}
				dp("TraceGen setting minor version to " + nextArg);
				RAS_MINOR_VERSION = Integer.parseInt(nextArg);
				i += 1;
			} else if (arg.equalsIgnoreCase("-debug")) {
				/* ignore here */
			} else if (arg.equalsIgnoreCase("-force")) {
				force = true;
			} else if (arg.equalsIgnoreCase("-verbose")) {
				verbose = true;
			} else if (arg.equalsIgnoreCase("-w2cd")) {
				writetocurrentdir = true;
			} else if (arg.equalsIgnoreCase("-generateCfiles")) {
				generatecfiles = true;
			} else if (arg.equalsIgnoreCase("-treatWarningAsError")) {
				treatWarningAsError = true;
			} else {
				dp("ignoring option: " + arg);
			}
		}

		/* find all tdf files under the build root */
		List<File> tdfFileList = findTDFFilesIn(rootDirectory);
		List<TraceComponent> components = new ArrayList<>();

		for (File file : tdfFileList) {
			if (verbose) {
				System.out.println("Processing tdf file: " + file.getPath());
			}

			components.addAll(processTDFFile(file));
		}

		/* merge components to get around sov tdf file quirks */
		for (int i = 0, n = components.size(); i < n; ++i) {
			TraceComponent tc1 = components.get(i);

			if (!tc1.isValid()) {
				continue;
			}

			String tc1Name = tc1.getComponentName();

			for (int j = i + 1; j < n; ++j) {
				TraceComponent tc2 = components.get(j);

				if (tc1Name.equals(tc2.getComponentName())) {
					System.out.println("Merging components " + tc1Name);

					/* merge TraceComponent tc1 and TraceComponent tc2 */
					tc1.merge(tc2);
					tc2.invalidate();
				}
			}
		}

		for (TraceComponent tc : components) {
			if (!tc.isValid()) {
				continue;
			}

			if (verbose) {
				System.out.println("Calculating groups for " + tc.getComponentName());
			}

			tc.calculateGroups();

			if (verbose) {
				System.out.println("Creating header file: " + tc.getRelativePath() + tc.getHeaderFileName());
			}

			boolean tcOK = writeTraceHeaderFileFor(tc);

			if (!tcOK) {
				System.err.println("Error(s) generating: " + tc.getHeaderFileName());
				System.exit(-1);
			}

			if (j9buildenv) {
				if (verbose) {
					System.out.println("Creating include file: " + tc.getRelativePath() + tc.getIncludeFileName());
				}
				createTraceIncludeFileFor(tc);
			}

			if (generatecfiles) {
				if (verbose) {
					System.out.println("Creating c data file: " + tc.getRelativePath() + tc.getCFileName());
				}
				generateCFileFor(tc);
			}
		}

		createDATFilesFor(components);
	}

	private static List<File> findTDFFilesIn(String rootDir) {
		List<File> tdfFiles = new LinkedList<>();

		if (verbose) {
			System.out.println("Searching for trace description files (TDF)...");
		}

		for (Queue<File> dirsToSearch = new LinkedList<>(Collections.singletonList(new File(rootDir)));;) {
			File dir = dirsToSearch.poll();

			if (dir == null) {
				break;
			}

			File[] files = dir.listFiles();

			if (files == null) {
				continue;
			}

			for (File file : files) {
				if (file.isDirectory()) {
					dirsToSearch.add(file);
				} else if (file.getName().endsWith(".tdf")) {
					if (verbose) {
						System.out.println("Found " + file.getPath());
					}

					tdfFiles.add(file);
				}
			}
		}

		return tdfFiles;
	}

	private static List<TraceComponent> processTDFFile(File tdffile) {
		dp("processing tdf file: " + tdffile.getPath());

		List<TraceComponent> traceComponents = readTracePointsFromTDFFile(tdffile);

		dp(traceComponents.size() + " trace components found in " + tdffile.getPath());

		return traceComponents;
	}

	/* returns all the tracepoints in a tdf file, from whatever component */
	private static List<TraceComponent> readTracePointsFromTDFFile(File tdffile) {
		List<TraceComponent> components = new ArrayList<>();
		int id = 0;
		String component = null;
		String currentDATFileName = traceformatfilename;
		String line;
		TraceComponent currentComponent = null;

		try (BufferedReader tdfin = new BufferedReader(new FileReader(tdffile))) {
			/* read in the tdf file and get a tracepoint for each */

			while ((line = tdfin.readLine()) != null) {
				if (line.trim().equals("") || line.startsWith("Inline")) {
					/* ignore it */
				} else if (line.toLowerCase().startsWith("executable=")) {
					component = line.substring(11);

					dp("component [" + component + "] found in " + tdffile.getPath());

					String headerfilename = null;
					String includefilename = null;
					String cfilename = null;
					String relativepath = null;

					/* open header file */
					if (classlibbuildenv) {
						headerfilename = "dgTrc" + component + ".h";
						includefilename = "dgTrc" + component + ".inc";
						cfilename = "dgTrc" + component + ".c";
						relativepath = "";
					} else if (writetocurrentdir) {
						headerfilename = "ut_" + component + ".h";
						includefilename = "ut_" + component + ".inc";
						cfilename = "ut_" + component + ".c";
						relativepath = "";
					} else {
						/* just use this file to get the output directory */
						headerfilename = "ut_" + component + ".h";
						/* just need to create the empty include file as far as I can see */
						includefilename = "ut_" + component + ".inc";
						cfilename = "ut_" + component + ".c";
						relativepath = tdffile.getParent() + File.separator;
					}

					currentComponent = new TraceComponent(component,
							headerfilename, includefilename, cfilename,
							relativepath, currentDATFileName);
					components.add(currentComponent);
					/* reset count as each module starts from 0 now */
					id = 0;
				} else if (line.toLowerCase().startsWith("datfilename=")) {
					currentDATFileName = line.substring(12);
					if (currentComponent != null) {
						currentComponent.setDatFileName(currentDATFileName);
					}
				} else if (line.toLowerCase().startsWith("submodules=")) {
					if (currentComponent == null) {
						System.err.println("No \"Executable=xxxx\" statement before \"Submodules=\" statement in file " + tdffile.getPath());
						break;
					}
					String submodules = line.substring("submodules=".length());
					currentComponent.setSubmodules(submodules);
				} else if (line.toLowerCase().startsWith("cfilesoutputdirectory=")) {
					if (currentComponent == null) {
						System.err.println("No \"Executable=xxxx\" statement before \"cfilesoutputdirectory=\" statement in file " + tdffile.getPath());
						break;
					}
					String cfilesoutputdirectory = line.substring("cfilesoutputdirectory=".length()) + File.separator;
					currentComponent.setCFileOutputPath(cfilesoutputdirectory);
				} else if (line.toLowerCase().startsWith("auxiliary")) {
					if (currentComponent == null) {
						System.err.println("No \"Executable=xxxx\" statement before \"auxiliary\" statement in file " + tdffile.getPath());
						break;
					}
					currentComponent.setAuxiliary();
				} else if (!line.startsWith("//")) {
					if (currentComponent == null) {
						System.err.println("No \"Executable=xxxx\" statement before first tracepoint in file " + tdffile.getPath());
						break;
					}
					TracePoint tp = TracePoint.parseTracePoint(line, currentComponent, id++);
					if (tp == null) {
						System.err.println("Invalid tracepoint in " + tdffile.getPath() + ": " + line);
					}
					currentComponent.addTracePoint(tp);
				}
			}
		} catch (IOException ioe) {
			System.err.println("Error handling " + tdffile.getPath());
			ioe.printStackTrace();
		}

		return components;
	}

	private static void writeHeaderRegistrationMacrosOnStream(TraceComponent component,
			BufferedWriter output, boolean directRegistration) throws IOException {
		/* directRegistration means passing UtInterface instead of J9JavaVM to the registration code */
		String componentName = component.getComponentName();
		String registerFunction = "register" + componentName + "WithTrace";
		String deregisterFunction = "deregister" + componentName + "WithTrace";
		String parameterType = directRegistration ? "UtInterface *" : "JavaVM *";
		String castString = directRegistration ? "" : "(" + parameterType + ")";
		String parameterName = directRegistration ? "utIntf" : "vm";
		output.write("I_32 "
				+ registerFunction + "(" + parameterType + " " + parameterName + ", UtModuleInfo* containerName);");
		output.newLine();
		output.write("I_32 "
				+ deregisterFunction + "(" + parameterType + " " + parameterName + ");");
		output.newLine();

		/* avoid build warnings if redefined */
		output.write("#define UT_MODULE_LOADED(" + parameterName + ") "
				+ registerFunction + "(" + castString + "(" + parameterName + "), NULL);");
		output.newLine();
		output.write("#define UT_MODULE_UNLOADED(" + parameterName + ") "
				+ deregisterFunction + "(" + castString + "(" + parameterName + "));");
		output.newLine();
		output.write("#define UT_" + componentName.toUpperCase() + "_MODULE_LOADED(" + parameterName + ") "
				+ registerFunction + "(" + castString + "(" + parameterName + "), NULL);");
		output.newLine();
		output.write("#define UT_" + componentName.toUpperCase() + "_MODULE_UNLOADED(" + parameterName + ") "
				+ deregisterFunction + "(" + castString + "(" + parameterName + "));");
		output.newLine();
	}

	private static void writeHeaderOpeningInfoOnStream(TraceComponent component,
			BufferedWriter output) throws IOException {
		output.write("/*");
		output.newLine();
		output.write(" *  Do not edit this file ");
		output.newLine();
		output.write(" *  Generated by TraceGen");
		output.newLine();
		output.write(" */");
		output.newLine();
		output.write("#ifndef " + component.getHeaderFileDefine());
		output.newLine();
		output.write("#define " + component.getHeaderFileDefine());
		output.newLine();
		output.write("#include \"ute_module.h\"");
		output.newLine();
		output.write("#if !defined(UT_DIRECT_TRACE_REGISTRATION)");
		output.newLine();
		output.write("#include \"jni.h\"");
		output.newLine();
		output.write("#endif /* !defined(UT_DIRECT_TRACE_REGISTRATION) */");
		output.newLine();
		output.write("#ifndef UT_TRACE_OVERHEAD");
		output.newLine();
		output.write("#define UT_TRACE_OVERHEAD " + threshold);
		output.newLine();
		output.write("#endif");
		output.newLine();
		output.write("#ifndef UT_THREAD");
		output.newLine();
		if (j9buildenv) {
			output.write("#define UT_THREAD(thr) (void *)thr");
			output.newLine();
		} else {
			output.write("#define UT_THREAD(thr) (void *)thr");
			output.newLine();
		}
		output.write("#endif /* UT_THREAD */");
		output.newLine();
		if (!j9buildenv) {
			output.write("#if defined(_IBM_DG_INITTRC_)");
			output.newLine();
			output.write("#define _UTE_STATIC_");
			output.newLine();
			output.write("#endif");
			output.newLine();
		}
		output.write("#ifndef UT_STR");
		output.newLine();
		output.write("#define UT_STR(arg) #arg");
		output.newLine();
		output.write("#endif");
		output.newLine();
		output.write("#ifdef __cplusplus");
		output.newLine();
		output.write("extern \"C\" {");
		output.newLine();
		output.write("#endif");
		output.newLine();

		output.newLine();
		output.write("#ifdef __clang__");
		output.newLine();
		output.write("#include <unistd.h>");
		output.newLine();
		output.write("#define Trace_Unreachable() _exit(-1)");
		output.newLine();
		output.write("#else");
		output.newLine();
		output.write("#define Trace_Unreachable()");
		output.newLine();
		output.write("#endif");
		output.newLine();

		if (generatecfiles) {
			output.newLine();
			output.write("#if defined(UT_DIRECT_TRACE_REGISTRATION)");
			output.newLine();
			writeHeaderRegistrationMacrosOnStream(component, output, true);
			output.write("#else /* defined(UT_DIRECT_TRACE_REGISTRATION) */");
			output.newLine();
			writeHeaderRegistrationMacrosOnStream(component, output, false);
			output.write("#endif /* defined(UT_DIRECT_TRACE_REGISTRATION) */");
			output.newLine();
		}
		output.newLine();
	}

	private static void writeComponentDataForNonTraceEnabledBuildsOnStream(
			String component, BufferedWriter output, TraceComponent tc)
			throws IOException {

		output.write("unsigned char " + component + "_UtActive[1];");
		output.newLine();
		output.write("UtModuleInfo " + component + "_UtModuleInfo = {\""
				+ component + "\", " + component.length()
				+ ", 0, 0, NULL, NULL, NULL, &" + component
				+ "_UtTraceVersionInfo, \"" + tc.getDatFileName()
				+ "\", NULL, NULL, NULL, NULL, 0, 0};");
		output.newLine();
	}

	private static void writeComponentDataOnStream(String component,
			int ntracepoints, int levels[], int[] earlyAssertDefaults, BufferedWriter output,
			TraceComponent tc) throws IOException {
		StringBuilder sb = new StringBuilder();

		if (ntracepoints == 0) {
			/* allow empty components e.g. frame */
			ntracepoints = 1;
		}

		writeActiveArray(component, ntracepoints, earlyAssertDefaults, output, sb);

		/* output level structure */
		writeLevels(component, ntracepoints, levels, output);

		writeGroups(component, output, tc);

		writeModuleInfo(component, ntracepoints, output, tc);
	}

	private static void writeModuleInfo(String component, int ntracepoints,
			BufferedWriter output, TraceComponent tc) throws IOException {
		output.write("UtModuleInfo " + component + "_UtModuleInfo = {\""
				+ component + "\", " + component.length() + ", " + ntracepoints
				+ ", 0, " + component + "_UtActive , NULL, NULL, &" + component
				+ "_UtTraceVersionInfo, \"" + tc.getDatFileName() + "\", "
				+ component + "_UtLevels, &" + component + "_group0, NULL, NULL, 0, " + (tc.isAuxiliary() ? "1" : "0") + "};");
		output.newLine();
	}

	private static void writeGroups(String component, BufferedWriter output, TraceComponent tc) throws IOException {
		TraceGroup groups[] = tc.getGroups();

		output.newLine();

		for (int tracegroups = groups.length; tracegroups > 0; tracegroups--) {
			int[] tpids = groups[tracegroups - 1].getTPIDS();
			if ((tpids != null) && (tpids.length > 0)) {
				output.newLine();
				output.write("I_32 " + component + "_tpids" + (tracegroups - 1) + "[] = {");
				for (int i = 0; i < tpids.length; i++) {
					if (i % 30 == 0) {
						output.newLine();
						output.write("\t");
					}
					output.write(Integer.toString(tpids[i]));
					if (i != (tpids.length - 1)) {
						output.write(",");
					}
				}
				output.write("\t};");
				output.newLine();
			}

			output.newLine();

			output.write("UtGroupDetails " + component + "_group" + (tracegroups - 1) + " = {");
			output.newLine();
			output.write("\t\"" + groups[tracegroups - 1].getName() + "\",");
			output.newLine();
			output.write("\t" + groups[tracegroups - 1].getCount() + ",");
			output.newLine();

			/* tpids */
			if ((tpids != null) && (tpids.length > 0)) {
				output.write("\t" + component + "_tpids" + (tracegroups - 1) + ",");
				output.newLine();
			} else {
				output.write("\tNULL,");
				output.newLine();
			}

			if (tracegroups == groups.length) {
				output.write("\tNULL};");
				output.newLine();
			} else {
				output.write("\t&" + component + "_group" + (tracegroups) + "};");
				output.newLine();
			}
		}
	}

	private static void writeLevels(String component, int ntracepoints, int[] levels, BufferedWriter output)
			throws IOException {
		/*
		 * deliberately using ntracepoints not levels.length so any mismatch
		 * will be a buildtime error, not a runtime error
		 */
		output.write("unsigned char " + component + "_UtLevels[" + ntracepoints + "] = { ");

		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < ntracepoints; i++) {
			if (i % 30 == 0) {
				output.write(sb.toString());
				sb.setLength(0);
				output.newLine();
				output.write("\t");
			}
			sb.append(levels[i]);
			if (i < ntracepoints - 1) {
				sb.append(",");
			}
		}

		output.write(sb.toString());
		output.write("};");
		output.newLine();
		output.newLine();
	}

	private static void writeActiveArray(String component, int ntracepoints, int[] earlyAssertDefaults,
			BufferedWriter output, StringBuilder sb) throws IOException {
		/* output active array */
		output.write("unsigned char " + component + "_UtActive[" + ntracepoints + "] = { ");

		for (int i = 0; i < ntracepoints; i++) {
			if (i % 30 == 0) {
				output.write(sb.toString());
				sb.setLength(0);
				output.newLine();
				output.write("\t");
			}
			sb.append(earlyAssertDefaults[i]);
			if (i < ntracepoints - 1) {
				sb.append(",");
			}
		}

		output.write(sb.toString());
		output.write("};");
		output.newLine();
		output.newLine();
	}

	private static void writeHeaderClosingFileOnStream(TraceComponent component,
			int levels[], int earlyAssertDefaults[], BufferedWriter output) throws IOException {
		String componentName = component.getComponentName();
		if (!generatecfiles) {
			output.write("#if defined(_UTE_STATIC_)");
			output.newLine();
			writeComponentDataOnStream(componentName, component.getTraceCount(), levels, earlyAssertDefaults, output,
					component);

			output.write("#else /* !_UTE_STATIC_ */");
			output.newLine();
		}
		output.write("extern UtModuleInfo " + componentName + "_UtModuleInfo;");
		output.newLine();

		output.write("extern unsigned char " + componentName + "_UtActive[];");
		output.newLine();

		if (!generatecfiles) {
			output.write("#endif /* !_UTE_STATIC_ */");
			output.newLine();
		}

		output.newLine();
		output.write("#ifndef UT_MODULE_INFO");
		output.newLine();
		output.write("#define UT_MODULE_INFO " + componentName + "_UtModuleInfo");
		output.newLine();
		output.write("#endif /* UT_MODULE_INFO */");
		output.newLine();
		output.write("");
		output.newLine();
		output.write("#ifndef UT_ACTIVE");
		output.newLine();
		output.write("#define UT_ACTIVE " + componentName + "_UtActive");
		output.newLine();
		output.write("#endif /* UT_ACTIVE */");
		output.newLine();
		output.newLine();
		output.write("#ifdef __cplusplus");
		output.newLine();
		output.write("} /* extern \"C\" */");
		output.newLine();
		output.write("#endif");
		output.newLine();

		if (!j9buildenv) {
			output.write("#include \"jvmras.h\"");
			output.newLine();
			output.write("#include \"ras_jni.h\"");
			output.newLine();
		}

		output.write("#endif /* " + component.getHeaderFileDefine() + " */");
		output.newLine();
		output.write("/*");
		output.newLine();
		output.write(" * End of file");
		output.newLine();
		output.write(" */");
		output.newLine();
	}

	private static boolean createDATFilesFor(List<TraceComponent> components) {
		String currentDATFileName = traceformatfilename;
		Map<String, DATFileWriter> datFileWriters = new HashMap<>();

		try {
			for (TraceComponent tpc : components) {
				currentDATFileName = tpc.getDatFileName();
				TracePoint tps[] = tpc.getTracePoints();
				for (TracePoint tp : tps) {
					if (tp == null) {
						System.err.println("Error: bad tracepoint in component " + tpc.getComponentName() + ", see previous messages");
						continue;
					}
					try {
						writeToTraceFile(tp.toDATFileEntry(), currentDATFileName, datFileWriters);
					} catch (IOException ioe) {
						System.err.println("TraceGen encountered problem writing to " + currentDATFileName + " - aborting: " + ioe);
						ioe.printStackTrace();
						return false;
					}
				}
			}
		} finally {
			/* close all open trace output files */
			for (DATFileWriter dfw : datFileWriters.values()) {
				dfw.close();
			}
		}

		return true;
	}

	private static boolean writeTraceHeaderFileFor(TraceComponent tc) {
		TracePoint[] tps = tc.getTracePoints();
		boolean tpsOK = true;
		int levels[];
		int earlyAssertDefaults[];
		if (tps.length > 0) {
			levels = new int[tps.length];
			earlyAssertDefaults = new int[tps.length];
		} else {
			levels = new int[1];
			earlyAssertDefaults = new int[1];
		}
		File headerFile = new File(tc.getRelativePath() + tc.getHeaderFileName());

		try (BufferedWriter hFileWriter = newWriter(headerFile)) {
			writeHeaderOpeningInfoOnStream(tc, hFileWriter);

			for (int i = 0; i < tps.length; i++) {
				TracePoint tp = tps[i];
				if (tp == null) {
					System.err.println("Error: bad tracepoint in component " + tc.getComponentName() + ", see previous messages");
					continue;
				}
				levels[i] = tp.getLevel();
				if (tp.typeNumber == TracePoint.TYPE_ASSERTION && tp.getLevel() == 1) {
					earlyAssertDefaults[i] = 1; // Enable default assertions prior to full trace initialisation.
				}
				if (tp.isobsolete == false) {
					tpsOK &= tp.toMacroOnStream(hFileWriter);
					hFileWriter.newLine();
				}
			}

			writeHeaderClosingFileOnStream(tc, levels, earlyAssertDefaults, hFileWriter);
		} catch (IOException ioe) {
			System.err.println("Error writing trace header file for " + tc.getHeaderFileName());
			System.err.println(ioe);
		}

		return tpsOK;
	}

	private static int createTraceIncludeFileFor(TraceComponent tc) {
		File includeFile = new File(tc.getRelativePath() + tc.getIncludeFileName());

		try (BufferedWriter iFileWriter = newWriter(includeFile)) {
			iFileWriter.close();
		} catch (IOException ioe) {
			System.err.println("Error writing trace include file for " + tc.getIncludeFileName());
			System.err.println(ioe);
		}

		return 0;
	}

	private static void writeRegistrationFunctionsOnStream(TraceComponent tc,
			BufferedWriter cFileWriter, boolean directRegistration) throws IOException {
		String componentName = tc.getComponentName();
		/* directRegistration means passing UtInterface instead of J9JavaVM to the registration code */
		String parameterType = directRegistration ? "UtInterface *" : "JavaVM *";
		String parameterName = directRegistration ? "utIntf" : "vm";
		String ok = directRegistration ? "0" : "JNI_OK";
		String err = directRegistration ? "-1" : "JNI_ERR";
		cFileWriter	.write("/* function to register with trace engine and configure current module */");
		cFileWriter.newLine();
		cFileWriter.write("I_32"); /* type */
		cFileWriter.newLine();
		cFileWriter.write("register" + componentName
				+ "WithTrace(" + parameterType + parameterName + ", UtModuleInfo * containerModule)"); /* func name */
		cFileWriter.newLine();
		cFileWriter.write("{");
		cFileWriter.newLine();
		cFileWriter.write("\tI_32 rc = " + ok + ";"); /* func vars */
		cFileWriter.newLine();
		cFileWriter.newLine();
		cFileWriter.write("#if defined(UT_TRACE_ENABLED_IN_BUILD)");
		cFileWriter.newLine();
		if (!directRegistration) {
			cFileWriter.write("\tUtInterface *utIntf;"); /* func vars */
			cFileWriter.newLine();
		}
		/* trace enabled function body */
		cFileWriter.newLine();
		cFileWriter.write("\t" + componentName + "_UtModuleInfo.containerModule = containerModule;");
		cFileWriter.newLine();
		cFileWriter.newLine();
		if (directRegistration) {
			cFileWriter.write("\tif( utIntf == NULL ) {");
			cFileWriter.newLine();
			cFileWriter.write("\t\trc = " + err + ";");
			cFileWriter.newLine();
			cFileWriter.write("\t}");
			cFileWriter.newLine();
		} else {
			cFileWriter.write("\tif( vm != NULL ) {");
			cFileWriter.newLine();
			cFileWriter.write("\t\trc = (*vm)->GetEnv(vm, (void**)&utIntf, UTE_VERSION_1_1);");
			cFileWriter.newLine();
			cFileWriter.write("\t} else {");
			cFileWriter.newLine();
			cFileWriter.write("\t\trc = " + err + ";");
			cFileWriter.newLine();
			cFileWriter.write("\t}");
			cFileWriter.newLine();
		}
		cFileWriter.newLine();
		cFileWriter.write("\tif (rc == " + ok + ") {");
		cFileWriter.newLine();
		cFileWriter.write("\t\tutIntf->module->TraceInit(NULL, &"
				+ componentName + "_UtModuleInfo);");
		cFileWriter.newLine();
		cFileWriter.write("\t}");
		cFileWriter.newLine();

		for (String submodule : tc.getSubmodules()) {
			cFileWriter.newLine();
			cFileWriter.write("\tif (rc == " + ok + ") {");
			cFileWriter.newLine();
			cFileWriter.write("\t\textern I_32 register" + submodule + "WithTrace(" + parameterType + parameterName + ", UtModuleInfo* containerModule);");
			cFileWriter.newLine();
			cFileWriter.newLine();
			cFileWriter.write("\t\trc = register" + submodule + "WithTrace(" + parameterName + ", &"
					+ componentName + "_UtModuleInfo);");
			cFileWriter.newLine();
			cFileWriter.write("\t}");
			cFileWriter.newLine();
		}

		cFileWriter.newLine();
		cFileWriter.write("#else");
		cFileWriter.newLine();
		cFileWriter.write("\t/* trace not present in build - NOOP */");
		/* no op function body */
		cFileWriter.newLine();
		cFileWriter.write("#endif /* defined(UT_TRACE_ENABLED_IN_BUILD) */");
		cFileWriter.newLine();
		cFileWriter.write("\treturn rc;"); /* function return */
		cFileWriter.newLine();
		cFileWriter.write("}"); /* function close */
		cFileWriter.newLine();

		/* include a shutdown function */
		cFileWriter.newLine();
		cFileWriter.write("/* function to deregister with trace engine and configure current module */");
		cFileWriter.newLine();
		cFileWriter.write("I_32"); /* type */
		cFileWriter.newLine();
		cFileWriter.write("deregister" + componentName
				+ "WithTrace(" + parameterType + parameterName + ")"); /* func name */
		cFileWriter.newLine();
		cFileWriter.write("{");
		cFileWriter.newLine();
		cFileWriter.write("\tI_32 rc = " + ok + ";"); /* func vars */
		cFileWriter.newLine();
		cFileWriter.newLine();
		cFileWriter.write("#if defined(UT_TRACE_ENABLED_IN_BUILD)");
		cFileWriter.newLine();
		if (directRegistration) {
			cFileWriter.write("\tif( utIntf == NULL ) {");
			cFileWriter.newLine();
			cFileWriter.write("\t\trc = " + err + ";");
			cFileWriter.newLine();
			cFileWriter.write("\t}");
			cFileWriter.newLine();
		} else {
			cFileWriter.write("\tUtInterface *utIntf;"); /* func vars */
			cFileWriter.newLine();
			/* trace enabled function body */
			cFileWriter.newLine();
			cFileWriter.write("\tif( vm != NULL ) {");
			cFileWriter.newLine();
			cFileWriter.write("\t\trc = (*vm)->GetEnv(vm, (void**)&utIntf, UTE_VERSION_1_1);");
			cFileWriter.newLine();
			cFileWriter.write("\t} else {");
			cFileWriter.newLine();
			cFileWriter.write("\t\trc = " + err + ";");
			cFileWriter.newLine();
			cFileWriter.write("\t}");
			cFileWriter.newLine();
		}
		cFileWriter.newLine();

		cFileWriter.write("\tif (rc == " + ok + ") {");
		cFileWriter.newLine();
		cFileWriter.write("\t\tutIntf->module->TraceTerm(NULL, &" + componentName + "_UtModuleInfo);");
		cFileWriter.newLine();
		cFileWriter.write("\t}");
		cFileWriter.newLine();
		for (String submodule : tc.getSubmodules()) {
			cFileWriter.newLine();
			cFileWriter.write("\tif (rc == " + ok + ") {");
			cFileWriter.newLine();
			cFileWriter.write("\t\textern I_32 deregister" + submodule + "WithTrace(" + parameterType + parameterName + ");");
			cFileWriter.newLine();
			cFileWriter.newLine();
			cFileWriter.write("\t\trc = deregister" + submodule + "WithTrace(" + parameterName + ");");
			cFileWriter.newLine();
			cFileWriter.write("\t}");
			cFileWriter.newLine();
		}
		cFileWriter.write("#else");
		cFileWriter.newLine();
		cFileWriter.write("\t/* trace not present in build - NOOP */");
		/* no op function body */
		cFileWriter.newLine();
		cFileWriter.write("#endif /* defined(UT_TRACE_ENABLED_IN_BUILD) */");
		cFileWriter.newLine();
		cFileWriter.write("\treturn rc;"); /* function return */
		cFileWriter.newLine();
		cFileWriter.write("}"); /* function close */
		cFileWriter.newLine();
	}

	private static void generateCFileFor(TraceComponent tc) {
		TracePoint[] tps = tc.getTracePoints();
		int levels[];
		int assertDefaults[];
		if (tps.length > 0) {
			levels = new int[tps.length];
			assertDefaults = new int[tps.length];
		} else {
			levels = new int[1];
			assertDefaults = new int[1];
		}

		String componentName = tc.getComponentName();
		File cFile = new File(tc.getRelativePath() + tc.getCFileName());

		try (BufferedWriter cFileWriter = newWriter(cFile)) {
			cFileWriter.write("/* File generated by tracegen, do not edit */");
			cFileWriter.newLine();
			cFileWriter.write("#include \"" + tc.getHeaderFileName() + "\"");
			cFileWriter.newLine();
			cFileWriter.newLine();

			cFileWriter.newLine();

			cFileWriter.newLine();
			cFileWriter.write("#define UT_TRACE_VERSION 8");
			cFileWriter.newLine();
			cFileWriter.write("UtTraceVersionInfo " + componentName
					+ "_UtTraceVersionInfo = {UT_TRACE_VERSION};");
			cFileWriter.newLine();

			cFileWriter.write("#if defined(UT_TRACE_ENABLED_IN_BUILD)");
			cFileWriter.newLine();

			for (int i = 0; i < tps.length; i++) {
				TracePoint tp = tps[i];
				if (tp == null) {
					System.err.println("Error: bad tracepoint in component " + componentName + ", see previous messages");
					continue;
				}
				levels[i] = tp.getLevel();
				if (tp.typeNumber == TracePoint.TYPE_ASSERTION && tp.getLevel() == 1 ) {
					assertDefaults[i] = 1; // Enable default assertions prior to full trace initialisation.
				}
			}
			writeComponentDataOnStream(componentName, tc.getTraceCount(), levels, assertDefaults, cFileWriter, tc);

			cFileWriter.newLine();
			cFileWriter.newLine();
			cFileWriter.write("#else");
			cFileWriter.newLine();

			writeComponentDataForNonTraceEnabledBuildsOnStream(componentName, cFileWriter, tc);

			cFileWriter.newLine();

			cFileWriter.write("#endif /* defined(UT_TRACE_ENABLED_IN_BUILD) */");
			cFileWriter.newLine();

			/* include an initialisation function */
			cFileWriter.newLine();
			cFileWriter.write("#if defined(UT_DIRECT_TRACE_REGISTRATION)");
			cFileWriter.newLine();
			cFileWriter.newLine();
			writeRegistrationFunctionsOnStream(tc, cFileWriter, true);
			cFileWriter.newLine();
			cFileWriter.write("#else /* defined(UT_DIRECT_TRACE_REGISTRATION) */");
			cFileWriter.newLine();
			cFileWriter.newLine();
			writeRegistrationFunctionsOnStream(tc, cFileWriter, false);
			cFileWriter.newLine();
			cFileWriter.write("#endif /* defined(UT_DIRECT_TRACE_REGISTRATION) */");
			cFileWriter.newLine();
			cFileWriter.newLine();

			cFileWriter.write("/* End of generated file */");
			cFileWriter.newLine();
		} catch (IOException ioe) {
			System.err.println("Error writing trace c file for " + tc.getHeaderFileName());
			System.err.println(ioe);
		}
	}

	private static void writeToTraceFile(String stringToWrite, String fileName, Map<String, DATFileWriter> traceFileWriters)
			throws IOException {
		DATFileWriter tfw = traceFileWriters.get(fileName);
		if (tfw == null) {
			if (verbose) {
				System.out.println("TraceGen creating dat file: " + fileName);
			}
			tfw = new DATFileWriter(fileName);
			traceFileWriters.put(fileName, tfw);
		}
		tfw.println(stringToWrite);
	}

	private static final class DATFileWriter {

		private String fileName;

		private PrintWriter printWriter;

		DATFileWriter(String fileName) throws IOException {
			super();
			this.fileName = fileName;
			this.printWriter = new PrintWriter(newWriter(new File(fileName)));

			initializeDATFile();
		}

		private void initializeDATFile() {
			println(RAS_MAJOR_VERSION + "." + RAS_MINOR_VERSION);
		}

		void println(String s) {
			if (printWriter == null) {
				throw new DATFileWriterError(fileName + " Cannot write to DATFileWriter - printWriter is null");
			}

			// Force DOS line endings in DAT files regardless of which platform we're on.
			printWriter.print(s);
			printWriter.print("\r\n");
		}

		void close() {
			if (printWriter != null) {
				printWriter.close();
				printWriter = null;
			}
		}

	}

	private static class DATFileWriterError extends Error {

		private static final long serialVersionUID = 1L;

		DATFileWriterError(String message) {
			super(message);
		}

	}

	static double getVersion() {
		String verStr = RAS_MAJOR_VERSION + "." + RAS_MINOR_VERSION;

		return Double.parseDouble(verStr);
	}

}
