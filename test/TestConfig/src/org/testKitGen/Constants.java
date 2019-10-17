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
import java.util.Set;
import java.util.stream.Collectors;
import java.util.Arrays;

public final class Constants {
	public static final String PLAYLIST = "playlist.xml";
	public static final String MODESXML = "resources/modes.xml";
	public static final String OTTAWACSV = "resources/ottawa.csv";
	public static final String TESTMK = "autoGen.mk";
	public static final String DEPENDMK = "dependencies.mk";
	public static final String UTILSMK = "utils.mk";
	public static final String COUNTMK = "count.mk";
	public static final String SETTINGSMK = "settings.mk";
	public static final String HEADERCOMMENTS = "########################################################\n"
			+ "# This is an auto generated file. Please do NOT modify!\n"
			+ "########################################################\n\n";
	public static final List<String> ALLGROUPS = Arrays.asList("functional", "openjdk", "external", "perf", "jck",
			"system");
	public static final List<String> ALLIMPLS = Arrays.asList("openj9", "ibm", "hotspot", "sap");
	public static final List<String> ALLLEVELS = Arrays.asList("sanity", "extended", "special");
	public static final List<String> ALLTYPES = Arrays.asList("regular", "native");
	public static final Set<String> INGORESPECS = Arrays
			.asList("linux_x86-32_hrt", "linux_x86-64_cmprssptrs_gcnext", "linux_ppc_purec", "linux_ppc-64_purec",
					"linux_ppc-64_cmprssptrs_purec", "linux_x86-64_cmprssptrs_cloud")
			.stream().collect(Collectors.toSet());

	private Constants() {
	}
}