/*******************************************************************************
 * Copyright (c) 2007, 2019 IBM Corp. and others
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
package com.ibm.j9tools.om;

import java.io.PrintStream;
import java.text.NumberFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Collection;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

/**
 * {@link BuildInfo} is a container class that keeps track of J9 runtime settings such as:
 * 
 * <ul>
 * <li>Version Number</li>
 * <li>VM Branch</li>
 * <li>Stream Name</li>
 * <li>Stream Parent</li>
 * <li>Stream Split Date</li>
 * <li>Repository Branch</li>
 * <li>Runtime</li>
 * <li>FS Roots</li>
 * <li>JCL Configurations</li>
 * <li>J9 Projects</li>
 * <li>Assembly Builders</li>
 * </ul>
 * 
 * @author 	Maciek Klimkowski
 * @author 	Gabriel Castro
 * @author	Han Xu
 */
public class BuildInfo extends OMObject {
	final public static String DATE_FORMAT_PATTERN = "yyyy-MM-dd'T'HH:mm:ss.SSS"; //$NON-NLS-1$

	private String majorVersion = null;
	private String minorVersion = null;
	private String prefix = ""; //$NON-NLS-1$
	private String parentStream = null;
	private Calendar streamSplitDate = null;

	private String productName = null;
	private String productRelease = null;

	private String runtime = null;
	private boolean validateDefaultSizes = false;

	private final Map<String, String> repositoryBranches = new HashMap<String, String>();

	private final Map<String, String> fsroots = new HashMap<String, String>();
	private final Map<String, String> defaultSizes = new TreeMap<String, String>();
	private final Set<String> jcls = new TreeSet<String>();
	private final Set<String> projects = new TreeSet<String>();
	private final Set<String> asmBuilders = new TreeSet<String>();

	/**
	 * Sets the VM major version number.
	 * 
	 * @param 	majorVersion	the VM major version number
	 */
	public void setMajorVersion(String majorVersion) {
		this.majorVersion = majorVersion;
	}

	/**
	 * Retrieves the VM major version number.
	 * 
	 * @return	the major version number
	 */
	public String getMajorVersion() {
		return majorVersion;
	}

	/**
	 * Sets the VM minor version number.
	 * 
	 * @param 	minorVersion	the VM minor version number
	 */
	public void setMinorVersion(String minorVersion) {
		this.minorVersion = minorVersion;
	}

	/**
	 * Retrieves the VM minor version number.
	 * 
	 * @return	the minor version number
	 */
	public String getMinorVersion() {
		return minorVersion;
	}

	/**
	 * Retrieves the VM branch name.
	 * 
	 * @return	the branch name
	 */
	public String getVmBranch() {
		StringBuilder sb = new StringBuilder(getProducName());

		if (getProductRelease() != null && getProductRelease().length() != 0) {
			sb.append("_"); //$NON-NLS-1$
			sb.append(getProductRelease());
		}
		return sb.toString();
	}

	public void setPrefix(String prefix) {
		this.prefix = prefix;
	}

	public String getPrefix() {
		return prefix;
	}

	/**
	 * Retrieves the name of this VM stream.
	 * 
	 * @return	the name of this VM stream
	 */
	public String getStreamName() {
		return prefix + majorVersion + '.' + minorVersion + ' ' + getVmBranch();
	}

	public String getStreamId() {
		return prefix + majorVersion + minorVersion + '_' + getVmBranch();
	}

	/**
	 * Retrieves the stream from which the current stream was split from.
	 * 
	 * @return	the parent stream
	 */
	public String getParentStream() {
		return parentStream;
	}

	/**
	 * Sets the stream from which the current stream was split from.
	 * 
	 * @param 	parentStream		the parent stream name
	 */
	public void setParentStream(String parentStream) {
		this.parentStream = parentStream;
	}

	/**
	 * Retrieves the date of the stream split.
	 * 
	 * @return	the date of the stream split
	 */
	public Date getStreamSplitDate() {
		return streamSplitDate.getTime();
	}

	/**
	 * Converts the stream split date to a valid XML dateTime.
	 * 
	 * @return	the stream split date in XML format
	 */
	public String getXMLStreamSplitDate() {
		// Create a formater with the proper timezone
		SimpleDateFormat formater = new SimpleDateFormat(DATE_FORMAT_PATTERN);
		formater.setTimeZone(streamSplitDate.getTimeZone());
		StringBuilder sb = new StringBuilder(formater.format(streamSplitDate.getTime()));

		// Format timezone as per the XML spec (+|-)zz:zz
		int offset = streamSplitDate.getTimeZone().getRawOffset();
		if (offset < 0) {
			sb.append('-');
			offset = -offset;
		} else {
			sb.append('+');
		}

		NumberFormat nf = NumberFormat.getInstance();
		nf.setMinimumIntegerDigits(2);

		sb.append(nf.format(offset / 3600000));
		sb.append(':');
		sb.append(nf.format((offset % 3600000) / 60000));

		return sb.toString();
	}

	/**
	 * Sets the date of the stream split.
	 * 
	 * @param 	date		the date of the stream split
	 */
	public void setStreamSplitDate(Calendar date) {
		this.streamSplitDate = date;
	}

	/**
	 * Sets the product name for this build info.
	 * 
	 * @param 	name	the product name
	 */
	public void setProductName(String name) {
		this.productName = name;
	}

	/**
	 * Returns the name of the product defined in this build info. If no product name is defined
	 * it returns "Unknown".
	 * 
	 * @return	the name of the product
	 */
	public String getProducName() {
		return (productName == null) ? "Unknown" : productName; //$NON-NLS-1$
	}

	/**
	 * Sets the release name for this build info.
	 * 
	 * @param 	release		the release name
	 */
	public void setProductRelease(String release) {
		this.productRelease = release;
	}

	/**
	 * Returns the release name defined in this build info, or <code>null</code> if none is defined.
	 * 
	 * @return	the release name
	 */
	public String getProductRelease() {
		return productRelease;
	}

	/**
	 * Returns the product information, combining the product name and the product release if one is set.
	 * 
	 * @return	the combination of product name and release
	 */
	public String getProduct() {
		StringBuilder sb = new StringBuilder(getProducName());

		if (getProductRelease() != null && getProductRelease().length() != 0) {
			sb.append(" "); //$NON-NLS-1$
			sb.append(getProductRelease());
		}
		return sb.toString().trim();
	}

	/**
	 * Adds a repository name to the list.
	 * 
	 * @param 	id		the branch type ID
	 * @param 	name	the branch name
	 */
	public void addRepositoryBranch(String id, String name) {
		repositoryBranches.put(id, name);
	}

	/**
	 * Retrieves the name of the repository branch for a given key.
	 * 
	 * @return	the repository branch name
	 */
	public String getRepositoryBranch(String id) {
		return repositoryBranches.get(id);
	}

	/**
	 * Retrieves the list of repository keys.
	 * 
	 * @return	the repository keys
	 */
	public Set<String> getRepositoryBranchIds() {
		return repositoryBranches.keySet();
	}

	/**
	 * Sets the runtime name for this J9 runtime settings (ie Java, PHP, etc).  The
	 * name must be unique between all runtime settings.
	 * 
	 * @param 	runtime		the runtime name
	 */
	public void setRuntime(String runtime) {
		this.runtime = runtime;
	}

	/**
	 * Retrieves the runtime name for this J9 runtime settings (ie Java, PHP, etc).
	 * 
	 * @return	the name of the runtime
	 */
	public String getRuntime() {
		return runtime;
	}

	/**
	 * Adds a file system root to this runtime.
	 * 
	 * @param 	fsname 		Name of the file system 
	 * @param 	fsroot		path to root of the build
	 */
	public void addFSRoot(String fsname, String fsroot) {
		fsroots.put(fsname, fsroot);
	}

	/**
	 * Retrieves a file system root.
	 * 
	 * @param 	fsname		the name of the file system
	 * @return	path to the root of the build
	 */
	public String getFSRoot(String fsname) {
		return fsroots.get(fsname);
	}

	/**
	 * Retrieves the file system root IDs.
	 * 
	 * @return	the IDs
	 */
	public Set<String> getFSRootsIds() {
		return fsroots.keySet();
	}

	/**
	 * Retrieves the list of file system roots.
	 * 
	 * @return	the file system roots
	 */
	public Collection<String> getFSRoots() {
		return fsroots.values();
	}

	public void addDefaultSize(String id, String value) {
		defaultSizes.put(id, value);
	}

	public String getDefaultSize(String id) {
		return defaultSizes.get(id);
	}

	public Map<String, String> getDefaultSizes() {
		return Collections.unmodifiableMap(defaultSizes);
	}

	/**
	 * Adds a JCL profile to this runtime.
	 * 
	 * @param 	jcl			the name of the profile
	 */
	public void addJCL(String jcl) {
		jcls.add(jcl);
	}

	/**
	 * Retrieves the valid JCL profiles for this runtime.
	 * 
	 * @return	the JCL profiles
	 */
	public Set<String> getJCLs() {
		return jcls;
	}

	/**
	 * Adds a J9 source project to this runtime.
	 * 
	 * @param 	project		the name of the project
	 */
	public void addSource(String project) {
		projects.add(project);
	}

	/**
	 * Retrieves the name of the valid J9 source projects for this runtime.
	 * 
	 * @return	the valid J9 source projects
	 */
	public Set<String> getSources() {
		return projects;
	}

	/**
	 * Adds an assembly builder to this runtime.
	 * 
	 * @param asmBuilder
	 */
	public void addASMBuilder(String asmBuilder) {
		asmBuilders.add(asmBuilder);
	}

	/**
	 * Retrieves the valid assembly builders for this runtime
	 * 
	 * @return	the valid assembly builders
	 */
	public Set<String> getASMBuilders() {
		return asmBuilders;
	}

	public boolean validateDefaultSizes() {
		return validateDefaultSizes;
	}

	public void setValidateDefaultSizes(boolean validateDefaultSizes) {
		this.validateDefaultSizes = validateDefaultSizes;
	}

	/**
	 * Debug helper used to dump information about this J9 runtime settings.
	 * 
	 * @param 	out 			output stream to dump on
	 * @param 	prefix 			prefix to prepend to each line
	 * @param 	indentLevel		number of spaces to append to the prefix
	 */
	public void dump(PrintStream out, String prefix, int indentLevel) {
		StringBuffer indent = new StringBuffer(prefix);
		for (int i = 0; i < indentLevel; i++) {
			indent.append(' ');
		}

		out.println(indent + "Build Info"); //$NON-NLS-1$
		out.println(indent + " |--- Version          "); //$NON-NLS-1$
		out.println(indent + " |    |--- majorVersion:  " + this.getMajorVersion()); //$NON-NLS-1$
		out.println(indent + " |    |--- minorVersion:  " + this.getMinorVersion()); //$NON-NLS-1$
		out.println(indent + " |    |--- branch:        " + this.getVmBranch()); //$NON-NLS-1$
		out.println(indent + " |    |--- streamName:    " + this.getStreamName()); //$NON-NLS-1$
		out.println(indent + " |"); //$NON-NLS-1$
		out.println(indent + " |--- FS Roots"); //$NON-NLS-1$
		for (String fsname : fsroots.keySet()) {
			String fsroot = fsroots.get(fsname);
			out.println(indent + " |    |--- " + fsname + " " + fsroot); //$NON-NLS-1$ //$NON-NLS-2$
		}
		out.println(indent + " |"); //$NON-NLS-1$
		out.println(indent + " |--- JCLs"); //$NON-NLS-1$
		for (String jcl : jcls) {
			out.println(indent + " |    |--- " + jcl); //$NON-NLS-1$
		}
	}
}
