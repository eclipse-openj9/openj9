/*******************************************************************************
 * Copyright (c) 2007, 2011 IBM Corp. and others
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
package com.ibm.j9tools.om;

import java.text.MessageFormat;
import java.util.Collection;
import java.util.HashSet;
import java.util.Map;

/**
 * Verifier class to check the validity of the flags in a given {@link IFlagContainer}.  Validity
 * is determined by the consistency of the requires and precludes list.  Consistency is determined as follows:
 *
 * <ol>
 * <li>A flag must not requires itself</li>
 * <li>A flag must not preclude itself</li>
 * <li>There must be no cycle in the requires list</li>
 * <li>There must be no cycle in the precludes list</li>
 * <li>There must be no conflict between a flag's recursive requires and precludes list</li>
 * </ol>
 *
 * @author	Gabriel Castro
 * @author	Han Xu
 */
public class FlagVerifier {
	private final Collection<OMException> errors = new HashSet<OMException>();
	private final FlagDefinitions flagDefinitions;
	private final Map<String, Flag> containerFlags;

	/**
	 * Creates a new {@link FlagVerifier} for the given {@link IFlagContainer} and the given flag definitions.
	 *
	 * @param 	container			the flag container to be verified
	 * @param 	flagDefinitions		the flag definitions from the container's build-info
	 */
	public FlagVerifier(IFlagContainer container, FlagDefinitions flagDefinitions) {
		this.flagDefinitions = flagDefinitions;
		this.containerFlags = container.getFlags();
	}

	/**
	 * Walks the flags provided by the Feature or BuildSpec.
	 *
	 * This will not do a complete validation because since it validates all flags, a complete validation of each single flag will be redundant.
	 * That is to say, checking that flag A precludes B, and checking that B is precluded by A, are the same.
	 */
	public Collection<OMException> verify() {
		for (Flag flag : containerFlags.values()) {
			verifyExistence(flag);

			// Only check requires and precludes if the flag is enabled, otherwise it's like the flags aren't there
			if (flag.getState()) {
				verifyRequires(flag, false);
				verifyPrecludes(flag, false);
			}
		}

		return errors;
	}

	/**
	 * Validate a flag
	 *
	 * @param 	flag			Flag to verify
	 * @param	parentToggled	The parent (AKA category) of the flag was toggled (from a tree), thus assume that all children of this parent are of the same state as <code>flag</code>
	 */
	public void verify(Flag flag, boolean parentToggled) {
		verifyExistence(flag);

		if (flag.getState()) {
			verifyRequires(flag, parentToggled);
			verifyPrecludes(flag, parentToggled);
		}
	}

	/**
	 * Validate a flag, assuming that the parent was not toggled. This is equivalent to:
	 * <code>validateFlag(flag, false);</code>
	 *
	 * @param 	flag			Flag to verify
	 */
	public void verify(Flag flag) {
		verify(flag, false);
	}

	/**
	 * Helper method used to verify existence of a given flag.
	 *
	 * @param 	flag		Flag to verify
	 */
	private void verifyExistence(Flag flag) {
		if (flagDefinitions.getFlagDefinition(flag.getId()) == null) {
			errors.add(new InvalidFlagException(MessageFormat.format(Messages.getString("FlagVerifier.invalidFlag"), new Object[] { flag.getId() }), flag)); //$NON-NLS-1$
		}
	}

	/**
	 * Check that the requirements are turned on
	 *
	 * @param 	flag			Flag to verify
	 * @param	parentToggled	The parent (AKA category) of the flag was toggled (from a tree), thus assume that all children of this parent are of the same state as <code>flag</code>
	 */
	private void verifyRequires(Flag flag, boolean parentToggled) {
		// Don't validate if the flag is disabled.  A disabled flag acts as a non-existent flag.
		if (!flag.getState() || flagDefinitions.getFlagDefinition(flag.getId()) == null) {
			return;
		}

		// Check the local requires
		for (FlagDefinition require : flagDefinitions.getFlagDefinition(flag.getId()).getLocalRequires().values()) {
			Flag requiredFlag = containerFlags.get(require.getId());

			// When a parent category is checked we don't need to check the children as they will all be enabled in the end
			if ((!parentToggled || !require.getCategory().equals(flag.getCategory())) && (requiredFlag == null || !requiredFlag.getState())) {
				errors.add(new InvalidFlagException(MessageFormat.format(Messages.getString("FlagVerifier.requiredFlag"), new Object[] { flag.getId(), require.getId() }), flag)); //$NON-NLS-1$
			}
		}

	}

	/**
	 * Verify that the flags which this <code>flag</code> precludes are turned off
	 *
	 * @param 	flag			Flag to verify
	 * @param	parentToggled	The parent (AKA category) of the flag was toggled (from a tree), thus assume that all children of this parent are of the same state as <code>flag</code>
	 */
	private void verifyPrecludes(Flag flag, boolean parentToggled) {
		// Don't validate if the flag is disabled.  A disabled flag acts as a non-existent flag.
		if (!flag.getState() || flagDefinitions.getFlagDefinition(flag.getId()) == null) {
			return;
		}

		// Check the local requires
		for (FlagDefinition preclude : flagDefinitions.getFlagDefinition(flag.getId()).getLocalPrecludes().values()) {
			Flag precludedFlag = containerFlags.get(preclude.getId());

			if ((parentToggled && precludedFlag != null && precludedFlag.getCategory().equals(flag.getCategory())) || (precludedFlag != null && precludedFlag.getState())) {
				errors.add(new InvalidFlagException(MessageFormat.format(Messages.getString("FlagVerifier.precludedFlag"), new Object[] { flag.getId(), precludedFlag.getId() }), flag)); //$NON-NLS-1$
			}
		}
	}
}
