<!--
Copyright (c) 2017, 2018 IBM Corp. and others

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

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

Building OpenJDK Version 8 with OpenJ9
======================================

Our website describes a simple [build process](http://www.eclipse.org/openj9/oj9_build.html)
that uses Docker and Dockerfiles to create a build environment that contains everything
you need to easily build a Linux binary of OpenJDK V8 with the Eclipse OpenJ9 virtual machine.
A more complete set of build instructions are included here for multiple platforms:

- [Linux :penguin:](#linux)
- [AIX :blue_book:](#aix)
- [Windows :ledger:](#windows)
- [MacOS :apple:](#macos)
- [ARM :iphone:](#arm)


----------------------------------

## Linux
:penguin:
This build process provides detailed instructions for building a Linux x86-64 binary of OpenJDK V8 with OpenJ9 on Ubuntu 16.04. The binary can be built directly on your system, in a virtual
machine, or in a Docker container :whale:.

If you are using a different Linux distribution, you might have to review the list of libraries that are bundled with your distribution and/or modify the instructions to use equivalent commands to the Advanced Packaging Tool (APT). For example, for Centos, substitute the `apt-get` command with `yum`.

If you want to build a binary for Linux on a different architecture, such as Power Systems&trade; or z Systems&trade;, the process is very similar and any additional information for those architectures are included as Notes :pencil: as we go along.


### 1. Prepare your system
:penguin:
Instructions are provided for preparing your system with and without the use of Docker technology.

Skip to [Setting up your build environment without Docker](#setting-up-your-build-environment-without-docker).

#### Setting up your build environment with Docker :whale:
If you want to build a binary by using a Docker container, follow these steps to prepare your system:

1. The first thing you need to do is install Docker. You can download the free Community edition from [here](https://docs.docker.com/engine/installation/), which also contains instructions for installing Docker on your system.  You should also read the [Getting started](https://docs.docker.com/get-started/) guide to familiarise yourself with the basic Docker concepts and terminology.

2. Obtain the [Linux on 64-bit x86 systems Dockerfile](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk8/x86_64/ubuntu16/Dockerfile) to build and run a container that has all the correct software pre-requisites.

    :pencil: Dockerfiles are also available for the following Linux architectures: [Linux on 64-bit Power systems&trade;](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk8/ppc64le/ubuntu16/Dockerfile) and [Linux on 64-bit z Systems&trade;](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk8/s390x/ubuntu16/Dockerfile)

    Either download one of these Dockerfiles to your local system or copy and paste one of the following commands:

  - For Linux on 64-bit x86 systems, run:
```
wget https://raw.githubusercontent.com/eclipse/openj9/master/buildenv/docker/jdk8/x86_64/ubuntu16/Dockerfile
```

  - For Linux on 64-bit Power systems, run:
```
wget https://raw.githubusercontent.com/eclipse/openj9/master/buildenv/docker/jdk8/ppc64le/ubuntu16/Dockerfile
```

  - For Linux on 64-bit z Systems, run:
```
wget https://raw.githubusercontent.com/eclipse/openj9/master/buildenv/docker/jdk8/s390x/ubuntu16/Dockerfile
```

3. Next, run the following command to build a Docker image, called **openj9**:
```
docker build -t openj9 -f Dockerfile .
```

4. Start a Docker container from the **openj9** image with the following command, where `-v` maps any directory, `<host_directory>`,
on your local system to the containers `/root/hostdir` directory so that you can store the binaries, once they are built:
```
docker run -v <host_directory>:/root/hostdir -it openj9
```

:pencil: Depending on your [Docker system configuration](https://docs.docker.com/engine/reference/commandline/cli/#description), you might need to prefix the `docker` commands with `sudo`.

Now that you have the Docker image running, you are ready to move to the next step, [Get the source](#2-get-the-source).

#### Setting up your build environment without Docker

If you don't want to user Docker, you can still build an OpenJDK V8 with OpenJ9 directly on your Ubuntu system or in a Ubuntu virtual machine. Use the
[Linux on x86 Dockerfile](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk8/x86_64/ubuntu16/Dockerfile) like a recipe card to determine the software dependencies
that must be installed on the system, plus a few configuration steps.

:pencil:
Not on x86? We also have Dockerfiles for the following Linux architectures: [Linux on Power systems](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk8/ppc64le/ubuntu16/Dockerfile) and [Linux on z Systems](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk8/s390x/ubuntu16/Dockerfile).


1. Install the list of dependencies that can be obtained with the `apt-get` command from the following section of the Dockerfile:

```
apt-get update \
  && apt-get install -qq -y --no-install-recommends \
    autoconf \
    ca-certificates \
    ...
    ...
```

:warning: For Linux on z Systems, the `openjdk-7-jdk` package does not contain a JIT compiler, which means that the initial compile time will take several hours.

2. This build uses the same gcc and g++ compiler levels as OpenJDK, which might be
backlevel compared with the versions you use on your system. Create links for
the compilers with the following commands:
```
ln -s g++ /usr/bin/c++
ln -s g++-4.8 /usr/bin/g++
ln -s gcc /usr/bin/cc
ln -s gcc-4.8 /usr/bin/gcc
```

3. Download and setup **freemarker.jar** into a directory. The example commands use `/root` to be consistent with the Docker instructions. If you aren't using Docker, you probably want to store the **freemarker.jar** in your home directory.
```
cd /root
wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz
tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2
rm -f freemarker.tgz
```

### 2. Get the source
:penguin:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches. Run the following command:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk8
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk8
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:
```
bash ./get_source.sh
```

### 3. Configure
:penguin:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash ./configure --with-freemarker-jar=/root/freemarker.jar
```
:warning: You must give an absolute path to freemarker.jar

### 4. Build
:penguin:
Now you're ready to build OpenJDK V8 with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will fail, because the default `make` target is `exploded-image`. If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script. For more information, read this [issue](https://github.com/ibmruntimes/openj9-openjdk-jdk9/issues/34).

Two Java builds are produced: a full developer kit (jdk) and a runtime environment (jre)
- **build/linux-x86_64-normal-server-release/images/j2sdk-image**
- **build/linux-x86_64-normal-server-release/images/j2re-image**

    :whale: If you built your binaries in a Docker container, copy the binaries to the containers **/root/hostdir** directory so that you can access them on your local system. You'll find them in the directory you set for `<host_directory>` when you started your Docker container. See [Setting up your build environment with Docker](#setting-up-your-build-environment-with-docker).

    :pencil: On other architectures the **/j2sdk-image** and **/j2re-image** directories are in **build/linux-ppc64le-normal-server-release/images** (Linux on 64-bit Power systems) and **build/linux-s390x-normal-server-release/images** (Linux on 64-bit z Systems)

### 5. Test
:penguin:
For a simple test, try running the `java -version` command.
Change to the **/j2re-image** directory:
```
cd build/linux-x86_64-normal-server-release/images/j2re-image
```
Run:
```
./bin/java -version
```

Here is some sample output:

```
openjdk version "1.8.0-internal"
OpenJDK Runtime Environment (build 1.8.0-internal-admin_2017_10_31_10_46-b00)
Eclipse OpenJ9 VM (build 2.9, JRE 1.8.0 Linux amd64-64 Compressed References 20171031_000000 (JIT enabled, AOT enabled)
OpenJ9   - 68d6fdb
OMR      - 7c3d3d7
OpenJDK  - 27f5b8f based on jdk8u152-b03)
```
:penguin: *Congratulations!* :tada:

----------------------------------

## AIX
:blue_book:

:construction:
This section is still under construction. Further contributions expected.

The following instructions guide you through the process of building an OpenJDK V8 binary that contains Eclipse OpenJ9 on AIX 7.2.

### 1. Prepare your system
:blue_book:
You must install the following AIX Licensed Program Products (LPPs):

- [Java7_64](https://developer.ibm.com/javasdk/support/aix-download-service/)
- [xlc/C++ 13.1.3](https://www.ibm.com/developerworks/downloads/r/xlcplusaix/)
- x11.adt.ext

A number of RPM packages are also required. The easiest method for installing these packages is to use `yum`, because `yum` takes care of any additional dependent packages for you.

Download the following file: [yum_install_aix-ppc64.txt](aix/jdk8/yum_install_aix-ppc64.txt)

This file contains a list of required RPM packages that you can install by specifying the following command:
```
yum shell yum_install_aix-ppc64.txt
```

It is important to take the list of package dependencies from this file because it is kept right up to date by our developers.

Download and setup freemarker.jar into your home directory by running the following commands:

```
cd /<my_home_dir>
wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz \
tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2 \
rm -f freemarker.tgz
```

### 2. Get the source
:blue_book:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches. Run the following command:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk8
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk8
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:

```
bash ./get_source.sh
```
### 3. Configure
:blue_book:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash ./configure --with-freemarker-jar=/<my_home_dir>/freemarker.jar /
                 --with-cups-include=<cups_include_path> /
                 --disable-warnings-as-errors
```
where `<my_home_dir>` is the location where you stored **freemarker.jar** and `<cups_include_path>` is the absolute path to CUPS. For example `/opt/freeware/include`.

### 4. build
:blue_book:
Now you're ready to build OpenJDK with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will fail, because the default `make` target is `exploded-image`. If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script. For more information, read this [issue](https://github.com/ibmruntimes/openj9-openjdk-jdk9/issues/34).

Two Java builds are produced: a full developer kit (jdk) and a runtime environment (jre)
- **build/aix-ppc64-normal-server-release/images/j2sdk-image**
- **build/aix-ppc64-normal-server-release/images/j2re-image**

    :pencil: A JRE binary is not currently generated due to an OpenJDK bug.


### 5. Test
:blue_book:
For a simple test, try running the `java -version` command.
Change to the **/j2sdk-image** directory:
```
cd build/aix-ppc64-normal-server-release/images/j2sdk-image
```
Run:
```
./bin/java -version
```

Here is some sample output:

```
openjdk version "1.8.0-internal"
OpenJDK Runtime Environment (build 1.8.0-internal-admin_2017_10_31_10_46-b00)
Eclipse OpenJ9 VM (build 2.9, JRE 8 AIX ppc-64 Compressed References 20171031_000000 (JIT enabled, AOT enabled)
OpenJ9   - 68d6fdb
OMR      - 7c3d3d7
OpenJDK  - 27f5b8f based on jdk8u152-b03)
```
:blue_book: *Congratulations!* :tada:

----------------------------------

## Windows
:ledger:
The following instructions guide you through the process of building a Windows 64-bit OpenJDK V8 binary that contains Eclipse OpenJ9. This process can be used to build binaries for Windows 7, 8, and 10.

### 1. Prepare your system
:ledger:
You must install a number of software dependencies to create a suitable build environment on your system:

- [Cygwin for 64-bit versions of Windows](https://cygwin.com/install.html), which provides a Unix-style command line interface. Install all packages in the `Devel` category, which includes `mingw`, a minimalist subset of GNU tools for Microsoft Windows. 
OpenJ9 uses the mingw/GCC compiler during the build process. In the `Archive` category, install the packages `zip`, `unzip`, and `cpio`. Install any further package dependencies that are identified by the installer. More information about using Cygwin can be found [here](https://cygwin.com/docs.html).
- [Windows JDK 7](http://www.oracle.com/technetwork/java/javase/downloads/java-archive-downloads-javase7-521261.html), which is used as the boot JDK.
- [Windows SDK 7 Debugging tools](https://www.microsoft.com/download/confirmation.aspx?id=8279).
- [Microsoft Visual Studio Professional 2010](https://www.visualstudio.com/vs/older-downloads/).
- [Microsoft Visual Studio 2010 Service Pack 1](https://support.microsoft.com/en-us/help/983509/description-of-visual-studio-2010-service-pack-1)
- [Freemarker V2.3.8](https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download)
- [Freetype2 V2.3 or newer](https://www.freetype.org/download.html)

Update your `LIB` and `INCLUDE` environment variables to provide a path to the Windows debugging tools with the following commands:

```
set INCLUDE =C:\Program Files\Debugging Tools for Windows (x64)\sdk\inc;%INCLUDE%
set LIB=C:\Program Files\Debugging Tools for Windows (x64)\sdk\lib;%LIB%;
```

You can download Freemarker and Freetype manually or obtain them using the [wget](http://www.gnu.org/software/wget/faq.html#download) utility. If you choose to use `wget`, follow these steps:

- Open a cygwin64 terminal and change to the `/temp` directory:
```
cd /cygdrive/c/temp
```

- Run the following commands:
```
wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz
wget http://download.savannah.gnu.org/releases/freetype/freetype-2.5.3.tar.gz
```

- To unpack the Freemarker and Freetype compressed files, run:
```
tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2
tar --one-top-level=/cygdrive/c/temp/freetype --strip-components=1 -xzf freetype-2.5.3.tar.gz
```

- To build the Freetype dynamic and static libraries, open the Visual Studio Command Prompt (VS2010) (see C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Microsoft Visual Studio 2010\Visual Studio Tools) and run:
```
cd c:\temp
msbuild.exe C:/temp/freetype/builds/windows/vc2010/freetype.vcxproj  /p:PlatformToolset=v100  /p:Configuration="Release Multithreaded"  /p:Platform=x64  /p:ConfigurationType=DynamicLibrary  /p:TargetName=freetype  /p:OutDir="C:/temp/freetype/lib64/"  /p:IntDir="C:/temp/freetype/obj64/" > freetype.log
msbuild.exe C:/temp/freetype/builds/windows/vc2010/freetype.vcxproj  /p:PlatformToolset=v100  /p:Configuration="Release Multithreaded"  /p:Platform=x64  /p:ConfigurationType=StaticLibrary  /p:TargetName=freetype  /p:OutDir="C:/temp/freetype/lib64/"  /p:IntDir="C:/temp/freetype/obj64/" >> freetype.log
```
:pencil: Check the `freetype.log` for errors.

### 2. Get the source
:ledger:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches.

Run the following command in the Cygwin terminal:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk8
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk8
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:

```
bash ./get_source.sh
```
### 3. Configure
:ledger:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure --disable-ccache --with-boot-jdk=/cygdrive/c/temp/jdk7 --with-freemarker-jar=/cygdrive/c/temp/freemarker.jar --with-freetype-include=/cygdrive/c/temp/freetype/include --with-freetype-lib=/cygdrive/c/temp/freetype/lib64
```

:pencil: Modify the paths for freemarker and freetype if you manually downloaded and unpacked these dependencies into different directories.

### 4. build
:ledger:
Now you're ready to build OpenJDK with OpenJ9:
```
make all
```

Two Java builds are produced: a full developer kit (jdk) and a runtime environment (jre)
- **build/windows-x86_64-normal-server-release/images/j2sdk-image**
- **build/windows-x86_64-normal-server-release/images/j2re-image**

### 5. Test
:ledger:
For a simple test, try running the `java -version` command.
Change to the **/j2sdk-image** directory:
```
cd build/windows-x86_64-normal-server-release/images/j2sdk-image
```
Run:
```
./bin/java -version
```

Here is some sample output:

```
OpenJDK Runtime Environment (build 1.8.0-internal-Administrator_2017_12_14_15_20-b00)
Eclipse OpenJ9 VM (build 2.9, JRE 1.8.0 Windows 10 amd64-64 Compressed References 20171214_000000 (JIT enabled, AOT enabled)
OpenJ9   - b8ac7e1747
OMR      - 101e793f
OpenJDK  - 2597cd5c6f based on jdk8u152-b16)
```

:ledger: *Congratulations!* :tada:

----------------------------------

## MacOS
:apple:

:construction:
We haven't created a full build process for macOS yet? Watch this space!

----------------------------------

## ARM
:iphone:

:construction:
We haven't created a full build process for ARM yet? Watch this space!
