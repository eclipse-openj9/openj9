/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package com.ibm.uma.om;

import java.util.Vector;

import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;


public class PredicateList {
	Vector<Predicate> predicates = new Vector<Predicate>();
	String containingFile;
	boolean cachedResult;
	boolean resultHasBeenCached = false;
	
	public PredicateList(String containingFile) {
		this.containingFile = containingFile;
	}
	
	public String getContainingFile() {
		return containingFile;
	}
	
	public boolean evaluate() throws UMAException {
		if ( resultHasBeenCached ) { 
			return cachedResult;
		}
		// if the there are no predicates return true;
		if ( predicates.size() < 1 ) {
			cachedResult = true;
			resultHasBeenCached = true;
			return cachedResult;
		}

		// the last true result in the list wins. 
		// if the last true result is for an exclude-if then return false
		// if the last true result is for an include-if then return true
		// if no predicates evaluated to true then 
		//   - return false if all predicates were include-if
		//   - return true if all predicates were exclude-if
		//   - return false otherwise
		boolean includeIfPresent = false;
		for ( int i=predicates.size()-1; i>=0; i-- ) {
			Predicate predicate = predicates.elementAt(i);
			if ( evalutePredicate( predicate.getPredicate() ) ) {
				switch ( predicate.getType() ) {
				case Predicate.EXCLUDE_IF: {
					cachedResult = false;
					resultHasBeenCached = true;
					return cachedResult;
				}
				case Predicate.INCLUDE_IF: {
					cachedResult = true;
					resultHasBeenCached = true;
					return cachedResult;
				}	
				}
			} else if ( predicate.getType() == Predicate.INCLUDE_IF ) {
				includeIfPresent = true;
			}
		}
		
		// an include-if was present, but no condition was satisfied
		if ( includeIfPresent ) {
			cachedResult = false;
			resultHasBeenCached = true;
			return cachedResult;
		}

		// no include-if was present
		cachedResult = true;
		resultHasBeenCached = true;
		return cachedResult;
	}
	
	boolean evaluateSinglePredicate(String predicate) throws UMAException {
		return UMA.getUma().getSinglePredicateEvaluator().evaluateSinglePredicate(predicate);
	}
	
	boolean evalutePredicate(String predicate) throws UMAException {
		String[] preds = predicate.split("\\s");
		boolean result = true;
		boolean predicateFound = false;
		boolean andOp = false;
		boolean orOp = false;
		boolean xorOp = false;
		boolean notOp = false;
		for ( String pred : preds ) {
			if ( pred.equalsIgnoreCase("and") ) {
				if ( orOp || xorOp || notOp || !predicateFound ) {
					throw new UMAException("Error: badly formed predicate [" + predicate + "] in " + containingFile );
				} 
				andOp = true;
				continue;
			} else if ( pred.equalsIgnoreCase("or") ) {
				if ( andOp || xorOp || notOp || !predicateFound ) {
					throw new UMAException("Error: badly formed predicate [" + predicate + "] in " + containingFile );
				}
				orOp = true;
				continue;
			} else if ( pred.equalsIgnoreCase("xor") ) {
				if ( andOp || orOp || notOp || !predicateFound ) {
					throw new UMAException("Error: badly formed predicate [" + predicate + "] in " + containingFile );
				}
				xorOp = true;
				continue;
			}else if ( pred.equalsIgnoreCase("not") ) {
				notOp = true;
				continue;
			} else {
				predicateFound = true;
			}
			if ( andOp ) {
				result = result && 
				( notOp ? !evaluateSinglePredicate(pred) :
					evaluateSinglePredicate(pred));
			} else if ( orOp ) {
				result = result || 
				( notOp ? !evaluateSinglePredicate(pred) :
					evaluateSinglePredicate(pred) );
			} else if ( xorOp ) {
				result = result ^ 
				( notOp ? !evaluateSinglePredicate(pred) :
					evaluateSinglePredicate(pred) );
			} else {
				result = notOp ? !evaluateSinglePredicate(pred) :
					evaluateSinglePredicate(pred);
			}
			notOp = false;
			orOp = false;
			andOp = false;
			xorOp = false;
		}
		return result;
	}

	public void add(Predicate predicate) {
		predicates.add(predicate);
	}
}
