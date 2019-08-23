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

import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Variation {
	private String variation;
	private String spec;
	private String platformRequirements;
	private String subTestName;
	private String mode;
	private String jvmOptions;
	private boolean isValid;

	public Variation(String subTestName, String variation, String platformRequirements) {
		this.subTestName = subTestName;
		this.spec = Options.getSpec();
		this.variation = variation;
		this.platformRequirements = platformRequirements;
		this.isValid = true;
		this.jvmOptions = "";
		parseVariation();
	}

	public String getVariation() {
		return this.variation;
	}

	public String getSubTestName() {
		return this.subTestName;
	}

	public String getMode() {
		return this.mode;
	}

	public String getJvmOptions() {
		return this.jvmOptions;
	}

	public boolean isValid() {
		return this.isValid;
	}

	private void parseVariation() {
		jvmOptions = " " + variation + " ";

		jvmOptions = jvmOptions.replaceAll(" NoOptions ", "");

		Pattern pattern = Pattern.compile(".* Mode([^ ]*?) .*");
		Matcher matcher = pattern.matcher(jvmOptions);

		if (matcher.matches()) {
			mode = matcher.group(1).trim();
			String clArgs = ModesDictionary.getClArgs(mode);
			List<String> invalidSpecs = ModesDictionary.getInvalidSpecs(mode);
			if (invalidSpecs.contains(spec)) {
				isValid = false;
			}
			jvmOptions = jvmOptions.replace("Mode" + mode, clArgs);
		}

		jvmOptions = jvmOptions.trim();

		isValid &= checkPlatformReq();
	}

	private boolean checkPlatformReq() {
		if ((platformRequirements != null) && (!platformRequirements.trim().isEmpty())) {
			for (String pr : platformRequirements.split("\\s*,\\s*")) {
				pr = pr.trim();
				String[] prSplitOnDot = pr.split("\\.");
				String fullSpec = spec;

				// Special case 32/31-bit specs which do not have 32 or 31 in the name (i.e.
				// aix_ppc)
				if (!spec.contains("-64")) {
					if (spec.contains("390")) {
						fullSpec = spec + "-31";
					} else {
						fullSpec = spec + "-32";
					}
				}

				if (prSplitOnDot[0].charAt(0) == '^') {
					if (fullSpec.contains(prSplitOnDot[1])) {
						return false;
					}
				} else {
					if (!fullSpec.contains(prSplitOnDot[1])) {
						return false;
					}
				}
			}
		}
		return true;
	}

}