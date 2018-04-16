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
package j9vm.test.hashCode;

/**
 * Large object with lots of fields.
 * Allocate this to use up heap space quickly.
 */
public class EvenBiggerHashedObject extends HashedObject {

	public long pad1, pad2, pad3, pad4, pad5, pad6, pad7;
	public long pad01, pad02, pad03, pad04, pad05, pad06, pad07;
	public long pad11, pad12, pad13, pad14, pad15, pad16, pad17;
	public long pad101, pad102, pad103, pad104, pad105, pad106, pad107;
	public long pad21, pad22, pad23, pad24, pad25, pad26, pad27;
	public long pad201, pad202, pad203, pad204, pad205, pad206, pad207;
	public long pad211, pad212, pad213, pad214, pad215, pad216, pad217;
	public long pad2101, pad2102, pad2103, pad2104, pad2105, pad2106, pad2107;
	public long pad31, pad32, pad33, pad34, pad35, pad36, pad37;
	public long pad301, pad302, pad303, pad304, pad305, pad306, pad307;
	public long pad311, pad312, pad313, pad314, pad315, pad316, pad317;
	public long pad3101, pad3102, pad3103, pad3104, pad3105, pad3106, pad3107;
	public long pad321, pad322, pad323, pad324, pad325, pad326, pad327;
	public long pad3201, pad3202, pad3203, pad3204, pad3205, pad3206, pad3207;
	public long pad3211, pad3212, pad3213, pad3214, pad3215, pad3216, pad3217;
	public long pad32101, pad32102, pad32103, pad32104, pad32105, pad32106, pad32107;
	public long pad41, pad42, pad43, pad44, pad45, pad46, pad47;
	public long pad401, pad402, pad403, pad404, pad405, pad406, pad407;
	public long pad411, pad412, pad413, pad414, pad415, pad416, pad417;
	public long pad4101, pad4102, pad4103, pad4104, pad4105, pad4106, pad4107;
	public long pad421, pad422, pad423, pad424, pad425, pad426, pad427;
	public long pad4201, pad4202, pad4203, pad4204, pad4205, pad4206, pad4207;
	public long pad4211, pad4212, pad4213, pad4214, pad4215, pad4216, pad4217;
	public long pad42101, pad42102, pad42103, pad42104, pad42105, pad42106, pad42107;
	public long pad431, pad432, pad433, pad434, pad435, pad436, pad437;
	public long pad4301, pad4302, pad4303, pad4304, pad4305, pad4306, pad4307;
	public long pad4311, pad4312, pad4313, pad4314, pad4315, pad4316, pad4317;
	public long pad43101, pad43102, pad43103, pad43104, pad43105, pad43106, pad43107;
	public long pad4321, pad4322, pad4323, pad4324, pad4325, pad4326, pad4327;
	public long pad43201, pad43202, pad43203, pad43204, pad43205, pad43206, pad43207;
	public long pad43211, pad43212, pad43213, pad43214, pad43215, pad43216, pad43217;
	public long pad432101, pad432102, pad432103, pad432104, pad432105, pad432106, pad432107;

	public EvenBiggerHashedObject(long serialNumber) {
		super(serialNumber);
	}

}
