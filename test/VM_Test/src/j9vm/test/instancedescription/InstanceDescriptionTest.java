/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
/*
 * Created on Jul 12, 2004
 */
package j9vm.test.instancedescription;

/**
 * @author Paul Church
 * 
 * Test the instance description bits in the ram class by creating
 * and GC'ing classes with a variety of int/object field configurations.
 * The interesting cases are around 30-33 and 63-65 fields.
 * 
 * This test will likely crash the vm rather than report failure if something
 * is wrong.
 */
public class InstanceDescriptionTest {
	public static void main( String args[] ) {
		testHarness( i30.class );
		System.gc();
		testHarness( i31.class );
		System.gc();
		testHarness( i32.class );
		System.gc();
		testHarness( i33.class );
		System.gc();
		testHarness( o30.class );
		System.gc();
		testHarness( o31.class );
		System.gc();
		testHarness( o32.class );
		System.gc();
		testHarness( o33.class );
		System.gc();
		testHarness( m30.class );
		System.gc();
		testHarness( m31.class );
		System.gc();
		testHarness( m32.class );
		System.gc();
		testHarness( m33.class );
		System.gc();
		testHarness( e63a.class );
		System.gc();
		testHarness( e63b.class );
		System.gc();
		testHarness( e63c.class );
		System.gc();
		testHarness( e64a.class );
		System.gc();
		testHarness( e64b.class );
		System.gc();
		testHarness( e64c.class );
		System.gc();
		testHarness( e65a.class );
		System.gc();
		testHarness( e65b.class );
		System.gc();
		testHarness( e65c.class );
		System.gc();
	}

	public static void testHarness( Class clazz ) {
		Object[] a = new Object[1000];
		Object foo;
		int i;

		try {
			for( i = 0; i < a.length; i++ ) {
				a[i] = clazz.newInstance();
				foo = clazz.newInstance();
			}
			System.gc();
		} catch( Exception e ) {
			System.out.println( "***FAILED***" );
			e.printStackTrace();
			throw new RuntimeException();
		}
	}
}

class i30 {
	int f1=1, f2=1, f3=1, f4=1, f5=1, f6=1, f7=1, f8=1, f9=1, f10=1;
	int f11=1, f12=1, f13=1, f14=1, f15=1, f16=1, f17=1, f18=1, f19=1, f20=1;
	int f21=1, f22=1, f23=1, f24=1, f25=1, f26=1, f27=1, f28=1, f29=1, f30=1;
}
class i31 {
	int f1=1, f2=1, f3=1, f4=1, f5=1, f6=1, f7=1, f8=1, f9=1, f10=1;
	int f11=1, f12=1, f13=1, f14=1, f15=1, f16=1, f17=1, f18=1, f19=1, f20=1;
	int f21=1, f22=1, f23=1, f24=1, f25=1, f26=1, f27=1, f28=1, f29=1, f30=1;
	int f31=1;
}
class i32 {
	int f1=1, f2=1, f3=1, f4=1, f5=1, f6=1, f7=1, f8=1, f9=1, f10=1;
	int f11=1, f12=1, f13=1, f14=1, f15=1, f16=1, f17=1, f18=1, f19=1, f20=1;
	int f21=1, f22=1, f23=1, f24=1, f25=1, f26=1, f27=1, f28=1, f29=1, f30=1;
	int f31=1, f32=1;
}
class i33 {
	int f1=1, f2=1, f3=1, f4=1, f5=1, f6=1, f7=1, f8=1, f9=1, f10=1;
	int f11=1, f12=1, f13=1, f14=1, f15=1, f16=1, f17=1, f18=1, f19=1, f20=1;
	int f21=1, f22=1, f23=1, f24=1, f25=1, f26=1, f27=1, f28=1, f29=1, f30=1;
	int f31=1, f32=1, f33=1;
}
class o30 {
	Object f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
	Object f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
	Object f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
	public o30() {
		f1 = new Object();
		f2 = new Object();
		f3 = new Object();
		f4 = new Object();
		f5 = new Object();
		f6 = new Object();
		f7 = new Object();
		f8 = new Object();
		f9 = new Object();
		f10 = new Object();
		f11 = new Object();
		f12 = new Object();
		f13 = new Object();
		f14 = new Object();
		f15 = new Object();
		f16 = new Object();
		f17 = new Object();
		f18 = new Object();
		f19 = new Object();
		f20 = new Object();
		f21 = new Object();
		f22 = new Object();
		f23 = new Object();
		f24 = new Object();
		f25 = new Object();
		f26 = new Object();
		f27 = new Object();
		f28 = new Object();
		f29 = new Object();
		f30 = new Object();
	}
}
class o31 {
	Object f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
	Object f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
	Object f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
	Object f31;
	public o31() {
		f1 = new Object();
		f2 = new Object();
		f3 = new Object();
		f4 = new Object();
		f5 = new Object();
		f6 = new Object();
		f7 = new Object();
		f8 = new Object();
		f9 = new Object();
		f10 = new Object();
		f11 = new Object();
		f12 = new Object();
		f13 = new Object();
		f14 = new Object();
		f15 = new Object();
		f16 = new Object();
		f17 = new Object();
		f18 = new Object();
		f19 = new Object();
		f20 = new Object();
		f21 = new Object();
		f22 = new Object();
		f23 = new Object();
		f24 = new Object();
		f25 = new Object();
		f26 = new Object();
		f27 = new Object();
		f28 = new Object();
		f29 = new Object();
		f30 = new Object();
		f31 = new Object();
	}
}
class o32 {
	Object f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
	Object f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
	Object f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
	Object f31, f32;
	public o32() {
		f1 = new Object();
		f2 = new Object();
		f3 = new Object();
		f4 = new Object();
		f5 = new Object();
		f6 = new Object();
		f7 = new Object();
		f8 = new Object();
		f9 = new Object();
		f10 = new Object();
		f11 = new Object();
		f12 = new Object();
		f13 = new Object();
		f14 = new Object();
		f15 = new Object();
		f16 = new Object();
		f17 = new Object();
		f18 = new Object();
		f19 = new Object();
		f20 = new Object();
		f21 = new Object();
		f22 = new Object();
		f23 = new Object();
		f24 = new Object();
		f25 = new Object();
		f26 = new Object();
		f27 = new Object();
		f28 = new Object();
		f29 = new Object();
		f30 = new Object();
		f31 = new Object();
		f32 = new Object();
	}
}
class o33 {
	Object f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
	Object f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
	Object f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
	Object f31, f32, f33;
	public o33() {
		f1 = new Object();
		f2 = new Object();
		f3 = new Object();
		f4 = new Object();
		f5 = new Object();
		f6 = new Object();
		f7 = new Object();
		f8 = new Object();
		f9 = new Object();
		f10 = new Object();
		f11 = new Object();
		f12 = new Object();
		f13 = new Object();
		f14 = new Object();
		f15 = new Object();
		f16 = new Object();
		f17 = new Object();
		f18 = new Object();
		f19 = new Object();
		f20 = new Object();
		f21 = new Object();
		f22 = new Object();
		f23 = new Object();
		f24 = new Object();
		f25 = new Object();
		f26 = new Object();
		f27 = new Object();
		f28 = new Object();
		f29 = new Object();
		f30 = new Object();
		f31 = new Object();
		f32 = new Object();
		f33 = new Object();
	}
}
class m30 {
	int f1=1, f2=1, f3=1, f4=1, f5=1, f6=1, f7=1, f8=1, f9=1, f10=1;
	int f11=1, f12=1, f13=1, f14=1, f15=1, f16=1, f17=1, f18=1, f19=1, f20=1;
	int f21=1, f22=1, f23=1, f24=1, f25=1, f26=1, f27=1, f28=1, f29=1;
	Object f30;
	public m30() {
		f30 = new Object();
	}
}
class m31 {
	int f1=1, f2=1, f3=1, f4=1, f5=1, f6=1, f7=1, f8=1, f9=1, f10=1;
	int f11=1, f12=1, f13=1, f14=1, f15=1, f16=1, f17=1, f18=1, f19=1, f20=1;
	int f21=1, f22=1, f23=1, f24=1, f25=1, f26=1, f27=1, f28=1, f29=1, f30=1;
	Object f31;
	public m31() {
		f31 = new Object();
	}
}
class m32 {
	int f1=1, f2=1, f3=1, f4=1, f5=1, f6=1, f7=1, f8=1, f9=1, f10=1;
	int f11=1, f12=1, f13=1, f14=1, f15=1, f16=1, f17=1, f18=1, f19=1, f20=1;
	int f21=1, f22=1, f23=1, f24=1, f25=1, f26=1, f27=1, f28=1, f29=1, f30=1;
	int f31=1;
	Object f32;
	public m32() {
		f32 = new Object();
	}
}
class m33 {
	int f1=1, f2=1, f3=1, f4=1, f5=1, f6=1, f7=1, f8=1, f9=1, f10=1;
	int f11=1, f12=1, f13=1, f14=1, f15=1, f16=1, f17=1, f18=1, f19=1, f20=1;
	int f21=1, f22=1, f23=1, f24=1, f25=1, f26=1, f27=1, f28=1, f29=1, f30=1;
	int f31=1, f32=1;
	Object f33;
	public m33() {
		f33 = new Object();
	}
}
class e63a extends m31 {
	int g1=1, g2=1, g3=1, g4=1, g5=1, g6=1, g7=1, g8=1, g9=1, g10=1;
	int g11=1, g12=1, g13=1, g14=1, g15=1, g16=1, g17=1, g18=1, g19=1, g20=1;
	int g21=1, g22=1, g23=1, g24=1, g25=1, g26=1, g27=1, g28=1, g29=1, g30=1;
	int g31=1;
	Object g32;
	public e63a() {
		super();
		g32 = new Object();
	}
}
class e63b extends m32 {
	int g1=1, g2=1, g3=1, g4=1, g5=1, g6=1, g7=1, g8=1, g9=1, g10=1;
	int g11=1, g12=1, g13=1, g14=1, g15=1, g16=1, g17=1, g18=1, g19=1, g20=1;
	int g21=1, g22=1, g23=1, g24=1, g25=1, g26=1, g27=1, g28=1, g29=1, g30=1;
	Object g31;
	public e63b() {
		super();
		g31 = new Object();
	}
}
class e63c extends m33 {
	int g1=1, g2=1, g3=1, g4=1, g5=1, g6=1, g7=1, g8=1, g9=1, g10=1;
	int g11=1, g12=1, g13=1, g14=1, g15=1, g16=1, g17=1, g18=1, g19=1, g20=1;
	int g21=1, g22=1, g23=1, g24=1, g25=1, g26=1, g27=1, g28=1, g29=1;
	Object g30;
	public e63c() {
		super();
		g30 = new Object();
	}
}
class e64a extends m31 {
	int g1=1, g2=1, g3=1, g4=1, g5=1, g6=1, g7=1, g8=1, g9=1, g10=1;
	int g11=1, g12=1, g13=1, g14=1, g15=1, g16=1, g17=1, g18=1, g19=1, g20=1;
	int g21=1, g22=1, g23=1, g24=1, g25=1, g26=1, g27=1, g28=1, g29=1, g30=1;
	int g31=1, g32=1;
	Object g33;
	public e64a() {
		super();
		g33 = new Object();
	}
}
class e64b extends m32 {
	int g1=1, g2=1, g3=1, g4=1, g5=1, g6=1, g7=1, g8=1, g9=1, g10=1;
	int g11=1, g12=1, g13=1, g14=1, g15=1, g16=1, g17=1, g18=1, g19=1, g20=1;
	int g21=1, g22=1, g23=1, g24=1, g25=1, g26=1, g27=1, g28=1, g29=1, g30=1;
	int g31=1;
	Object g32;
	public e64b() {
		super();
		g32 = new Object();
	}
}
class e64c extends m33 {
	int g1=1, g2=1, g3=1, g4=1, g5=1, g6=1, g7=1, g8=1, g9=1, g10=1;
	int g11=1, g12=1, g13=1, g14=1, g15=1, g16=1, g17=1, g18=1, g19=1, g20=1;
	int g21=1, g22=1, g23=1, g24=1, g25=1, g26=1, g27=1, g28=1, g29=1, g30=1;
	Object g31;
	public e64c() {
		super();
		g31 = new Object();
	}
}
class e65a extends m31 {
	int g1=1, g2=1, g3=1, g4=1, g5=1, g6=1, g7=1, g8=1, g9=1, g10=1;
	int g11=1, g12=1, g13=1, g14=1, g15=1, g16=1, g17=1, g18=1, g19=1, g20=1;
	int g21=1, g22=1, g23=1, g24=1, g25=1, g26=1, g27=1, g28=1, g29=1, g30=1;
	int g31=1, g32=1, g33=1;
	Object g34;
	public e65a() {
		super();
		g34 = new Object();
	}
}
class e65b extends m32 {
	int g1=1, g2=1, g3=1, g4=1, g5=1, g6=1, g7=1, g8=1, g9=1, g10=1;
	int g11=1, g12=1, g13=1, g14=1, g15=1, g16=1, g17=1, g18=1, g19=1, g20=1;
	int g21=1, g22=1, g23=1, g24=1, g25=1, g26=1, g27=1, g28=1, g29=1, g30=1;
	int g31=1, g32=1;
	Object g33;
	public e65b() {
		super();
		g33 = new Object();
	}
}
class e65c extends m33 {
	int g1=1, g2=1, g3=1, g4=1, g5=1, g6=1, g7=1, g8=1, g9=1, g10=1;
	int g11=1, g12=1, g13=1, g14=1, g15=1, g16=1, g17=1, g18=1, g19=1, g20=1;
	int g21=1, g22=1, g23=1, g24=1, g25=1, g26=1, g27=1, g28=1, g29=1, g30=1;
	int g31=1;
	Object g32;
	public e65c() {
		super();
		g32 = new Object();
	}
}

