/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package org.testKitGen;

import java.io.FileWriter;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;

public class UtilsGen {
	private static String utilsmk = Options.getProjectRootDir() + "/TestConfig/" + Constants.UTILSMK;
	private static String dependmk = Options.getProjectRootDir() + "/TestConfig/" + Constants.DEPENDMK;

	private UtilsGen() {
	}

	public static void start() {
		try {
			// TODO: combine two mk files
			genDependMk();
			genUtilsMk();
		} catch (IOException e) {
			e.printStackTrace();
			System.exit(1);
		}
	}

	private static void genDependMk() throws IOException {
		FileWriter f = new FileWriter(dependmk);

		f.write(Constants.HEADERCOMMENTS);

		List<String> allDisHead = Arrays.asList("", "disabled.", "echo.disabled.");
		for (String eachDisHead : allDisHead) {
			for (String eachLevel : Constants.ALLLEVELS) {
				for (String eachGroup : Constants.ALLGROUPS) {
					String hlgKey = eachDisHead + eachLevel + '.' + eachGroup;
					f.write(hlgKey + ":");
					for (String eachType : Constants.ALLTYPES) {
						String hlgtKey = eachDisHead + eachLevel + '.' + eachGroup + '.' + eachType;
						f.write(" \\\n" + hlgtKey);
					}
					f.write("\n\n.PHONY: " + hlgKey + "\n\n");
				}
			}

			for (String eachGroup : Constants.ALLGROUPS) {
				for (String eachType : Constants.ALLTYPES) {
					String gtKey = eachDisHead + eachGroup + '.' + eachType;
					f.write(gtKey + ":");
					for (String eachLevel : Constants.ALLLEVELS) {
						String lgtKey = eachDisHead + eachLevel + '.' + eachGroup + '.' + eachType;
						f.write(" \\\n" + lgtKey);
					}
					f.write("\n\n.PHONY: " + gtKey + "\n\n");
				}
			}

			for (String eachType : Constants.ALLTYPES) {
				for (String eachLevel : Constants.ALLLEVELS) {
					String ltKey = eachDisHead + eachLevel + '.' + eachType;
					f.write(ltKey + ":");
					for (String eachGroup : Constants.ALLGROUPS) {
						String lgtKey = eachDisHead + eachLevel + '.' + eachGroup + '.' + eachType;
						f.write(" \\\n" + lgtKey);
					}
					f.write("\n\n.PHONY: " + ltKey + "\n\n");
				}
			}

			for (String eachLevel : Constants.ALLLEVELS) {
				String lKey = eachDisHead + eachLevel;
				f.write(lKey + ":");
				for (String eachGroup : Constants.ALLGROUPS) {
					f.write(" \\\n" + eachDisHead + eachLevel + '.' + eachGroup);
				}
				f.write("\n\n.PHONY: " + lKey + "\n\n");
			}

			for (String eachGroup : Constants.ALLGROUPS) {
				String gKey = eachDisHead + eachGroup;
				f.write(gKey + ":");
				for (String eachLevel : Constants.ALLLEVELS) {
					f.write(" \\\n" + eachDisHead + eachLevel + '.' + eachGroup);
				}
				f.write("\n\n.PHONY: " + gKey + "\n\n");
			}

			for (String eachType : Constants.ALLTYPES) {
				String tKey = eachDisHead + eachType;
				f.write(tKey + ":");
				for (String eachLevel : Constants.ALLLEVELS) {
					f.write(" \\\n" + eachDisHead + eachLevel + '.' + eachType);
				}
				f.write("\n\n.PHONY: " + tKey + "\n\n");
			}

			String allKey = eachDisHead + "all";
			f.write(allKey + ":");
			for (String eachLevel : Constants.ALLLEVELS) {
				f.write(" \\\n" + eachDisHead + eachLevel);
			}
			f.write("\n\n.PHONY: " + allKey + "\n\n");
		}

		f.close();

		System.out.println();
		System.out.println("Generated " + dependmk);
	}

	private static void genUtilsMk() throws IOException {
		FileWriter f = new FileWriter(utilsmk);

		f.write(Constants.HEADERCOMMENTS);
		f.write("PLATFORM=\n");
		String plat = ModesDictionary.getPlat(Options.getSpec());
		if (!plat.isEmpty()) {
			f.write("ifeq" + " ($(SPEC)," + Options.getSpec() + ")\n\tPLATFORM=" + plat + "\nendif\n\n");
		}

		f.close();
		System.out.println();
		System.out.println("Generated " + utilsmk);
	}
}