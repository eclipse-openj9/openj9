/*******************************************************************************
 * Copyright (c) 2007, 2019 IBM Corp. and others
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
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

/**
 * 
 * @author 	Maciek Klimkowski
 * @author 	Gabriel Castro
 */
public class FlagDefinitions extends OMObject {
	private String runtime;
	private final Map<String, FlagDefinition> _flagDefinitions = new TreeMap<String, FlagDefinition>();
	private final Map<String, Set<FlagDefinition>> _flagCategories = new HashMap<String, Set<FlagDefinition>>();

	/**
	 * Defaults FlagDefinitions constructor 
	 */
	public FlagDefinitions() {
	}

	/**
	 * Sets the runtime to which this flag definitions list belongs.
	 * 
	 * @param runtime 	the parent runtime
	 */
	public void setRuntime(String runtime) {
		this.runtime = runtime;
	}

	/**
	 * Returns the runtime to which this flag definition belongs to, or <code>null</code> if none.
	 * 
	 * @return the runtime
	 */
	public String getRuntime() {
		return runtime;
	}

	public boolean contains(String flagDefinitionId) {
		return _flagDefinitions.containsKey(flagDefinitionId);
	}

	public boolean contains(FlagDefinition flagDefinition) {
		return _flagDefinitions.containsValue(flagDefinition);
	}

	/**
	 * @param 	flagDefinition		Flag Definition to be added to this flag definition list
	 */
	public void addFlagDefinition(FlagDefinition flagDefinition) {
		_flagDefinitions.put(flagDefinition.getId(), flagDefinition);

		Set<FlagDefinition> definitions = _flagCategories.get(flagDefinition.getCategory());

		if (definitions == null) {
			definitions = new HashSet<FlagDefinition>();
			_flagCategories.put(flagDefinition.getCategory(), definitions);
		}

		definitions.add(flagDefinition);
	}

	/**
	 * Remove a flag definition
	 * 
	 * @param 	flagDefinitionId		ID of the flag definition
	 */
	public FlagDefinition removeFlagDefinition(String flagDefinitionId) {
		FlagDefinition flagDefinition = _flagDefinitions.get(flagDefinitionId);

		return (flagDefinition != null) ? removeFlagDefinition(flagDefinition) : null;
	}

	/**
	 * Remove a flag definition
	 * 
	 * @param 	flagDefinition		The FlagDefinition to remove (based on its ID)
	 */
	public FlagDefinition removeFlagDefinition(FlagDefinition flagDefinition) {
		if (_flagDefinitions.containsValue(flagDefinition)) {
			// Remove from category list
			Set<FlagDefinition> categoryDefinitions = _flagCategories.get(flagDefinition.getCategory());
			categoryDefinitions.remove(flagDefinition);

			// Remove category if this was the last flag definition
			if (categoryDefinitions.size() == 0) {
				_flagCategories.remove(flagDefinition.getCategory());
			}

			// Remove any require/preclude reference to this category
			for (FlagDefinition def : _flagDefinitions.values()) {
				if (def.getRequires().containsKey(flagDefinition.getId())) {
					def.removeRequires(flagDefinition.getId());
				}

				if (def.getPrecludes().containsKey(flagDefinition.getId())) {
					def.removePrecludes(flagDefinition.getId());
				}
			}

			return _flagDefinitions.remove(flagDefinition.getId());
		}

		return null;
	}

	/**
	 * Retrieve a FlagDefinition. 
	 * 
	 * @param 	flagDefinitionId	The ID of FlagDefinition to be retrieved
	 * @return	A FlagDefinition identified by flagDefinitionId argument
	 */
	public FlagDefinition getFlagDefinition(String flagDefinitionId) {
		return _flagDefinitions.get(flagDefinitionId);
	}

	/**
	 * Retrieve the HashMap containing all flag definitions contained by this FlagDefinitions object. 
	 * 
	 * @return	A HashMap with all the flag definitions that this FlagDefinitions object holds.
	 */
	public Map<String, FlagDefinition> getFlagDefinitions() {
		return Collections.unmodifiableMap(_flagDefinitions);
	}

	/**
	 * Retrieves the list of {@link FlagDefinition} for the given category.
	 * 
	 * @param 	category	the category for which to retrieve flag definitions
	 * @return	the list of {@link FlagDefinition}
	 */
	public Set<FlagDefinition> getFlagDefinitions(String category) {
		return _flagCategories.containsKey(category) ? _flagCategories.get(category) : new HashSet<FlagDefinition>();
	}

	/**
	 * Retrieves the list of flag definitions categories.
	 * 
	 * @return	the list of categories
	 */
	public Set<String> getCategories() {
		return _flagCategories.keySet();
	}

	/**
	 * Retrieves the list of {@link FlagDefinition} IDs that preclude the given flag ID.
	 * 
	 * @param 	flagId		the ID of the flag definition for which preclusions should be computed
	 * @return	the list of precluded by flag definitions IDs
	 */
	public Set<String> getPrecludedBy(String flagId) {
		Set<String> precludedBys = new TreeSet<String>();

		for (FlagDefinition flagDefinition : _flagDefinitions.values()) {
			for (FlagDefinition preclude : flagDefinition.getPrecludes().values()) {
				if (flagId.equals(preclude.getId())) {
					precludedBys.add(flagDefinition.getId());
				}
			}
		}

		return precludedBys;
	}

	/**
	 * Retrieves the list of {@link FlagDefinition} IDs that preclude the given flag.
	 * 
	 * @param 	flag		the flag definition for which preclusions should be computed
	 * @return	the list of flag definitions IDs
	 */
	public Set<String> getPrecludedBy(FlagDefinition flag) {
		return getPrecludedBy(flag.getId());
	}

	/**
	 * Retrieves the list of {@link FlagDefinition} IDs that require the given flag ID.
	 * 
	 * @param 	flagId		the ID of the flag definition for which dependencies should be computed
	 * @return	the list of dependent flag definitions IDs
	 */
	public Set<String> getDependents(String flagId) {
		Set<String> requiredBys = new TreeSet<String>();

		for (FlagDefinition flagDefinition : _flagDefinitions.values()) {
			for (FlagDefinition require : flagDefinition.getRequires().values()) {
				if (flagId.equals(require.getId())) {
					requiredBys.add(flagDefinition.getId());
				}
			}
		}

		return requiredBys;
	}

	/**
	 * Retrieves the list of {@link FlagDefinition} IDs that required the given flag.
	 * 
	 * @param 	flag		the flag definition for which dependencies should be computed
	 * @return	the list of dependent flag definitions IDs
	 */
	public Set<String> getDependents(FlagDefinition flag) {
		return getDependents(flag.getId());
	}

	/**
	 * Verify that the requires and precludes flag lists of each FlagDefinition refer to flag 
	 * definitions that exist in _flagDefinitions. 
	 */
	public void verify() throws InvalidFlagDefinitionsException {
		Collection<Throwable> errors = new HashSet<Throwable>();

		for (FlagDefinition flagDef : _flagDefinitions.values()) {
			errors.addAll(verifyDefinition(flagDef));
		}

		if (errors.size() > 0) {
			throw new InvalidFlagDefinitionsException(errors, this);
		}
	}

	public Collection<OMException> verifyDefinition(FlagDefinition flagDef) {
		Collection<OMException> errors = new HashSet<OMException>();

		for (FlagDefinition req : flagDef.getLocalRequires().values()) {
			if (!_flagDefinitions.containsKey(req.getId())) {
				errors.add(new InvalidFlagException(MessageFormat.format(Messages.getString("FlagDefinitions.invalidRequires"), new Object[] { req.getId() }), flagDef)); //$NON-NLS-1$
			}
		}

		for (FlagDefinition prec : flagDef.getLocalPrecludes().values()) {
			if (!_flagDefinitions.containsKey(prec.getId())) {
				errors.add(new InvalidFlagException(MessageFormat.format(Messages.getString("FlagDefinitions.invalidPrecludes"), new Object[] { prec.getId() }), flagDef)); //$NON-NLS-1$
			}
		}

		if (flagDef.getLocalRequires().containsKey(flagDef.getId())) {
			errors.add(new InvalidFlagException(MessageFormat.format(Messages.getString("FlagDefinitions.requireSelf"), new Object[] { flagDef.getId() }), flagDef)); //$NON-NLS-1$
		}

		if (flagDef.getLocalPrecludes().containsKey(flagDef.getId())) {
			errors.add(new InvalidFlagException(MessageFormat.format(Messages.getString("FlagDefinitions.precludeSelf"), new Object[] { flagDef.getId() }), flagDef)); //$NON-NLS-1$
		}

		Map<String, FlagDefinition> allRequires = flagDef.getRequires();
		Map<String, FlagDefinition> allPrecludes = flagDef.getPrecludes();

		if (allPrecludes.containsKey(flagDef.getId())) {
			errors.add(new InvalidFlagException(MessageFormat.format(Messages.getString("FlagDefinitions.precludeCycle"), new Object[] { flagDef.getId() }), flagDef)); //$NON-NLS-1$
		}

		for (FlagDefinition req : allRequires.values()) {
			if (allPrecludes.containsKey(req.getId())) {
				errors.add(new InvalidFlagException(MessageFormat.format(Messages.getString("FlagDefinitions.requireAndPreclude"), new Object[] { flagDef.getId(), req.getId() }), flagDef)); //$NON-NLS-1$
			}
		}

		return errors;
	}
}
