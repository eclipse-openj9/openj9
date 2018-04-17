/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package org.openj9.test.floatsanity;

import java.util.ArrayList;
import java.util.List;

import org.openj9.test.floatsanity.arithmetic.*;
import org.openj9.test.floatsanity.assumptions.*;
import org.openj9.test.floatsanity.conversions.*;
import org.openj9.test.floatsanity.custom.*;
import org.openj9.test.floatsanity.functions.*;
import org.openj9.test.floatsanity.relational.*;
import org.testng.annotations.Factory;
import org.testng.log4testng.Logger;

public class TestFactory {

	public static Logger logger = Logger.getLogger(TestFactory.class);

	@Factory
	public Object[] tests() {
		List<Object> theTests = new ArrayList<>();
		
		/* Assumption tests */
		theTests.add(new CheckFloatAssumptions());
		theTests.add(new CheckDoubleAssumptions());
		theTests.add(new CheckExponentAssumptions());
		theTests.add(new CheckTrigAssumptions());

		/* Arithmetic tests */
		theTests.add(new CheckFloatPlusBehaviour());
		theTests.add(new CheckDoublePlusBehaviour());
		theTests.add(new CheckFloatMinusBehaviour());
		theTests.add(new CheckDoubleMinusBehaviour());
		theTests.add(new CheckNegateFloatBehaviour());
		theTests.add(new CheckNegateDoubleBehaviour());

		/* Conversion tests */
		theTests.add(new CheckConversionsToFloat());
		theTests.add(new CheckConversionsToDouble());
		theTests.add(new CheckConversionsFromFloat());
		theTests.add(new CheckConversionsFromDouble());

		/* Special tests */
		theTests.add(new CheckZeroFloatBehaviour());
		theTests.add(new CheckZeroDoubleBehaviour());
		theTests.add(new CheckNaNFloatBehaviour());
		theTests.add(new CheckNaNDoubleBehaviour());

		/* More arithmetic tests */
		theTests.add(new CheckFloatTimesBehaviour());
		theTests.add(new CheckDoubleTimesBehaviour());
		theTests.add(new CheckFloatDivRemBehaviour());
		theTests.add(new CheckDoubleDivRemBehaviour());

		/* Relational tests */
		theTests.add(new CheckFloatComparisons());
		theTests.add(new CheckDoubleComparisons());

		/* Function tests */
		theTests.add(new CheckFloatMinMaxFunctions());
		theTests.add(new CheckDoubleMinMaxFunctions());
		theTests.add(new CheckFloatRoundingFunctions());
		theTests.add(new CheckDoubleRoundingFunctions());
		theTests.add(new CheckDoubleMiscFunctions());
		theTests.add(new CheckDoubleExponentFunctions());
		theTests.add(new CheckDoubleTrigFunctions());
		
		/* Custom tests */
		theTests.add(new CheckFunctionSymmetry());
		theTests.add(new CheckKnownProblems());
		theTests.add(new CheckStrictMath());
		theTests.add(new Check1ULPMath());

		/* Denormal Arithmetic tests */
		theTests.add(new CheckDenormalFloatPlusBehaviour());
		theTests.add(new CheckDenormalDoublePlusBehaviour());
		theTests.add(new CheckDenormalFloatMinusBehaviour());
		theTests.add(new CheckDenormalDoubleMinusBehaviour());
		theTests.add(new CheckDenormalFloatTimesBehaviour());
		theTests.add(new CheckDenormalDoubleTimesBehaviour());
		theTests.add(new CheckDenormalFloatDivBehaviour());
		theTests.add(new CheckDenormalDoubleDivBehaviour());
		
		return theTests.toArray();
	}

}
