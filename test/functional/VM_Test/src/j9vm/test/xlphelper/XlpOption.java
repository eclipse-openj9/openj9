/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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

package j9vm.test.xlphelper;

public class XlpOption {
	
	/* Actual -Xlp option string to passed to JVM on command line */
	private String option = null;
	
	/* true if command with this -Xlp option can fail if the large pages are not supported */
	private boolean failIfLargePageSizeNotSupported = false;
	
	/* pageSize and pageType are used to check if warning message about using a different page size/type should be printed by JVM */
	private long pageSize = 0;
	private	String pageType = XlpUtil.XLP_PAGE_TYPE_NOT_USED;

	public XlpOption(String option) {
		this.option = option;
	}
	
	public XlpOption(String option, boolean canFail) {
		this(option);
		failIfLargePageSizeNotSupported = canFail;
	}
	
	public XlpOption(String option, long pageSize, String pageType, boolean canFail) {
		this(option, canFail);
		this.pageSize = pageSize;
		this.pageType = pageType;
	}
	
	public String getOption() {
		return option;
	}
	
	public boolean canFail() {
		return failIfLargePageSizeNotSupported;
	}
	
	public long getPageSize() {
		return pageSize;
	}
	
	public String getPageType() {
		return pageType;
	}
}
