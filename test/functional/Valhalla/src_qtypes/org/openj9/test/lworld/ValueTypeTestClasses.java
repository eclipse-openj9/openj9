/*
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
 */
package org.openj9.test.lworld;

import jdk.internal.vm.annotation.ImplicitlyConstructible;
import jdk.internal.vm.annotation.NullRestricted;

public class ValueTypeTestClasses {

	@ImplicitlyConstructible
	static value class ValueTypeInt {
		ValueTypeInt(int i) { this.i = i; }
		final int i;
	}

	static value class ValueClassInt {
		ValueClassInt(int i) { this.i = i; }
		final int i;
	}

	@ImplicitlyConstructible
	static value class ValueTypeLong {
		final long l;
		ValueTypeLong(long l) {this.l = l;}
		public long getL() {
			return l;
		}
	}

	@ImplicitlyConstructible
	static value class ValueTypePoint2D {
		@NullRestricted
		final ValueTypeInt x, y;

		ValueTypePoint2D(ValueTypeInt x, ValueTypeInt y) {
			this.x = x;
			this.y = y;
		}
	}

	static value class ValueClassPoint2D {
		final ValueClassInt x, y;

		ValueClassPoint2D(ValueClassInt x, ValueClassInt y) {
			this.x = x;
			this.y = y;
		}
	}

	static class IntWrapper {
		IntWrapper(int i) { this.vti = new ValueTypeInt(i); }
		@NullRestricted
		final ValueTypeInt vti;
	}

	static class Point2D {
		final int x, y;

		Point2D(int x, int y) {
			this.x = x;
			this.y = y;
		}
	}

	@ImplicitlyConstructible
	static value class ValueTypeWithLongField {
		@NullRestricted
		final ValueTypeLong l;

		ValueTypeWithLongField() {
			l = new ValueTypeLong(1);
		}
	}

	@ImplicitlyConstructible
	static value class ValueTypeLongPoint2D {
		@NullRestricted
		final ValueTypeLong x, y;

		ValueTypeLongPoint2D(long x, long y) {
			this.x = new ValueTypeLong(x);
			this.y = new ValueTypeLong(y);
		}
	}

	@ImplicitlyConstructible
	static value class ZeroSizeValueType {
		ZeroSizeValueType() {}
	}

	@ImplicitlyConstructible
	static value class ZeroSizeValueTypeWrapper {
		@NullRestricted
		final ZeroSizeValueType z;

		ZeroSizeValueTypeWrapper() {
			z = new ZeroSizeValueType();
		}
	}

	@ImplicitlyConstructible
	static value class ValueTypeInt2 {
		ValueTypeInt2(int i) { this.i = i; }
		final int i;
	}

	@ImplicitlyConstructible
	static value class ValueTypeFastSubVT {
		final int x,y,z;
		final Object[] arr;
		ValueTypeFastSubVT(int x, int y, int z, Object[] arr) {
			this.x = x;
			this.y = y;
			this.z = z;
			this.arr = arr;
		}
	}

	@ImplicitlyConstructible
	static value class ValueTypeDoubleLong {
		@NullRestricted
		final ValueTypeLong l;
		final long l2;
		ValueTypeDoubleLong(ValueTypeLong l, long l2) {
			this.l = l;
			this.l2 = l2;
		}
		public ValueTypeLong getL() {
			return l;
		}
		public long getL2() {
			return l2;
		}
	}

	@ImplicitlyConstructible
	static value class ValueTypeQuadLong {
		@NullRestricted
		final ValueTypeDoubleLong l;
		@NullRestricted
		final ValueTypeLong l2;
		final long l3;
		ValueTypeQuadLong(ValueTypeDoubleLong l, ValueTypeLong l2, long l3) {
			this.l = l;
			this.l2 = l2;
			this.l3 = l3;
		}
		public ValueTypeDoubleLong getL() {
			return l;
		}
		public ValueTypeLong getL2() {
			return l2;
		}
		public long getL3() {
			return l3;
		}
	}

	@ImplicitlyConstructible
	static value class ValueTypeDoubleQuadLong {
		@NullRestricted
		final ValueTypeQuadLong l;
		@NullRestricted
		final ValueTypeDoubleLong l2;
		@NullRestricted
		final ValueTypeLong l3;
		final long l4;
		ValueTypeDoubleQuadLong(ValueTypeQuadLong l, ValueTypeDoubleLong l2, ValueTypeLong l3, long l4) {
			this.l = l;
			this.l2 = l2;
			this.l3 = l3;
			this.l4 = l4;
		}
		public ValueTypeQuadLong getL() {
			return l;
		}
		public ValueTypeDoubleLong getL2() {
			return l2;
		}
		public ValueTypeLong getL3() {
			return l3;
		}
		public long getL4() {
			return l4;
		}
	}
}
