/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.jvm.trace.format.api;

import java.util.ArrayList;

/*
 * represents a trace component (eg: j9vm) and is responsible for holding on to
 * all known trace messages for the component. 
 */
public class Component {

	private String name;
	private byte nameBytes[];
	ArrayList messageList;
	private int base;

	public Component(String componentName) {
		name = componentName;
		nameBytes = componentName.getBytes();
		messageList = new ArrayList();
		base = -1;
	}

	public void setBase(int newBase) {
		base = newBase;
	}

	public String getName() {
		return name;
	}

	public void addMessage(Message msg) {
		messageList.add(msg);
	}

	public Message getMessageByID(int id) {
		if (id < 0) {
			return null;
		}
		if (base == -1) {
			if (id >= messageList.size()) {
				return null;
			}
			return (Message)messageList.get(id);
		} else {
			if (id - base >= messageList.size()) {
				return null;
			}
			return (Message)messageList.get(id - base);
		}
	}

	/*
	 * add a message at a given position. Pad the array if there are
	 * elements missing between the previous entry and this one.
	 */
	public void addMessage(Message msg, int id) {
		if (base == -1) {
			throw new Error("must set base before adding an IDed message");
		}
		int currSize = messageList.size();
		id -= base;
		for (int i = 0; i < (id + 1) - currSize; i++) {
			messageList.add(null);
		}
		messageList.add(id, msg);
		messageList.remove(id + 1);
	}

	/*
	 * compares otherName to this component. returns -1 if this is shorter
	 * than otherName or is alphabetically less than otherName returns 1 if
	 * this is longer than otherName or is alphabetically greater than
	 * otherName returns 0 if they are equal
	 */
	public int compareTo(byte otherName[]) {
		int len = nameBytes.length;
		if (len > otherName.length) {
			return 1;
		} else if (len < otherName.length) {
			return -1;
		} else {
			for (int i = 0; i < len; i++) {
				if (nameBytes[i] != otherName[i]) {
					return nameBytes[i] > otherName[i] ? 1 : -1;
				}
			}
			return 0;
		}
	}
}
