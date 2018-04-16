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
package com.ibm.j9.numa;

import java.io.BufferedReader;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.SortedMap;
import java.util.TreeMap;

/**
 * This test checks that the heap is allocated with the correct NUMA memory
 * policy, and that the heap NUMA nodemask is correct.
 * 
 * To get the NUMA memory policy information for all memory regions, parse the
 * numa_maps file /proc/<PID>/numa_maps for the current process (running the
 * jvm).
 * 
 * To get the location of the heap in virtual memory, parse a java dump file.
 * The shell command 'kill -QUIT <PID>' is used cause a java dump of the current
 * process and parse this to find the location of the heap,
 * 
 * Then use the heap address to check the NUMA memory policy of the heap
 * indicated in the numa_maps.
 * 
 * To get information about the default NUMA memory policy for the process, the
 * shell command 'numactl --show' is executed and its output parsed.
 * 
 * The code to execute and parse the output from 'numactl --hardware' is
 * included within; it is no longer used or called but might be useful if this
 * test is expanded in the future.
 * 
 * For an explanation of the command line options and use of this test, refer to
 * the output of printUsage(), accessed with command line option --help.
 */
public class LinuxNumaTest {

	// System.exit( error codes )
	private static final int ERR_BAD_PID_FORMAT = -1;
	private static final int ERR_FAILED_TO_SEND_KILL_SIGNAL = -2;
	private static final int ERR_NUMA_MAPS_NOT_FOUND = -3;
	private static final int ERR_NUMA_MAPS_IO_EXCEPTION = -4;
	private static final int ERR_JAVA_DUMP_NOT_FOUND = -5;
	private static final int ERR_CANT_PARSE_HEAP_ADDRESS = -6;
	private static final int ERR_INTERRUPTED = -7;
	private static final int ERR_JAVA_DUMP_IO_EXCEPTION = -8;
	private static final int ERR_UNKNOWN_HEAP_ADDRESS = -9;
	private static final int ERR_UNKNOWN_HEAP_POLICY = -10;
	private static final int ERR_PARSE_NUMA_REGION = -11;
	private static final int ERR_BAD_COMMAND_LINE = -12;
	private static final int ERR_PARSE_RANGE = -13;
	private static final int ERR_PARSE_NODE_ALLOCATED_PAGES = -14;
	private static final int ERR_CANT_READ_PID = -15;
	private static final int ERR_NUMA_MAPS_EXCEPTION = -16;
	private static final int ERR_PARSE_REGION_START_ADDR = -17;
	private static final int ERR_FAILED_TO_PARSE_SRC_NODE = -18;
	private static final int ERR_BAD_MULTIPLIER = -19;
	private static final int ERR_FAILED_TO_PARSE_DISTANCE = -20;
	public static final int ERR_FAILED_PARSE_POLICY_INFO = -21;
	private static final int ERR_TOO_FEW_NODES = -22;
	
	/**
	 * Name of java dump file, set by command line option
	 * -Xdump:java:events=user,file=javacore.txt
	 */
	private static final String JAVACORE_TXT_FILENAME = "javacore.txt";

	/** Start address of the heap. */
	private Long heapStart;

	/** Size of the heap in bytes. */
	private Long heapSize;

	/** Represents 1 node on a NUMA system, as described by numactl --hardware. */
	public static class NumaNode {

		/** The numeric id of this node. */
		int id;

		/** List of numeric CPU ids. */
		private final List<Integer> cpuIds;

		/** Map of distances from this node to other nodes. */
		private final Map<Integer, Integer> distances;

		/** Total memory on this node, in bytes. */
		private long totalMemory;

		/** Free memory on this node, in bytes. */
		private long freeMemory;

		/** Construct an empty node. */
		public NumaNode() {
			cpuIds = new ArrayList<Integer>();
			distances = new HashMap<Integer, Integer>();
		}

		/**
		 * Get the distance from this node to another node.
		 * 
		 * @param node
		 *            the other node.
		 * @return the distance.
		 */
		public int getDistance(int node) {
			return distances.get(node);
		}

		/**
		 * Get the total memory on this node.
		 * 
		 * @return the total memory on this node.
		 */
		public long getTotalMemory() {
			return totalMemory;
		}

		/**
		 * Get the free memory on this node.
		 * 
		 * @return the free memory on this node.
		 */
		public long getFreeMemory() {
			return freeMemory;
		}

		/** Get the numeric id for this node. */
		public int getId() {
			return id;
		}

		/**
		 * Read the description of a NUMA node from numactl --hardware output
		 * 
		 * @param br
		 *            the input stream.
		 * @throws IOException
		 *             for I/O problems.
		 */
		public void parse(BufferedReader br) throws IOException {

			// 1st line:
			// node 0 cpus: 0 4 8 12 16 20 24 28 32 36 40 44 48 52 (... etc.)
			String[] parts = br.readLine().split(" ");
			id = Integer.parseInt(parts[1]);
			for (int part = 3; part < parts.length; part++) {
				cpuIds.add(Integer.parseInt(parts[part]));
			}

			// 2nd line:
			// node 0 size: 16307 MB
			parts = br.readLine().split(" ");
			totalMemory = Integer.parseInt(parts[3]) * getSuffix(parts[4]);

			// 3rd line:
			// node 0 free: 13794 MB
			parts = br.readLine().split(" ");
			freeMemory = Integer.parseInt(parts[3]) * getSuffix(parts[4]);
		}

		/**
		 * Get the multiplier for suffix.
		 * 
		 * @param suffix
		 *            either "GB", "MB" or "KB".
		 * @return the multiplier for memory.
		 */
		private long getSuffix(String suffix) {

			long multiplier = 0;

			// length can have g (GB), m (MB) or k (KB) suffixes
			if (suffix.equalsIgnoreCase("GB")) {
				multiplier = 1024 * 1024 * 1024;
			} else if (suffix.equalsIgnoreCase("MB")) {
				multiplier = 1024 * 1024;
			} else if (suffix.equalsIgnoreCase("KB")) {
				multiplier = 1024;
			} else {
				System.err.println("Unknown metric multiplier " + suffix);
				System.exit(ERR_BAD_MULTIPLIER);
			}
			return multiplier;
		}

		/**
		 * Parse node distances line
		 * 
		 * @param parts
		 *            the array of node distances as Strings.
		 * @param srcNodeIds
		 *            the list of node IDs corresponding to each distance.
		 */
		public void parseDistances(String[] parts, List<Integer> srcNodeIds) {
			// 0: 10 21 21 21
			int part = 1;
			for (Integer srcNodeId : srcNodeIds) {
				String distStr = parts[part];
				if (!distStr.equals("")) {
					try {
						int distance = Integer.parseInt(distStr);
						distances.put(srcNodeId, distance);
					} catch (NumberFormatException nfe) {
						System.err.println("Failed to parse distance from '"
								+ distStr + "'");
						System.exit(ERR_FAILED_TO_PARSE_DISTANCE);
					}
				}
				part++;
			}
		}
	}

	/**
	 * Represents a NUMA machine with multiple nodes, as described by numactl
	 * --hardware.
	 */
	public static class NumaMachine {

		/** Map of (node id, node) for all nodes on this machine. */
		private final Map<Integer, NumaNode> nodeMap;

		/** Construct an empty NumaMachine. */
		public NumaMachine() {
			nodeMap = new HashMap<Integer, NumaNode>();
		}

		/**
		 * Print some diagnostic info.
		 * 
		 * @param out
		 *            stream where output is dumped.
		 */
		public void dump(PrintWriter out) {
			out.print("nodes: " + nodeMap.size());
		}

		/**
		 * Parse NUMA machine hardware description from output of 'numactl
		 * --hardware'.
		 * 
		 * @param br
		 *            input stream
		 * @throws IOException
		 *             for I/O problem.
		 */
		public void parse(BufferedReader br) throws IOException {

			// Read the file line by line
			String strLine = br.readLine();
			// 1st line:
			// available: 4 nodes (0-3)
			int numNodes = Integer.parseInt(strLine.split(" ")[1]);
			for (int i = 0; i < numNodes; i++) {

				// Parse each node
				NumaNode node = new NumaNode();
				node.parse(br);
				nodeMap.put(node.getId(), node);
			}

			strLine = br.readLine(); // Skip 'node distances:' title line

			// 1st line:
			// node 0 1 2 3
			strLine = br.readLine();
			String[] parts = strLine.split(" ");

			// Read the source node ids
			List<Integer> srcNodeIds = new ArrayList<Integer>();
			for (int part = 1; part < parts.length; part++) {
				// Skip whitespace
				if (!parts[part].equals("")) {
					String idStr = parts[part];
					try {
						int srcNodeId = Integer.parseInt(idStr);
						srcNodeIds.add(srcNodeId);
					} catch (NumberFormatException nfe) {
						System.err
								.println("Couldn't parse source nodeID from: '"
										+ idStr + "'");
						System.err.println("on line: " + strLine);
						System.err.println("part=" + part);
						System.exit(ERR_FAILED_TO_PARSE_SRC_NODE);
					}
				}
			}

			// Additional lines:
			// 0: 10 21 21 21
			while ((strLine = br.readLine()) != null) {
				strLine = strLine.trim();
				parts = strLine.split(":");
				int destNodeId = Integer.parseInt(parts[0]);
				NumaNode numaNode = nodeMap.get(destNodeId);
				numaNode.parseDistances(parts[1].split(" "), srcNodeIds);
			}
		}
	}

	/**
	 * Use the 'numactl --hardware' command to get information about the
	 * hardware this is running on.
	 * 
	 * @return a parsed, populated NumaMachine.
	 */
	private static NumaMachine getHardware() {

		NumaMachine machine = new NumaMachine();
		String cmdLine = "numactl --hardware";
		try {
			System.out.println("Executing another process: " + cmdLine);
			Runtime rt = Runtime.getRuntime();
			Process pr = rt.exec(cmdLine);
			InputStream in = pr.getInputStream();
			InputStreamReader isr = new InputStreamReader(in);
			BufferedReader br = new BufferedReader(isr);
			machine.parse(br);
			in.close();
		} catch (IOException ioe) {
			ioe.printStackTrace();
			System.err.print("Failed to execute" + cmdLine + ", IOException");
			System.exit(ERR_FAILED_TO_SEND_KILL_SIGNAL);
		}
		return machine;
	}

	/**
	 * Represents the NUMA physical memory allocation policy for a process as
	 * described by numactl --show.
	 */
	public static class NumaPolicy {

		/** The system or process policy. */
		private String policy;

		/**
		 * The preferred node for physical memory allocation by the current
		 * process. Note that the preferred node may not exist, e.g. on a 4 node
		 * NUMA machine, "numactl --membind=0-1 numactl --show" has a preferred
		 * node=4.
		 */
		private String preferredNode;

		/** List of CPUs to which the current process is bound. */
		private final Set<Integer> physCpuBind = new HashSet<Integer>();

		private final Set<Integer> cpuBind = new HashSet<Integer>();
		private final Set<Integer> nodeBind = new HashSet<Integer>();
		private final Set<Integer> memBind = new HashSet<Integer>();

		/**
		 * Parse this policy from the output of the numactl --show command.
		 * 
		 * @param br
		 *            the stream written by the numactl process.
		 */
		public void parse(BufferedReader br) {
			try {
				// 1st line:
				// policy: default
				policy = br.readLine().split(":")[1].trim();

				// 2nd line:
				// preferred node: current
				preferredNode = br.readLine().split(":")[1].trim();

				// 3rd line:
				// physcpubind: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18
				// 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38
				// 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58
				// 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78
				// 79
				String[] parts = br.readLine().split(":")[1].trim().split(" ");
				for (String part : parts) {
					physCpuBind.add(Integer.parseInt(part));
				}

				// 4th line:
				// cpubind: 0 1 2 3
				parts = br.readLine().split(":")[1].trim().split(" ");
				for (String part : parts) {
					cpuBind.add(Integer.parseInt(part));
				}

				// 5th line:
				// nodebind: 0 1 2 3
				parts = br.readLine().split(":")[1].trim().split(" ");
				for (String part : parts) {
					nodeBind.add(Integer.parseInt(part));
				}

				// 6th line:
				// membind: 0 1 2 3
				parts = br.readLine().split(":")[1].trim().split(" ");
				for (String part : parts) {
					int node = Integer.parseInt(part);
					System.out.println("mem node: " + node);
					memBind.add(node);
				}
			} catch (IOException ioe) {
				ioe.printStackTrace();
				System.err.print("Failed to parse numa policy information.");
				System.exit(ERR_FAILED_PARSE_POLICY_INFO);
			}
		}

		/**
		 * Get the policy for this policy description.
		 * 
		 * @return the policy description string.
		 */
		public String getPolicy() {
			return policy;
		}

		/**
		 * Get the NUMA node preferred by this policy description.
		 * 
		 * @return preferred node description string, typically either 'current'
		 *         or an integer node number.
		 */
		public String getPreferredNode() {
			return preferredNode;
		}

		/**
		 * Get the set of physical CPU numbers bound in this NUMA policy.
		 * 
		 * @return the set of integer physical CPU numbers.
		 */
		public Set<Integer> getPhysCpuBind() {
			return physCpuBind;
		}

		/**
		 * Get the set of nodes to which execution is limited.
		 * 
		 * @return the set of integer node numbers.
		 */
		public Set<Integer> getCpuBind() {
			return cpuBind;
		}

		/**
		 * I can't find any documentation on this set.
		 * 
		 * @return the set of integer node numbers.
		 */
		public Set<Integer> getNodeBind() {
			return nodeBind;
		}

		/**
		 * Get the set of nodes to which physical memory allocation is limited.
		 * 
		 * @return the set of integer node numbers.
		 */
		public Set<Integer> getMemBind() {
			return memBind;
		}
	}

	/**
	 * Execute the command 'numactl --show' to get the default memory policy for
	 * the current process.
	 * 
	 * @return the numa policy parsed from the command output.
	 */
	private static NumaPolicy getPolicy() {

		NumaPolicy policy = new NumaPolicy();
		String cmdLine = "numactl --show";
		try {
			System.out.println("Executing another process: " + cmdLine);
			Runtime rt = Runtime.getRuntime();
			Process pr = rt.exec(cmdLine);
			InputStream in = pr.getInputStream();
			InputStreamReader isr = new InputStreamReader(in);
			BufferedReader br = new BufferedReader(isr);
			policy.parse(br);
			in.close();
		} catch (IOException ioe) {
			ioe.printStackTrace();
			System.err.print("Failed to execute" + cmdLine + ", IOException");
			System.exit(ERR_FAILED_TO_SEND_KILL_SIGNAL);
		}
		return policy;
	}

	/** Corresponds to one line read and parsed from a numa_maps file. */
	public static class VMRegion {

		/** Start address of the region. */
		private long startAddress;

		/** NUMA policy for the region. */
		private String policy;

		/** Node range for NUMA policy, e.g. '0', or '0-3', or '', etc. */
		private String policyRange;

		/** The line from the numa_maps file that describes this region. */
		private final String line;

		/** A map of name=value parsed from the line. */
		private final Map<String, String> attributes;

		/** A set of flags parsed from the line. */
		private final Set<String> flags;

		/** Length of this region in bytes. */
		private long length;

		/** Set of nodes identified by the numa_maps policy for this region. */
		private final Set<Integer> policyNodeSet;

		/**
		 * Map of (node, pages) allocated by this region according to numa_maps.
		 */
		private final Map<Integer, Integer> allocatedNodeMap;

		/**
		 * Create an instance by parsing a line from a numa_maps file
		 * 
		 * @param line
		 *            a line from a numa_maps file.
		 */
		VMRegion(final String line) {

			policyNodeSet = new HashSet<Integer>();
			allocatedNodeMap = new HashMap<Integer, Integer>();

			this.line = line;

			// Sample line:
			// 7f4a8a950000 default anon=1764 dirty=1764 active=1761 N0=39 N1=62
			// N3=1663
			//
			// A different sample line (with numactl --membind=0,2):
			// 0db40000 interleave:0,2 anon=1196 dirty=1196 active=1194 N0=598
			// N2=598
			//
			// A sample line with a discontinuous range for a node mask:
			// 0e440000 interleave:0,2-3 anon=1196 dirty=1196 active=1192
			// N0=1081 N2=59 N3=56
			//
			// 1st column (7f4a8a950000) is the region start address in hex.
			// 2nd column is the region numa policy, and node mask if any.
			// Nn=xxx (e.g. N0=39) are the number of pages of physical memory
			// allocated from the nodes.
			// name=value pairs are called attributes in this code.
			// name alone is called a flag in this code.

			// Split the line into tokens
			String[] tokens = line.split(" ");

			try {
				// 1st token is the start address of the virtual memory region
				startAddress = Long.parseLong(tokens[0].trim(), 16);
			} catch (NumberFormatException nfe) {
				System.err.println("Can't parse vm region start address: "
						+ tokens[0].trim());
				System.exit(ERR_PARSE_REGION_START_ADDR);
			}

			// 2nd token is the NUMA policy for the region
			// e.g. 'default', 'bind:0' or 'interleave:0-3', etc.
			parsePolicyAndNodeMask(tokens[1].trim());

			// Now parse the rest of the tokens for attributes and flags
			attributes = new HashMap<String, String>();
			flags = new HashSet<String>();
			for (int i = 2; i < tokens.length; i++) {

				// Split the token at '='
				String[] pieces = tokens[i].trim().split("=");
				if (pieces.length == 1) {
					addFlag(pieces[0]);
				} else if (pieces.length == 2) {
					addAttribute(pieces[0], pieces[1]);
				} else {
					System.err
							.println("Error while parsing numa_maps, don't understand token:"
									+ tokens[i]);
					System.exit(ERR_PARSE_NUMA_REGION);
				}
			}
		}

		/** Parse the policy and node mask from numa_maps line. */
		private void parsePolicyAndNodeMask(String token) {
			// On some machines, the policy and nodemask are separated by
			// = not :, e.g.
			// 1e820000 interleave=0 anon=27 dirty=27 N0=27
			String[] policyTokens;
			if (token.contains("=")) {
				policyTokens = token.split("=");
			} else {
				policyTokens = token.split(":");
			}
			policy = policyTokens[0].trim();
			if (policyTokens.length < 2) {
				policyRange = "";
			} else {
				policyRange = policyTokens[1].trim();
				policyNodeSet.addAll(parseRange(policyRange));
			}
		}

		/**
		 * Add an attribute (name=value) to this VMRegion.
		 * 
		 * @param name
		 *            name of the attribute
		 * @param value
		 *            value of the attribute
		 */
		private void addAttribute(String name, String value) {

			// Is this of the form 'Nn=m', e.g. N2=534
			if (('N' == name.charAt(0)) && Character.isDigit(name.charAt(1))) {
				try {
					int node = Integer.parseInt(name.substring(1), 10);
					int allocatedPages = Integer.parseInt(value, 10);
					addAllocatedNode(node, allocatedPages);
				} catch (NumberFormatException nfe) {
					System.err.println("Can't parse node range: " + name + "="
							+ value);
					System.exit(ERR_PARSE_NODE_ALLOCATED_PAGES);
				}
			}
			// Attribute in the form 'name=value'
			attributes.put(name, value);
		}

		/**
		 * Add pages allocated to a node to the allocatedNodeMap.
		 * 
		 * @param node
		 *            the node where pages are allocated.
		 * @param allocatedPages
		 *            the number of pages allocated to node.
		 */
		private void addAllocatedNode(int node, int allocatedPages) {
			allocatedNodeMap.put(node, allocatedPages);
		}

		/**
		 * Check if a node is in the specified policy range,
		 * 
		 * @param node
		 *            the node
		 * @return true if a node is in the specified policy range.
		 */
		private boolean isNodeInPolicyRange(int node) {
			if (policyNodeSet.isEmpty()) {
				// Empty policy refers to all nodes
				return true;
			}
			return policyNodeSet.contains(node);
		}

		/**
		 * Check that only nodes specified by the policy range are allocated
		 * pages.
		 * 
		 * @return true if only nodes specified by the policy range are
		 *         allocated pages.
		 */
		public boolean isPolicyRespected() {

			if (policyNodeSet.isEmpty()) {
				// Empty policy refers to all nodes
				return true;
			}

			// Check all nodes against policy
			for (int node : allocatedNodeMap.keySet()) {
				if (!isNodeInPolicyRange(node)) {
					return false;
				}
			}

			return true;
		}

		/**
		 * Add a flag to this VMRegion, e.g. 'heap'.
		 * 
		 * @param flag
		 *            name of the flag
		 */
		private void addFlag(String flag) {
			// Flag in the form 'flag'
			flags.add(flag);
		}

		/** Convenience for printing, debugging this test. */
		@Override
		public String toString() {
			return getLine();
		}

		/**
		 * Return true if this is an anonymous region.
		 * 
		 * @return true if this is an anonymous region.
		 */
		public boolean isAnon() {
			String value = attributes.get("anon");
			return value != null;
		}

		/**
		 * Return true if this is region is mapped to a file.
		 * 
		 * @return true if this is region is mapped to a file.
		 */
		public boolean isFile() {
			String value = attributes.get("file");
			return value != null;
		}

		/**
		 * Get the start address of this region
		 * 
		 * @return the start address of this region.
		 */
		public long getStartAddress() {
			return startAddress;
		}

		/**
		 * Get the line that initialized this region
		 * 
		 * @return the line from numa_maps this region was parsed from.
		 */
		public String getLine() {
			return line;
		}

		/**
		 * Get the length of this region in bytes.
		 * 
		 * @return the length of this region in bytes.
		 */
		public long getLength() {
			return length;
		}

		/**
		 * Set the length of this region in bytes.
		 * 
		 * @param length
		 *            the length of this region in bytes.
		 */
		public void setLength(long length) {
			this.length = length;
		}

		/**
		 * Get the policy description string for this region.
		 * 
		 * @return the policy description string for this region.
		 */
		public String getPolicy() {
			return policy;
		}

		/**
		 * Get the policy range description string for this region.
		 * 
		 * @return the policy range description string for this region.
		 */
		public String getPolicyRange() {
			return policyRange;
		}

		public Set<Integer> getPolicyNodeSet() {
			return policyNodeSet;
		}

		/**
		 * Get the map of (name, value) attributes for this region.
		 * 
		 * @return the map of (name, value) attributes for this region.
		 */
		public Map<String, String> getAttributes() {
			return attributes;
		}

		/**
		 * Get the set of flags for this region.
		 * 
		 * @return the set of flags for this region.
		 */
		public Set<String> getFlags() {
			return flags;
		}

		/**
		 * Get the map of (node, allocated pages) for this region.
		 * 
		 * @return the map of (node, allocated pages) for this region.
		 */
		public Map<Integer, Integer> getAllocatedNodeMap() {
			return allocatedNodeMap;
		}
	}

	/** Corresponds to a parsed numa_maps file. */
	public static class NumaMap {

		/** Map of (Starting address, VMRegion) */
		private final SortedMap<Long, VMRegion> regionMap = new TreeMap<Long, VMRegion>();

		/** The last VMRegion added. */
		private VMRegion last = null;

		/** Construct and empty map. */
		public NumaMap() {
			clearAndResetMap();
		}

		/** Remove all regions. */
		public void clearAndResetMap() {
			last = null;
			regionMap.clear();
		}

		/**
		 * Parse a numa maps file and add all regions to this map.
		 * 
		 * @param numa_maps_file
		 *            the file to parse
		 * @throws Exception
		 *             if unable to parse any line
		 */
		public void readNumaMapsFile(File numa_maps_file) throws Exception {

			clearAndResetMap();

			FileInputStream fstream_numaMaps = new FileInputStream(
					numa_maps_file);
			DataInputStream in_numaMaps = new DataInputStream(fstream_numaMaps);
			BufferedReader br_numaMaps = new BufferedReader(
					new InputStreamReader(in_numaMaps));

			// Read numa_maps file Line By Line
			String strLine;
			while ((strLine = br_numaMaps.readLine()) != null) {
				try {
					// Create a VMRegion for each line and add it
					VMRegion vmr = new VMRegion(strLine);
					addRegion(vmr);
					if (!vmr.isPolicyRespected()) {
						// Some regions don't respect their policies
						// Print a warning, but this isn't a problem
						// Some policies (e.g. MPOL_INTERLEAVE) allow
						// allocation on other nodes if none is available
						// in the region.
						System.out.println("WARNING: policy not respected - "
								+ vmr.getLine());
					}
				} catch (Exception e) {
					System.err
							.println("Couldn't parse this line in numa_maps:");
					System.err.println(strLine);
					throw e;
				}
			}
			in_numaMaps.close();
		}

		/**
		 * Assumes they're added in order of startAddress
		 * 
		 * @param region
		 *            the new VMRegion to add to the NumaMap
		 */
		public void addRegion(VMRegion region) {
			regionMap.put(region.getStartAddress(), region);
			if (last != null) {
				// Set the length of the last region
				long length = region.getStartAddress() - last.getStartAddress();
				last.setLength(length);
			}
			last = region;
		}

		/**
		 * Get the collection of regions.
		 * 
		 * @return the collection of all regions in this map.
		 */
		public Collection<VMRegion> getRegions() {
			return regionMap.values();
		}

		/**
		 * Look up a region given its starting address
		 * 
		 * @param startAddr
		 *            the starting address of the desired VMRegion
		 * @return the corresponding VMRegion, or null if not found.
		 * 
		 * @throws IllegalArgumentException
		 *             if no region found at startAddr
		 */
		public VMRegion getRegionByStartAddress(long startAddr) {
			VMRegion region = regionMap.get(startAddr);
			if (region == null) {
				throw new IllegalArgumentException(
						"Can't find region starting at 0x"
								+ Long.toHexString(startAddr));
			}
			return region;
		}

	}

	/**
	 * Run this test from the command line.
	 * 
	 * @param args
	 *            the command line arguments.
	 */
	public static void main(String[] args) {

		// For incorrect arguments, print usage.
		if (args.length == 0) {
			System.out.println("for usage, use --help");
			System.out.println("Using default arguments.");
		}

		if ((args.length == 1) && (args[0].equalsIgnoreCase("--help"))) {
			printUsage();
			System.exit(0);
		}

		System.out.println("Begining heap memory policy test...");

		// Run as an instance so we have access to an object monitor so we can
		// wait() for other processes to complete.
		String expectedHeapPolicy = null;
		String expectedPolicyNodeRange = null;
		if (args.length >= 1) {
			expectedHeapPolicy = args[0];
		}
		if (args.length >= 2) {
			expectedPolicyNodeRange = args[1];
		}
		new LinuxNumaTest().run(expectedHeapPolicy, expectedPolicyNodeRange);
	}

	/**
	 * Print the command line instructions for using this test.
	 */
	private static void printUsage() {
		System.out.println("Use:");
		System.out
				.println("    java -Xdump:java:events=user,file=javacore.txt LinuxNumaTest [expectedHeapPolicy] [expectedPolicyNodeRange]");
		System.out.println("            or");
		System.out
				.println("    java LinuxNumaTest --help           (displays this message)");
		System.out.println();
		System.out.println("    [expectedHeapPolicy]");
		System.out
				.println("        Optional. The expected NUMA policy for the heap as");
		System.out.println("        found by parsing numa_maps.");
		System.out
				.println("        If not specified, default policy from numactl --show is used.");
		System.out.println("        Possible values:");
		System.out
				.println("            default    - use existing process or system policy.");
		System.out
				.println("            preferred  - 1st from preferred node, others if necessary.");
		System.out
				.println("            bind       - restrict allocation to available nodes.");
		System.out
				.println("            interleave - round robin allocation from available nodes.");
		System.out.println();
		System.out.println("    [expectedPolicyNodeRange]");
		System.out
				.println("        Optional. The expected range of nodes for the heap policy, e.g '0'");
		System.out
				.println("        for'interleave:0', or '0-3' for 'interleave:0-3'. Use 'NONE' for");
		System.out
				.println("        no range, e.g. 'default' Use commas to separate range bounds,");
		System.out.println("        e.g. '0,2-3'");
		System.out
				.println("        If not specified, default node range from numactl --show is used.");
		System.out.println();
	}

	/**
	 * The body of the test.
	 * 
	 * @param expectedHeapPolicy
	 *            a String containing the expected NUMA virtual memory policy
	 *            for the heap.
	 * @param expectedPolicyNodeRange
	 *            a String containing the expected NUMA node range(s) for the
	 *            heap.
	 */
	private synchronized void run(String expectedHeapPolicy,
			String expectedPolicyNodeRange) {

		// Remove any old javacore.txt
		System.out.println("Checking for old java dump: "
				+ JAVACORE_TXT_FILENAME);
		File oldJavaDumpFile = new File(JAVACORE_TXT_FILENAME);
		if (oldJavaDumpFile != null) {
			System.out.println("deleting old java dump: "
					+ JAVACORE_TXT_FILENAME);
			oldJavaDumpFile.delete();
		}

		// Get our pid
		int pid = getPidOfCurrentProc();

		// Parse the numa_maps for the current process
		NumaMap numa_map = parseNumaMapsFile(pid);

		// Send the signal to generate a java dump
		System.out.println("Sending kill signal to generate java dump");
		sendKillSignal(pid);

		// Get information about the current policy.
		NumaPolicy policy = getPolicy();
		if (policy.getNodeBind().size() < 2) {
			System.err.println("Machine CAPABILITY error, this test only runs on NUMA machines with 2 or more nodes.");
			System.err.println("Confirm number of nodes with numactl --show or numactl --hardware");
			System.err.println("or remove capability 'numa' from this machine");
			System.exit(ERR_TOO_FEW_NODES);
		}
		if (expectedHeapPolicy == null) {
			// If no expectedHeapPolicy, expect the process default policy.
			expectedHeapPolicy = policy.getPolicy();
		}

		// Wait for the dump to occur.
		// (Simpler to just wait than to work out some IPC mechanism.)
		System.out
				.println("Sleeping to allow exec'ed process to send signal and generate java dump");
		sleep(5 * 1000);

		// Now read the java dump file to get the heap address
		parseJavaDump(JAVACORE_TXT_FILENAME);

		// If we don't know the heap address, there's no point in continuing
		if (heapStart == null) {
			System.err.println("heap address unknown, aborting");
			System.exit(ERR_UNKNOWN_HEAP_ADDRESS);
		}

		System.out.println("----------------------------------------------");
		System.out.println("preferred node: " + policy.getPreferredNode());
		System.out.println("policy: " + policy.getPolicy());
		for (Integer memnode : policy.getMemBind()) {
			System.out.println("MemNode " + memnode);
		}
		System.out.println("----------------------------------------------");
		System.out.println("Comparing expected vs. actual results");

		// Get the memory policy for the heap from the numa_map
		VMRegion heapRegion = numa_map.getRegionByStartAddress(heapStart);
		if (heapRegion == null) {
			System.err.println("heap not found in numa_maps, aborting");
			System.exit(ERR_UNKNOWN_HEAP_POLICY);
		}
		System.out.println("numa_maps line for heap:");
		System.out.println(heapRegion.getLine());

		System.out.println("Heap memory    size, from java dump: 0x"
				+ Long.toHexString(heapSize));
		System.out.println("Heap VM region size, from numa_maps: 0x"
				+ Long.toHexString(heapRegion.getLength()));

		// Test expected policy
		String heapPolicy = heapRegion.getPolicy();
		System.out.println("Actual   policy: " + heapPolicy);
		System.out.println("Expected policy: " + expectedHeapPolicy);
		boolean policyTestPassed = comparePolicies(expectedHeapPolicy,
				heapPolicy);

		// Test expected range
		String heapPolicyRange = heapRegion.getPolicyRange();
		System.out.println("Actual   range: " + heapPolicyRange);

		// Determine the expected node mask
		Set<Integer> expectedPolicyNodeSet;
		if (expectedPolicyNodeRange == null) {
			// if not specified, use process default nodemask
			expectedPolicyNodeSet = policy.getMemBind();
		} else {
			// if specified, parse and use the nodemask from the cmdline
			expectedPolicyNodeSet = parseRange(expectedPolicyNodeRange);
		}

		// Check the node range
		boolean nodemaskTestPassed = false;
		Set<Integer> policyNodeSet = heapRegion.getPolicyNodeSet();
		if (expectedHeapPolicy.equalsIgnoreCase("bind")
				|| expectedHeapPolicy.equalsIgnoreCase("interleave")) {
			System.out.println("Expected range: " + expectedPolicyNodeRange);

			System.out.println("heap     policy nodes = "+ policyNodeSet);
			System.out.println("expected policy nodes = "+ expectedPolicyNodeSet);
			if (expectedPolicyNodeSet.equals(policyNodeSet)) {
				nodemaskTestPassed = true;
			} else {
				System.out.println("POLICY NODEMASK MISMATCH");
			}
		} else if (expectedHeapPolicy.equalsIgnoreCase("default")){
			System.out.println("heap     policy nodes = "+ policyNodeSet);
			if ((null == policyNodeSet) || (policyNodeSet.size() == 0)) {
				nodemaskTestPassed = true;
			}
		}

		// Repeat warning for heap, if any, to make it stand out
		if (!heapRegion.isPolicyRespected()) {
			System.out.println("WARNING: heap policy not respected - "
					+ heapRegion.getLine());
		}

		// Display final test verdict
		if (policyTestPassed && nodemaskTestPassed) {
			System.out.println("TEST PASSED");
		} else {
			System.out.println("TEST FAILED");
		}
	}

	/**
	 * Compare actual and expected policies. The heap policies need not be
	 * identical, for example, 'bind' and 'interleave' are compatible if the
	 * node masks are identical.
	 * 
	 * @param expectedHeapPolicy
	 *            what we think it should be.
	 * @param actualHeapPolicy
	 *            what it actually is.
	 * @return true if the actual heap policy is acceptable given the expected
	 *         heap policy.
	 */
	private boolean comparePolicies(String expectedHeapPolicy,
			String actualHeapPolicy) {

		// If they're the same, no problem
		if (actualHeapPolicy.equals(expectedHeapPolicy)) {
			return true;
		}

		// If we expect bind and actually have interleave, there's no conflict
		// as long as the node masks match which is tested later.
		if (expectedHeapPolicy.equalsIgnoreCase("bind")
				&& actualHeapPolicy.equalsIgnoreCase("interleave")) {
			return true;
		}

		System.out.println("POLICY MISMATCH");
		return false;
	}

	/**
	 * Open, read and parse the Java dump file in JAVACORE_TXT_FILENAME. Read
	 * the heapStart and heapSize into those fields.
	 */
	private void parseJavaDump(String javacore_txt_filename) {
		String strLine = "<initialized>";
		try {
			System.out.println("Parsing java dump: " + javacore_txt_filename);
			File javacore = new File(javacore_txt_filename);
			FileInputStream fstream_javacore = new FileInputStream(javacore);
			DataInputStream in_javacore = new DataInputStream(fstream_javacore);
			BufferedReader br_javacore = new BufferedReader(
					new InputStreamReader(in_javacore));

			// Read File Line By Line
			while ((strLine = br_javacore.readLine()) != null) {

				// Split line to find 1st column
				String[] tokens = strLine.split(" ");

				// Looking for the 1st line that describes the heap
				if (tokens[0].equals("1STHEAPREGION")) {
					System.out.println(javacore_txt_filename
							+ " line describing heap:");
					System.out.println(strLine);

					if (heapStart == null) {
						heapStart = Long.parseLong(tokens[3].substring(2), 16);
						System.out.println("heap start="
								+ Long.toHexString(heapStart));
					}

					// Found what we're looking for, bail out
					continue;

				} else if (tokens[0].equals("1STHEAPTOTAL")) {

					System.out.println(javacore_txt_filename
							+ " line describing heap size:");
					System.out.println(strLine);

					if (heapSize == null) {
						String heapSizeStr = strLine.substring(30).trim()
								.split(" ")[0];
						heapSize = Long.parseLong(heapSizeStr);
						System.out.println("heap size="
								+ Long.toHexString(heapStart));
					}

					// Found what we're looking for, bail out
					break;
				}
			}
			in_javacore.close();
			System.out.println("Completed parsing java dump");
		} catch (FileNotFoundException fnfe) {
			fnfe.printStackTrace();
			System.err.println("Can't find file " + javacore_txt_filename);
			System.exit(ERR_JAVA_DUMP_NOT_FOUND);
		} catch (NumberFormatException nfe) {
			nfe.printStackTrace();
			System.err.println("Can't parse heap address");
			System.err.println(strLine);
			System.exit(ERR_CANT_PARSE_HEAP_ADDRESS);
		} catch (IOException ioe) {
			ioe.printStackTrace();
			System.err.println("Can't read file " + javacore_txt_filename);
			System.exit(ERR_JAVA_DUMP_IO_EXCEPTION);
		}
	}

	/**
	 * Parse the numa_maps file for the given PID.
	 * 
	 * @param pid
	 *            the PID of the process to parse.
	 * @return the parsed NumaMap.
	 */
	private NumaMap parseNumaMapsFile(int pid) {
		NumaMap numa_map = new NumaMap();
		try {
			String numa_maps_path = "/proc/" + pid + "/numa_maps";
			File numa_maps_file = new File(numa_maps_path);
			numa_map.readNumaMapsFile(numa_maps_file);
			System.out.println("Completed parsing " + numa_maps_path);
		} catch (FileNotFoundException fnfe) {
			fnfe.printStackTrace();
			System.err.println("Can't find numa_maps file");
			System.exit(ERR_NUMA_MAPS_NOT_FOUND);
		} catch (IOException ioe) {
			ioe.printStackTrace();
			System.err.println("Can't read numa_maps file");
			System.exit(ERR_NUMA_MAPS_IO_EXCEPTION);
		} catch (Exception e) {
			e.printStackTrace();
			System.err.println("Can't parse numa_maps file");
			System.exit(ERR_NUMA_MAPS_EXCEPTION);
		}
		return numa_map;
	}

	/**
	 * Use the linux filesystem to get the PID of the current process.
	 * 
	 * @return pid of the current process.
	 */
	private int getPidOfCurrentProc() {
		int pid = 0;
		try {
			System.out.println("Getting PID of current process");
			pid = Integer.parseInt(new File("/proc/self").getCanonicalFile()
					.getName());
			System.out.println("JVM process pid=" + pid);
			return pid;
		} catch (IOException ioe) {
			ioe.printStackTrace();
			System.err
					.println("Can't read PID from /proc/self canonical file name");
			System.exit(ERR_CANT_READ_PID);
		} catch (NumberFormatException nfe) {
			nfe.printStackTrace();
			System.err
					.println("Can't parse PID from /proc/self canonical file name");
			System.exit(ERR_BAD_PID_FORMAT);
		}
		return pid;
	}

	/**
	 * Sleep, to allow other processes to work
	 * 
	 * @param delayInMillis
	 *            milliseconds to sleep.
	 */
	private void sleep(int delayInMillis) {
		try {
			System.out.println("Sleeping for " + delayInMillis
					+ " milliseconds");
			wait(delayInMillis);
		} catch (InterruptedException e) {
			e.printStackTrace();
			System.err
					.println("Interrupted while waiting for kill process signal to be sent");
			System.exit(ERR_INTERRUPTED);
		}
	}

	/**
	 * Send a KILL -QUIT signal to a process. Sending this signal to a JVM
	 * process results in a java dump.
	 * 
	 * @param pid
	 *            the PID of the process to signal.
	 * @return the filename of the java dump.
	 * @throws IOException
	 *             for I/O problems communicating with the new process (the new
	 *             process which sends the signal).
	 */
	private void sendKillSignal(int pid) {

		try {
			// Send user QUIT signal to generate javacore.txt
			// (javacore filename set on command line)
			String cmdLine = "kill -QUIT " + pid;
			System.out.println("Executing another process: " + cmdLine);
			Runtime rt = Runtime.getRuntime();
			@SuppressWarnings("unused")
			Process pr = rt.exec(cmdLine);
		} catch (IOException ioe) {
			ioe.printStackTrace();
			System.err.print("Failed to send kill signal, IOException");
			System.exit(ERR_FAILED_TO_SEND_KILL_SIGNAL);
		}
	}

	/**
	 * Parse a range of nodes from a numa policy description, e.g. '0', '0-1',
	 * '0,2-3', etc.
	 * 
	 * @param rangeStr
	 *            the input string
	 * @return the set of node numbers.
	 */
	private static Set<Integer> parseRange(String rangeStr) {

		Set<Integer> result = new HashSet<Integer>();

		// Split the whole range into discontinuous parts
		String[] tokens = rangeStr.trim().split(",");

		// Parse each part
		for (String token : tokens) {

			// Split into range bounds, if any
			String[] rangeBounds = token.split("-");
			try {
				switch (rangeBounds.length) {
				case 1: // single number, e.g. 0
					result.add(Integer.parseInt(rangeBounds[0], 10));
					break;
				case 2: // two numbers, e.g. 2-3
					int startNode = Integer.parseInt(rangeBounds[0], 10);
					int endNode = Integer.parseInt(rangeBounds[1], 10);
					for (int node = startNode; node <= endNode; node++) {
						result.add(node);
					}
					break;
				default: // Unknown case, error
					throw new NumberFormatException("Can't parse range: "
							+ token + " from " + rangeStr);
				}
			} catch (NumberFormatException nfe) {
				nfe.printStackTrace();
				System.err.println("Can't parse range: " + token + " from "
						+ rangeStr);
				System.exit(ERR_PARSE_RANGE);
			}
		}
		return result;
	}
}
