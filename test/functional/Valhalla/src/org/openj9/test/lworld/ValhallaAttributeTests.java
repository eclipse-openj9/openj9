/*
 * Copyright IBM Corp. and others 2023
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
import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity" })
public class ValhallaAttributeTests {

	class MultiLoadableDescriptorsTest {}

	/* A class may have no more than one LoadableDescriptors attribute. */
	@Test(expectedExceptions = java.lang.ClassFormatError.class,
		expectedExceptionsMessageRegExp = ".*Multiple LoadableDescriptors attributes.*")
	static public void testMultipleLoadableDescriptorsAttributes() throws Throwable {
		String className = MultiLoadableDescriptorsTest.class.getName();
		ValhallaAttributeGenerator.generateClassWithTwoLoadableDescriptorsAttributes(
			"MultiLoadableDescriptorsAttributes",
			new String[]{"L" + className + ";"}, new String[]{"L" + className + ";"}
		);
	}

	class LoadableDescriptorsClass {}

	@Test
	static public void testLoadableDescriptorsBehavior() throws Throwable {
		String className = LoadableDescriptorsClass.class.getName();
		/* Verify LoadableDescriptorsClass is not loaded */
		Assert.assertNull(ValhallaAttributeGenerator.findLoadedTestClass(className));
		/* Generate and load class that preloads LoadableDescriptorsClass */
		ValhallaAttributeGenerator.generateClassWithLoadableDescriptorsAttribute(
			"LoadableDescriptorsBehavior",
			new String[]{"L" + className + ";"}
		);
		/* Verify that LoadableDescriptorsClass is loaded */
		Assert.assertNotNull(ValhallaAttributeGenerator.findLoadedTestClass(className));
	}

	value class LoadableDescriptorsValueClass {}

	@Test
	static public void testLoadableDescriptorsValueClassBehavior() throws Throwable {
		String className = LoadableDescriptorsValueClass.class.getName();
		/* Verify LoadableDescriptorsValueClass is not loaded */
		Assert.assertNull(ValhallaAttributeGenerator.findLoadedTestClass(className));
		/* Generate and load class that preloads LoadableDescriptorsClass */
		ValhallaAttributeGenerator.generateClassWithLoadableDescriptorsAttribute(
			"LoadableDescriptorsValueClassBehavior",
			new String[]{"L" + className + ";"}
		);
		/* Verify that LoadableDescriptorsValueClass is loaded */
		Assert.assertNotNull(ValhallaAttributeGenerator.findLoadedTestClass(className));
	}

	class LoadableDescriptorsArrayClass {}

	@Test
	static public void testLoadableDescriptorsArrayBehavior() throws Throwable {
		String className = LoadableDescriptorsArrayClass.class.getName();
		/* Verify LoadableDescriptorsArrayClass is not loaded */
		Assert.assertNull(ValhallaAttributeGenerator.findLoadedTestClass(className));
		/* Generate and load class that preloads LoadableDescriptorsClass */
		ValhallaAttributeGenerator.generateClassWithLoadableDescriptorsAttribute(
			"testLoadableDescriptorsArrayBehavior",
			new String[]{"[L" + className + ";"}
		);
		/* Verify that LoadableDescriptorsClass is loaded */
		Assert.assertNotNull(ValhallaAttributeGenerator.findLoadedTestClass(className));
	}

	/* A class may have no more than one ImplicitCreation attribute. */
	@Test(expectedExceptions = java.lang.ClassFormatError.class, expectedExceptionsMessageRegExp = ".*Multiple ImplicitCreation attributes.*")
	static public void testMultipleImplicitCreationAttributes() throws Throwable {
		ValhallaAttributeGenerator.generateClassWithTwoImplicitCreationAttributes("MultiImplicitCreationAttributes");
	}

	/* There must not be an ImplicitCreation attribute in the attributes table of any other ClassFile structure representing a class, interface, or module.
	 * In other words any non value class.
	 */
	@Test(expectedExceptions = java.lang.ClassFormatError.class, expectedExceptionsMessageRegExp = ".*The ImplicitCreation attribute is only allowed in a non-abstract value class.*")
	static public void testNonValueTypeClassWithImplicitCreationAttribute() throws Throwable {
		ValhallaAttributeGenerator.generateNonValueTypeClassWithImplicitCreationAttribute("NonValueTypeImplicitCreationAttribute");
	}

	/* There must be no more than one NullRestricted attribute in the attributes table of a field_info structure */
	@Test(expectedExceptions = java.lang.ClassFormatError.class, expectedExceptionsMessageRegExp = ".*Multiple NullRestricted attributes present.*")
	static public void testMultipleNullRestrictedAttributes() throws Throwable {
		ValhallaAttributeGenerator.generateFieldWithMultipleNullRestrictedAttributes("TestMultipleNullRestrictedAttributes", "TestMultipleNullRestrictedAttributesField");
	}

	/* There must not be a NullRestricted attribute in the attributes table of a field_info structure whose descriptor_index references a primitive type. */
	@Test(expectedExceptions = java.lang.ClassFormatError.class, expectedExceptionsMessageRegExp = ".*A field with a primitive type cannot have a NullRestricted attribute.*")
	static public void testNullRestrictedNotAllowedInPrimitiveField() throws Throwable {
		ValhallaAttributeGenerator.generateNullRestrictedAttributeInPrimitiveField("TestNullRestrictedNotAllowedInPrimitiveField");
	}

	/* There must not be a NullRestricted attribute in the attributes table of a field_info structure whose descriptor_index references an array type */
	@Test(expectedExceptions = java.lang.ClassFormatError.class, expectedExceptionsMessageRegExp = ".*A field with an array type cannot have a NullRestricted attribute.*")
	static public void testNullRestrictedNotAllowedInArrayTypeField() throws Throwable {
		ValhallaAttributeGenerator.generateNullRestrictedAttributeInArrayField("TestNullRestrictedNotAllowedInArrayTypeField", "TestNullRestrictedNotAllowedInArrayTypeFieldField");
	}

	/* The descriptor_index of the field should name a value class that has an ImplicitCreation attribute with its ACC_DEFAULT flag is set.
	 * Failure for non-static fields should occur during class creation.
	 */
	@Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class, expectedExceptionsMessageRegExp = ".*An instance field with a NullRestricted attribute must be in a value class with an implicit constructor.*")
	static public void testNullRestrictedFieldMustBeInValueClass() throws Throwable {
		ValhallaAttributeGenerator.generateNullRestrictedAttributeInIdentityClass(false, "TestNullRestrictedFieldMustBeInValueClass", "TestNullRestrictedFieldMustBeInValueClassField");
	}

	/* The descriptor_index of the field should name a value class that has an ImplicitCreation attribute with its ACC_DEFAULT flag is set.
	 * Failure for non-static fields should occur during class creation.
	 */
	@Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class, expectedExceptionsMessageRegExp = ".*An instance field with a NullRestricted attribute must be in a value class with an implicit constructor.*")
	static public void testNullRestrictedFieldClassMustHaveImplicitCreation() throws Throwable {
		ValhallaAttributeGenerator.generateNullRestrictedAttributeInValueClassWithoutIC(false, "TestNullRestrictedFieldClassMustHaveImplicitCreation", "TestNullRestrictedFieldClassMustHaveImplicitCreationField");
	}

	/* The descriptor_index of the field should name a value class that has an ImplicitCreation attribute with its ACC_DEFAULT flag is set.
	 * Failure for non-static fields should occur during class creation.
	 */
	@Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class, expectedExceptionsMessageRegExp = ".*An instance field with a NullRestricted attribute must be in a value class with an implicit constructor.*")
	static public void testNullRestrictedFieldWhereImplicitCreationHasNoDefaultFlag() throws Throwable {
		ValhallaAttributeGenerator.generateNullRestrictedFieldWhereICHasNoDefaultFlag(false, "TestNullRestrictedAttributeWhereImplicitCreationHasNoDefaultFlag", "TestNullRestrictedAttributeWhereImplicitCreationHasNoDefaultFlagField");
	}

	/* The descriptor_index of the field should name a value class that has an ImplicitCreation attribute with its ACC_DEFAULT flag is set.
	 * Static fields should fail during the preparation stage of linking.
	 */
	@Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class, expectedExceptionsMessageRegExp = ".*A static field with a NullRestricted attribute must be in a value class with an implicit constructor.*")
	static public void testStaticNullRestrictedFieldMustBeInValueClass() throws Throwable {
		Class<?> c = ValhallaAttributeGenerator.generateNullRestrictedAttributeInIdentityClass(true, "TestStaticNullRestrictedFieldMustBeInValueClass", "TestStaticNullRestrictedFieldMustBeInValueClassField");
		c.newInstance();
	}

	/* The descriptor_index of the field should name a value class that has an ImplicitCreation attribute with its ACC_DEFAULT flag is set.
	 * Static fields should fail during the preparation stage of linking.
	 */
	@Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class, expectedExceptionsMessageRegExp = ".*A static field with a NullRestricted attribute must be in a value class with an implicit constructor.*")
	static public void testStaticNullRestrictedFieldClassMustHaveImplicitCreation() throws Throwable {
		Class<?> c = ValhallaAttributeGenerator.generateNullRestrictedAttributeInValueClassWithoutIC(true, "TestStaticNullRestrictedFieldClassMustHaveImplicitCreation", "TestStaticNullRestrictedFieldClassMustHaveImplicitCreationField");
		c.newInstance();
	}

	/* The descriptor_index of the field should name a value class that has an ImplicitCreation attribute with its ACC_DEFAULT flag is set.
	 * Static fields should fail during the preparation stage of linking.
	 */
	@Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class, expectedExceptionsMessageRegExp = ".*A static field with a NullRestricted attribute must be in a value class with an implicit constructor.*")
	static public void testStaticNullRestrictedFieldWhereImplicitCreationHasNoDefaultFlag() throws Throwable {
		Class<?> c = ValhallaAttributeGenerator.generateNullRestrictedFieldWhereICHasNoDefaultFlag(true, "TestStaticNullRestrictedAttributeWhereImplicitCreationHasNoDefaultFlag", "TestStaticNullRestrictedAttributeWhereImplicitCreationHasNoDefaultFlagField");
		c.newInstance();
	}

	@Test
	static public void testPutFieldNullToValueTypeField() throws Throwable {
		Class<?> c = ValhallaAttributeGenerator.generatePutFieldNullToField("TestPutFieldNullToValueTypeField", "TestPutFieldNullToValueTypeFieldField", false);
		c.newInstance();
	}

	static public Class<?> testPutFieldNullToNullRestrictedFieldClass = null;
	static public Class<?> testPutStaticNullToNullRestrictedFieldClass = null;

	@Test(priority=1)
	static public void testCreateTestPutFieldNullToNullRestrictedField() throws Throwable {
		testPutFieldNullToNullRestrictedFieldClass = ValhallaAttributeGenerator.generatePutFieldNullToField("TestPutFieldNullToNullRestrictedField", "TestPutFieldNullToNullRestrictedFieldField", true);
	}

	/* Instance field with NullRestricted attribute cannot be set to null. */
	@Test(priority=2, invocationCount=2, expectedExceptions = java.lang.NullPointerException.class)
	static public void testPutFieldNullToNullRestrictedField() throws Throwable {
		testPutFieldNullToNullRestrictedFieldClass.newInstance();
	}

	@Test(priority=1)
	static public void testCreateTestPutStaticNullToNullRestrictedField() throws Throwable {
		testPutStaticNullToNullRestrictedFieldClass = ValhallaAttributeGenerator.generatePutStaticNullToNullRestrictedField("TestPutStaticNullToNullRestrictedField", "TestPutStaticNullToNullRestrictedFieldField");
	}

	/* Static field with NullRestricted attribute cannot be set to null.
	 * putstatic should throw NPE if field with NullRestricted attribute is assigned null.
	 * JVMS 5.5 says if putstatic casuses a class to initialize and fails with an exception
	 * it should be wrapped in ExceptionInInitializerError.
	 * Since value fields are implicitly final this will always be the case.
	 */
	@Test(priority=2, invocationCount=2)
	static public void testPutStaticNullToNullRestrictedField() throws Throwable {
		try {
			testPutStaticNullToNullRestrictedFieldClass.newInstance();
		} catch(java.lang.ExceptionInInitializerError e) {
			if (e.getCause() instanceof NullPointerException) {
				return; /* pass */
			}
			throw e;
		} catch (java.lang.NoClassDefFoundError e) {
			// In the second invocation, java.lang.NoClassDefFoundError should be thrown
			// because the initialization of Class has previously failed initialization
			if (e.getCause() instanceof java.lang.ExceptionInInitializerError) {
				return; /* pass */
			}
			throw e;
		}
		Assert.fail("Test expected a NullPointerException wrapped in ExceptionInInitializerError.");
	}

	@ImplicitlyConstructible
	static value class ImplicitClass {
		Object o;
		ImplicitClass(Object o) {
			this.o = o;
		}
	}

	/* Test to verify JVM_IsImplicitlyConstructibleClass */
	@Test
	static public void testValueClassIsImplicitlyConstructible() {
		ImplicitClass ic = (ImplicitClass)jdk.internal.value.ValueClass.zeroInstance(ImplicitClass.class);
	}

	static value class NonImplicitClass {
		Object o;
		NonImplicitClass(Object o) {
			this.o = o;
		}
	}

	/* Test to verify JVM_IsImplicitlyConstructibleClass */
	@Test(expectedExceptions = IllegalArgumentException.class)
	static public void testValueClassIsImplicitlyConstructible2() {
		jdk.internal.value.ValueClass.zeroInstance(NonImplicitClass.class);
	}
}
