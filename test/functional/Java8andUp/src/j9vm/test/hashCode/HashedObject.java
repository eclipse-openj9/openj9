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

import j9vm.test.hashCode.generator.DummyGenerator;
import j9vm.test.hashCode.generator.HashCodeGenerator;

/**
 * Object for testing hashing algorithms.  The algorithm may be changed to evaluate performance.
 *
 */
public class HashedObject {
	/* names of various hashing algorithms */
	public static final String STRATEGY_LFSR1 = "lfsr1";
	public static final String STRATEGY_LFSR2 = "lfsr2";
	public static final String STRATEGY_RAND = "rand";
	public static final String STRATEGY_LINCON = "lincon";
	public static final String STRATEGY_LFSRA1 = "lfsra1";
	public static final String STRATEGY_LNCLFSR = "lnclfsr";
	/* cached copy of the hashcode, since in some cases the hashcode algorithm is non-reproducible */
	int myHashCode;
	protected long serialNumber;
	
	/* fields used to build a binary tree of objects.  The metrics for this tree indicate the quality of the hashcode */
	HashedObject left, right, collisions;
	public int newHash;
	public static long leftPlacementCount, rightPlacementCount, collisionCount;
	static HashStats stats = new HashStats();
	static HashCodeGenerator hashGen = null;

	protected HashedObject(long serialNumber) {
		myHashCode = hashCode();
		stats.update(myHashCode);
		this.serialNumber = serialNumber;
		left = right = collisions = null;
	}

	/**
	 * Add the current object to the tree.  If an object with the same hash is already there,
	 * return this and add it to a list of colliding objects.
	 * @param node root of the subtree to which the node is to be added.
	 * @return the head of a linked list of colliding objects.
	 */
	public HashedObject addNode(HashedObject node) {
		if (node.myHashCode == myHashCode) {
			node.collisions = collisions;
			collisions = node;
			++collisionCount;
			return this;
		} else {
			if (node.myHashCode > myHashCode) {
				++leftPlacementCount;
				if (null == left) {
					left = node;
					return null;
				} else {
					return left.addNode(node);
				}
			} else {
				++rightPlacementCount;
				if (null == right) {
					right = node;
					return null;
				} else {
					return right.addNode(node);
				}
			}
		}
	}

	/**
	 * Find a node with a given hashcode in a binary tree
	 * @param node root of the subtree to search
	 * @return the node, or null if none is found.
	 */
	public HashedObject findNode(HashedObject node) {
		if (node.myHashCode == myHashCode) {
			return this;
		} else {
			if (node.myHashCode > myHashCode) {
				if (null == left) {
					return null;
				} else {
					return left.findNode(node);
				}
			} else {
				if (null == right) {
					return null;
				} else {
					return right.findNode(node);
				}
			}
		}
	}

	@Override
	public String toString() {
		return "serial: " + Long.toString(serialNumber) + " hashCode: "
				+ Integer.toString(myHashCode, 16);
	}

	/**
	 * reset variables and set the hashcode algorithm
	 * @param hashStrategy Hashcode name
	 */
	public static void initialize(String hashStrategy) {
		leftPlacementCount = rightPlacementCount = collisionCount = 0;
		if (null == hashStrategy) {
			hashGen = new DummyGenerator();
		} else if (hashStrategy.equals(STRATEGY_LFSR1)) {
			hashGen = new LfsrMaskGenerator(0x5e8c2a09);
		} else if (hashStrategy.equals(STRATEGY_LFSRA1)) {
			hashGen = new LfsrAddingMaskGenerator(0x5e8c2a09);
		} else if (hashStrategy.equals(STRATEGY_LFSR2)) {
			hashGen = new ResettingLfsrMaskGenerator(0x5e8c2a09);
		} else if (hashStrategy.equals(STRATEGY_RAND)) {
			hashGen = new RandHashCodeGenerator();
		} else if (hashStrategy.equals(STRATEGY_LINCON)) {
			hashGen = new LinConHashCodeGenerator(27182835);
		} else if (hashStrategy.equals(STRATEGY_LNCLFSR)) {
			hashGen = new LinConLfsrHashGenerator(27182835);
		} else {
			System.err.println("Hash strategy " + hashStrategy + " invalid");
			System.exit(1);
		}

		stats = new HashStats();
	}

	/**
	 * Get the statistics for the hashcodes so far.
	 * @return HashStats object
	 */
	public static HashStats getStats() {
		return stats;
	}

	@Override
	public int hashCode() {
		if (null == hashGen) {
			/* use Object.hashCode() */
			return getRawHashCode();
		} else {
			/* modify the hashcode */
			return hashGen.processHashCode(getRawHashCode());
		}
	}

	protected int getRawHashCode() {
		return super.hashCode();
	}

	public HashCodeGenerator getHashGen() {
		return hashGen;
	}

	public void setHashGen(HashCodeGenerator hashGen) {
		HashedObject.hashGen = hashGen;
	}

	public boolean seedChanged() {
		if (null != hashGen) {
			return hashGen.seedChanged();
		} else {
			return false;
		}
	}

	public int getSeed() {
		if (null != hashGen) {
			int seed = hashGen.getSeed();
			return seed;
		} else {
			return 0;
		}
	}

	/**
	 * recursively walk the tree and verify that the current hashcode is the same as the original.
	 * @return null if all nodes in theis subtree are correct; reference to the failing node if not.
	 */
	public HashedObject verifyHashCode() {
		newHash = hashCode();
		if (myHashCode != newHash) {
			System.err.println("serial " + serialNumber + " old hash code "
					+ myHashCode + " new hash code " + newHash);
			return this;
		}
		HashedObject result = null;
		if (null != collisions) {
			result = collisions.verifyHashCode();
			if (null != result) {
				return result;
			}
		}
		if (null != left) {
			result = left.verifyHashCode();
			if (null != result) {
				return result;
			}
		}
		if (null != right) {
			result = right.verifyHashCode();
			if (null != result) {
				return result;
			}
		}
		return null;
	}

}
