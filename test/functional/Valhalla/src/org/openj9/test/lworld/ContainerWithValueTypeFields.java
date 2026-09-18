/*
 * Copyright IBM Corp. and others 2026
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.lworld;

/**
 * This is a placeholder version of {@code ContainerWithValueTypeFields} that is used
 * while building tests.  When tests are actually run, versions of this class that
 * are built by {@ref ValhallaAOTTestClassGenerator} are used.
 */
public class ContainerWithValueTypeFields {
	public ContainerWithValueTypeFields() {
		before = this;
		f0 = new Node(0);
		f1 = new Node(1);
		f2 = new Node(2);
		f3 = new Node(3);
		after = this;
		throw new RuntimeException("Did not expect this version of the class to be used");
	}

	public Object before;
	public Node f0;
	public Node f1;
	public Node f2;
	public Node f3;
	public Object after;
}
