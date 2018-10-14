/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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
package org.openj9.test.contendedfields;

import java.lang.ref.WeakReference;
import java.util.HashMap;

import jdk.internal.vm.annotation.Contended;

public  class TestClasses {

	/* =========== Test classes ===============*/
	public static class ClassWithOneInt {
		int a;
	}

	public static class ClassWithTwoInt {
		int a;
		int b;
	}

	public static class ClassWithThreeInt {
		int a;
		int b;
		int c;
	}

	public static class ClassWithFourInt {
		int a;
		int b;
		int c;
		int d;
	}

	static class MultipleFieldFinalizableClass {
		public int intField;
		public long longField;
		public Object objectField;
		@Override
		protected void finalize() throws Throwable {
			hashCode(); /* placeholder */
			return;
		}
	}

	static class SingleFieldFinalizableClass {
		public int intField;
		@Override
		protected void finalize() throws Throwable {
			hashCode(); /* placeholder */
			return;
		}		
	}

	static class DoubleFieldFinalizableClass {
		public int intField;
		@Override
		protected void finalize() throws Throwable {
			hashCode(); /* placeholder */
			return;
		}		
	}

	static class SingleFieldClass {
		public int intField;
	}

	static class DoubleFieldClass {
		public int intField;
	}

	static class SubclassOfSingleFieldFinalizableClass extends SingleFieldFinalizableClass {
		public int intField;
	}

	static class SubclassOfDoubleFieldFinalizableClass extends DoubleFieldFinalizableClass {
		public long longField;
	}

	static class NonFinalizableSubclassOfSingleFieldFinalizableClass extends SingleFieldFinalizableClass {
		public void finalize(){}
		public int intField;
	}

	static class NonFinalizableSubclassOfDoubleFieldFinalizableClass extends DoubleFieldFinalizableClass {
		public void finalize(){}
		public long longField;
	}

	static class SubclassOfSingleFieldClass extends SingleFieldClass {
		public Object objectField;
	}

	static class SubclassOfDoubleFieldClass extends DoubleFieldClass {
		public long longField;
		public Object objectField;
	}

	static class FinalizableSubclassOfSingleFieldClass extends SingleFieldClass {
		public int intField;
		public Object objectField;
		@Override
		protected void finalize() throws Throwable {
			hashCode(); /* placeholder */
			return;
		}		
	}

	static class FinalizableSubclassOfDoubleFieldClass extends DoubleFieldClass {
		public int intField;
		public long longField;
		@Override
		protected void finalize() throws Throwable {
			hashCode(); /* placeholder */
			return;
		}		
	}

	static class FinalizableSubclassOfSingleFieldFinalizableClass extends SingleFieldFinalizableClass {
		public int intField;
		public Object objectField;
		@Override
		protected void finalize() throws Throwable {
			hashCode(); /* placeholder */
			return;
		}		
	}

	static class FinalizableSubclassOfDoubleFieldFinalizableClass extends DoubleFieldFinalizableClass {
		public int intField;
		public long longField;
		@Override
		protected void finalize() throws Throwable {
			hashCode(); /* placeholder */
			return;
		}		
	}

	static class OneDoubleFinalizableReferenceClass extends WeakReference<Object> {
		public long longField;
		
		public OneDoubleFinalizableReferenceClass(Object r) {
			super(r);
		}
		protected void finalize() throws Throwable {
			hashCode(); /* placeholder */
			return;
		}		
	}

	static class OneDoubleOneSingleReferenceClass extends WeakReference<Object> {
		public long longField;
		public int intField;
		
		public OneDoubleOneSingleReferenceClass(Object r) {
			super(r);
		}
	}

	static class OneDoubleReferenceClass extends WeakReference<Object> {
		public long longField;
		
		public OneDoubleReferenceClass(Object r) {
			super(r);
		}
	}

	static class OneSingleReferenceClass extends WeakReference<Object> {
		public int intField;
		
		public OneSingleReferenceClass(Object r) {
			super(r);
		}
	}

	static class OneDoubleOneSingleFinalizableReferenceClass extends WeakReference<Object> {
		public long longField;
		public int intField;
		
		public OneDoubleOneSingleFinalizableReferenceClass(Object r) {
			super(r);
		}
		protected void finalize() throws Throwable {
			hashCode(); /* placeholder */
			return;
		}		
	}

	static class MultipleContFieldClass {
		@Contended
		public int intField;
		public long longField;
		@Contended
		public Object objectField;
	}

	static class OneContFieldClass {
		@Contended
		public int intField1;
	}

	static class TwoContFieldClass {
		@Contended
		public int intField1;
		@Contended
		public int intField2;
	}

	static class ThreeContFieldClass {
		@Contended
		public int intField1;
		@Contended
		public int intField2;
		@Contended
		public int intField3;
	}

	static class ContGroupClass {
		@Contended("g1")
		public int intField1;
		@Contended("g1")
		public int intField2;
		@Contended("g1")
		public int intField3;
	}

	@Contended
	static class ContGroupContClass {
		@Contended("g1")
		public int intField1;
		@Contended("g1")
		public int intField2;
		@Contended("g1")
		public int intField3;
	}

	static class ContGroupsClass {
		@Contended("g1")
		public int intField1;
		@Contended("g2")
		public int intField2;
		@Contended("g3")
		public int intField3;
		@Contended("g3")
		public long longField;
		public Object ObjectField3;
		@Contended("g2")
		public Object ObjectField2;
		@Contended("g1")
		public Object ObjectField1;
	}

	@Contended
	static class ContGroupsContClass {
		@Contended("g1")
		public int intField1;
		@Contended("g2")
		public int intField2;
		@Contended("g3")
		public int intField3;
		@Contended("g3")
		public long longField;
		public Object ObjectField3;
		@Contended("g2")
		public Object objectField;
		public Object ObjectField2;
		@Contended("g1")
		public Object ObjectField1;
	}

	@Contended
	static class MultipleFieldContClass {
		public int intField;
		public long longField;
		public Object objectField;
		public int intField1;
		public int intField2;
		public int intField3;
	}

	@Contended
	static class MultipleContFieldContClass {
		@Contended
		public int intField;
		public long longField;
		@Contended
		public Object objectField;
		public Object objectField2;
	}

	@Contended
	static class FinalizableMultipleFieldContClass {
		public int intField;
		public long longField;
		public Object objectField;
		@Override
		protected void finalize() throws Throwable {
			return;
		}		
	}

	static class ClassWithManyFields {
		public long long1;
		public long long2;
		public long long3;
		public long long4;
		public long long5;
		public long long6;
		public long long7;
		public long long8;
		public long long9;
		public long long10;
		public long long11;
		public long long12;
		public long long13;
		public long long14;
		public long long15;
	}

	static class ContendedSubclassOfClassWithManyFields extends ClassWithManyFields {
		public int intField;
		public long longField;
		public Object objectField;
		
	}

	static class ContendedSubclassWithmanyFieldsOfClassWithManyFields extends ClassWithManyFields {
		public int intField;
		public long longField;
		public Object objectField;
		public long long1a;
		public long long2a;
		public long long3a;
		public long long4a;
		public long long5a;
		public long long6a;
		public long long7a;
		public long long8a;
		public long long9a;
		public long long10a;
		public long long11a;
		public long long12a;
		public long long13a;
		public long long14a;
		public long long15a;
		
	}

	@SuppressWarnings({ "rawtypes", "serial" })
	static class SubclassWithLockwordOfClassWithNoLockword extends HashMap {
		public int intField;
		public long longField;
		public Object objectField;
	}

}
