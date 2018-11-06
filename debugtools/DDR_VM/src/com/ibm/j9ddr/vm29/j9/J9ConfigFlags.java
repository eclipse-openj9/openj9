/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;

/**
 * This class deals with an oversight in the migration to new DDR tools.
 *
 * The DDR tooling from OMR, normally generates constants with names as they appear in
 * the source code. Field overrides can be used to adjust those names. Unfortunately,
 * overrides were only specified for fields that were actually used in DDR_VM code and
 * it was later discovered that the host_XXX fields from TRBuildFlags were not properly
 * captured in the blob. The arch_XXX fields in J9BuildFlags are reliable, but some
 * interim builds don't include the overrides for all flags we need to use. This class
 * adapts to core files from those builds.
 */
public final class J9ConfigFlags {

	public static final boolean arch_arm;
	public static final boolean arch_power;
	public static final boolean arch_s390;
	public static final boolean arch_x86;

	static {
		Class<?> flagsClass = J9BuildFlags.class;

		arch_arm = getFlag(flagsClass, "arch_arm", "J9VM_ARCH_ARM");
		arch_power = getFlag(flagsClass, "arch_power", "J9VM_ARCH_POWER");
		arch_s390 = getFlag(flagsClass, "arch_s390", "J9VM_ARCH_S390");
		arch_x86 = getFlag(flagsClass, "arch_x86", "J9VM_ARCH_X86");
	}

	private static boolean getFlag(Class<?> flagsClass, String... fieldNames) {
		Boolean value = null;

		for (String fieldName : fieldNames) {
			try {
				Field field = flagsClass.getField(fieldName);
				int modifiers = field.getModifiers();

				if (!Modifier.isPublic(modifiers) || !Modifier.isStatic(modifiers) || !Modifier.isFinal(modifiers)) {
					// ignore fields with unexpected modifiers
					continue;
				}

				if (field.getType() != boolean.class) {
					// ignore non-boolean fields
					continue;
				}

				if (value != null) {
					// only one of the expected field names should match
					throw new InternalError("ambiguous flag");
				}

				value = Boolean.valueOf(field.getBoolean(null));
			} catch (IllegalArgumentException | IllegalAccessException e) {
				// this should not happen - the field is public static final boolean
				throw new InternalError("unexpected exception", e);
			} catch (NoSuchFieldException e) {
				// ignore: check below that exactly one field is found
			} catch (SecurityException e) {
				throw new InternalError("unexpected exception", e);
			}
		}

		if (value == null) {
			// exactly one of the expected field names should match
			throw new InternalError("missing flag");
		}

		return value.booleanValue();
	}

}
