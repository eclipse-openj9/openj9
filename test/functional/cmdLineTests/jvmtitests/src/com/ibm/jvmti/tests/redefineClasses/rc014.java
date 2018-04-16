/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.jvmti.tests.redefineClasses;

import java.lang.reflect.Method;
import java.net.URL;

import com.ibm.jvmti.tests.util.Util;

public class rc014 {
	public static native boolean redefineClass(Class name, int classBytesSize, byte[] classBytes);
	public static native boolean redefineClassExpectFailure(Class name, int classBytesSize, byte[] classBytes);

	public boolean setup(String args) {
		return true;
	}

	private static boolean methodExists(Class klass, String methodName, Class[] parameterTypes) {
		try {
			Method method = klass.getMethod(methodName, parameterTypes);
			if (method != null) {
				return true;
			}
		} catch (Exception e) {
		}
		return false;
	}

	// rather than calling !methodExists(), this specifically checks for a NoSuchMethodException
	private static boolean noSuchMethod(Class klass, String methodName, Class[] parameterTypes) {
		try {
			klass.getMethod(methodName, parameterTypes);
		} catch (NoSuchMethodException e) {
			return true;
		} catch (Exception e) {
		}
		return false;
	}

	//****************************************************************************************
	// 
	// 

	public boolean testAddFinalizer() {
		rc014_testAddFinalizer_O1 t_pre = new rc014_testAddFinalizer_O1();

		// original version has a meth1 but no meth2
		if (!methodExists(t_pre.getClass(), "meth1", null)) {
			return false;
		}

		boolean redefined = Util.redefineClassExpectFailure(getClass(), rc014_testAddFinalizer_O1.class, rc014_testAddFinalizer_R1.class);
		if (redefined) {
			System.out.println("class was redefined when we expected it not to be");
			/* we expected this to fail */
			return false;
		}

		return true;
	}

	public String helpAddFinalizer() {
		return "Attempt to add a finalizer.  This should fail"; 
	}

	//****************************************************************************************
	// 
	// 

	public boolean testDeleteFinalizer() {
		rc014_testDeleteFinalizer_O1 t_pre = new rc014_testDeleteFinalizer_O1();

		// original version has a meth1 and a finalizer
		if (!methodExists(t_pre.getClass(), "meth1", null)) {
			return false;
		} 
		
		boolean redefined = Util.redefineClassExpectFailure(getClass(), rc014_testDeleteFinalizer_O1.class, rc014_testDeleteFinalizer_R1.class);
		if (redefined) {
			/* we expected it to fail the redefinition */
			return false;
		}

		return true;
	}
	
	public String helpDeleteFinalizer()
	{
		return "Attempt to delete a finalizer.  This should fail"; 
	}
	
	//****************************************************************************************
	// 
	// we handle finalizers which are forwarders specially.  Without HCR we don't have the field that 
	// would allow us to support changing the method to something that actually does something.  
	// We need to ensure that for HCR the field is added and modifying it works

	public boolean testModifyFinalizerForwarder() {
		rc014_testModifyFinalizerForwarder_O1 t_pre = new rc014_testModifyFinalizerForwarder_O1();

		boolean redefined = Util.redefineClass(getClass(), rc014_testModifyFinalizerForwarder_O1.class, rc014_testModifyFinalizerForwarder_R1.class);
		if (!redefined) {
			return false;
		}

		// now transform back
		redefined = Util.redefineClass(getClass(), rc014_testModifyFinalizerForwarder_R1.class, rc014_testModifyFinalizerForwarder_O1.class);
		if (!redefined) {
			return false;
		}
		return true;
	}
	
	public String helpModifyFinalizerForwarder()
	{
		return "Modify a forwarder finalizer to a regular finalizer and back"; 
	}
	
	//****************************************************************************************
	// 
	// we handle finalizers which are empty specially.  Without HCR we don't have the field that 
	// would allow us to support changing the method to something that actually does something.  
	// We need to ensure that for HCR the field is added and modifying it works

	public boolean testModifyFinalizerEmpty() {
		rc014_testModifyFinalizerEmpty_O1 t_pre = new rc014_testModifyFinalizerEmpty_O1();

		boolean redefined = Util.redefineClass(getClass(), rc014_testModifyFinalizerEmpty_O1.class, rc014_testModifyFinalizerEmpty_R1.class);
		if (!redefined) {
			return false;
		}

		// now transform back
		redefined = Util.redefineClass(getClass(), rc014_testModifyFinalizerEmpty_R1.class, rc014_testModifyFinalizerEmpty_O1.class);
		if (!redefined) {
			return false;
		}
		return true;
	}
	
	public String helpModifyFinalizerEmpty()
	{
		return "Modify an empty finalizer to a regular finalizer and back"; 
	}
	
	
	//****************************************************************************************
	// 
	// Validate that we don't allow the finalize method in java.lang.Object to be transformed to
	// be non-empty.  Note this test is too dependent on the Object shape to be run normally but is included here
	// commented out to make any future manual testing easier.  In order to run this test in its current form you also have
	// to remove the check in classIsModifiable in hshelp which prevents java.lang.Object from being replaced and also comment out the check that the
	// superclasses are the same in verifyClassesAreCompatible
/*	public boolean testModifyJavaLangObjectFinalizer() {
		try {
			
			// get the bytes for the current Object implementation
			// in order to update the test create a new version of java.lang.Object and point this line so it reads that new
			// version and uncomment the lines which output the class bytes, once you have the output with the updated
			// bytes update the newClassBytesString and then point it back to the original
			byte classBytes[] = Util.getClassBytesFromUrl(Object.class,new URL("file:/" + System.getProperty("java.home") + "/bin/default/jclSC170/vm.jar"));
			StringBuffer classBytesString = new StringBuffer();
			for (int i=0;i<classBytes.length;i++){
				classBytesString.append(String.format("%02X", classBytes[i]));
			}
//			System.out.println();
//			System.out.println("classBytesString:" + classBytesString.toString());
//			System.out.println();
			
			String newClassBytesString = "CAFEBABE0000003200550700020100106A6176612F6C616E672F4F626A6563740100063C696E69743E010003282956010004436F646501000F4C696E654E756D6265725461626C650100124C6F63616C5661726961626C655461626C65010004746869730100124C6A6176612F6C616E672F4F626A6563743B010005636C6F6E6501001428294C6A6176612F6C616E672F4F626A6563743B01000A457863657074696F6E7307000E0100246A6176612F6C616E672F436C6F6E654E6F74537570706F72746564457863657074696F6E010006657175616C73010015284C6A6176612F6C616E672F4F626A6563743B295A0100016F01000D537461636B4D61705461626C6501000866696E616C697A650700150100136A6176612F6C616E672F5468726F7761626C650700170100116A6176612F6C616E672F496E74656765720A001600190C0003001A01000428492956010008676574436C61737301001328294C6A6176612F6C616E672F436C6173733B0100095369676E617475726501002828294C6A6176612F6C616E672F436C6173733C2B4C6A6176612F6C616E672F4F626A6563743B3E3B01000868617368436F64650100032829490100066E6F746966790100096E6F74696679416C6C010008746F537472696E6701001428294C6A6176612F6C616E672F537472696E673B0700260100176A6176612F6C616E672F537472696E674275696C6465720A000100280C001B001C0A002A002C07002B01000F6A6176612F6C616E672F436C6173730C002D00240100076765744E616D650A002F00310700300100106A6176612F6C616E672F537472696E670C0032003301000776616C75654F66010026284C6A6176612F6C616E672F4F626A6563743B294C6A6176612F6C616E672F537472696E673B0A002500350C00030036010015284C6A6176612F6C616E672F537472696E673B29560A002500380C0039003A010006617070656E6401001C2843294C6A6176612F6C616E672F537472696E674275696C6465723B0A0001003C0C001F00200A0016003E0C003F004001000B746F486578537472696E670100152849294C6A6176612F6C616E672F537472696E673B0A002500420C0039004301002D284C6A6176612F6C616E672F537472696E673B294C6A6176612F6C616E672F537472696E674275696C6465723B0A002500450C002300240100047761697407004801001E6A6176612F6C616E672F496E746572727570746564457863657074696F6E0A0001004A0C0046004B010005284A492956010004284A295601000474696D650100014A0100146E6577496E7374616E636550726F746F74797065010025284C6A6176612F6C616E672F436C6173733B294C6A6176612F6C616E672F4F626A6563743B01000B63616C6C6572436C6173730100114C6A6176612F6C616E672F436C6173733B01000A536F7572636546696C6501000B4F626A6563742E6A61766100210001000000000000000D000100030004000100050000002B0000000100000001B10000000200060000000600010000001500070000000C0001000000010008000900000104000A000B0001000C000000040001000D0001000F0010000100050000004600020002000000092A2BA6000504AC03AC0000000300060000000600010000003A000700000016000200000009000800090000000000090011000900010012000000030001070004001300040002000C00000004000100140005000000360002000100000008BB001604B70018B10000000200060000000A00020000004E0007004F00070000000C0001000000080008000900000111001B001C0001001D00000002001E0101001F0020000001110021000400000111002200040000000100230024000100050000004E0003000100000024BB0025592AB60027B60029B8002EB700341040B600372AB6003BB8003DB60041B60044B00000000200060000000600010000008C00070000000C0001000000240008000900000011004600040002000C000000040001004700050000003500040001000000072A0903B60049B10000000200060000000A0002000000A8000600A900070000000C00010000000700080009000000110046004C0002000C000000040001004700050000003F00040003000000072A1F03B60049B10000000200060000000A0002000000C5000600C600070000001600020000000700080009000000000007004D004E000101110046004B0001000C00000004000100470002004F00500001000500000036000100020000000201B0000000020006000000060001000000EA0007000000160002000000020008000900000000000200510052000100010053000000020054";
			byte newClassBytes[] = new byte[newClassBytesString.length()/2];
			for (int i=0;i<newClassBytesString.length();i=i+2){
				newClassBytes[i/2] = (byte) Integer.parseInt(newClassBytesString.substring(i,i+2),16);
			}
			
			classBytesString = new StringBuffer();
			for (int i=0;i<newClassBytes.length;i++){
				classBytesString.append(String.format("%02X", newClassBytes[i]));
			}
//			System.out.println();
//			System.out.println("new classBytesString:" + classBytesString.toString());
//			System.out.println();
			
			// check if new class bytes are same as the old 
			if (newClassBytes.length != classBytes.length){
				System.out.println("Object being redefined");
			} else {
				for (int i=0;i<classBytes.length;i++){
					if (classBytes[i] != newClassBytes[i]){
						System.out.println("Object being redefined");
						break;
					}
				}
			}
			
			boolean redefined = Util.redefineClassWithBytesExpectFailure(getClass(), Object.class, newClassBytes);
			if (redefined) {
				return false;
			}
	
			return true;
		} catch (Exception e){
			System.out.println("Unexpected exception:" + e);
			return false;
		}
	}
	
	public String helpModifyJavaLangObjectFinalizer()
	{
		return "Validate that we don't allow the finalize method in java.lang.Object to be transformed"; 
	}
*/	
}
