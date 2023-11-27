/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
 *******************************************************************************/
package org.openj9.test.lworld;
import static org.openj9.test.lworld.ValueTypeTests.*;


public class ValueTypeTestClasses {
	static value class ZeroSizeValueType {
		public implicit ZeroSizeValueType();
	}

	static value class ZeroSizeValueTypeWrapper {
		ZeroSizeValueType! z;

		public implicit ZeroSizeValueTypeWrapper();
	}

	static class ClassTypePoint2D {
		final int x, y;

		ClassTypePoint2D(int x, int y) {
			this.x = x;
			this.y = y;
		}
	}

	static value class ValueTypePoint2D {
		final ValueInt! x, y;

		public implicit ValueTypePoint2D();

		ValueTypePoint2D(ValueInt! x, ValueInt! y) {
			this.x = x;
			this.y = y;
		}
	}

	static value class ValueTypeLongPoint2D {
		final ValueLong! x, y;

		public implicit ValueTypeLongPoint2D();

		ValueTypeLongPoint2D(long x, long y) {
			this.x = new ValueLong(x);
			this.y = new ValueLong(y);
		}
	}

	static value class ValueInt2 {
		int i;
		public implicit ValueInt2();
		ValueInt2(int i) { this.i = i; }
	}

	static class IntWrapper {
		IntWrapper(int i) { this.vti = new ValueInt(i); }
		final ValueInt! vti;
	}

	static value class ValueTypeWithLongField {
		ValueLong! l;

		public implicit ValueTypeWithLongField();
	}

	static value class ValueClassInt {
		int i;
		ValueClassInt(int i) { this.i = i; }
	}

	static value class ValueClassPoint2D {
		ValueClassInt x, y;

		ValueClassPoint2D(ValueClassInt x, ValueClassInt y) {
			this.x = x;
			this.y = y;
		}
	}
}
