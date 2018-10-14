/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

package com.ibm.j9ddr.corereaders.tdump.zebedee.util;

/**
 * This class provides a compact representation of an array of sparse bit sets. Each individual
 * set is implemented by one of a number of classes which provide optimum memory usage for the
 * size of set.
 */

// XXX add a method to set from an array of bits

// XXX reuse IntEnums

public class BitSetArray {

    /** Possible values: zero indicates empty, negative indicates single member,
     *  positive indicates index + 1 into sets list. */
    private int[] roots;
    private int setsSize;
    private BitSet[] sets;
    private int tmpintervals[];
    private int tmplengths[];

    /**
     * Create a new BitSetArray with the given number of members (it will still grow as necessary
     * if an attempt is made to set bits at an index greater than the given size).
     * @param size the initial number of sets
     */
    public BitSetArray(int size) {
        roots = new int[size];
        sets = new BitSet[size];
    }

    /**
     * Create a new BitSetArray with the default number of members being 16 (it will grow
     * as necessary).
     */
    public BitSetArray() {
        this(16);
    }

    /**
     * Set a bit in a bit set.
     * @param index the index of the bit set
     * @param bit the number of the bit in the bit set
     */
    public void set(int index, int bit) {
        assert bit >= 0;
        assert index >= 0;
        ensureRootsCapacity(index + 1);
        int root = roots[index];
        if (root == 0) {
            roots[index] = bit | 0x80000000;
        } else if (root < 0) {
            roots[index] = addSet(new TwoMemberSet(root & 0x7fffffff, bit));
        } else {
            sets[root - 1] = sets[root - 1].set(bit);
        }
    }

    /**
     * Do a boolean OR operation on the two sets. The second set is ORed into the first set.
     */
    public void or(int toIndex, int fromIndex) {
        assert toIndex >= 0 && toIndex < roots.length : toIndex;
        assert fromIndex >= 0 && fromIndex < roots.length : fromIndex;
        if (roots[fromIndex] < 0) {
            /* Just need to set a single bit */
            set(toIndex, roots[fromIndex] & 0x7fffffff);
        } else if (roots[fromIndex] > 0) {
            int fromRoot = roots[fromIndex] - 1;
            if (roots[toIndex] == 0) {
                /* To is empty so just copy the from */
                roots[toIndex] = addSet(sets[fromRoot].cloneSet());
            } else if (roots[toIndex] < 0) {
                /* Clone the from set and then add the single bit */
                int bit = roots[toIndex] & 0x7fffffff;
                roots[toIndex] = addSet(sets[fromRoot].cloneSet());
                set(toIndex, bit);
            } else {
                /* Or the two sets together */
                int toRoot = roots[toIndex] - 1;
                sets[toRoot] = sets[toRoot].or(sets[fromRoot]);
            }
        } else {
            /* From is empty, nothing to do */
        }
    }

    /**
     * Add the set to the sets array and return the index + 1 (all indexes are thus
     * non-zero).
     */
    private int addSet(BitSet set) {
        ensureSetsCapacity(setsSize + 1);
        sets[setsSize++] = set;
        return setsSize;
    }

    private void ensureRootsCapacity(int minCapacity) {
        int oldCapacity = roots.length;
        if (minCapacity > oldCapacity) {
            int newCapacity = (oldCapacity * 3)/2 + 1;
            if (newCapacity < minCapacity)
                newCapacity = minCapacity;
            int[] tmp = new int[newCapacity];
            System.arraycopy(roots, 0, tmp, 0, oldCapacity);
            roots = tmp;
        }
    }

    private void ensureSetsCapacity(int minCapacity) {
        int oldCapacity = sets.length;
        if (minCapacity > oldCapacity) {
            int newCapacity = (oldCapacity * 3)/2 + 1;
            if (newCapacity < minCapacity)
                newCapacity = minCapacity;
            BitSet[] tmp = new BitSet[newCapacity];
            System.arraycopy(sets, 0, tmp, 0, oldCapacity);
            sets = tmp;
        }
    }

    static int[] arraycopy(int[] array, int size) {
        int[] newArray = new int[size];
        System.arraycopy(array, 0, newArray, 0, array.length);
        return newArray;
    }

    /**
     * Returns an IntEnumeration of all the elements in the set at the given index. Note that
     * the implementation of IntEnumeration that is returned has an important limitation: for
     * performance reasons, these objects are reused and once you have reached the end of the
     * enumeration (ie hasMoreElements returned false) the object may be recycled. For that
     * reason you cannot call hasMoreElements more than once once it has returned false. I
     * can't think why you would want to do that but I just thought I'd mention it!
     */
    public IntEnumeration elements(int index) {
        int root = roots[index];
        if (root == 0) {
            return nullEnum;
        } else if (root < 0) {
            return null;
        } else {
        }
        return null;
    }
    
    IntEnumeration nullEnum = new IntEnumeration() {
        public boolean hasMoreElements() { return false; }
        public Object nextElement() { return null; }
        public long nextInt() { return 0; }
    };

    class SmallEnum implements IntEnumeration {
        int[] bits = new int[SmallSet.MAX_SIZE];
        int size;
        int index;

        SmallEnum(int bit) {
            reset(bit);
        }

        void reset(int bit) {
            bits[0] = bit;
            size = 1;
            index = 0;
        }

        public boolean hasMoreElements() {
            return index < size;
        }

        public Object nextElement() {
            return null;
        }

        public long nextInt() {
            return bits[index++];
        }
    }

    /**
     * This interface is implemented by the a number of classes. Each class may promote itself
     * to the next class up by returning a new representation from the setter methods.
     */
    interface BitSet {
        /** Set the required bit and return the resulting representation */
        BitSet set(int bit);
        /** Or the given set into this set and return the resulting representation */
        BitSet or(BitSet set);
        BitSet cloneSet();
    }

    class TwoMemberSet implements BitSet, Cloneable {
        private int l;
        private int h;

        TwoMemberSet(int b1, int b2) {
            if (b1 < b2) {
                l = b1;
                h = b2;
            } else {
                l = b2;
                h = b1;
            }
        }

        public BitSet set(int bit) {
            if (bit == l || bit == h)
                return this;
            if (bit < l)
                return new ThreeMemberSet(bit, l, h);
            else if (bit < h)
                return new ThreeMemberSet(l, bit, h);
            else
                return new ThreeMemberSet(l, h, bit);
        }

        public BitSet or(BitSet set) {
            if (set.getClass() == TwoMemberSet.class) {
                TwoMemberSet tset = (TwoMemberSet)set;
                return set(tset.l).set(tset.h);
            }
            return (set.cloneSet()).or(this);
        }

        public BitSet cloneSet() {
            try {
                return (TwoMemberSet)super.clone();
            } catch (CloneNotSupportedException e) {
                throw new Error("impossible");
            }
        }
    }

    class ThreeMemberSet implements BitSet, Cloneable {
        private int l;
        private int m;
        private int h;

        ThreeMemberSet(int l, int m, int h) {
            this.l = l;
            this.m = m;
            this.h = h;
        }

        public BitSet set(int bit) {
            if (bit == l || bit == m || bit == h)
                return this;
            if (bit < l)
                return new SmallSet(bit, l, m, h);
            else if (bit < m)
                return new SmallSet(l, bit, m, h);
            else if (bit < h)
                return new SmallSet(l, m, bit, h);
            else
                return new SmallSet(l, m, h, bit);
        }

        public BitSet or(BitSet set) {
            if (set.getClass() == TwoMemberSet.class) {
                TwoMemberSet tset = (TwoMemberSet)set;
                return set(tset.l).set(tset.h);
            } else if (set.getClass() == ThreeMemberSet.class) {
                ThreeMemberSet tset = (ThreeMemberSet)set;
                return set(tset.l).set(tset.m).set(tset.h);
            }
            return (set.cloneSet()).or(this);
        }

        public BitSet cloneSet() {
            try {
                return (ThreeMemberSet)super.clone();
            } catch (CloneNotSupportedException e) {
                throw new Error("impossible");
            }
        }
    }

    /**
     * Very simple implementation that just uses an array of bit values.
     */
    class SmallSet implements BitSet, Cloneable {
        private int[] bits = new int[8];
        private int numBits = 4;
        private static final int MAX_SIZE = 16;

        SmallSet(int b1, int b2, int b3, int b4) {
            bits[0] = b1;
            bits[1] = b2;
            bits[2] = b3;
            bits[3] = b4;
        }

        public BitSet set(int bit) {
            int[] oldBits = bits;
            int[] newBits = oldBits;
            int n = numBits;
            for (int i = 0; i <= n; i++) {
                if (i == n || bit < oldBits[i]) {
                    if (n == oldBits.length) {
                        if (n < MAX_SIZE) {
                            newBits = new int[n << 1];
                            bits = newBits;
                            for (int j = 0; j < i; j++) {
                                newBits[j] = oldBits[j];
                            }
                        } else {
                            return new IntervalSet(bits, bit);
                        }
                    }
                    for (int j = n; j > i; j--) {
                        newBits[j] = oldBits[j - 1];
                    }
                    newBits[i] = bit;
                    numBits++;
                    return this;
                } else if (bit == oldBits[i]) {
                    return this;
                }
            }
            assert false : "should never reach here";
            return null;
        }

        public BitSet or(BitSet set) {
            if (set.getClass() == TwoMemberSet.class) {
                TwoMemberSet tset = (TwoMemberSet)set;
                return set(tset.l).set(tset.h);
            } else if (set.getClass() == ThreeMemberSet.class) {
                ThreeMemberSet tset = (ThreeMemberSet)set;
                return set(tset.l).set(tset.m).set(tset.h);
            } else if (set.getClass() == SmallSet.class) {
                SmallSet oset = (SmallSet)set;
                BitSet nset = this;
                int n = oset.numBits;
                int[] obits = oset.bits;
                for (int i = 0; i < n; i++) {
                    nset = nset.set(obits[i]);
                }
                return nset;
            }
            assert set.getClass() == IntervalSet.class;
            return (set.cloneSet()).or(this);
        }

        public BitSet cloneSet() {
            try {
                SmallSet clone = (SmallSet)super.clone();
                clone.bits = (int[])bits.clone();
                return clone;
            } catch (CloneNotSupportedException e) {
                throw new Error("impossible");
            }
        }
    }

    /**
     * This implementation uses intervals. An interval consists of a start bit and a length.
     */
    class IntervalSet implements BitSet, Cloneable {

        private int size;
        private int[] intervals;
        private int[] lengths;

        IntervalSet(int[] bits, int bit) {
            intervals = new int[bits.length << 1];
            lengths = new int[bits.length << 1];
            for (int i = 0; i < bits.length; i++) {
                set(bits[i]);
            }
            set(bit);
        }

        public BitSet set(int bit) {
            int low = 0;
            int high = size - 1;
            int mid = -1;
            int midVal = -1;
            int max = intervals.length;

            if (size == 0) {
                intervals[0] = bit;
                lengths[0] = 1;
                size = 1;
                return this;
            }

            while (low <= high) {
                mid = (low + high) >> 1;
                midVal = intervals[mid];

                if (bit >= midVal) {
                    if (bit < (midVal + lengths[mid]))
                        return this;                      // already set
                    else if (bit == (midVal + lengths[mid])) {
                        lengths[mid] = lengths[mid] + 1;  // add to interval
                        if (mid < (size - 1) && (bit + 1) == intervals[mid + 1]) {
                            intervalMerge(mid);
                        }
                        return this;
                    }
                    low = mid + 1;                        // continue search
                } else {
                    if (bit == (midVal - 1)) {
                        // add to interval
                        intervals[mid] = intervals[mid] - 1;
                        lengths[mid] = lengths[mid] + 1;
                        if (mid > 0 && bit == (intervals[mid - 1] + lengths[mid - 1])) {
                            intervalMerge(mid - 1);
                        }
                        return this;
                    }
                    high = mid - 1;                       // continue search
                }
            }
            if (size == max) {
                max <<= 1;
                intervals = BitSetArray.arraycopy(intervals, max);
                lengths = BitSetArray.arraycopy(lengths, max);
            }
            if (bit < intervals[0]) {
                System.arraycopy(intervals, 0, intervals, 1, size);
                System.arraycopy(lengths, 0, lengths, 1, size);
                intervals[0] = bit;
                lengths[0] = 1;
            } else if (bit > intervals[size - 1]) {
                intervals[size] = bit;
                lengths[size] = 1;
            } else {
                if (bit < midVal)
                    mid--;
                assert size - mid - 1 > 0;
                System.arraycopy(intervals, mid + 1, intervals, mid + 2, size - mid - 1);
                System.arraycopy(lengths, mid + 1, lengths, mid + 2, size - mid - 1);
                intervals[mid + 1] = bit;
                lengths[mid + 1] = 1;
            }
            size++;
            return this;
        }

        /**
         * This method check for overlapping intervals and merges them if found. Note that
         * this can result in the size of the arrays actually shrinking!
         */
        private void intervalMerge(int lower) {
            int max = intervals.length;
            int lowInt = intervals[lower];
            int highInt = intervals[lower + 1];
            int lowLen = lengths[lower];
            int highLen = lengths[lower + 1];
            int lowTop = lowInt + lowLen;
            int highTop = highInt + highLen;
            int bottom;
            if (lowInt < highInt) {
                intervals[lower] = lowInt;
                bottom = lowInt;
            } else {
                intervals[lower] = highInt;
                bottom = highInt;
            }
            if (lowTop < highTop) {
                lengths[lower] = highTop - bottom;
            } else {
                lengths[lower] = lowTop - bottom;
            }
            if (lower < size - 2) {
                int len = size - (lower + 2);
                System.arraycopy(intervals, lower + 2, intervals, lower + 1, len);
                System.arraycopy(lengths, lower + 2, lengths, lower + 1, len);
            }
            size--;
            assert size > 0;
            if (size * 3 < max) {
                max >>= 1;
                int[] nintervals = new int[max];
                int[] nlengths = new int[max];
                System.arraycopy(intervals, 0, nintervals, 0, size);
                System.arraycopy(lengths, 0, nlengths, 0, size);
                intervals = nintervals;
                lengths = nlengths;
            }
            if (lower > 0 && intervals[lower] <= (intervals[lower - 1] + lengths[lower - 1])) {
                intervalMerge(lower - 1);
            }
            if (lower < (size - 1) && (intervals[lower] + lengths[lower]) >= intervals[lower + 1]) {
                intervalMerge(lower);
            }
        }

        public BitSet or(BitSet set) {
            assert false;
            return null;
        }

        /*
        int intervalOr(int root, int oroot) {
            Assert(getType(root) == MEDIUM_SET && getType(oroot) == MEDIUM_SET);
            int r = root & MAX_BIT;
            int or = oroot & MAX_BIT;
            int uslen = memory.get(r);
            int themlen = memory.get(or);
            Assert(uslen != 0x7fffffff);
            Assert(themlen != 0x7fffffff);
            int intervals = r + 2;
            int ointervals = or + 2;
            int max = memory.get(r + 1);
            int lengths = intervals + max;
            int olengths = ointervals + memory.get(or + 1);
            if (tmpintervals == null || tmpintervals.length < (uslen + themlen)) {
                tmpintervals = new int[uslen + themlen];
                tmplengths = new int[uslen + themlen];
            }
            int count = 0, us = 0, them = 0;
            if (memory.get(intervals) <= memory.get(ointervals)) {
                tmpintervals[0] = memory.get(intervals);
                tmplengths[0] = memory.get(lengths);
                us++;
            } else {
                tmpintervals[0] = memory.get(ointervals);
                tmplengths[0] = memory.get(olengths);
                them++;
            }
            for (; us < uslen || them < themlen; ) {
                int intervalsThem = 0x7fffffff, intervalsUs = 0x7fffffff;
                if (us < uslen) {
                    intervalsUs = memory.get(intervals + us);
                }
                if (them < themlen) {
                    intervalsThem = memory.get(ointervals + them);
                }
                if (us == uslen || (them != themlen && intervalsThem <= intervalsUs)) {
                    if (intervalsThem > (tmpintervals[count] + tmplengths[count])) {
                        count++;
                        tmpintervals[count] = intervalsThem;
                        tmplengths[count] = memory.get(olengths + them);
                    } else {
                        int tmpmax = tmpintervals[count] + tmplengths[count];
                        int themmax = intervalsThem + memory.get(olengths + them);
                        if (tmpmax > themmax) 
                            tmplengths[count] = tmpmax - tmpintervals[count];
                        else
                            tmplengths[count] = themmax - tmpintervals[count];
                    }
                    them++;
                } else {
                    if (intervalsUs > (tmpintervals[count] + tmplengths[count])) {
                        count++;
                        tmpintervals[count] = intervalsUs;
                        tmplengths[count] = memory.get(lengths + us);
                    } else {
                        int tmpmax = tmpintervals[count] + tmplengths[count];
                        int usmax = intervalsUs + memory.get(lengths + us);
                        if (tmpmax > usmax) 
                            tmplengths[count] = tmpmax - tmpintervals[count];
                        else
                            tmplengths[count] = usmax - tmpintervals[count];
                    }
                    us++;
                }
            }
            count++;
            if (count > max) {
                free(root);
                max = (count * 3) / 2;
                root = rootalloc((max << 1) + 2);
                intervals = root + 2;
                lengths = intervals + max;
                memory.put(root + 1, max);
            }
            for (int i = 0; i < count; i++) {
                memory.put(intervals + i, tmpintervals[i]);
                memory.put(lengths + i, tmplengths[i]);
            }
            memory.put(root & MAX_BIT, count);
            root = setType(root, MEDIUM_SET);
            if (max > maxmedium) {
                int oldRoot = root;
                root = promoteToLarge(root);
                free(oldRoot);
                if (verbose) System.out.println("promoted medium set of size " + max);
            }
            return root;
        }
        */

        public BitSet cloneSet() {
            try {
                IntervalSet clone = (IntervalSet)super.clone();
                clone.intervals = (int[])intervals.clone();
                clone.lengths = (int[])lengths.clone();
                return clone;
            } catch (CloneNotSupportedException e) {
                throw new Error("impossible");
            }
        }
    }
}

    /**
     * This class is only used for testing.
     */
    /*
    static class TestBits {
        BitSet bits;
        static BitSetArray sets = new BitSetArray(0);
        int set;
        static boolean closed;

        TestBits(int min, int interval, int max) {
            if (closed) {
                sets = new BitSetArray(0);
                closed = false;
            }
            set = sets.addSlot();
            bits = new BitSet();
            if (interval == 0)
                interval = 1;
            for (int i = min; i < max; i += interval) {
                sets.set(set, i);
                bits.set(i);
            }
            sets.checkMemory();
        }

        TestBits(int max) {
            if (closed) {
                sets = new BitSetArray(0);
                closed = false;
            }
            set = sets.addSlot();
            bits = new BitSet();
            for (int i = 0; i < max; i++) {
                bits.set(i);
            }
            sets.setAll(set, max - 1);
            sets.checkMemory();
        }

        TestBits(int min, int max) {
            if (closed) {
                sets = new BitSetArray(0);
                closed = false;
            }
            set = sets.addSlot();
            bits = new BitSet();
            set(min, max - 1);
            sets.checkMemory();
        }

        TestBits() {
            if (closed) {
                sets = new BitSetArray(0);
                closed = false;
            }
        }

        void closeAll() {
            sets.checkMemory();
            sets.close();
            sets.checkMemory();
            closed = true;
        }

        void close() {
            sets.checkMemory();
            sets.close(set);
            sets.checkMemory();
        }

        public Object clone() {
            TestBits c = new TestBits();
            c.bits = (BitSet)bits.clone();
            c.set = sets.addSlot();
            sets.roots[c.set] = sets._clone(sets.roots[set]);
            sets.checkMemory();
            return c;
        }

        void set(int fromBit, int toBit) {
            sets.set(set, fromBit, toBit);
            for (int i = fromBit; i <= toBit; i++)
                bits.set(i);
            sets.checkMemory();
        }

        void set(int bit) {
            sets.set(set, bit);
            bits.set(bit);
            sets.checkMemory();
        }

        void and(TestBits other) {
            if (set < 0) throw new Error("bad");
            if (other.set < 0) throw new Error("bad");
            sets.and(set, other.set);
            bits.and(other.bits);
            sets.checkMemory();
        }

        void or(TestBits other) {
            sets.or(set, other.set);
            bits.or(other.bits);
            sets.checkMemory();
        }

        void clearAll() {
            sets.clearAll(set);
            bits = new BitSet();
            sets.checkMemory();
        }

        void check() {
            sets.check(set);
        }

        void compare() {
            int length = bits.length();
            if (length != sets.length(set))
                throw new Error("length mismatch, bits = " + length + " set = " + sets.length(set));
            for (int i = 0; i < length; i++) {
                if (bits.get(i) != sets.get(set, i))
                    throw new Error("mismatch on bit " + i);
            }
        }

        void expect(int min, int interval, int max) {
            sets.checkMemory();
            int count = 0;
            int i;
            boolean done = false;
            for (i = min; i < max; i += interval, count++) {
                done = true;
                if (!sets.get(set, i))
                    throw new Error("i " + i + " was not set!");
            }
            if (done && sets.length(set) != (i - interval + 1))
                throw new Error("expected length " + (i - interval) + " but found " + sets.length(set));
            if (sets.numberOfElements(set) != count)
                throw new Error("expected " + count + " found " + sets.numberOfElements(set));
            for (i = max; i < max * 2; i++) {
                if (sets.get(set, i))
                    throw new Error("i " + i + " IS set!");
            }
            BitSet nbits = new BitSet();
            for (IntEnumeration e = sets.elements(set); e.hasMoreElements(); ) {
                i = e.nextInt();
                if (!bits.get(i))
                    throw new Error("i " + i + " was not set!");
                nbits.set(i);
            }
            if (!bits.equals(nbits))
                throw new Error("enum did not match");
        }
    }

    /**
     * Run some tests on this class.
     */
    /*
    public static void main(String args[]) {
        TestBits a = new TestBits(0, 128);
        a.expect(0, 1, 128);
        a.set(128, 255);
        a.expect(0, 1, 256);
        a = new TestBits(0, 1);
        for (int i = 2; i < 1000; i+= 2) {
            a.set(i, i);
        }
        a.expect(0, 2, 1000);
        for (int i = 1; i < 1000; i+= 2) {
            a.set(i, i);
        }
        a.expect(0, 1, 1000);
        TestBits.sets.printMemoryUsage();
        TestBits b = new TestBits(0, 2, 59);
        b.close();
        a = new TestBits(0, 2, 65);
        a.expect(0, 2, 65);
        System.out.println("first ok");
        a = new TestBits(0, 2, 2049);
        a.expect(0, 2, 2049);
        TestBits e = new TestBits(6833, 71, 7034);
        e.compare();
        TestBits ec = (TestBits)e.clone();
        ec.compare();
        a = new TestBits(0, 1, 1000);
        a.expect(0, 1, 1000);
        a = new TestBits(0, 2, 1000);
        a.expect(0, 2, 1000);
        b = new TestBits(1, 2, 1000);
        b.close();
        b.expect(1, 2, 1000);
        a.or(b);
        a.expect(0, 1, 1000);
        b = new TestBits(1000, 2, 2000);
        b.expect(1000, 2, 2000);
        a.or(b);
        b.expect(1000, 2, 2000);
        b = new TestBits(1001, 2, 2000);
        b.close();
        a.or(b);
        b.expect(1001, 2, 2000);
        a.expect(0, 1, 2000);
        a = new TestBits(0, 2, 8000);
        b = new TestBits(1, 2, 2);
        b.or(a);
        a = new TestBits(3, 2, 8000);
        TestBits c = new TestBits(3, 2, 8000);
        c.or(b);
        c.expect(0, 1, 8000);
        c = new TestBits(3, 2, 4);
        c.or(b);
        c.or(new TestBits(0, 1, 3));
        TestBits d = new TestBits(3, 2, 4);
        d.or(b);
        c.or(d);
        c.or(new TestBits(3, 2, 8000));
        c.expect(0, 1, 8000);
        b.or(a);
        b.expect(0, 1, 8000);
        for (int i = 0; i < 1000; i += 1) {
            System.out.println("doing " + i);
            a = new TestBits(0, 100, i);
            a.expect(0, 100, i);
            a = new TestBits(0, 2, i);
            a.expect(0, 2, i);
            a = new TestBits(0, 1, i);
            a.expect(0, 1, i);
            a = new TestBits(0, 1, i);
            a.check();
            b = new TestBits(0, 2, i);
            c = new TestBits(i);
            b.close();
            c.close();
            a.and(b);
            a.check();
            a.and(c);
            a.expect(0, 2, i);
            a = new TestBits(0, 256, i);
            b = new TestBits(0, 128, i);
            a.or(b);
            a.expect(0, 128, i);
            b.clearAll();
            b = new TestBits(0, 4, i);
            a.or(b);
            a.expect(0, 4, i);
            c = new TestBits(0, 1, i);
            b.or(c);
            a.expect(0, 4, i);
            a = new TestBits(0, 2, i);
            b = new TestBits(0, 1, i);
            a.or(b);
            a.expect(0, 1, i);
            a.closeAll();
        }
        //Random r = new Random(System.currentTimeMillis());
        Random r = new Random(23);
        for (int i = 0; i < 10000; i++) {
            int interval = r.nextInt(100);
            int min = r.nextInt(10000);
            int max = min + r.nextInt(10000);
            System.out.println("i = " + i + " doing min " + min + " int " + interval + " max " + max);
            a = new TestBits(min, interval, max);
            a.compare();
            a.close();
            a.expect(min, interval, max);
            a = new TestBits(min, interval, max);
            interval = r.nextInt(100);
            min = r.nextInt(10000);
            max = min + r.nextInt(10000);
            System.out.println("doing min " + min + " int " + interval + " max " + max);
            b = new TestBits(min, interval, max);
            b.close();
            b.expect(min, interval, max);
            a.or(b);
            a.compare();
            interval = r.nextInt(100);
            min = r.nextInt(10000);
            max = min + r.nextInt(10000);
            System.out.println("doing min " + min + " int " + interval + " max " + max);
            c = new TestBits(min, interval, max);
            c.and(a);
            c.compare();
            for (int j = 0; j < 100; j++) {
                a.set(r.nextInt(1000));
                a.set(r.nextInt(10000));
                c.set(r.nextInt(1000));
            }
            a.compare();
            b.compare();
            c.compare();
            a.close();
            a.compare();
            TestBits ac = (TestBits)a.clone();
            ac.compare();
            TestBits bc = (TestBits)b.clone();
            bc.compare();
            TestBits cc = (TestBits)c.clone();
            cc.compare();
            cc.clearAll();
            cc.compare();
            a.closeAll();
        }
    }
    */
