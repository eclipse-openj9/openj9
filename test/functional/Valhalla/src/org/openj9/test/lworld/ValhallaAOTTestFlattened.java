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

import org.testng.annotations.Test;
import org.testng.Assert;

public class ValhallaAOTTestFlattened {
	public static final void main() {
		final int maxIters = 1000000;

		for (int i = 0; i < maxIters; i++) {
			ContainerWithValueTypeFields container = new ContainerWithValueTypeFields();
			if (!test(container)) throw new RuntimeException();

			String extraFieldValue = Integer.toString(i);
			ContainerWithValueTypeFieldsSubclass containerSub = new ContainerWithValueTypeFieldsSubclass(extraFieldValue);
			if (!test(containerSub, extraFieldValue)) throw new RuntimeException();
		}
	}

	/**
	 * Test that fields before and after a pair of fields inside {@link ContainerWithValueTypeFields}
	 * that are instances of a value type are accessed at the correct offsets.
	 * The value type fields might null-restricted and flattened.
	 */
	public static final boolean test(ContainerWithValueTypeFields container) {
		return (container.before == container) && (container.after == container);
	}

	/**
	 * Test that a field {@link ContainerWithValueTypeFieldsSubclass} that comes after fields
	 * inherited from its superclass is accessed at the correct offsets.
	 * Some fields inherited from the superclass might be null-restricted and flattened.
	 */
	public static final boolean test(ContainerWithValueTypeFieldsSubclass container, String extraFieldValue) {
		return (container.extraField == extraFieldValue);
	}
}
