<!-- 
	Copyright (c) 2013, 2017 IBM Corp. and others
	
	This program and the accompanying materials are made available under
	the terms of the Eclipse Public License 2.0 which accompanies this
	distribution and is available at https://www.eclipse.org/legal/epl-2.0/
	or the Apache License, Version 2.0 which accompanies this distribution and
	is available at https://www.apache.org/licenses/LICENSE-2.0.
	
	This Source Code may also be made available under the following
	Secondary Licenses when the conditions for such availability set
	forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
	General Public License, version 2 with the GNU Classpath
	Exception [1] and GNU General Public License, version 2 with the
	OpenJDK Assembly Exception [2].
	
	[1] https://www.gnu.org/software/classpath/license.html
	[2] http://openjdk.java.net/legal/assembly-exception.html

	SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
-->
<project name="jvm" default="all" basedir=".">
	<description>
		Ant XML file to compose a jvm-bin build into a shippable SDK structure
	</description>

	<!-- Add optional tasks -->
	<taskdef resource="net/sf/antcontrib/antlib.xml" />

	<!-- set global properties for this build -->
	<property name="vm.buildId" value="${uma.buildinfo.build_date}_${uma.buildinfo.buildid}" />
	<property name="vm.stream" value="${uma.buildinfo.version.major}${uma.buildinfo.version.minor}" />
	<if>
		<equals arg1="${r"${product}"}" arg2="java8" />
		<then>
			<property name="java9build" value="n/a" />
		</then>
		<else>
			<if>
				<matches string="${r"${product}"}" pattern="^java9b\d+$$" />
				<then>
					<property name="java9build" value="${r"${product}"}" />
				</then>
				<else>
					<property name="java9build" value="${r"${product}b148"}" />
				</else>
			</if>
			<if>
				<equals arg1="${r"${java9build}"}" arg2="java9b136" />
				<then>
					<property name="jpp.config" value="SIDECAR19-SE-B136" />
				</then>
				<elseif>
					<matches string="${r"${java9build}"}" pattern="^(java9b148|java9b150|java9b156)$$" />
					<then>
						<property name="jpp.config" value="SIDECAR19-SE-B148" />
					</then>
				</elseif>
				<elseif>
					<matches string="${r"${java9build}"}" pattern="^(java9b165)$$" />
					<then>
						<property name="jpp.config" value="SIDECAR19-SE-B165" />
					</then>
				</elseif>
				<else>
					<property name="jpp.config" value="SIDECAR19-SE-B169" />
				</else>
			</if>	
		</else>
	</if>
	<echo message="Composing for product=${r"${product}; java9build=${java9build}"}." />

<#if uma.spec.id?starts_with("aix")>
	<property name="aix" value="true" />
</#if>
<#if uma.spec.id?starts_with("win")>
	<property name="windows" value="true" />
	<property name="lib.prefix" value="" />
	<property name="lib.suffix" value=".dll" />
	<property name="exe.suffix" value=".exe" />
<#else>
	<property name="lib.prefix" value="lib" />
	<property name="lib.suffix" value=".so" />
	<property name="exe.suffix" value="" />
</#if>
<#if uma.spec.id?starts_with("zos")>
	<property name="zos" value="true" />
</#if>
<#if uma.spec.id?starts_with("linux")>
	<property name="linux" value="true" />
</#if>
	<property name="platform.arch" value="${uma.spec.properties.platform_arch.value}" />
	<property name="platform.code" value="${uma.spec.properties.main_shortname.value}" />
	<property name="default.module.version" value="9-internal" />

	<if>
		<matches string="${r"${java9build}"}" pattern="^(java9b136|java9b148|java9b150|java9b156)$$" />
		<then>
			<property name="java.base.extracted" value="java.base_extracted" />
			<condition property="opt.invoke.strconcat" value="-J-Djava.lang.invoke.stringConcat=BC_SB" else="">
				<equals arg1="${r"${java9build}"}" arg2="java9b136" />
			</condition>
			<if>
				<equals arg1="${r"${java9build}"}" arg2="java9b136" />
				<then>
					<property name="opt.list.modules" value="-listmods:" />
				</then>
				<elseif>
					<matches string="${r"${java9build}"}" pattern="^(java9b148|java9b150|java9b156)$$" />
					<then>
						<property name="opt.list.modules" value="--list-modules" />
					</then>
				</elseif>
				<else>
					<property name="opt.list.modules" value="--describe-module" />
				</else>
			</if>	
			<condition property="use.public.keyword" value="-DusePublicKeyword=true" else="-DusePublicKeyword=false">
				<equals arg1="${r"${java9build}"}" arg2="java9b136" />
			</condition>
			<echo message="Platform code is ${r"${platform.code}"}" />
			<!-- Set os-name option value for jmod according to platforms -->
			<condition property="jmod.os.name.value" value="Linux">
				<matches string="${r"${platform.code}"}" pattern="^(xa64|xi32|xl64|xr32|xs32|xz31|xz64)$$" />
			</condition>
			<condition property="jmod.os.name.value" value="AIX">
				<equals arg1="${r"${platform.code}"}" arg2="ap64"/>
			</condition>
			<condition property="jmod.os.name.value" value="Windows">
				<equals arg1="${r"${platform.code}"}" arg2="wa64"/>
			</condition>
			<condition property="jmod.os.name.value" value="ZOS">
				<matches string="${r"${platform.code}"}" pattern="^(mz31|mz64)$$" />
			</condition>
			<echo message="OS name is ${r"${jmod.os.name.value}"}" />
			<!-- Set os-arch option value for jmod according to platforms -->
			<condition property="jmod.os.arch.value" value="i586">
				<equals arg1="${r"${platform.code}"}" arg2="xi32"/>
			</condition>
			<condition property="jmod.os.arch.value" value="arm">
				<equals arg1="${r"${platform.code}"}" arg2="xr32"/>
			</condition>
			<condition property="jmod.os.arch.value" value="arm-sf">
				<equals arg1="${r"${platform.code}"}" arg2="xs32"/>
			</condition>
			<condition property="jmod.os.arch.value" value="amd64">
				<matches string="${r"${platform.code}"}" pattern="^(wa64|xa64)$$" />
			</condition>
			<condition property="jmod.os.arch.value" value="ppc64le">
				<equals arg1="${r"${platform.code}"}" arg2="xl64"/>
			</condition>
			<condition property="jmod.os.arch.value" value="s390">
				<matches string="${r"${platform.code}"}" pattern="^(mz31|xz31)$$" />
			</condition>
			<condition property="jmod.os.arch.value" value="s390x">
				<matches string="${r"${platform.code}"}" pattern="^(mz64|xz64)$$" />
			</condition>
			<condition property="jmod.os.arch.value" value="ppc64">
				<equals arg1="${r"${platform.code}"}" arg2="ap64"/>
			</condition>
			<echo message="OS arch is ${r"${jmod.os.arch.value}"}" />
			<!-- Set os-version option value for jmod according to platforms -->
			<condition property="jmod.os.version.value" value="2.6">
				<matches string="${r"${platform.code}"}" pattern="^(xa64|xi32|xl64|xr32|xs32|xz31|xz64)$$" />
			</condition>
			<condition property="jmod.os.version.value" value="7.1">
				<equals arg1="${r"${platform.code}"}" arg2="ap64"/>
			</condition>
			<condition property="jmod.os.version.value" value="5.2">
				<equals arg1="${r"${platform.code}"}" arg2="wa64"/>
			</condition>
			<condition property="jmod.os.version.value" value="V2R1">
				<matches string="${r"${platform.code}"}" pattern="^(mz31|mz64)$$" />
			</condition>
			<echo message="OS version is ${r"${jmod.os.version.value}"}" />
		</then>
	</if>

	<condition property="jdk.bin" value="${r"${base.jdk}/bin"}" else="${r"${compose.dir}/bin"}">
		<isset property="base.jdk" />
	</condition>

	<condition property="mini.jdk.bin" value="${r"${jcl.bin.dir}"}/mini-jdk/bin" else="${r"${jdk.bin}"}">
		<!-- this is a hack to allow JCL bootstrap b136 build, should be removed later -->
		<available file="${r"${jcl.bin.dir}"}/mini-jdk" type="dir" />
	</condition>

	<target name="init">
		<!-- Ensure the output directory exists -->
		<mkdir dir="${r"${output}"}" />

		<!-- Jazz 21607 -->
		<if>
			<isset property="windows" />
			<then>
				<property name="interim.jvm.target.dir" value="${r"${output}/${jre.dir}/${bin.dir}"}" />
			</then>
			<elseif>
				<or>
					<equals arg1="${r"${product}"}" arg2="java8" />
					<matches string="${r"${java9build}"}" pattern="^(java9b136|java9b148)$$" />
				</or>
				<then>
					<property name="interim.jvm.target.dir" value="${r"${output}/${jre.dir}/${lib.dir}/${platform.arch}"}" />
				</then>
			</elseif>
			<else>
				<!-- 'platform.arch' directory is not present beginning with java9b148 -->
				<property name="interim.jvm.target.dir" value="${r"${output}/${jre.dir}/${lib.dir}"}" />
			</else>
		</if>

		<condition property="jvm.target.dir" value="${r"${interim.jvm.target.dir}/${sidecar.name}"}" else="${r"${interim.jvm.target.dir}"}">
			<isset property="sidecar.name" />
		</condition>

		<condition property="isJava8">
			<equals arg1="${r"${product}"}" arg2="java8" />
		</condition>

		<if>
			<equals arg1="${r"${clean.jvm.target.dir}"}" arg2="true" />
			<then>
				<echo>Cleaning DIR : ${r"${jvm.target.dir}"}</echo>
				<delete includeemptydirs="true" verbose="true" failonerror="false">
					<fileset dir="${r"${jvm.target.dir}"}" includes="**/*" />
				</delete>
				<if>
					<isset property="windows" />
					<then>
						<delete includeemptydirs="true" verbose="true" failonerror="false">
							<fileset dir="${r"${output}/${lib.dir}/${sidecar.name}"}" includes="**/*" />
						</delete>
					</then>
				</if>
			</then>
		</if>
	</target>

	<target name="copy_j9_jvm" depends="init" description="copy all files except for known directories that we don't want">
		<property name="classic.target.dir" value="${r"${interim.jvm.target.dir}/classic"}" />
		<property name="jre.bin.classic.target.dir" value="${r"${output}/${jre.dir}/${bin.dir}/classic"}" />
		<property name="jre.bin.j9vm.target.dir" value="${r"${output}/${jre.dir}/${bin.dir}/j9vm"}" />
		<property name="j9vm.target.dir" value="${r"${interim.jvm.target.dir}/j9vm"}" />
		<condition property="j9vm.source.dir" value="redirector" else="j9vm">
			<isset property="sidecar.name" />
		</condition>

		<!-- For jvmbuild, copy the entire source tree into target destination before doing anything else.
			This is critical since the following composition steps will copy the right files into right place.
			Doing entire source copy upfront prevents composition logic from being overwritten -->
		<if>
			<and>
				<equals arg1="${r"${dest.java.base}"}" arg2="false" />
				<istrue value="${r"${jvmbuild}"}" />
			</and>
			<then>
				<echo>Copy entire source tree from ${r"${basedir}"} into ${r"${jvm.target.dir}"}</echo>
				<copy verbose="false" todir="${r"${jvm.target.dir}"}/">
					<fileset dir="${r"${basedir}"}">
						<include name="**/*" />
						<exclude name="**/java.properties" />
						<exclude name="**/java_*.properties" />
						<exclude name="options.default" />
						<exclude name="j9ddr.dat" />
						<!-- Shared libraries are added as appropriate below. -->
						<exclude name="**/${r"${lib.prefix}*${lib.suffix}"}" />
					</fileset>
				</copy>
			</then>
		</if>

		<echo>Copy shared libraries to vm dir : ${r"${jvm.target.dir}"}</echo>
		<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
			<fileset dir=".">
				<include name="${r"${lib.prefix}"}j9dbg${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9dmp${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9gc${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9gcchk${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9hookable${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9jextract${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9jit${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9jitd${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9jvmti${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9prt${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9rdbi${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9shr${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9thr${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9trc${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9vm${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9vrb${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9zlib${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9jnichk${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}j9vmchk${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}jsig${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}omrsig${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}jvm${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}management${lib.suffix}"}" unless="isJava8" />
				<include name="${r"${lib.prefix}management_ext${lib.suffix}"}" unless="isJava8" />
<#if uma.spec.flags.opt_harmony.enabled>
				<include name="${r"${lib.prefix}"}vmi${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}hyprtshim${r"${vm.stream}"}${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}hythr${r"${lib.suffix}"}" />
</#if>
<#if uma.spec.flags.opt_useFfi.enabled>
				<include name="${r"${lib.prefix}"}ffi${r"${vm.stream}"}${r"${lib.suffix}"}" />
</#if>
			</fileset>
		</copy>

		<switch value="${r"${java9build}"}">
			<case value="java9b148">
				<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
					<fileset dir="j9vm">
						<include name="${r"${lib.prefix}jvm${lib.suffix}"}" />
					</fileset>
				</copy>
			</case>
			<case value="java9b150">
				<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
					<fileset dir="j9vm_b150">
						<include name="${r"${lib.prefix}jvm${lib.suffix}"}" />
					</fileset>
				</copy>
			</case>
			<case value="java9b156">
				<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
					<fileset dir="j9vm_b156">
						<include name="${r"${lib.prefix}jvm${lib.suffix}"}" />
					</fileset>
				</copy>
			</case>
			<default>
			</default>
		</switch>

		<if>
			<equals arg1="${r"${product}"}" arg2="java8" />
			<then>
				<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
					<fileset dir=".">
						<include name="${r"${lib.prefix}"}jclse7_${r"${vm.stream}"}${r"${lib.suffix}"}" />
						<include name="${r"${lib.prefix}"}jclse7b_${r"${vm.stream}"}${r"${lib.suffix}"}" />
					</fileset>
				</copy>
			</then>
			<elseif>
				<matches string="${r"${java9build}"}" pattern="^(java9b136|java9b148|java9b150|java9b156)$$" />
				<then>
					<switch value="${r"${java9build}"}">
						<case value="java9b136">
							<property name="vm.shape.value" value="b136" />
						</case>
						<case value="java9b148">
							<property name="vm.shape.value" value="b148" />
						</case>
						<case value="java9b150">
							<property name="vm.shape.value" value="b150" />
						</case>
						<case value="java9b156">
							<property name="vm.shape.value" value="b156" />
						</case>
						<default>
							<!-- latest CCM supported level -->
							<property name="vm.shape.value" value="b162" />
						</default>
					</switch>
					<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
						<fileset dir=".">
							<include name="${r"${lib.prefix}"}jclse9_${r"${vm.stream}"}${r"${lib.suffix}"}" />
						</fileset>
					</copy>
<#if uma.spec.id?starts_with("zos")>
					<delete file="${r"${output}/${jre.dir}/${lib.dir}"}/classlib.properties" quiet="true" />
					<native2ascii encoding="ibm-1047" src="." dest="${r"${output}/${jre.dir}/${lib.dir}"}"
						includes="classlib.properties" />
<#else>
					<copy file="classlib.properties" todir="${r"${output}/${jre.dir}/${lib.dir}"}" verbose="true" overwrite="yes" />
</#if>
					<replace file="${r"${output}/${jre.dir}/${lib.dir}"}/classlib.properties" token="vm.shape" value="${r"${vm.shape.value}"}" />
<#if uma.spec.id?starts_with("zos")>
					<native2ascii encoding="ibm-1047" src="${r"${output}/${jre.dir}/${lib.dir}"}" dest="${r"${output}/${jre.dir}/${lib.dir}"}" reverse="true"
						includes="classlib.properties" ext=".ebcdic" />
					<move file="${r"${output}/${jre.dir}/${lib.dir}"}/classlib.ebcdic" tofile="${r"${output}/${jre.dir}/${lib.dir}"}/classlib.properties" />
</#if>
				</then>
			</elseif>
			<else>
				<fail message="Unsupported product version: ${r"${product}"}" />
			</else>
		</if>

		<!-- copy include files to include -->
		<if>
			<equals arg1="${r"${dest.java.base}"}" arg2="false" />
			<then>
				<copy verbose="true" todir="${r"${output}"}/include/" overwrite="true">
					<fileset dir="include/">
						<include name="ibmjvmti.h" />
						<include name="jni.h" />
						<include name="jniport.h" />
						<include name="jvmri.h" />
						<include name="jvmti.h" />
					</fileset>
				</copy>
			</then>
		</if>

		<!-- copy J9TraceFormat.dat to jre/lib -->
		<copy verbose="true" todir="${r"${output}"}/${r"${jre.dir}"}/${r"${lib.dir}"}/" overwrite="true">
			<fileset dir=".">
				<include name="J9TraceFormat.dat" />
				<include name="OMRTraceFormat.dat" />
			</fileset>
		</copy>

		<!-- Jazz 31127: copy j9ddr.dat -->
		<if>
			<and>
				<not>
					<equals arg1="${r"${product}"}" arg2="java8" />
				</not>
				<isset property="windows" />
			</and>
			<then>
				<!-- For post-Java8 windows build copy j9ddr.dat to [jre]/[lib]/<sidecar> -->
				<copy verbose="true" todir="${r"${output}/${jre.dir}/${lib.dir}/${sidecar.name}"}" overwrite="true">
					<fileset dir="${r"${basedir}"}">
						<include name="j9ddr.dat" />
					</fileset>
				</copy>
			</then>
			<else>
				<copy verbose="true" todir="${r"${jvm.target.dir}/"}" overwrite="true">
					<fileset dir="${r"${basedir}"}">
						<include name="j9ddr.dat" />
					</fileset>
				</copy>
			</else>
		</if>

		<if>
			<equals arg1="${r"${product}"}" arg2="java8" />
			<then>
				<!-- Jazz 39274: copy schema.xsd to ${r"${jvm.target.dir}"}/ -->
				<copy verbose="true" todir="${r"${jvm.target.dir}/"}" overwrite="true">
					<fileset dir="omr/gc/verbose">
						<include name="*.xsd" />
					</fileset>
				</copy>
			</then>
			<else>
				<!-- Jazz 39274: copy schema.xsd to lib dir -->
				<copy verbose="true" todir="${r"${output}/${jre.dir}/${lib.dir}"}/" overwrite="true">
					<fileset dir="omr/gc/verbose">
						<include name="*.xsd" />
					</fileset>
				</copy>
			</else>
		</if>

		<if>
			<equals arg1="${r"${product}"}" arg2="java8" />
			<then>
				<!-- copy properties files to vm dir -->
				<copy verbose="true" todir="${r"${jvm.target.dir}"}" overwrite="true">
					<fileset dir=".">
						<include name="**/java.properties" />
						<include name="**/java_*.properties" />
					</fileset>
				</copy>
			</then>
			<else>
				<!-- copy properties files to lib dir -->
				<copy verbose="true" todir="${r"${output}/${jre.dir}/${lib.dir}"}/" overwrite="true">
					<fileset dir=".">
						<include name="**/java.properties" />
						<include name="**/java_*.properties" />
					</fileset>
				</copy>
			</else>
		</if>

		<if>
			<and>
				<equals arg1="${r"${dest.java.base}"}" arg2="false" />
				<isset property="linux" />
			</and>
			<then>
				<if>
					<matches string="${r"${platform.arch}"}" pattern="^(s390|s390x)$$" />
					<then>
						<!-- copy machine speeds file to vm dir -->
						<copy verbose="true" todir="${r"${jvm.target.dir}"}" overwrite="true">
							<fileset dir=".">
								<include name="zlinux-machine-speeds.txt" />
							</fileset>
						</copy>
					</then>
				</if>
			</then>
		</if>

		<if>
			<equals arg1="${r"${product}"}" arg2="java8" />
			<then>
				<!-- copy default options file to vm dir -->
				<copy verbose="true" todir="${r"${jvm.target.dir}"}" overwrite="true">
					<fileset dir=".">
						<include name="options.default" />
					</fileset>
				</copy>
			</then>
			<else>
				<!-- copy default options file to lib dir -->
				<copy verbose="true" todir="${r"${output}/${jre.dir}/${lib.dir}"}/" overwrite="true">
					<fileset dir=".">
						<include name="options.default" />
					</fileset>
				</copy>
			</else>
		</if>

		<!-- standard IBM Java -->
		<property name="target.jvm.name" value="${r"${lib.prefix}jvm${lib.suffix}"}" />
		<switch value="${r"${java9build}"}">
			<case value="java9b148">
				<property name="redirector.jvm.name" value="${r"${lib.prefix}jvm${lib.suffix}"}" />
			</case>
			<case value="java9b150">
				<property name="redirector.jvm.name" value="${r"${lib.prefix}jvm_b150${lib.suffix}"}" />
			</case>
			<case value="java9b156">
				<property name="redirector.jvm.name" value="${r"${lib.prefix}jvm_b156${lib.suffix}"}" />
			</case>
			<default>
				<property name="redirector.jvm.name" value="${r"${target.jvm.name}"}" />
			</default>
		</switch>
		<!-- copy jvm shared lib to classic -->
		<copy verbose="true" tofile="${r"${classic.target.dir}/${target.jvm.name}"}" overwrite="true">
			<fileset dir="redirector">
				<include name="${r"${redirector.jvm.name}"}" />
			</fileset>
		</copy>
		<!-- copy jvm shared lib to jre/bin/classic -->
		<copy verbose="true" tofile="${r"${jre.bin.classic.target.dir}/${target.jvm.name}"}" overwrite="true">
			<fileset dir="redirector">
				<include name="${r"${redirector.jvm.name}"}" />
			</fileset>
		</copy>
		<!-- copy jvm shared lib to jre/bin/j9vm -->
		<copy verbose="true" tofile="${r"${jre.bin.j9vm.target.dir}/${target.jvm.name}"}" overwrite="true">
			<fileset dir="redirector">
				<include name="${r"${redirector.jvm.name}"}" />
			</fileset>
		</copy>
		<!-- copy jvm shared lib to j9vm, do this after jre/bin/j9vm to ensure we get the proper jvm shared lib if jre/bin/j9vm == j9vm.target.dir -->
		<copy verbose="true" tofile="${r"${j9vm.target.dir}/${target.jvm.name}"}" overwrite="true">
			<fileset dir="${r"${j9vm.source.dir}"}/">
				<include name="${r"${redirector.jvm.name}"}" />
			</fileset>
		</copy>

		<!-- CMVC 168020 : making sure jsig and omrsig libraries are found under jre/bin. -->
		<copy verbose="true" todir="${r"${interim.jvm.target.dir}"}/" overwrite="true">
			<fileset dir=".">
				<include name="${r"${lib.prefix}"}jsig${r"${lib.suffix}"}" />
				<include name="${r"${lib.prefix}"}omrsig${r"${lib.suffix}"}" />
			</fileset>
		</copy>

<#if uma.spec.flags.opt_cuda.enabled>
		<!-- Copy CUDA4J library to jre/bin or jre/lib/<arch>. -->
		<copy verbose="true" todir="${r"${interim.jvm.target.dir}"}/" overwrite="true">
			<fileset dir=".">
				<include name="${r"${lib.prefix}"}cuda4j${r"${vm.stream}"}${r"${lib.suffix}"}" />
			</fileset>
		</copy>
</#if>

<#if uma.spec.flags.opt_fips.enabled>
		<copy verbose="true" todir="${r"${interim.jvm.target.dir}"}" overwrite="yes">
			<fileset dir="./fips">
				<include name="**/${r"${lib.prefix}"}j9fips.*" />
			</fileset>
		</copy>
</#if>

		<if>
			<isset property="windows" />
			<then>
				<!-- Do windows specific packaging -->
				<!-- copy dbghelp.dll -->
				<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
					<#if !uma.spec.flags.env_data64.enabled>
						<fileset dir="win">
							<include name="dbghelp.dll" />
						</fileset>
					</#if>
					<#if uma.spec.flags.env_data64.enabled>
						<fileset dir="win_64">
							<include name="dbghelp.dll" />
						</fileset>
					</#if>
				</copy>

				<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
					<#if !uma.spec.flags.env_data64.enabled>
						<fileset dir="win">
							<include name="msvcr100.dll" />
							<include name="msvcp100.dll" />
						</fileset>
					</#if>
					<#if uma.spec.flags.env_data64.enabled>
						<fileset dir="win_64">
							<include name="msvcr100.dll" />
							<include name="msvcp100.dll" />
						</fileset>
					</#if>
				</copy>
				<copy verbose="true" todir="${r"${interim.jvm.target.dir}"}/" overwrite="true">
					<#if !uma.spec.flags.env_data64.enabled>
						<fileset dir="win">
							<include name="msvcr100.dll" />
							<include name="msvcp100.dll" />
						</fileset>
					</#if>
					<#if uma.spec.flags.env_data64.enabled>
						<fileset dir="win_64">
							<include name="msvcr100.dll" />
							<include name="msvcp100.dll" />
						</fileset>
					</#if>
				</copy>
				<!-- make sure msvcr100.dll and msvcp100.dll is found in bin -->
				<if>
					<equals arg1="${r"${dest.java.base}"}" arg2="false" />
					<then>
						<copy verbose="true" todir="${r"${interim.jvm.target.dir}"}/../../bin" overwrite="true">
							<#if !uma.spec.flags.env_data64.enabled>
								<fileset dir="win">
									<include name="msvcr100.dll" />
									<include name="msvcp100.dll" />
								</fileset>
							</#if>
							<#if uma.spec.flags.env_data64.enabled>
								<fileset dir="win_64">
									<include name="msvcr100.dll" />
									<include name="msvcp100.dll" />
								</fileset>
							</#if>
						</copy>
					</then>
				</if>

				<!-- copy jvm.pdb and msvcr100.dll to j9vm-->
				<copy verbose="true" todir="${r"${j9vm.target.dir}"}/" overwrite="true">
					<#if !uma.spec.flags.env_data64.enabled>
						<fileset dir="win">
							<include name="msvcr100.dll" />
						</fileset>
					</#if>
					<#if uma.spec.flags.env_data64.enabled>
						<fileset dir="win_64">
							<include name="msvcr100.dll" />
						</fileset>
					</#if>
					<fileset dir="${r"${j9vm.source.dir}"}/">
						<include name="jvm.pdb" />
					</fileset>
				</copy>

				<!--
				Copy jvm.pdb and msvcr100.dll to classic.
				NOTE: No need to copy to ${r"${jre.bin.classic.target.dir}"} as well since
				on Windows that is the same directory as ${r"${classic.target.dir}"}.
				-->
				<copy verbose="true" todir="${r"${classic.target.dir}"}/" overwrite="true">
					<#if !uma.spec.flags.env_data64.enabled>
						<fileset dir="win">
							<include name="msvcr100.dll" />
						</fileset>
					</#if>
					<#if uma.spec.flags.env_data64.enabled>
						<fileset dir="win_64">
							<include name="msvcr100.dll" />
						</fileset>
					</#if>
					<fileset dir="redirector/">
						<include name="jvm.pdb" />
					</fileset>
				</copy>

				<!-- copy jvm.lib to lib-->
				<copy verbose="true" todir="${r"${output}"}/${r"${lib.dir}"}" overwrite="true">
					<fileset dir="redirector/">
						<include name="redirector_jvm.lib" />
					</fileset>
					<globmapper from="redirector_jvm.lib" to="jvm.lib" />
				</copy>

				<!-- copy jsig.lib and omrsig.lib to lib-->
				<copy verbose="true" todir="${r"${output}"}/${r"${lib.dir}"}" overwrite="true">
					<fileset dir="lib">
						<include name="jsig.lib" />
						<include name="omrsig.lib" />
					</fileset>
				</copy>

				<!-- CMVC 174297 : jsig.pdb and omrsig.pdb should appear in jre/bin -->
				<copy verbose="true" todir="${r"${interim.jvm.target.dir}"}/" overwrite="true">
					<fileset dir=".">
						<include name="jsig.pdb" />
						<include name="omrsig.pdb" />
					</fileset>
				</copy>

<#if uma.spec.flags.opt_cuda.enabled>
				<!-- cuda4j.pdb should be in the same directory as cuda4j.dll -->
				<copy verbose="true" todir="${r"${interim.jvm.target.dir}"}/" overwrite="true">
					<fileset dir=".">
						<include name="cuda4j${r"${vm.stream}"}.pdb" />
					</fileset>
				</copy>
</#if>

				<!-- copy pdbs to vm dir-->
				<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
					<fileset dir=".">
						<include name="j9bcv${r"${vm.stream}"}.pdb" />
						<include name="j9dbg${r"${vm.stream}"}.pdb" />
						<include name="j9dmp${r"${vm.stream}"}.pdb" />
						<include name="j9dyn${r"${vm.stream}"}.pdb" />
						<include name="j9gc${r"${vm.stream}"}.pdb" />
						<include name="j9gcchk${r"${vm.stream}"}.pdb" />
						<include name="j9hookable${r"${vm.stream}"}.pdb" />
						<include name="j9jextract.pdb" />
						<include name="j9jit${r"${vm.stream}"}.pdb" />
						<include name="j9jitd${r"${vm.stream}"}.pdb" />
						<include name="j9jvmti${r"${vm.stream}"}.pdb" />
						<include name="j9prt${r"${vm.stream}"}.pdb" />
						<include name="j9shr${r"${vm.stream}"}.pdb" />
						<include name="j9thr${r"${vm.stream}"}.pdb" />
						<include name="j9trc${r"${vm.stream}"}.pdb" />
						<include name="j9vm${r"${vm.stream}"}.pdb" />
						<include name="j9vrb${r"${vm.stream}"}.pdb" />
						<include name="j9zlib${r"${vm.stream}"}.pdb" />
						<include name="j9jnichk${r"${vm.stream}"}.pdb" />
						<include name="j9vmchk${r"${vm.stream}"}.pdb" />
						<include name="j9rcm${r"${vm.stream}"}.pdb" />
						<include name="jsig.pdb" />
						<include name="omrsig.pdb" />
						<include name="jvm.pdb" />
						<include name="management.pdb" unless="isJava8" />
						<include name="management_ext.pdb" unless="isJava8" />
<#if uma.spec.flags.opt_harmony.enabled>
						<include name="vmi.pdb" />
						<include name="hyprtshim${r"${vm.stream}"}.pdb" />
						<include name="hythr.pdb" />
</#if>
					</fileset>
				</copy>
				<if>
					<equals arg1="${r"${product}"}" arg2="java8" />
					<then>
						<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
							<fileset dir=".">
								<include name="jclse7b_${r"${vm.stream}"}.pdb" />
							</fileset>
						</copy>
					</then>
					<elseif>
						<matches string="${r"${java9build}"}" pattern="^(java9b136|java9b148|java9b150|java9b156)$$" />
						<then>
							<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
								<fileset dir=".">
									<include name="jclse9_${r"${vm.stream}"}.pdb" />
								</fileset>
							</copy>
						</then>
					</elseif>
					<else>
						<fail message="Unsupported product version: ${r"${product}"}" />
					</else>
				</if>
			</then>
		</if>
		<if>
			<isset property="zos" />
			<then>
				<!-- Do zos specific packaging -->
				<!-- copy headers to include -->
				<if>
					<equals arg1="${r"${dest.java.base}"}" arg2="false" />
					<then>
						<copy verbose="true" todir="${r"${output}"}/include/" overwrite="true">
							<fileset dir="include/">
								<include name="jni_convert.h" />
							</fileset>
						</copy>
					</then>
				</if>
				<!-- copy libjsig.x and libomrsig.x to bin -->
				<copy verbose="true" todir="${r"${output}"}/${r"${bin.dir}"}/" overwrite="true">
					<fileset dir="lib/">
						<include name="libjsig.x" />
						<include name="libomrsig.x" />
					</fileset>
				</copy>
				<!-- copy libjsig.x and libomrsig.x to lib/[arch] -->
				<copy verbose="true" todir="${r"${interim.jvm.target.dir}"}/" overwrite="true">
					<fileset dir="lib/">
						<include name="libjsig.x" />
						<include name="libomrsig.x" />
					</fileset>
				</copy>
				<if>
					<equals arg1="${r"${dest.java.base}"}" arg2="false" />
					<then>
						<!-- copy libjvm.x, libj9a2e*.so to classic -->
						<copy verbose="true" todir="${r"${classic.target.dir}"}/" overwrite="true">
							<fileset dir="./lib/">
								<include name="libjvm.x" />
							</fileset>
							<fileset dir=".">
								<include name="${r"${lib.prefix}"}j9a2e${r"${lib.suffix}"}" />
							</fileset>
						</copy>
						<!-- copy libjvm.x, libj9a2e*.so to jre/bin/classic -->
						<copy verbose="true" todir="${r"${jre.bin.classic.target.dir}"}/" overwrite="true">
							<fileset dir="./lib/">
								<include name="libjvm.x" />
							</fileset>
							<fileset dir=".">
								<include name="${r"${lib.prefix}"}j9a2e${r"${lib.suffix}"}" />
							</fileset>
						</copy>
					</then>
				</if>
				<!-- copy libjvm.x, libj9a2e*.so to jre/bin/j9vm -->
				<copy verbose="true" todir="${r"${jre.bin.j9vm.target.dir}"}/" overwrite="true">
					<fileset dir="${r"${basedir}"}/lib/">
						<include name="libjvm.x" />
					</fileset>
					<fileset dir="${r"${basedir}"}">
						<include name="${r"${lib.prefix}"}j9a2e${r"${lib.suffix}"}" />
					</fileset>
				</copy>
				<!-- copy libjvm.x, libj9a2e*.so to j9vm -->
				<copy verbose="true" todir="${r"${j9vm.target.dir}"}/" overwrite="true">
					<fileset dir="./lib/">
						<include name="libjvm.x" />
					</fileset>
					<fileset dir=".">
						<include name="${r"${lib.prefix}"}j9a2e${r"${lib.suffix}"}" />
					</fileset>
				</copy>
				<!-- copy libjvm.x up to vm dir if required -->
				<if>
					<isset property="sidecar.name" />
					<then>
						<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
							<fileset dir="./lib/">
								<include name="libjvm.x" />
							</fileset>
						</copy>
					</then>
				</if>
				<!-- copy zos only shared libs to vm dir -->
				<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
					<fileset dir=".">
						<include name="${r"${lib.prefix}"}dbx_j9${r"${lib.suffix}"}" />
						<include name="${r"${lib.prefix}"}j9a2e${r"${lib.suffix}"}" />
						<include name="${r"${lib.prefix}"}j9ifa${r"${vm.stream}"}${r"${lib.suffix}"}" />
					</fileset>
				</copy>
			</then>
		</if>
		<if>
			<isset property="aix" />
			<then>
				<!-- Do aix specific packaging -->
				<!-- copy aix only shared libs to vm dir -->
				<copy verbose="true" todir="${r"${jvm.target.dir}"}/" overwrite="true">
					<fileset dir=".">
						<include name="${r"${lib.prefix}"}dbx_j9${r"${lib.suffix}"}" />
						<include name="dbx_plug64*" /><!-- this is the 64 bit J9 dbx lib extension for dbx used by the dbx_j9 lib on that platform -->
					</fileset>
				</copy>
			</then>
		</if>

		<if>
			<and>
				<equals arg1="${r"${dest.java.base}"}" arg2="false" />
				<isset property="testbuild" />
			</and>
			<then>
				<echo>Copying J9 test material</echo>
				<copy verbose="true" todir="${r"${jvm.target.dir}"}/">
					<fileset dir="${r"${basedir}"}">
						<!-- include all shared libs and jars -->
						<include name="**/*${r"${lib.suffix}"}/" />
						<include name="**/*.jar" />
						<exclude name="**/java9_*.jar" />
						<include name="**/jars/*.fil" />

						<!-- Exclude shared libraries that are handled explicitly above. -->
<#if uma.spec.flags.opt_cuda.enabled>
						<exclude name="${r"${lib.prefix}cuda4j${vm.stream}${lib.suffix}"}" />
</#if>
						<exclude name="**/${r"${lib.prefix}"}j9fips.*" />

						<exclude name="${r"${lib.prefix}management${lib.suffix}"}" />
						<exclude name="${r"${lib.prefix}management_ext${lib.suffix}"}" />

						<!-- Offload-specific files -->
						<include name="**/localJava*.properties" />
						<include name="**/mappedDirectories.properties" />
						<include name="**/isolation.properties" />

						<!-- grab additional jvmti test material that goes hand in hand with a particular version of sdk -->
						<include name="**/tests/jvmti/cmdlinetester/*" />

						<!-- executables, find using: find . -name module.xml | xargs grep executable -->
						<include name="algotest${r"${exe.suffix}"}/" />
						<include name="dyntest${r"${exe.suffix}"}/" />
						<include name="bcvunit${r"${exe.suffix}"}/" />
						<include name="ctest${r"${exe.suffix}"}/" />
						<include name="cfdump${r"${exe.suffix}"}/" />
						<include name="dbx_plug64_26${r"${exe.suffix}"}/" />
						<include name="j9ddrgen${r"${exe.suffix}"}/" />
						<include name="invtest${r"${exe.suffix}"}/" />
						<include name="pltest${r"${exe.suffix}"}/" />
						<include name="propstest${r"${exe.suffix}"}/" />
						<include name="vmLifecyleTests${r"${exe.suffix}"}/" />
						<include name="vmtest${r"${exe.suffix}"}/" />
						<include name="cleartest${r"${exe.suffix}"}/" />
						<include name="javasrv${r"${exe.suffix}"}/" />
						<include name="j9quarantine32${r"${exe.suffix}"}/" />
						<include name="j9qcontroller${r"${exe.suffix}"}/" />
						<include name="jsigjnitest${r"${exe.suffix}"}/" />
						<include name="cibench${r"${exe.suffix}"}/" />
						<include name="citest${r"${exe.suffix}"}/" />
						<include name="quarantinetest${r"${exe.suffix}"}/" />
						<include name="rasta${r"${exe.suffix}"}/" />
						<include name="remjniinvtest${r"${exe.suffix}"}/" />
						<include name="rpcbench${r"${exe.suffix}"}/" />
						<include name="rpctest${r"${exe.suffix}"}/" />
						<include name="tcpbench${r"${exe.suffix}"}/" />
						<include name="glaunch${r"${exe.suffix}"}/" />
						<include name="shrtest${r"${exe.suffix}"}/" />
						<include name="thread_test${r"${exe.suffix}"}/" />
						<include name="thrbasetest${r"${exe.suffix}"}/" />
						<include name="thrcreatetest${r"${exe.suffix}"}/" />
						<include name="thrstatetest${r"${exe.suffix}"}/" />
						<include name="thrextendedtest${r"${exe.suffix}"}/" />
						<include name="servicetest${r"${exe.suffix}"}/" />
						<include name="gc_rwlocktest${r"${exe.suffix}"}/" />
						<include name="testjep178_static${r"${exe.suffix}"}/" />
						<include name="testjep178_dynamic${r"${exe.suffix}"}/" />
					</fileset>
				</copy>
			</then>
		</if>
	</target>

	<target name="src" depends="init" description="Add source to ${r"${output}"}/src.zip">
		<if>
			<equals arg1="${r"${product}"}" arg2="java8" />
			<then>
				<condition property="full_src" value="full_" else="">
					<istrue value="${r"${jvmbuild}"}" />
				</condition>
				<unzip src="${r"${output}"}/src.zip" dest="src_jar/src" />
				<delete file="${r"${output}"}/src.zip" />
				<unjar src="src_jar/src_${r"${full_src}"}j8.jar" dest="src_jar/src" />
				<zip destfile="${r"${output}"}/src.zip" basedir="src_jar/src" filesonly="true" />
				<delete dir="src_jar/src" />
			</then>
			<else>
				<echo message="Source jars already complete for product version: ${r"${product}"}" />
			</else>
		</if>
	</target>

	<target name="jars" depends="init" description="copy jar files to proper locations">
		<if>
			<equals arg1="${r"${product}"}" arg2="java8" />
			<then>
				<!-- Copy the RAS binaries verbatim into the ext directory -->
				<copy todir="${r"${output}"}/jre/lib/ext" overwrite="true">
					<fileset dir="./RAS_Binaries">
						<include name="dtfj.jar" />
						<include name="dtfjview.jar" />
						<include name="traceformat.jar" />
					</fileset>
				</copy>
				<!-- Jazz 31127: Copy the DDR binaries into the ddr directory -->
				<copy file="./j9ddr.jar" todir="${r"${output}"}/${r"${jre.dir}"}/lib/ddr" verbose="true" overwrite="yes" />
			</then>
			<else>
				<copy file="./java9_j9ddr.jar" tofile="${r"${output}"}/${r"${jre.dir}"}/lib/ddr/j9ddr.jar" verbose="true" overwrite="yes" />
			</else>
		</if>

		<if>
			<equals arg1="${r"${product}"}" arg2="java8" />
			<then>
				<property name="kernel-classlib.target.dir" value="${r"${jvm.target.dir}"}/jclSC180" />
				<mkdir dir="${r"${kernel-classlib.target.dir}"}" />
				<copy
					file="src_jar/vm_j8-vm.jar"
					tofile="${r"${kernel-classlib.target.dir}"}/vm.jar"
					verbose="true" overwrite="yes" />

				<copy
					file="src_jar/dataaccess8.jar"
					tofile="${r"${output}/${jre.dir}"}/lib/dataaccess.jar"
					verbose="true" overwrite="yes" />
<#if uma.spec.flags.opt_cuda.enabled>
				<copy
					file="cuda4j_j8.jar"
					tofile="${r"${output}/${jre.dir}"}/lib/cuda4j.jar"
					verbose="true" overwrite="yes" />
</#if>
			</then>
			<else>
				<echo message="No jars to copy for product version: ${r"${product}"}" />
			</else>
		</if>
	</target>

	<target name="-create-module-info">
		<fail unless="module.info.class.loc" message="internal error, module.info.class.loc not defined" />
		<!-- Use javap to disassemble module-info.class file -->
		<condition property="javap.disassemble" value="${r"${jdk.bin}"}" else="${r"${mini.jdk.bin}"}">
			<equals arg1="${r"${java9build}"}" arg2="java9b136" />
		</condition>
		<apply executable="${r"${javap.disassemble}"}/javap" output="${r"${module.info.class.loc}"}/module-info.javap" logError="true">
			<fileset dir="${r"${module.info.class.loc}"}" includes="module-info.class" />
		</apply>

		<var name="module.info.javap" unset="true" />
		<loadfile srcFile="${r"${module.info.class.loc}"}/module-info.javap" property="module.info.javap">
			<filterchain>
				<!-- exclude the first line: Compiled from "module-info.java" -->
				<headfilter lines="-1" skip="1" />
				<!-- change 'class' to 'module' for old module-info.class files -->
				<tokenfilter>
					<replaceregex pattern="^\s*class\b" replace="module" />
				</tokenfilter>
				<!-- remove the version qualifier following the module name if present -->
				<tokenfilter>
					<replaceregex pattern="(module\s+\S+)@\S+" replace="\1" />
				</tokenfilter>
				<!-- replace any occurrence of "$" (e.g. in nested classes) with "." -->
				<tokenfilter>
					<replaceregex pattern="\$" replace="." flags="g" />
				</tokenfilter>
			</filterchain>
		</loadfile>
		<delete file="${r"${module.info.class.loc}"}/module-info.javap" />
		<echo file="${r"${module.info.class.loc}"}/module-info.java" message="${r"${module.info.javap}"}" />

		<antcall target="-merge-module-info">
			<param name="module.info.first"  value="${r"${module.info.class.loc}/module-info.java"}" />
			<param name="module.info.second" value="${r"${module.info.class.loc}/module-info.java"}" />
		</antcall>
	</target>

	<target name="-merge-module-info" description="merge module.info.second into module.info.first">
		<fail unless="module.info.first" message="internal error, module.info.first not defined" />
		<fail unless="module.info.second" message="internal error, module.info.second not defined" />

		<if>
			<equals arg1="${r"${module.info.first}"}" arg2="${r"${module.info.second}"}" />
			<then>
				<echo message="Normalizing ${r"${module.info.first}"}:" />
				<exec executable="cat">
					<arg value="${r"${module.info.first}"}" />
				</exec>
			</then>
			<else>
				<echo message="Merging ${r"${module.info.second}"} into ${r"${module.info.first}"}." />
				<echo message="merge inputs:" />
				<exec executable="cat">
					<arg value="${r"${module.info.first}"}" />
					<arg value="${r"${module.info.second}"}" />
				</exec>
			</else>
		</if>

		<exec executable="${r"${jdk.bin}/java"}" failonerror="true">
			<arg value="${r"${use.public.keyword}"}" />
			<arg value="-cp" />
			<arg value="${r"${build.tools.dir}"}" />
			<arg value="com.ibm.moduletools.ModuleInfoMerger" />
			<arg value="${r"${module.info.first}"}" />
			<arg value="${r"${module.info.second}"}" />
			<arg value="${r"${module.info.first}"}" />
		</exec>

		<echo message="merge output:" />
		<exec executable="cat">
			<arg value="${r"${module.info.first}"}" />
		</exec>
	</target>

	<target name="-create-module-jmod">
		<fail unless="module.name" message="internal error, module.name not defined" />
		<fail unless="module.jmod" message="internal error, module.jmod not defined" />
		<fail unless="module.dir" message="internal error, module.dir not defined" />
		<fail unless="modules.path" message="internal error, modules.path not defined" />

		<echo message="Recreating ${r"${module.jmod}"}" />

		<var name="jmod.options" value="" />
		<!-- Get module version of the module to be recreated -->
		<exec executable="${r"${jdk.bin}/java"}" output="${r"${module.name}"}.info" failonerror="false">
			<arg value="${r"${opt.list.modules}"}" />
			<arg value="${r"${module.name}"}" />
		</exec>
		<if>
			<available file="${r"${module.name}"}.info" type="file" />
			<then>
				<loadfile srcFile="${r"${module.name}"}.info" property="module.info">
					<filterchain>
						<headfilter lines="1" />
					</filterchain>
				</loadfile>
				<propertyregex property="module.version" input="${r"${module.info}"}" regexp="${r"${module.name}"}@(\S+)" select="\1" />
				<delete file="${r"${module.name}"}.info" />
			</then>
		</if>
		<if>
			<not>
				<isset property="module.version" />
			</not>
			<then>
				<property name="module.version" value="${r"${default.module.version}"}" />
				<echo message="No version information available for module ${r"${module.name} - assuming ${module.version}"}" />
			</then>
		</if>
		<var name="jmod.options" value="${r"${jmod.options}"} --module-version ${r"${module.version}"}" />
		<if>
			<available file="${r"${module.dir}"}/conf" type="dir" />
			<then>
				<var name="jmod.options" value="${r"${jmod.options}"} --config ${r"${module.dir}"}/conf" />
			</then>
		</if>
		<if>
			<available file="${r"${module.dir}"}/native" type="dir" />
			<then>
				<var name="jmod.options" value="${r"${jmod.options}"} --libs ${r"${module.dir}"}/native" />
			</then>
		</if>
		<if>
			<available file="${r"${module.dir}"}/bin" type="dir" />
			<then>
				<var name="jmod.options" value="${r"${jmod.options}"} --cmds ${r"${module.dir}"}/bin" />
			</then>
		</if>

		<if>
			<isset property="os-info" />
			<then>
				<var name="jmod.options" value="${r"${jmod.options}"} ${r"${os-info}"}" />
			</then>
		</if>

		<echo message="jmod options list: ${r"${jmod.options}"}" />
		<if>
			<matches string="${r"${java9build}"}" pattern="^(java9b136|java9b148|java9b150|java9b156)$$" />
			<then>
				<exec executable="${r"${mini.jdk.bin}"}/jmod" failonerror="false">
					<arg line="${r"${opt.invoke.strconcat}"}" />
					<arg value="create" />
					<arg value="--os-name" />
					<arg value="${r"${jmod.os.name.value}"}" />
					<arg value="--os-arch" />
					<arg value="${r"${jmod.os.arch.value}"}" />
					<arg value="--os-version" />
					<arg value="${r"${jmod.os.version.value}"}" />
					<arg value="--class-path" />
					<arg file="${r"${module.dir}"}/classes" />
					<arg line="${r"${jmod.options}"}" />
					<arg file="${r"${module.jmod}"}" />
				</exec>
			</then>
			<else>
				<exec executable="${r"${mini.jdk.bin}"}/jmod" failonerror="false">
					<arg line="${r"${opt.invoke.strconcat}"}" />
					<arg value="create" />
					<arg value="--target-platform" />
					<arg value="${r"${jmod.os.name.value}"}" />
					<arg value="--class-path" />
					<arg file="${r"${module.dir}"}/classes" />
					<arg line="${r"${jmod.options}"}" />
					<arg file="${r"${module.jmod}"}" />
				</exec>
			</else>
		</if>
	</target>

	<if>
		<matches string="${r"${java9build}"}" pattern="^(java9b136|java9b148)$$" />
		<then>
			<macrodef name="jmod.extract">
				<attribute name="jmod" />
				<attribute name="todir" />
				<sequential>
					<!-- workaround for builds when there is a warning: extra bytes at beginning or within zipfile -->
					<exec executable="unzip" failonerror="false">
						<arg value="-qn" />
						<arg value="@{jmod}" />
						<arg value="-d" />
						<arg value="@{todir}" />
					</exec>
				</sequential>
			</macrodef>
		</then>
		<else>
			<macrodef name="jmod.extract">
				<attribute name="jmod" />
				<attribute name="todir" />
				<sequential>
					<exec executable="${r"${mini.jdk.bin}/jmod"}">
						<arg value="extract" />
						<arg value="@{jmod}" />
						<arg value="--dir" />
						<arg value="@{todir}" />
					</exec>
				</sequential>
			</macrodef>
		</else>
	</if>

	<target name="-prepare-jmod">
		<fail unless="module.name" message="internal error, module.name not defined" />
		<fail unless="j9jcl.module.info" message="internal error, j9jcl.module.info not defined" />

		<!-- The contents of the module may already be present: we don't want to overwrite it. -->
		<available property="module_extracted.dir.present" file="${r"${output}/${module.name}_extracted"}" type="dir" />

		<mkdir dir="${r"${output}/${module.name}_extracted/classes"}" />

		<if>
			<available file="${r"${output}/jmods/${module.name}.jmod"}" type="file" />
			<then>
				<echo message="Preparing standard module : ${r"${module.name}"}" />

				<!-- Explode the module only if the directory wasn't already present. -->
				<if>
					<isfalse value="${r"${module_extracted.dir.present}"}" />
					<then>
						<jmod.extract jmod="${r"${output}/jmods/${module.name}.jmod"}" todir="${r"${output}/${module.name}_extracted"}" />
					</then>
				</if>

				<if>
					<length string="${r"${j9jcl.module.info}"}" when="greater" length="0" />
					<then>
						<antcall target="-create-module-info">
							<param name="module.info.class.loc" value="${r"${output}/${module.name}_extracted/classes"}" />
						</antcall>

						<echo message="Applying J9 JCL modifications" />
						<ant antfile="j9modules.xml" target="prepare-module" dir="jcl">
							<property name="module.dir" value="${r"${output}/${module.name}_extracted"}" />
							<property name="module.name" value="${r"${module.name}"}" />
							<property name="module.platform" value="${r"${platform.code}"}" />
						</ant>

						<!-- Rename module-info.java.extra to module-info.java if necessary. -->
						<if>
							<available file="${r"${j9jcl.module.info}/module-info.java.extra"}" type="file" />
							<then>
								<move file="${r"${j9jcl.module.info}/module-info.java.extra"}"
									tofile="${r"${j9jcl.module.info}/module-info.java"}"
									overwrite="false" verbose="true" />
							</then>
						</if>

						<antcall target="-merge-module-info">
							<param name="module.info.first"  value="${r"${j9jcl.module.info}/module-info.java"}" />
							<param name="module.info.second" value="${r"${output}/${module.name}_extracted/classes/module-info.java"}" />
						</antcall>
						<!-- Remove the module-info source so it won't be included in the jmod file. -->
						<delete file="${r"${output}/${module.name}_extracted/classes/module-info.java"}" verbose="true" />
					</then>
				</if>
			</then>
			<elseif>
				<length string="${r"${j9jcl.module.info}"}" when="greater" length="0" />
				<then>
					<echo message="Preparing J9-specific module : ${r"${module.name}"}" />
				</then>
			</elseif>
		</if>

		<mkdir dir="${r"${output}/modules_root/${module.name}"}" />
		<copy todir="${r"${output}/modules_root/${module.name}"}">
			<fileset dir="${r"${output}/${module.name}_extracted/classes"}" />
		</copy>

		<switch value="${r"${module.name}"}">
			<case value="com.ibm.dtfj">
				<mkdir dir="${r"${output}/module_patches"}" />
				<copy file="sourcetools/lib/ibmjzos.jar" tofile="${r"${output}/module_patches/com.ibm.jzos.jar"}" />

				<mkdir dir="${r"${output}/${module.name}_extracted/native/ddr"}" />
				<copy file="java9_j9ddr.jar" tofile="${r"${output}/${module.name}_extracted/native/ddr/j9ddr.jar"}" verbose="true" overwrite="yes" />
			</case>
			<case value="openj9.sharedclasses">
				<copy file="./${r"${lib.prefix}j9shr${vm.stream}${lib.suffix}"}" todir="${r"${output}/${module.name}_extracted/native/"}" verbose="true" overwrite="yes" />
			</case>
			<default>
			</default>
		</switch>
	</target>

	<target name="-create-jmod">
		<fail unless="module.name" message="internal error, module.name not defined" />
		<echo message="Compiling ${r"${module.name}"}" />
		<if>
			<equals arg1="${r"${java9build}"}" arg2="java9b136" />
			<then>
				<replaceregexp file="${r"${output}/modules_root/${module.name}/module-info.java"}"
					match=" transitive " replace=" public " byline="true" flags="g" />
			</then>
		</if>

		<!-- java.base can't safely use InvokeDynamic for string concatenation -->
		<condition property="string.concat.option" value="-XDstringConcat=inline" else="">
			<equals arg1="${r"${module.name}"}" arg2="java.base" />
		</condition>

		<!--
		Windows seriously limits the length of command lines. To deal with that,
		we put the list of files we want to compile in a file and use @<file>
		to communicate that list to javac.
		-->
		<var name="module.source.list" unset="true" />
		<pathconvert pathsep="${r"${line.separator}"}" property="module.source.list">
			<fileset dir="${r"${j9jcl.dir}/${module.name}/share/classes"}" includes="**/*.java" />
		</pathconvert>

		<delete file="${r"${j9jcl.dir}/${module.name}/source.files.list"}" />
		<echo file="${r"${j9jcl.dir}/${module.name}/source.files.list"}" message="${r"${module.source.list}"}" />

		<var name="before.javac.start" unset="true" />
		<tstamp>
			<format property="before.javac.start" pattern="yyyyMMdd-HHmmss" />
		</tstamp>
		<!--
		Pause a short time to make sure that files written by javac have
		timestamps newer than the timestamp captured in 'before.javac.start'.
		-->
		<sleep seconds="5"/>

		<exec executable="${r"${mini.jdk.bin}/javac"}" failonerror="true">
			<arg value="-verbose" />
			<arg value="-g" />
			<arg value="-Xlint:unchecked" />
			<arg line="${r"${string.concat.option}"}" />

			<arg value="-d" />
			<arg value="${r"${output}/modules_root"}" />

			<arg value="--module-source-path" />
			<arg value="${r"${j9jcl.dir}/*/share/classes${path.separator}${output}/modules_root"}" />

			<arg value="--module-path" />
			<arg value="${r"${output}/modules_root${path.separator}${output}/module_patches/com.ibm.jzos.jar"}" />

			<arg value="--system" />
			<arg value="none" />

			<arg value="@${r"${j9jcl.dir}/${module.name}/source.files.list"}" />
		</exec>
		<delete file="${r"${j9jcl.dir}/${module.name}/source.files.list"}" />

		<switch value="${r"${module.name}"}">
			<case value="com.ibm.dtfj">
				<!--
				Until we have a real module for com.ibm.jzos, we don't want to include it
				in the jlink step, so we must remove the requires clause and recompile
				com.ibm.dtfj/module-info.java.
				-->
				<echo message="Removing com.ibm.dtfj requires com.ibm.jzos" />
				<replaceregexp file="${r"${j9jcl.dir}/${module.name}/share/classes/module-info.java"}" replace="" flags="g">
					<regexp pattern="\brequires\b.*\bcom\.ibm\.jzos\s*;" />
				</replaceregexp>
				<apply executable="${r"${mini.jdk.bin}/javac"}" failonerror="true">
					<arg value="-verbose" />
					<arg value="-g" />
					<arg value="-Xlint:unchecked" />

					<arg value="-d" />
					<arg value="${r"${output}/modules_root"}" />

					<arg value="--module-source-path" />
					<arg value="${r"${j9jcl.dir}/*/share/classes${path.separator}${output}/modules_root"}" />

					<arg value="--module-path" />
					<arg value="${r"${output}/modules_root"}" />

					<arg value="--system" />
					<arg value="none" />

					<fileset dir="${r"${j9jcl.dir}/${module.name}/share/classes"}" includes="module-info.java" />
				</apply>
			</case>
			<default>
			</default>
		</switch>

		<!-- Copy files written by javac to vm_modules_root. -->
		<copy todir="${r"${output}/vm_modules_root/${module.name}"}">
			<fileset dir="${r"${output}/modules_root/${module.name}"}">
				<date when="after" datetime="${r"${before.javac.start}"}" pattern="yyyyMMdd-HHmmss" />
			</fileset>
		</copy>

		<copy todir="${r"${output}/${module.name}_extracted/classes"}">
			<fileset dir="${r"${output}/modules_root/${module.name}"}" />
		</copy>

		<!-- Copy non-java (e.g. *.properties) files. -->
		<copy todir="${r"${output}/${module.name}_extracted/classes"}" verbose="true">
			<fileset dir="${r"${j9jcl.dir}/${module.name}"}" excludes="**/*.java" />
		</copy>

		<delete file="${r"${output}/jmods/${module.name}.jmod"}" />

		<antcall target="-create-module-jmod">
			<param name="module.name" value="${r"${module.name}"}" />
			<param name="module.jmod" value="${r"${output}/jmods/${module.name}.jmod"}" />
			<param name="module.dir" value="${r"${output}/${module.name}_extracted"}" />
			<param name="modules.path" value="${r"${output}/jmods"}" />
		</antcall>
		<echo message="Finished recreating ${r"${module.name}.jmod"}" />
	</target>

	<target name="-build_tools_init">
		<property name="output" value="${r"${compose.dir}"}" />

		<echo message="${r"jdk.bin: ${jdk.bin}, mini.jdk.bin: ${mini.jdk.bin}"}" />

		<echo message="Compiling J9 JCL Build Tools" />

		<tempfile property="build.tools.dir" destdir="/tmp" prefix="j9jcl_tools-" deleteonexit="true" />
		<delete dir="${r"${build.tools.dir}"}" />
		<mkdir dir="${r"${build.tools.dir}"}" />

		<javac destdir="${r"${build.tools.dir}"}" executable="${r"${mini.jdk.bin}/javac"}" failonerror="true" fork="yes" includeAntRuntime="no" listfiles="yes" source="1.8" target="1.8">
			<src path="sourcetools/J9_JCL_Build_Tools/src" />
		</javac>
	</target>

	<target name="merge_j9jcl_modules" depends="-build_tools_init">
		<if>
			<matches string="${r"${java9build}"}" pattern="^(java9b136|java9b148|java9b150|java9b156)$$" />
			<then>
				<if>
					<available file="${r"${output}"}/jmods" type="dir" />
					<then>
						<property name="j9jcl.dir" value="${r"${output}"}/j9jcl" />
						<property name="updated.sdk.dir" value="${r"${output}"}/updated_sdk" />
<#if uma.spec.flags.env_littleEndian.enabled>
						<property name="jlink.endian.value" value="little" />
<#else>
						<property name="jlink.endian.value" value="big" />
</#if>

						<mkdir dir="${r"${j9jcl.dir}"}" />

						<echo message="Preprocessing JCL source with config ${r"${jpp.config}"}" />

						<exec executable="${r"${mini.jdk.bin}/java"}" failonerror="true">
							<arg value="-Dfile.encoding=US-ASCII" />
							<arg value="-cp" />
							<arg value="sourcetools/lib/jpp.jar" />
							<arg value="com.ibm.jpp.commandline.CommandlineBuilder" />
							<arg value="-verdict" />
							<arg value="-baseDir" />
							<arg value="./" />
							<arg value="-config" />
							<arg value="${r"${jpp.config}"}" />
							<arg value="-srcRoot" />
							<arg value="jcl/" />
							<arg value="-xml" />
							<arg value="jpp_configuration.xml" />
							<arg value="-dest" />
							<arg value="${r"${j9jcl.dir}"}" />
							<arg value="-macro:define" />
							<arg value="${r"com.ibm.oti.vm.library.version=${vm.stream}"}" />
							<arg value="-tag:define" />
							<arg value="${r"PLATFORM-${platform.code}"}" />
						</exec>

						<!--
						Put the list of all modules into a property to be used by other targets.
						Modules must be processed in dependency order: lexicographical order will not work.
						-->
						<loadfile property="prop.modules.list" srcfile="jcl/j9modules.txt">
							<filterchain>
								<!-- remove end of line comments -->
								<replaceregex pattern="#.*$" replace="" />
								<tokenfilter delimoutput=";">
									<replaceregex pattern="\s" replace="" flags="g" />
									<ignoreblank />
								</tokenfilter>
								<replaceregex pattern=";$" replace="" />
							</filterchain>
						</loadfile>

						<delete file="${r"${output}"}/jmods/com.ibm.sharedclasses.jmod" quiet="true" />
						<delete file="${r"${output}"}/jmods/com.ibm.dataaccess.jmod" quiet="true" />
						<delete file="${r"${output}"}/jmods/com.ibm.traceformat.jmod" quiet="true" />

						<!-- List all modules; because we're only concerned with those not updated for J9 JCL, order isn't significant. -->
						<pathconvert property="prop.jmods.list" pathsep=";">
							<fileset dir="${r"${output}"}/jmods" includes="*.jmod" />
							<mapper>
								<chainedmapper>
									<flattenmapper />
									<globmapper from="*.jmod" to="*" />
								</chainedmapper>
							</mapper>
						</pathconvert>

						<!-- Prepare jmods for the modules with content from J9 JCL. -->
						<for list="${r"${prop.modules.list}"}" delimiter=";" param="module.name">
							<sequential>
								<if>
									<or>
										<available file="${r"${j9jcl.dir}"}/@{module.name}/share/classes/module-info.java" type="file" />
										<available file="${r"${j9jcl.dir}"}/@{module.name}/share/classes/module-info.java.extra" type="file" />
									</or>
									<then>
										<property name="j9jcl-module.@{module.name}" value="true" />
										<antcall target="-prepare-jmod">
											<param name="module.name" value="@{module.name}" />
											<param name="j9jcl.module.info" value="${r"${j9jcl.dir}"}/@{module.name}/share/classes" />
										</antcall>
									</then>
								</if>
							</sequential>
						</for>

						<!-- Prepare jmods for all other modules. -->
						<for list="${r"${prop.jmods.list}"}" delimiter=";" param="module.name">
							<sequential>
								<if>
									<not>
										<isset property="j9jcl-module.@{module.name}" />
 									</not>
									<then>
										<antcall target="-prepare-jmod">
											<param name="module.name" value="@{module.name}" />
											<param name="j9jcl.module.info" value="" />
										</antcall>
									</then>
								</if>
							</sequential>
						</for>

						<!-- Create jmods for the modules with content from J9 JCL. -->
						<mkdir dir="${r"${output}/vm_modules_root"}" />
						<for list="${r"${prop.modules.list}"}" delimiter=";" param="module.name">
							<sequential>
								<if>
									<isset property="j9jcl-module.@{module.name}" />
									<then>
										<antcall target="-create-jmod">
											<param name="module.name" value="@{module.name}" />
										</antcall>
									</then>
								</if>
							</sequential>
						</for>

						<!-- Get list of modules in jmods directory. -->
						<pathconvert property="modules.name.list" pathsep=",">
							<fileset dir="${r"${output}"}/jmods" includes="*.jmod" />
							<mapper>
								<chainedmapper>
									<flattenmapper />
									<globmapper from="*.jmod" to="*" />
								</chainedmapper>
							</mapper>
						</pathconvert>

						<!-- delete all {module.name}_extracted directories -->
						<for list="${r"${modules.name.list}"}" delimiter="," param="module.name">
							<sequential>
								<delete dir="${r"${output}/@{module.name}_extracted"}" />
							</sequential>
						</for>

						<!-- Create a runtime image using updated jmods. -->
						<echo message="modules.name.list: ${r"${modules.name.list}"}" />
						<if>
							<isset property="jlink.endian.value" />
							<then>
								<exec executable="${r"${mini.jdk.bin}"}/jlink" failonerror="true">
									<arg line="${r"${opt.invoke.strconcat}"}" />
									<arg value="--module-path" />
									<arg file="${r"${output}"}/jmods" />
									<arg value="--add-modules" />
									<arg value="${r"${modules.name.list}"}" />
									<arg value="--endian" />
									<arg value="${r"${jlink.endian.value}"}" />
									<arg value="--output" />
									<arg value="${r"${updated.sdk.dir}"}" />
								</exec>
							</then>
							<else>
								<exec executable="${r"${mini.jdk.bin}"}/jlink" failonerror="true">
									<arg line="${r"${opt.invoke.strconcat}"}" />
									<arg value="--module-path" />
									<arg file="${r"${output}"}/jmods" />
									<arg value="--add-modules" />
									<arg value="${r"${modules.name.list}"}" />
									<arg value="--output" />
									<arg value="${r"${updated.sdk.dir}"}" />
								</exec>
							</else>
						</if>
						<copy file="${r"${updated.sdk.dir}"}/lib/modules" tofile="${r"${output}"}/lib/modules" failonerror="false" />

						<!-- update src.zip with (appropriate subset of) generated sources -->
						<property name="src_zip.dir" value="${r"${output}/zip-src"}" />

						<delete dir="${r"${src_zip.dir}"}" />
						<mkdir dir="${r"${src_zip.dir}"}" />
						<condition property="src.zip.location" value="${r"${output}"}" else="${r"${output}"}/lib">
							<matches string="${r"${java9build}"}" pattern="^(java9b136|java9b148)$$" />
						</condition>
						<unzip src="${r"${src.zip.location}/src.zip"}" dest="${r"${src_zip.dir}"}" />
						<delete file="${r"${src.zip.location}/src.zip"}" />

						<echo message="Updating src.zip for build.type='${r"${build.type}"}'." />
						<if>
							<not>
								<equals arg1="${r"${build.type}"}" arg2="jvmbuild" />
							</not>
							<then>
								<!-- exclude IBM Confidential source -->
								<delete verbose="true">
									<fileset dir="${r"${j9jcl.dir}"}">
										<contains text="IBM Confidential" casesensitive="false" ignorewhitespace="true" />
									</fileset>
								</delete>
								<!-- exclude module-info.java until they have reasonable copyright notices -->
								<delete verbose="true">
									<fileset dir="${r"${j9jcl.dir}"}">
										<include name="**/module-info.java" />
									</fileset>
								</delete>
							</then>
						</if>
						<copy todir="${r"${src_zip.dir}"}" overwrite="true">
							<fileset dir="${r"${j9jcl.dir}"}" />
						</copy>

						<zip destfile="${r"${src.zip.location}/src.zip"}" basedir="${r"${src_zip.dir}"}" filesonly="true" />
						<delete dir="${r"${src_zip.dir}"}" />

						<echo message="Creating vm.jar with J9JCL classes." />
						<mkdir dir="${r"${output}/J9_JCL/jclSC19"}" />
						<zip destfile="${r"${output}/J9_JCL/jclSC19/vm.jar"}" filesonly="true">
							<fileset dir="${r"${output}/vm_modules_root"}">
								<exclude name="**/module-info.class" />
							</fileset>
						</zip>

						<delete dir="${r"${j9jcl.dir}"}" />
						<delete dir="${r"${updated.sdk.dir}"}" />
						<delete dir="${r"${output}/modules_root"}" />
						<delete dir="${r"${output}/module_patches"}" />
						<delete dir="${r"${output}/vm_modules_root"}" />
					</then>
					<else>
						<echo message="Did not find ${r"${output}"}/jmods. Merging of J9 JCL is not possible." />
					</else>
				</if>
			</then>
			<else>
				<echo message="Nothing to be done for product=${r"${product}"}" />
			</else>
		</if>
	</target>

	<target name="prepare_j9source" description="Preprocess J9JCL source">
		<fail unless="output" message="'output' directory must be specified" />
		<fail unless="platform" message="'platform' must be specified" />
		<fail unless="product" message="'product' must be specified" />

		<mkdir dir="${r"${output}"}" />
		<echo message="Preprocessing JCL source with config ${r"${jpp.config}"}" />
		<java classname="com.ibm.jpp.commandline.CommandlineBuilder">
			<jvmarg value="-Dfile.encoding=US-ASCII" />
			<classpath>
				<fileset dir="sourcetools/lib">
					<include name="jpp.jar" />
				</fileset>
			</classpath>
			<arg value="-verdict" />
			<arg value="-baseDir" />
			<arg value="./" />
			<arg value="-config" />
			<arg value="${r"${jpp.config}"}" />
			<arg value="-srcRoot" />
			<arg value="jcl/" />
			<arg value="-xml" />
			<arg value="jpp_configuration.xml" />
			<arg value="-dest" />
			<arg value="${r"${output}"}" />
			<arg value="-macro:define" />
			<arg value="${r"com.ibm.oti.vm.library.version=${vm.stream}"}" />
			<arg value="-tag:define" />
			<arg value="${r"PLATFORM-${platform.code}"}" />
		</java>
	</target>

	<target name="copy_jvm_to_javabase">
		<property name="output" value="${r"${compose.dir}/java.base_extracted/native"}" />
		<property name="dest.java.base" value="true" />
		<property name="jre.dir" value="" />
		<property name="bin.dir" value="" />
		<property name="lib.dir" value="" />
		<if>
			<matches string="${r"${java9build}"}" pattern="^(java9b136|java9b148|java9b150|java9b156)$$" />
			<then>
				<if>
					<not>
						<available file="${r"${compose.dir}/java.base_extracted"}" type="dir" />
					</not>
					<then>
						<mkdir dir="${r"${compose.dir}/java.base_extracted"}" />
						<jmod.extract jmod="${r"${compose.dir}/jmods/java.base.jmod"}" todir="${r"${compose.dir}/java.base_extracted"}" />
					</then>
				</if>
				<property name="clean.jvm.target.dir" value="true" />
				<antcall target="copy_j9_jvm" />
				<echo message="Copying of VM libraries for sidecar=${r"${sidecar.name}"} is done" />
			</then>
			<else>
				<echo message="Nothing to be done for product=${r"${product}"}" />
			</else>
		</if>
	</target>

	<target name="all" description="Run all targets">
		<property name="output" value="${r"${compose.dir}"}" />
		<property name="bin.dir" value="bin" />
		<property name="lib.dir" value="lib" />
		<property name="dest.java.base" value="false" />
		<!-- Determine if we want the old SDK structure or the new one (JEP 200, 220) RTC 87795. -->
		<if>
			<equals arg1="${r"${directory.layout}"}" arg2="jdk" />
			<then>
				<var name="jre.dir" value="" />
				<antcall target="-all" />
			</then>
			<elseif>
				<equals arg1="${r"${directory.layout}"}" arg2="both" />
				<then>
					<var name="jre.dir" value="jre" />
					<antcall target="-all" />
					<var name="jre.dir" value="" />
					<antcall target="-all" />
				</then>
			</elseif>
			<else>
				<var name="jre.dir" value="jre" />
				<antcall target="-all" />
			</else>
		</if>
	</target>

	<target name="-all" depends="init,jars,copy_j9_jvm,src" description="Run all targets" />

	<target name="clean" description="clean up" />

</project>
