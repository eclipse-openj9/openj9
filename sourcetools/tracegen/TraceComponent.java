/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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

import java.util.ArrayList;
import java.util.Hashtable;
import java.util.List;
import java.util.Map;
import java.util.StringTokenizer;

@SuppressWarnings("nls")
public class TraceComponent {
	private final String componentName;
	private int count;
	private List<TracePoint> tracepoints = new ArrayList<>();
	private String headerfile;
	private String includefile;
	private String cfilefordata;
	private String relativepath;
	private String cfileoutputpath;
	private boolean isValid;
	private Map<String, TraceGroup> groupsTable = new Hashtable<>();
	private String datFileName;
	private String[] submodules = new String[0];
	private boolean isAuxiliary;

	public TraceComponent(String componentName, String headerfile, String includefile, String cfilename, String relativepath, String datFileName){
		super();
		this.count = 0;
		this.componentName = componentName;
		this.headerfile = headerfile;
		this.includefile = includefile;
		this.isValid = true;
		this.cfilefordata = cfilename;
		this.relativepath = relativepath;
		this.datFileName = datFileName;
		this.cfileoutputpath = relativepath;

		groupsTable.put("Entry", new TraceGroup("Entry"));
		groupsTable.put("Exit", new TraceGroup("Exit"));
		groupsTable.put("Event", new TraceGroup("Event"));
		groupsTable.put("Exception", new TraceGroup("Exception"));
		groupsTable.put("Debug", new TraceGroup("Debug"));
	}

	public String getDatFileName() {
		return datFileName;
	}

	public void setDatFileName(String datFileName) {
		this.datFileName = datFileName;
	}

	public int getTraceCount() {
		return count;
	}

	public synchronized int addTracePoint(TracePoint tp) {
		tracepoints.add(tp);
		count++;
		return 0;
	}

	public synchronized TracePoint[] getTracePoints() {
		TracePoint ret[] = new TracePoint[count];
		Object[] o = tracepoints.toArray();
		for (int i = 0; i < count; i++) {
			ret[i] = (TracePoint) o[i];
		}
		return ret;
	}

	public String getHeaderFileName() {
		return headerfile;
	}

	public String getIncludeFileName() {
		return includefile;
	}

	public String getComponentName() {
		return componentName;
	}

	public String getRelativePath() {
		return relativepath;
	}

	public void setRelativePath(String relativePath) {
		this.relativepath = relativePath;
	}

	public String getCFileName() {
		return cfilefordata;
	}

	@Override
	public String toString() {
		return componentName;
	}

	public void invalidate() {
		isValid = false;
	}

	public boolean isValid() {
		return isValid;
	}

	public void merge(TraceComponent tc) {
		TracePoint[] tps = tc.getTracePoints();
		TracePoint newtp = null;
		int startingvalue = count;

		for (int i = 0; i < tps.length; i++) {
			newtp = new TracePoint(tps[i], startingvalue + i);
			this.addTracePoint(newtp);
		}
	}

	public TraceGroup[] getGroups() {
		return groupsTable.values().toArray(new TraceGroup[groupsTable.size()]);
	}

	public void calculateGroups() {
		/* if (groups != null) {
		    System.err.println("Error - component " + componentName + " has been asked to calculate its groups twice.");
		    return null;
		} */
		TracePoint[] tps = getTracePoints();
		String type = null;
		String[] groups = null;
		TraceGroup grp;

		for (int i = 0; i < tps.length; i++) {
			if (tps[i] == null) {
				System.err.println("Error: bad tracepoint in component " + getComponentName() + ", see previous messages");
				continue;
			}
			type = tps[i].getType();
			groups = tps[i].getGroups();
			if (groups != null) {
				/* System.err.println("tp processing, " + componentName + " type = " + type + " group = " + group); */
				/* find a "group" group and add this tpid */
				for (int g = 0; g < groups.length; g++) {
					String group = groups[g];
					grp = groupsTable.get(group);
					if (grp == null) {
						grp = new TraceGroup(group);
						groupsTable.put(group, grp);
					}
					grp.addTPID(tps[i].getTPID());
				}
			}
			if (type != null) {
				/* find a group with the type name, and add this tpid */
				grp = groupsTable.get(type);
				if (grp == null) {
					grp = new TraceGroup(type);
					groupsTable.put(type, grp);
				}
				grp.addTPID(tps[i].getTPID());
			}
		}
	}

	public void setSubmodules(String string) {
		StringTokenizer tokenizer = new StringTokenizer(string, ",");
		String[] list = new String[tokenizer.countTokens()];
		for (int i = 0; tokenizer.hasMoreTokens(); i++) {
			list[i] = tokenizer.nextToken();
		}
		setSubmodules(list);
	}

	public void setSubmodules(String[] submodules) {
		this.submodules = submodules.clone();
	}

	public String[] getSubmodules() {
		return submodules.clone();
	}

	public String getHeaderFileDefine() {
		return "UTE_" + componentName.toUpperCase() + "_MODULE_HEADER";
	}

	public String getCFileOutputPath() {
		return cfileoutputpath;
	}

	public void setCFileOutputPath(String cfileoutputpath) {
		this.cfileoutputpath = cfileoutputpath;
	}

	public boolean isAuxiliary() {
		return isAuxiliary;
	}

	public void setAuxiliary() {
		isAuxiliary = true;
	}

}
