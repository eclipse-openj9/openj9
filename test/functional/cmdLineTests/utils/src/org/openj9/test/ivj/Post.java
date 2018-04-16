/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

package org.openj9.test.ivj;

class Post {
	private String label;
	private Disk[] disks;
	private int numberOfDisks;

	Post(String label, int height) {
		this.label = label;
		this.disks = new Disk[height];
		this.numberOfDisks = 0;
	}

	void addDisk(Disk disk) {
		this.disks[(++this.numberOfDisks - 1)] = disk;
	}

	Disk[] getDisks() {
		return this.disks;
	}

	int getHeight() {
		return this.disks.length;
	}

	String getLabel() {
		return this.label;
	}

	int getNumberOfDisks() {
		return this.numberOfDisks;
	}

	void loadDisks() {
		unloadDisks();
		for (int i = this.disks.length; i > 0; --i)
			addDisk(new Disk(i));
	}

	Disk removeDisk() {
		this.numberOfDisks -= 1;
		return this.disks[this.numberOfDisks];
	}

	void unloadDisks() {
		this.numberOfDisks = 0;
	}
}
