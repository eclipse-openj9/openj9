/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import com.ibm.dtfj.javacore.parser.framework.Section;
import com.ibm.dtfj.javacore.parser.framework.parser.ISectionParser;
import com.ibm.dtfj.javacore.parser.framework.tag.ITagParser;
import com.ibm.dtfj.javacore.parser.j9.section.classloader.ClassLoaderSectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.classloader.ClassLoaderTagParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonTagParser;
import com.ibm.dtfj.javacore.parser.j9.section.environment.EnvironmentSectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.environment.EnvironmentTagParser;
import com.ibm.dtfj.javacore.parser.j9.section.memory.MemorySectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.memory.MemoryTagParser;
import com.ibm.dtfj.javacore.parser.j9.section.monitor.MonitorSectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.monitor.MonitorSovSectionParserPart;
import com.ibm.dtfj.javacore.parser.j9.section.monitor.MonitorTagParser;
import com.ibm.dtfj.javacore.parser.j9.section.nativememory.NativeMemorySectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.nativememory.NativeMemoryTagParser;
import com.ibm.dtfj.javacore.parser.j9.section.platform.PlatformSectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.platform.PlatformTagParser;
import com.ibm.dtfj.javacore.parser.j9.section.stack.StackSectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.stack.StackTagParser;
import com.ibm.dtfj.javacore.parser.j9.section.thread.ThreadSectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.thread.ThreadSovSectionParserPart;
import com.ibm.dtfj.javacore.parser.j9.section.thread.ThreadTagParser;
import com.ibm.dtfj.javacore.parser.j9.section.title.TitleSectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.title.TitleTagParser;


/**
 * Simulates loading different components from an external file. At the moment section parsers and their
 * corresponding tag parsers and sovereign extensions are hardcoded.
 *
 */
public class DTFJComponentLoader {
	
	public List loadSections()	{
		ArrayList sections = getSections();
		if (sections == null) {
			throw new NullPointerException("No java core sections loaded.");
		}
		J9TagManager.getCurrent().loadTagParsers(getTagParsers(sections));
		SovereignParserPartManager.getCurrent().loadSovParts(getSovParts(sections));
		return getSectionParsers(sections);	
	}

	/**
	 * 
	 * @param sections
	 * 
	 */
	private ArrayList getTagParsers(ArrayList sections) {
		ArrayList tagParsers = null;
		if (sections != null) {
			tagParsers = new ArrayList();
			for (Iterator it = sections.iterator(); it.hasNext();) {
				Section section = (Section)it.next();
				if (section != null) {
					ITagParser tagParser = section.getTagParser();
					if (tagParser != null && !tagParsers.contains(tagParser)) {
						tagParsers.add(tagParser);
					}
				}
			}
		}
		return tagParsers;
	}
	
	/**
	 * 
	 * @param sections
	 * 
	 */
	private ArrayList getSovParts(ArrayList sections) {
		ArrayList sovParts = null;
		if (sections != null) {
			sovParts = new ArrayList();
			for (Iterator it = sections.iterator(); it.hasNext();) {
				Section section = (Section)it.next();
				if (section != null) {
					SovereignSectionParserPart part = section.getSovPart();
					if (part != null && !sovParts.contains(part)) {
						sovParts.add(part);
					}
				}
			}
		}
		return sovParts;
	}
	
	/**
	 * 
	 * @param sections
	 * 
	 */
	private ArrayList getSectionParsers(ArrayList sections) {
		ArrayList sectionParsers = null;
		if (sections != null) {
			sectionParsers = new ArrayList();
			for (Iterator it = sections.iterator(); it.hasNext();) {
				Section section = (Section)it.next();
				if (section != null) {
					ISectionParser part = section.getSectionParser();
					if (part != null && !sectionParsers.contains(part)) {
						sectionParsers.add(part);
					}
				}
			}
		}
		return sectionParsers;
	}
	
	/**
	 * Hardcoded for now
	 * 
	 */
	private ArrayList getSections() {
		ArrayList loadedSections = new ArrayList();
		Section section = new Section(null, new CommonTagParser(), null);
		loadedSections.add(section);
		section = new Section(new TitleSectionParser(), new TitleTagParser(),  null);
		loadedSections.add(section);
		section = new Section(new PlatformSectionParser(), new PlatformTagParser(),  null);
		loadedSections.add(section);
		section = new Section(new EnvironmentSectionParser(), new EnvironmentTagParser(),  null);
		loadedSections.add(section);
		section = new Section(new NativeMemorySectionParser(), new NativeMemoryTagParser(), null);
		loadedSections.add(section);
		section = new Section(new MemorySectionParser(), new MemoryTagParser(),  null);
		loadedSections.add(section);
		section = new Section(new MonitorSectionParser(), new MonitorTagParser(), new MonitorSovSectionParserPart());
		loadedSections.add(section);
		section = new Section(new ThreadSectionParser(), new ThreadTagParser(),  new ThreadSovSectionParserPart());
		loadedSections.add(section);
		section = new Section(new StackSectionParser(), new StackTagParser(), null);
		loadedSections.add(section);
		section = new Section(new ClassLoaderSectionParser(), new ClassLoaderTagParser(),  null);
		loadedSections.add(section);
		
		return loadedSections;
	}
}
