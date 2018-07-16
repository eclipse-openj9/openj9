/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package j9vm.test.ddrext;

import j9vm.test.ddrext.util.CoreDumpGenerator;
import j9vm.test.ddrext.util.DDROutputStream;
import java.io.PrintStream;
import org.testng.log4testng.Logger;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive;

public class SetupConfig {
	private static Logger log = Logger.getLogger(SetupConfig.class);

	/* Handle for DDROutputStream class that manages the output buffer */
	private static DDROutputStream ps = null;

	/* Handle for DDRInteractive */
	private static DDRInteractive ddrInstance = null;

	/* Handle for DDR Context, used when we initiate the test suite as a ddr plugin*/
	private static Context ddrContext = null;

	public static synchronized DDROutputStream getPrintStream() {
		if (ps == null) {
			ps = new DDROutputStream(System.out);
		}
		return ps;
	}

	public static synchronized void setPrintStream(DDROutputStream ddrOutput) {
		ps = ddrOutput;
		return;
	}

	public static synchronized DDRInteractive getDDRInstance() {
		return ddrInstance;
	}

	public static synchronized void setDDRInstance(DDRInteractive ddr) {
		ddrInstance = ddr;
		return;
	}

	/**Creates a new DDRInteractive instance using the given core file
	 * @param coreFile - The full path of the core dump file to use
	 */
	private static synchronized void initDDRInteractive(String coreFile) {
		if (ddrInstance == null) {
			if (coreFile != null) {
				try {
					DDROutputStream ps = getPrintStream();
					ddrInstance = new DDRInteractive(coreFile, ps);
					log.info("Created new DDR Interactive instance using core file : "
							+ coreFile);
				} catch (Exception e) {
					log.error("Error loading core dump");
					ddrInstance = null;
					log.error(e.getMessage());
				}
			}
		}
	}

	public static synchronized boolean init(String javaHome, String j9vmSE60Jar) {
		// deleteCoreFile();
		CoreDumpGenerator dumpGenerator = new CoreDumpGenerator(javaHome,
				j9vmSE60Jar);
		String generatedCoreFile = dumpGenerator.generateDump();
		if (generatedCoreFile == null) {
			log.fatal("No dynamic dump created");
			return false;
		}
		initDDRInteractive(generatedCoreFile);
		if (ddrInstance == null) {
			log.fatal("Can not load dump file using DDR");
			return false;
		}
		return true;
	}

	/**
	 * This method is to be invoked when DDR Junit test framework is to be launched as a stand alone Java application
	 * @param core - Full path to the core file to launch DDR with
	 * @return - True if DDR was initialized correctly
	 */
	public static synchronized boolean init(String core) {
		if (core == null) {
			log.fatal("Core file is null");
			return false;
		}

		initDDRInteractive(core);
		if (ddrInstance == null) {
			log.fatal("Can not load dump file using DDR");
			return false;
		}
		return true;
	}

	/**
	 * This method is to be invoked when DDR Junit test framework is to be launched from a DDR Plugin implementation 
	 * @param ctx - DDR context 
	 * @param out - PrintStream object to which DDR output will be flushed
	 * @return
	 */
	public static synchronized boolean init(Context ctx, PrintStream out) {
		ddrContext = ctx;
		DDROutputStream ps = new DDROutputStream(out);
		setPrintStream(ps);
		return true;
	}

	public static synchronized void reset() {
		if (ddrInstance != null ) {
			try {
				ddrInstance.close();
			} catch (Exception e) {
				log.error("Error closing DDRInteractive");
				log.error(e.getMessage());
			}
		}
		ddrInstance = null;
	}

	public static Context getDDRContxt() {
		return ddrContext;
	}
}
