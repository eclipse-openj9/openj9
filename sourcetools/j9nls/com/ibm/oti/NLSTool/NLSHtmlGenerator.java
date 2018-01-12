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
package com.ibm.oti.NLSTool;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Collections;
import java.util.Enumeration;
import java.util.Vector;

public class NLSHtmlGenerator {
	private File htmlFile;
	private FileWriter htmlWriter;
	
	public void generateHTML(Vector nlsInfos, String outFileName, String htmlFileName) throws IOException, FileNotFoundException {
		Collections.sort(nlsInfos);
		htmlFile = new File(htmlFileName);
		StringBuffer buffer = new StringBuffer("");

		writeHTMLHeader(buffer, outFileName);
		writeIndexTable(nlsInfos, buffer);
		buffer.append("<br>");
		writeMsgTable(nlsInfos, buffer);
		writeHTMLFooter(buffer);
		
		if (J9NLS.differentFromCopyOnDisk(htmlFileName, buffer)) {	
			htmlWriter = new FileWriter(htmlFile);
			htmlWriter.write(buffer.toString());
			htmlWriter.close();
		
			J9NLS.dp("** Generated " + htmlFile.getAbsolutePath());
		}
	}
	
	private void writeIndexTable(Vector nlsInfos, StringBuffer buffer) {
		buffer.append("<table border=\"1\" width=\"100%\" cellpadding=\"5\" cellspacing=\"0\">\n");
		buffer.append("<colgroup span=\"4\"</colgroup>\n");
		buffer.append("<tr>");
		buffer.append("<td>Module Name</td>\n");
		buffer.append("<td>Path</td>\n");
		buffer.append("<td>Header Name</td>\n");
		buffer.append("<td>Locales</td>\n");
		buffer.append("</tr>\n");
		for(Enumeration enumer = nlsInfos.elements(); enumer.hasMoreElements(); ) {
			NLSInfo nlsInfo = (NLSInfo)enumer.nextElement();
			Vector locales = sortLocales(nlsInfo.getLocales());
			buffer.append("<tr>\n");
			buffer.append("<td>");
			buffer.append("<a href=\"#" + nlsInfo.getModule() + "\">\n");
			buffer.append(nlsInfo.getModule() + "</a></td>\n");
			buffer.append("<td>" + nlsInfo.getPath() + "</td>\n");
			buffer.append("<td>" + nlsInfo.getHeaderName() + "</td>\n");
			buffer.append("<td>\n");
			for(Enumeration e = locales.elements(); e.hasMoreElements(); ) {
				String temp = (String)e.nextElement();
				String locale = getLocale(temp);
				buffer.append(locale);
				if(e.hasMoreElements())
					buffer.append(", ");
			}
			buffer.append("</td>\n");
			buffer.append("</tr>\n\n");
		}
		buffer.append("</table>");
	}
	
	private void writeMsgTable(Vector nlsInfos, StringBuffer buffer) {
		buffer.append("<table border=\"1\" width=\"100%\" cellpadding=\"5\" cellspacing=\"0\">\n");
		buffer.append("<colgroup span=\"4\"</colgroup>\n");
		buffer.append("<tr>");
		buffer.append("<td>Macro</td>\n");
		buffer.append("<td>ID</td>\n");
		buffer.append("<td>Key</td>\n");
		buffer.append("<td>Message</td>\n");
		buffer.append("</tr>\n\n");
		for(Enumeration enumer = nlsInfos.elements(); enumer.hasMoreElements(); ) {
			NLSInfo nlsInfo = (NLSInfo)enumer.nextElement();
			Vector msgInfos = nlsInfo.getMsgInfo();
			boolean firstMessage = true;
			for(Enumeration e = msgInfos.elements(); e.hasMoreElements(); ) {
				MsgInfo msgInfo = (MsgInfo)e.nextElement();
				writeMsgTable(msgInfo, buffer, firstMessage);
				firstMessage = false;
			}
		}
		buffer.append("</table>\n");
	}
	
	private void writeMsgTable(MsgInfo msgInfo, StringBuffer buffer, boolean firstMessage) {
		buffer.append("<tr>\n");
		buffer.append("<td>" + msgInfo.getMacro() + "</td>\n");
		buffer.append("<td>" + msgInfo.getId() + "</td>\n");
		buffer.append("<td>" + msgInfo.getKey() + "</td>\n");
		
		buffer.append("<td>");
		if(firstMessage)
			buffer.append("<a name=" + msgInfo.getMacro().substring(0,4) + ">");
		if(msgInfo.getMsg().equals("")) 
			buffer.append("<i>No Value</i>");
		else 
			buffer.append("<xmp>" + msgInfo.getMsg() + "</xmp>");

		if (msgInfo.containsDiagnostics()) {
			buffer.append("<p><b>Explanation: </b>" + msgInfo.getExplanation());
			buffer.append("<p><b>System Action: </b>" + msgInfo.getSystemAction());
			buffer.append("<p><b>User Response: </b>" + msgInfo.getUserResponse());
		}
		
		if(firstMessage)
			buffer.append("</a>");
		buffer.append("</td>\n");
		buffer.append("</tr>\n\n");
	}
			
	private void writeHTMLHeader(StringBuffer buffer, String outFileName) {
		buffer.append("<html>\n");
		buffer.append("<head>\n");
		buffer.append("<title>");
		buffer.append(outFileName);
		buffer.append("</title>\n");
		buffer.append("</head>\n");
		buffer.append("<body bgcolor=\"#C0C0C0\" link=black vlink=blue>");
	}
	
	private void writeHTMLFooter(StringBuffer buffer) {
		buffer.append("</body>\n");
		buffer.append("</html>");
	}
	
	private Vector sortLocales(Vector locales) {
		Vector sortedLocales = new Vector();
		if(locales.contains("")) {
			sortedLocales.addElement("");
			locales.remove("");
		}
		Collections.sort(locales);
		sortedLocales.addAll(locales);
		return sortedLocales;
	}
	
	private String getLocale(String nlsFileName) {
		if(nlsFileName.lastIndexOf('_') != -1)
			return nlsFileName.substring(nlsFileName.indexOf('_')+1);
		else 
			return "(default)";
	}
}
