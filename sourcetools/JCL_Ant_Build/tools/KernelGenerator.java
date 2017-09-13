/*******************************************************************************
 * Copyright (c) 2013, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
import java.io.PrintStream;
import java.util.Calendar;

/**
 * Tool to generate kernels for bitonic sort networks.
 *
 * @author keithc
 */
@SuppressWarnings("nls")
public final class KernelGenerator {

	public static void main(String[] args) {
		KernelGenerator generator = new KernelGenerator(System.out);

		generator.writeAll();
	}

	private int indent;

	private final PrintStream out;

	private int stepCount;

	private KernelGenerator(PrintStream out) {
		super();
		this.indent = 0;
		this.out = out;
		this.stepCount = 0;
	}

	private void compareAndSwap(int index0, int index1) {
		write("if (%1$s > %2$s) { tmp = %1$s; %1$s = %2$s; %2$s = tmp; moved |= %3$s; }", // <br/>
				valueVar(index0), // <br/>
				valueVar(index1), // <br/>
				constant((1 << index0) | (1 << index1)));
	}

	private void computeIndices(boolean first) {
		// At least 1 step, but no more than 5 because 'moved' holds only 32 (1 << 5) bits.
		assert 1 <= stepCount && stepCount <= 5;

		final int groupSize = 1 << stepCount;
		String step = "stride";

		if (stepCount != 1) {
			step = "step";
			write("const uint32_t step = stride >> %s;",
					Integer.valueOf(stepCount - 1));
		}

		write("const uint32_t mask = %s - 1;", step);
		write("const uint32_t threadId = globalThreadId();");

		write();
		write("/* Compute indices of input elements. */");
		write("const uint32_t %s = ((threadId & ~mask) * %s) + (threadId & mask);",
				indexVar(0), Integer.valueOf(groupSize));

		if (first) {
			int halfGroupSize = groupSize >> 1;

			for (int i = 0; ++i < halfGroupSize;) {
				write("const uint32_t %s = %s + %s;", // <br/>
						indexVar(i), indexVar(i - 1), step);
			}

			write("const uint32_t %s = %s ^ ((stride << 1) - 1);", // <br/>
					indexVar(halfGroupSize), indexVar(halfGroupSize - 1));
			for (int i = halfGroupSize; ++i < groupSize;) {
				write("const uint32_t %s = %s + %s;", // <br/>
						indexVar(i), indexVar(i - 1), step);
			}
		} else {
			for (int i = 0; ++i < groupSize;) {
				write("const uint32_t %s = %s + %s;", // <br/>
						indexVar(i), indexVar(i - 1), step);
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

		return String.format(format, Integer.valueOf(value));
	}

	private void gatherData(String typeName,  String maxValue) {
		write();
		write("/* Get local copy of relevant data elements. */");

		for (int i = 0, groupSize = 1 << stepCount; i < groupSize; ++i) {
			write("%s %s = (%s < length) ? data[%s] : %s;", // <br/>
					typeName, valueVar(i), indexVar(i), indexVar(i), maxValue);
		}
	}

	private void indent(int delta) {
		indent += delta;
	}

	private String indexVar(int id) {
		return varName("i", id);
	}

	private void scatterData() {
		write();
		write("/* Store data elements that have moved. */");

		for (int i = 0, groupSize = 1 << stepCount; i < groupSize; ++i) {
			write("if (0 != (moved & %s)) { data[%s] = %s; }",  // <br/>
					constant(1 << i), indexVar(i), valueVar(i));
		}
	}

	private void sortLocally(boolean first) {
		final int groupSize = 1 << stepCount;
		final int halfGroupSize = groupSize >> 1;
		int step = 0;

		if (first) {
			writeStepStart(step);

			int groupMirror = groupSize - 1;

			for (int index0 = 0; index0 < halfGroupSize; ++index0) {
				int index1 = index0 ^ groupMirror;

				compareAndSwap(index0, index1);
			}

			step += 1;
		}

		/* write the 'other' steps */
		for (; step < stepCount; ++step) {
			writeStepStart(step);

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

	private String valueVar(int id) {
		return varName("v", id);
	}

	private String varName(String p, int id) {
		String format = stepCount <= 3 ? "%s%d" : "%s%02d";

		return String.format(format, p, Integer.valueOf(id));
	}

	private void write() {
		write(null);
	}

	private void write(String line) {
		if (line == null || line.trim().isEmpty()) {
			out.println();
		} else {
			for (int i = 0; i < indent; ++i) {
				out.print("\t");
			}
			out.println(line);
		}
	}

	private void write(String format, Object... arguments) {
		for (int i = 0; i < indent; ++i) {
			out.print("\t");
		}
		out.printf(format, arguments);
		out.println();
	}

	private void writeAll() {
		writeCopyright();
		writePreamble();
		writeGlobalBlockId(2);
		writeGlobalThreadId(1);

		writeKernelGroup("D", "double", "CUDART_NAN");
		writeKernelGroup("F", "float", "CUDART_NAN_F");
		writeKernelGroup("I", "int32_t", "INT_MAX");
		writeKernelGroup("J", "int64_t", "LLONG_MAX");
	}

	private void writeCopyright() {
		int year = Calendar.getInstance().get(Calendar.YEAR);

		write("/*******************************************************************************");
		write(" * Copyright (c) 2013, %d IBM Corp. and others", Integer.valueOf(year));
		write(" *");
		write(" * This program and the accompanying materials are made available under");
		write(" * the terms of the Eclipse Public License 2.0 which accompanies this");
		write(" * distribution and is available at https://www.eclipse.org/legal/epl-2.0/");
		write(" * or the Apache License, Version 2.0 which accompanies this distribution");
		write(" * and is available at https://www.apache.org/licenses/LICENSE-2.0.");
		write(" *");
		write(" * This Source Code may also be made available under the following");
		write(" * Secondary Licenses when the conditions for such availability set");
		write(" * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU");
		write(" * General Public License, version 2 with the GNU Classpath");
		write(" * Exception [1] and GNU General Public License, version 2 with the");
		write(" * OpenJDK Assembly Exception [2].");
		write(" *");
		write(" * [1] https://www.gnu.org/software/classpath/license.html");
		write(" * [2] http://openjdk.java.net/legal/assembly-exception.html");
		write(" *");
		write(" * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0");
		write(" *******************************************************************************/");
	}

	private void writeFirstPhases(String prefix, String type, String maxValue) {
		final int phaseCount = 9;
		final Integer inputCount = Integer.valueOf(1 << phaseCount);
		final Integer workUnits = Integer.valueOf(1 << (phaseCount - 1));

		write();
		write("extern \"C\" __global__ void");
		write("__launch_bounds__(256)");
		write("%sPhase%d(%s * data, uint32_t length)", // <br/>
				prefix, Integer.valueOf(phaseCount), type);
		write("{");
		indent(+1);

		write("__shared__ %s sharedData[%d];", type, inputCount);
		write("const uint32_t baseIndex = globalBlockId() * %d;", inputCount);

		write();
		write("/* Copy data to shared memory. */");
		write("for (uint32_t localIndex = threadIdx.x; localIndex < %d; localIndex += blockDim.x) {", inputCount);
		indent(+1);
		write("const uint32_t globalIndex = baseIndex + localIndex;");
		write();
		write("sharedData[localIndex] = (globalIndex < length) ? data[globalIndex] : %s;", maxValue);
		indent(-1);
		write("}");

		for (int phase = 0; phase < phaseCount; ++phase) {
			for (int step = 0; step <= phase; ++step) {
				write();
				write("__syncthreads();");

				write();
				write("/* phase %d, step %d */", // <br/>
						Integer.valueOf(phase + 1),
						Integer.valueOf(step + 1));

				write("for (uint32_t workId = threadIdx.x; workId < %d; workId += blockDim.x) {", workUnits);
				indent(+1);

				if (step == phase) {
					write("const uint32_t i0 = workId * 2;");
				} else {
					write("const uint32_t i0 = (workId * 2) - (workId & %s);", constant((1 << (phase - step)) - 1));
				}

				if (step == 0 && step != phase) {
					write("const uint32_t i1 = i0 ^ %s;", constant((2 << phase) - 1));
				} else {
					write("const uint32_t i1 = i0 + %s;", constant(1 << (phase - step)));
				}

				write("const %s v0 = sharedData[i0];", type);
				write("const %s v1 = sharedData[i1];", type);

				write();
				write("if (v0 > v1) {");
				indent(+1);
				write("sharedData[i0] = v1;");
				write("sharedData[i1] = v0;");
				indent(-1);
				write("}");

				indent(-1);
				write("}");
			}
		}

		write();
		write("__syncthreads();");

		write();
		write("/* Copy data back to global memory. */");

		write("for (uint32_t workId = threadIdx.x; workId < %d; workId += blockDim.x) {", inputCount);
		indent(+1);

		write("const uint32_t globalIndex = baseIndex + workId;");
		write();
		write("if (globalIndex < length) {");
		indent(+1);
		write("data[globalIndex] = sharedData[workId];");
		indent(-1);
		write("}");

		indent(-1);
		write("}");

		indent(-1);
		write("}");
	}

	private void writeGlobalBlockId(int numGridDims) {
		write();

		write("static __device__ uint32_t");
		write("globalBlockId()");
		write("{");
		indent(+1);
		write("uint32_t blockId = 0;");

		if (numGridDims >= 3) {
			write();
			write("blockId += blockIdx.z;");
			write("blockId *= gridDim.y;");
		}

		if (numGridDims >= 2) {
			write();
			write("blockId += blockIdx.y;");
			write("blockId *= gridDim.x;");
		}

		write();
		write("blockId += blockIdx.x;");
		write();
		write("return blockId;");
		indent(-1);
		write("}");
	}

	private void writeGlobalThreadId(int numBlockDims) {
		write();

		write("static __device__ uint32_t");
		write("globalThreadId()");
		write("{");
		indent(+1);

		write("uint32_t threadId = globalBlockId();");

		if (numBlockDims >= 3) {
			write();
			write("threadId *= blockDim.z;");
			write("threadId += threadIdx.z;");
		}

		if (numBlockDims >= 2) {
			write();
			write("threadId *= blockDim.y;");
			write("threadId += threadIdx.y;");
		}

		write();
		write("threadId *= blockDim.x;");
		write("threadId += threadIdx.x;");
		write();
		write("return threadId;");

		indent(-1);
		write("}");
	}

	private void writeKernel(String prefix, String type, String maxValue, boolean first) {
		write();
		write("extern \"C\" __global__ void");
		write("__launch_bounds__(256)");
		write("%s%s%d(%s * data, uint32_t length, uint32_t stride)", // <br/>
				prefix, (first ? "First" : "Other"), Integer.valueOf(stepCount), type);
		write("{");
		indent(+1);

		computeIndices(first);

		gatherData(type, maxValue);

		write();
		write("%s tmp = 0;", type);
		write("uint32_t moved = 0;");

		sortLocally(first);

		scatterData();

		indent(-1);
		write("}");
	}

	private void writeKernelGroup(String prefix, String type, String maxValue) {
		final int MaxStepCount = 4;

		writeFirstPhases(prefix, type, maxValue);

		stepCount = MaxStepCount;

		writeKernel(prefix, type, maxValue, true);

		for (; stepCount > 0; --stepCount) {
			writeKernel(prefix, type, maxValue, false);
		}
	}

	private void writePreamble() {
		write();
		write("/* This file was automatically generated: do not edit. */");

		write();
		write("#include <cfloat>");
		write("#include <stdint.h>");
		write("#include <math_constants.h>");

		write();
		write("#if ! defined(__CUDACC__)");
		write("#define __launch_bounds__(maxThreadsPerBlock) /* nothing */");
		write("#endif /* ! defined(__CUDACC__) */");
	}

	private void writeStepStart(int step) {
		write();
		write("/* Local sort step %d. */", Integer.valueOf(step + 1));
	}

}
