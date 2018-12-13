/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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
package com.ibm.gpu;

import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;

@SuppressWarnings({ "boxing", "nls" })
final class PtxKernelGenerator {

	/**
	 * Generate the PTX code for a collection of sort network kernels.
	 *
	 * @param capability the compute capability of the device where the kernel
	 *     will be used
	 * @param elementType the type of the elements in the array to be sorted;
	 *     must be one of 'D', 'F', 'I' or 'J' (the Java encoding of primitive
	 *     types double, float, int or long, respectively)
	 * @param out where the code is to be written
	 * @throws IOException if a problem is encountered writing to the given output
	 *     stream
	 */
	public static void writeTo(int capability, char elementType, OutputStream out) throws IOException {
		PtxKernelGenerator generator = new PtxKernelGenerator(out, elementType);

		generator.generate(capability);
	}

	/**
	 * A string that in PTX encodes a value such that no value compares larger
	 * than it. For scalar types, this is the maximum value; for floating-point
	 * types, it is a non-signalling NaN.
	 */
	private final String maxValue;

	/**
	 * The current number of steps in the sort network kernel being generated.
	 */
	private int stepCount;

	/**
	 * Indicates if the element type is scalar (int, long) or not (double, float).
	 */
	private final boolean typeIsScalar;

	/**
	 * A string that expresses the data type in PTX code.
	 */
	private final String typeName;

	/**
	 * The size of the data type in bytes.
	 */
	private final int typeSize;

	/**
	 * An ASCII encoder for the user-supplied output stream.
	 */
	private final OutputStreamWriter writer;

	private PtxKernelGenerator(OutputStream out, char type) {
		super();
		switch (type) {
		case 'D':
			this.typeIsScalar = false;
			this.typeName = ".f64";
			this.typeSize = 8;
			this.maxValue = "0dFFF8000000000000"; // double NaN
			break;
		case 'F':
			this.typeIsScalar = false;
			this.typeName = ".f32";
			this.typeSize = 4;
			this.maxValue = "0f7FFFFFFF"; // float NaN
			break;
		case 'I':
			this.typeIsScalar = true;
			this.typeName = ".s32";
			this.typeSize = 4;
			this.maxValue = "0x" + Integer.toHexString(Integer.MAX_VALUE);
			break;
		case 'J':
			this.typeIsScalar = true;
			this.typeName = ".s64";
			this.typeSize = 8;
			this.maxValue = "0x" + Long.toHexString(Long.MAX_VALUE);
			break;
		default:
			throw new IllegalArgumentException(String.valueOf(type));
		}
		this.stepCount = 0;
		this.writer = new OutputStreamWriter(out, StandardCharsets.US_ASCII);
	}

	private void append(String line) throws IOException {
		writer.append(line).append('\n');
	}

	private void compare(int index0, int index1) throws IOException {
		// if (v[index0] > v[index1]) { tmp = v[index0]; v[index0] = v[index1]; v[index1] = tmp; moved |= (1 << index0) | (1 << index1); }
		if (typeIsScalar) {
			format("setp.gt%s p0,vl%d,vl%d;", typeName, index0, index1);
		} else {
			// Floating point NaN values need special handling because they're also
			// our out-of-bounds signal. So, if v[index1] is a NaN, we don't want to
			// swap. Otherwise if v[index0] is a NaN, or larger than v[index1] we
			// need to swap. The 'gtu' comparison is true if the first operand is
			// larger than the second or if either operand is NaN. By ruling out
			// the second operand being a NaN, we achieve the desired test.
			format("testp.number%s p1,vl%d;", typeName, index1);
			format("setp.gtu.and%s p0,vl%d,vl%d,p1;", typeName, index0, index1);

			// Finally we must order -0.0 before +0.0.

			// copy raw floating point bits to scalar registers
			format("@!p0 mov.b%d vs0,vl%d;", typeSize * 8, index0);
			format("@!p0 mov.b%d vs1,vl%d;", typeSize * 8, index1);

			// compare for floating point equality
			format("@!p0 setp.eq%s p1,vl%d,vl%d;", typeName, index0, index1);

			// compare as scalars, updating the result
			format("@!p0 setp.gt.and.s%d p0,vs0,vs1,p1;", typeSize * 8);
		}
	}

	private void compareAndSwap(int index0, int index1) throws IOException {
		compare(index0, index1);
		format("@p0 mov%s tmp,vl%d;", typeName, index0);
		format("@p0 mov%s vl%d,vl%d;", typeName, index0, index1);
		format("@p0 mov%s vl%d,tmp;", typeName, index1);
		format("@p0 or.b32 moved,moved,%s;", constant((1 << index0) | (1 << index1)));
	}

	private void computeIndices(boolean first) throws IOException {
		// At least 1 step, but no more than 5 because 'moved' holds only 32 (1 << 5) bits.
		assert 1 <= stepCount && stepCount <= 5;

		final int groupSize = 1 << stepCount;
		String step = "stride";

		if (stepCount != 1) {
			step = "step";
			format("shr.u32 step,stride,%d;", stepCount - 1);
		}

		format("sub.s32 mask,%s,1;", step);

		// threadId = globalThreadId();
		append("mov.u32 rt0,%nctaid.x;");
		append("mov.u32 rt1,%ctaid.y;");
		append("mov.u32 rt2,%ctaid.x;");
		append("mad.lo.u32 threadId,rt0,rt1,rt2;");
		append("mov.u32 rt0,%ntid.x;");
		append("mov.u32 rt1,%tid.x;");
		append("mad.lo.u32 threadId,threadId,rt0,rt1;");

		// Compute indices of input elements.

		// ix0 = ((threadId & ~mask) * groupSize) + (threadId & mask);
		append("not.b32 rt0,mask;");
		append("and.b32 rt0,rt0,threadId;");
		append("and.b32 rt1,threadId,mask;");
		format("mad.lo.u32 ix0,rt0,%d,rt1;", groupSize);

		if (first) {
			int halfGroupSize = groupSize >> 1;

			for (int i = 0; ++i < halfGroupSize;) {
				// ix[i] = ix[i - 1] + step;
				format("add.u32 ix%d,ix%d,%s;", i, i - 1, step);
			}

			// ix[halfGroupSize] = ix[halfGroupSize - 1] ^ ((stride << 1) - 1);
			append("mad.lo.u32 rt0,stride,2,-1;");
			format("xor.b32 ix%d,ix%d,rt0;", halfGroupSize, halfGroupSize - 1);

			for (int i = halfGroupSize; ++i < groupSize;) {
				// ix[i] = ix[i - 1] + step;
				format("add.u32 ix%d,ix%d,%s;", i, i - 1, step);
			}
		} else {
			for (int i = 0; ++i < groupSize;) {
				// ix[i] = ix[i - 1] + step;
				format("add.u32 ix%d,ix%d,%s;", i, i - 1, step);
			}
		}
	}

	private String constant(int value) {
		String format;

		switch (stepCount) {
		case 1:
			format = "%d";
			break;
		case 2:
			format = "0x%x";
			break;
		case 3:
			format = "0x%02x";
			break;
		default:
			format = "0x%04x";
			break;
		}

		return String.format(format, value);
	}

	private void declareLocals() throws IOException {
		int groupSize = 1 << stepCount;

		append(".reg .u64 data;");
		append(".reg .u32 length;");
		append(".reg .u32 stride;");
		append(".reg .u32 threadId;");
		append(".reg .u32 mask;");
		if (stepCount != 1) {
			append(".reg .u32 step;");
		}
		format(".reg %s tmp;", typeName);
		append(".reg .b32 moved;");
		append(".reg .b32 bit;");
		append(".reg .u32 rt<3>;");
		format(".reg .u32 ix<%d>;", groupSize);
		format(".reg %s vl<%d>;", typeName, groupSize);
		append(".reg .pred p<2>;");
		format(".reg .s%d vs<2>;", typeSize * 8);
		append(".reg .u64 ptr;");
	}

	private void emitFirstPhases() throws IOException {
		final int phaseCount = 9;
		final int inputCount = 1 << phaseCount;
		final int workUnits = 1 << (phaseCount - 1);

		append(".visible .entry");
		format("phase%d(.param .u64 _data,.param .u32 _length)", phaseCount);
		append(".maxntid 256,1,1");
		append("{");

		format(".shared .align %d %s _sharedData[%d];", typeSize, typeName, inputCount);

		append(".reg .u64 data;");
		append(".reg .u32 length;");
		append(".reg .u64 sharedData;");
		append(".reg .u64 dataPtr;");
		append(".reg .u64 sharedPtr<2>;");
		append(".reg .u32 baseIndex;");
		append(".reg .u32 blockDimX;");
		append(".reg .u32 globalIndex;");
		append(".reg .u32 workId;");
		append(".reg .pred p<2>;");
		format(".reg .s%d vs<2>;", typeSize * 8);
		append(".reg .u32 ix<2>;");
		append(".reg .u32 rt<3>;");
		format(".reg %s vl<2>;", typeName);

		append("ld.param.u64 data,[_data];");
		append("cvta.to.global.u64 data,data;");
		append("ld.param.u32 length,[_length];");
		append("mov.u64 sharedData,_sharedData;");

		// blockDimX = blockDim.x;
		append("mov.u32 blockDimX,%ntid.x;");

		// baseIndex = globalBlockId() << phaseCount;
		append("mov.u32 rt0,%nctaid.x;");
		append("mov.u32 rt1,%ctaid.y;");
		append("mov.u32 rt2,%ctaid.x;");
		append("mad.lo.u32 baseIndex,rt0,rt1,rt2;");
		format("shl.b32 baseIndex,baseIndex,%d;", phaseCount);

		// Load data into shared memory.

		// workId = threadIdx.x;
		append("mov.u32 workId,%tid.x;");
		// while (workId < inputCount)
		append("bra loadTest;");
		append("loadLoop:");

		append("add.u32 globalIndex,baseIndex,workId;");

		// sharedData[workId] = (globalIndex < length) ? data[globalIndex] : maxValue;
		format("mov%s vl0,%s;", typeName, maxValue);
		append("setp.lt.u32 p0,globalIndex,length;");
		format("@p0 mad.wide.u32 dataPtr,globalIndex,%d,data;", typeSize);
		format("@p0 ld.global%s vl0,[dataPtr];", typeName);
		format("mad.wide.u32 sharedPtr0,workId,%d,sharedData;", typeSize);
		format("st.shared%s [sharedPtr0],vl0;", typeName);

		// workId += blockDim.x;
		append("add.u32 workId,workId,blockDimX;");

		append("loadTest:");
		format("setp.lt.u32 p0,workId,%d;", inputCount);
		append("@p0 bra loadLoop;");

		for (int phase = 0; phase < phaseCount; ++phase) {
			for (int step = 0; step <= phase; ++step) {
				append("bar.sync 0;");

				String workLoop = String.format("workLoop_%d_%d", phase + 1, step + 1);
				String workTest = String.format("workTest_%d_%d", phase + 1, step + 1);

				// workId = threadIdx.x
				append("mov.u32 workId,%tid.x;");
				// while (workId < workUnits)
				format("bra %s;", workTest);
				format("%s:", workLoop);

				if (step == phase) {
					// ix0 = workId * 2;
					append("shl.b32 ix0,workId,1;");
				} else {
					// ix0 = (workId * 2) - (workId & ((1 << (phase - step)) - 1));
					append("shl.b32 ix0,workId,1;");
					format("and.b32 rt0,workId,%s;", constant((1 << (phase - step)) - 1));
					append("sub.u32 ix0,ix0,rt0;");
				}

				if (step == 0 && step != phase) {
					// ix1 = ix0 ^ ((2 << phase) - 1);
					format("xor.b32 ix1,ix0,%s;", constant((2 << phase) - 1));
				} else {
					// ix1 = ix0 + (1 << (phase - step));
					format("add.u32 ix1,ix0,%s;", constant(1 << (phase - step)));
				}

				// vl0 = sharedData[ix0];
				format("mad.wide.u32 sharedPtr0,ix0,%d,sharedData;", typeSize);
				format("ld.shared%s vl0,[sharedPtr0];", typeName);

				// vl1 = sharedData[ix1];
				format("mad.wide.u32 sharedPtr1,ix1,%d,sharedData;", typeSize);
				format("ld.shared%s vl1,[sharedPtr1];", typeName);

				// if (vl0 > vl1) { sharedData[ix0] = vl1; sharedData[ix1] = vl0; }
				compare(0, 1);
				format("@p0 st.shared%s [sharedPtr0],vl1;", typeName);
				format("@p0 st.shared%s [sharedPtr1],vl0;", typeName);

				// workId += blockDim.x;
				append("add.u32 workId,workId,blockDimX;");

				format("%s:", workTest);
				format("setp.lt.u32 p0,workId,%d;", workUnits);
				format("@p0 bra %s;", workLoop);
			}
		}

		append("bar.sync 0;");

		// Store data back in global memory.

		// workId = threadIdx.x;
		append("mov.u32 workId,%tid.x;");
		// while (workId < inputCount)
		append("bra storeTest;");
		append("storeLoop:");
		append("{");

		// globalIndex = baseIndex + workId;
		append("add.u32 globalIndex,baseIndex,workId;");

		// if (globalIndex < length) { data[globalIndex] = sharedData[workId]; }
		append("setp.lt.u32 p0,globalIndex,length;");
		format("@p0 mad.wide.u32 sharedPtr0,workId,%d,sharedData;", typeSize);
		format("@p0 ld.shared%s vl0,[sharedPtr0];", typeName);
		format("@p0 mad.wide.u32 dataPtr,globalIndex,%d,data;", typeSize);
		format("@p0 st.global%s [dataPtr],vl0;", typeName);

		// workId += blockDim.x;
		append("add.u32 workId,workId,blockDimX;");

		append("}");
		append("storeTest:");
		format("setp.lt.u32 p0,workId,%d;", inputCount);
		append("@p0 bra storeLoop;");

		append("}");
	}

	private void emitKernel(boolean first) throws IOException {
		append(".visible .entry");
		format("%s%d(.param .u64 _data,.param .u32 _length,.param .u32 _stride)", // <br/>
				(first ? "first" : "other"), stepCount);
		append(".maxntid 256,1,1");
		append("{");

		declareLocals();

		append("ld.param.u64 data,[_data];");
		append("cvta.to.global.u64 data,data;");
		append("ld.param.u32 length,[_length];");
		append("ld.param.u32 stride,[_stride];");

		computeIndices(first);
		gatherData();
		sortLocally(first);
		scatterData();

		append("}");
	}

	private void emitPreamble(int capability) throws IOException {
		append(".version 3.2");
		format(".target sm_%d", (capability < 3) ? 20 : 30);
		append(".address_size 64");
	}

	private void format(String format, Object... arguments) throws IOException {
		append(String.format(format, arguments));
	}

	private void gatherData() throws IOException {
		// Get a local copy of relevant data elements.

		int groupSize = 1 << stepCount;

		for (int i = 0; i < groupSize; ++i) {
			// vl[i] = maxValue; if (ix[i] < length) { vl[i] = data[ix[i]]; }
			format("mov%s vl%d,%s;", typeName, i, maxValue);
			format("setp.lt.u32 p0,ix%d,length;", i);
			format("@p0 mad.wide.u32 ptr,ix%d,%d,data;", i, typeSize);
			format("@p0 ld.global%s vl%d,[ptr];", typeName, i);
		}
	}

	private void generate(int capability) throws IOException {
		emitPreamble(capability);

		final int MaxStepCount = 4;

		for (stepCount = 1;; ++stepCount) {
			emitKernel(false);
			if (stepCount == MaxStepCount) {
				emitKernel(true);
				break;
			}
		}

		emitFirstPhases();

		writer.flush();
	}

	private void scatterData() throws IOException {
		// Store data elements that have moved.
		for (int i = 0, groupSize = 1 << stepCount; i < groupSize; ++i) {
			// if (moved & (1 << i)) { data[ix[i]] = vl[i]; }
			format("and.b32 bit,moved,%s;", constant(1 << i));
			append("setp.ne.b32 p0,bit,0;");
			format("@p0 mad.wide.u32 ptr,ix%d,%d,data;", i, typeSize);
			format("@p0 st.global%s [ptr],vl%d;", typeName, i);
		}
	}

	private void sortLocally(boolean first) throws IOException {
		final int groupSize = 1 << stepCount;
		final int halfGroupSize = groupSize >> 1;
		int step = 0;

		append("mov.b32 moved,0;");

		if (first) {
			int groupMirror = groupSize - 1;

			for (int index0 = 0; index0 < halfGroupSize; ++index0) {
				int index1 = index0 ^ groupMirror;

				compareAndSwap(index0, index1);
			}

			step += 1;
		}

		// write the 'other' steps
		for (; step < stepCount; ++step) {
			int stepStride = groupSize >> (step + 1);
			int baseStep = stepStride << 1;

			for (int base = 0; base < groupSize; base += baseStep) {
				for (int index = 0; index < stepStride; ++index) {
					int index0 = base + (index & -stepStride) + index;
					int index1 = index0 + stepStride;

					compareAndSwap(index0, index1);
				}
			}
		}
	}

}
