/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package org.openj9.envInfo;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.nio.file.*;
import java.lang.management.ManagementFactory;
public class MachineInfo {
	private static final int _1 = 1;
	public static String UNAME_CMD = "uname -a";
	public static String SYS_ARCH_CMD = "uname -m";
	public static String PROC_ARCH_CMD = "uname -p";
	public static final String ULIMIT_CMD = "ulimit -a";

	public static String INSTALLED_MEM_CMD = "grep MemTotal /proc/meminfo | awk '{print $2}";
	public static String FREE_MEM_CMD = "grep MemFree /proc/meminfo | awk '{print $2}";
	public static String CPU_CORES_CMD = "cat /proc/cpuinfo | grep processor | wc -l";

	public static String NUMA_CMD = "numactl --show | grep 'No NUMA support available on this system";
	public static String SYS_VIRT_CMD = "";

	// Software
	public static String SYS_OS_CMD = "uname -s";
	public static String KERNEL_VERSION_CMD = "uname -r";
	public static String GCC_VERSION_CMD = "gcc -dumpversion";

	public static final String XLC_VERSION_CMD = "xlC -qversion | grep 'Version' ";
	public static final String GDB_VERSION_CMD = "gdb --version | head -1"; // debugger on Linux
	public static String LLDB_VERSION_CMD = "lldb --version"; // debugger on Darwin/Mac
	public static final String GCLIBC_VERSION_CMD = "ldd --version | head -1";

	// Console
	public static final String NUM_JAVA_PROCESSES_CMD = "ps -ef | grep -i [j]ava | wc -l";
	public static final String ACTIVE_JAVA_PROCESSES_CMD = "ps -ef | grep -i [j]ava";

	String uname = "";
	String ulimit = "";
	String installedMem = "";
	String freeMem = "";
	String cpuCores = "";
	String cpuModel = "";
	String cpuSpeed = "";
	String sysArch = "";
	String sysOS = "";
	String procArch = "";
	String numa = "Unknown";
	String sysVirt = "Unknown";
	long totalMemory = -1;
	long freeMemory = -1;
	
	String vmVendor = "";
	String vmVersion = "";
	String specVendor = "";
	String specVersion = "";
	String javaVersion = "";

	public String toString() {
		return "\nuname: " + uname
		+ "\ncpuCores: " + cpuCores 
		+ "\nsysArch: " + sysArch 
		+ "\nprocArch: " + procArch 
		+ "\nsysOS: " + sysOS
		+ "\nulimit: " + ulimit 
		+ "\n"
		+ "\nvmVendor: " + vmVendor 
		+ "\nvmVersion: " + vmVersion 
		+ "\nspecVendor: " + specVendor 
		+ "\nspecVersion: " + specVersion 
		+ "\njavaVersion: " + javaVersion 
		+ "\n"
		+"\nTotal memory (bytes): " + totalMemory
		+"\nFree memory (bytes): " + freeMemory
		+ "\n";
	}

	public long getFreeMemory() {
		return freeMemory;
	}

	public void setFreeMemory(long freeMemory) {
		this.freeMemory = freeMemory;
	}

	public long getTotalMemory() {
		return totalMemory;
	}

	public void setTotalMemory(long totalMemory) {
		this.totalMemory = totalMemory;
	}

	public String getUname() {
		return uname;
	}
	
	public String getUlimit() {
		return ulimit;
	}

	public void setUname(String uname) {
		this.uname = uname;
	}

	public void setUlimit(String ulimit) {
		this.ulimit = ulimit;
	}
	
	public String getVmVendor() {
		return vmVendor;
	}

	public void setVmVendor(String vmVendor) {
		this.vmVendor = vmVendor;
	}

	public String getVmVersion() {
		return vmVersion;
	}

	public void setVmVersion(String vmVersion) {
		this.vmVersion = vmVersion;
	}

	public String getSpecVendor() {
		return specVendor;
	}

	public void setSpecVendor(String specVendor) {
		this.specVendor = specVendor;
	}

	public String getSpecVersion() {
		return specVersion;
	}

	public void setSpecVersion(String specVersion) {
		this.specVersion = specVersion;
	}

	public String getJavaVersion() {
		return javaVersion;
	}

	public void setJavaVersion(String javaVersion) {
		this.javaVersion = javaVersion;
	}

	public String getInstalledMem() {
		return installedMem;
	}

	public void setInstalledMem(String installedMem) {
		this.installedMem = installedMem;
	}

	public String getFreeMem() {
		return freeMem;
	}

	public void setFreeMem(String freeMem) {
		this.freeMem = freeMem;
	}

	public String getCpuCores() {
		return cpuCores;
	}

	public void setCpuCores(String cpuCores) {
		this.cpuCores = cpuCores;
	}

	public String getCpuModel() {
		return cpuModel;
	}

	public void setCpuModel(String cpuModel) {
		this.cpuModel = cpuModel;
	}

	public String getCpuSpeed() {
		return cpuSpeed;
	}

	public void setCpuSpeed(String cpuSpeed) {
		this.cpuSpeed = cpuSpeed;
	}

	public String getSysArch() {
		return sysArch;
	}

	public void setSysArch(String sysArch) {
		this.sysArch = sysArch;
	}

	public String getSysOS() {
		return sysOS;
	}

	public void setSysOS(String sysOS) {
		this.sysOS = sysOS;
	}

	public String getProcArch() {
		return procArch;
	}

	public void setProcArch(String procArch) {
		this.procArch = procArch;
	}

	public String getNuma() {
		return numa;
	}

	public void setNuma(String numa) {
		this.numa = numa;
	}

	public String getSysVirt() {
		return sysVirt;
	}

	public void setSysVirt(String sysVirt) {
		this.sysVirt = sysVirt;
	}

	public void getRuntimeInfo() {
		setVmVendor(ManagementFactory.getRuntimeMXBean().getVmVendor());
		setVmVersion(ManagementFactory.getRuntimeMXBean().getVmVersion());
		setSpecVendor(ManagementFactory.getRuntimeMXBean().getSpecVendor());
		setSpecVersion(ManagementFactory.getRuntimeMXBean().getSpecVersion());
		setJavaVersion(System.getProperty("java.version"));

		setFreeMemory(Runtime.getRuntime().freeMemory());
		setTotalMemory(Runtime.getRuntime().totalMemory());
	}

	public void getMachineInfo(String command) {
		try { 
			Process proc = null;
			// ulimit needs to be invoked via shell with -c option for it to work on all platforms
			if (command.equals(MachineInfo.ULIMIT_CMD)) { 
				proc = Runtime.getRuntime().exec(new String[] { "bash", "-c", MachineInfo.ULIMIT_CMD });
			} else {
				proc = Runtime.getRuntime().exec(command);
			}
			
			BufferedReader sout = new BufferedReader(new InputStreamReader(proc.getInputStream()));
			String line = sout.readLine();
	
			if (command.equals(MachineInfo.UNAME_CMD)) {
				setUname(line);
			} else if (command.equals(MachineInfo.ULIMIT_CMD)) {
				String rest = "";
				while (true) {
					rest = sout.readLine();
					if (rest == null) {
						break;
					}
					line = line + "\n" + rest; 
				}
				setUlimit(line);
			} else if (command.equals(MachineInfo.SYS_ARCH_CMD)) {
				setSysArch(line);
			} else if (command.equals(MachineInfo.PROC_ARCH_CMD)) {
				setProcArch(line);
			} else if (command.equals(MachineInfo.SYS_OS_CMD)) {
				setSysOS(line);
			} else if (command.equals(MachineInfo.CPU_CORES_CMD)) {
				setCpuCores(line);
				if (getCpuCores() == null) {
					setCpuCores("" + Runtime.getRuntime().availableProcessors());
				}
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	public void getSpaceInfo(String path) {
		Path userPath = Paths.get(path);
		File file = userPath.toAbsolutePath().toFile();

		System.out.println("File path: " + file.getAbsolutePath());
		System.out.println("Total space (bytes): " + file.getTotalSpace());
		System.out.println("Free space (bytes): " + file.getFreeSpace());
		System.out.println("Usable space (bytes): " + file.getUsableSpace());
	}
}
