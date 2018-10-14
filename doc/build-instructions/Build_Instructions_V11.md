<!--
Copyright (c) 2018, 2018 IBM Corp. and others

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

Building OpenJDK Version 11 with OpenJ9
======================================

Our website describes a simple [build process](http://www.eclipse.org/openj9/oj9_build.html)
that uses Docker and Dockerfiles to create a build environment that contains everything
you need to easily build a Linux binary of **OpenJDK V11** with the Eclipse OpenJ9 virtual machine.
A more complete set of build instructions are included here for multiple platforms:

- [Linux :penguin:](#linux)
- [AIX :blue_book:](#aix)
- [Windows :ledger:](#windows)
- [MacOS :apple:](#macos)
- [ARM :iphone:](#arm)

----------------------------------

## Linux
:penguin:
This build process provides detailed instructions for building a Linux x86-64 binary of **OpenJDK V11** with OpenJ9 on Ubuntu 16.04. The binary can be built directly on your system, in a virtual
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

2. Obtain the [Linux on 64-bit x86 systems Dockerfile](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk11/x86_64/ubuntu16/Dockerfile) to build and run a container that has all the correct software pre-requisites.

    :pencil: Dockerfiles are also available for the following Linux architectures: [Linux on 64-bit Power systems&trade;](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk11/ppc64le/ubuntu16/Dockerfile) and [Linux on 64-bit z Systems&trade;](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk11/s390x/ubuntu16/Dockerfile)

    Either download one of these Dockerfiles to your local system or copy and paste one of the following commands:

  - For Linux on 64-bit x86 systems, run:
```
wget https://raw.githubusercontent.com/eclipse/openj9/master/buildenv/docker/jdk11/x86_64/ubuntu16/Dockerfile
```

  - For Linux on 64-bit Power systems, run:
```
wget https://raw.githubusercontent.com/eclipse/openj9/master/buildenv/docker/jdk11/ppc64le/ubuntu16/Dockerfile
```

  - For Linux on 64-bit z Systems, run:
```
wget https://raw.githubusercontent.com/eclipse/openj9/master/buildenv/docker/jdk11/s390x/ubuntu16/Dockerfile
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

If you don't want to user Docker, you can still build an **OpenJDK V11** with OpenJ9 directly on your Ubuntu system or in a Ubuntu virtual machine. Use the
[Linux on x86 Dockerfile](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk11/x86_64/ubuntu16/Dockerfile) like a recipe card to determine the software dependencies
that must be installed on the system, plus a few configuration steps.

:pencil:
Not on x86? We also have Dockerfiles for the following Linux architectures: [Linux on Power systems](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk11/ppc64le/ubuntu16/Dockerfile) and [Linux on z Systems](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk11/s390x/ubuntu16/Dockerfile).

1. Install the list of dependencies that can be obtained with the `apt-get` command from the following section of the Dockerfile:
```
apt-get update \
  && apt-get install -qq -y --no-install-recommends \
    gcc-7 \
    g++-7 \
    autoconf \
    ca-certificates \
    ...
```

2. This build uses the same gcc and g++ compiler levels as OpenJDK, which might be
backlevel compared with the versions you use on your system. Create links for
the compilers with the following commands:
```
ln -s g++ /usr/bin/c++
ln -s g++-7 /usr/bin/g++
ln -s gcc /usr/bin/cc
ln -s gcc-7 /usr/bin/gcc
```

3. Download and setup **freemarker.jar** into a directory.
```
cd /<my_home_dir>
wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz
tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2
rm -f freemarker.tgz
```

4. Download and setup the boot JDK using the latest AdoptOpenJDK v9 build.
```
cd /<my_home_dir>
wget -O bootjdk10.tar.gz "https://api.adoptopenjdk.net/v2/binary/releases/openjdk10?openjdk_impl=openj9&os=linux&arch=x64&release=latest&type=jdk"
tar -xzf bootjdk10.tar.gz
rm -f bootjdk10.tar.gz
mv $(ls | grep -i jdk) bootjdk10

export JAVA_HOME="/<my_home_dir>/bootjdk10"
export PATH="${JAVA_HOME}/bin:${PATH}"
```

### 2. Get the source
:penguin:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches. Run the following command:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk11.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk11
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:
```
bash get_source.sh
```

### 3. Configure
:penguin:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure --with-freemarker-jar=/<my_home_dir>/freemarker.jar
```
:warning: You must give an absolute path to freemarker.jar

:pencil: If you require a heap size greater than 57GB, enable a noncompressedrefs build with the `--with-noncompressedrefs` option during this step.

### 4. Build
:penguin:
Now you're ready to build **OpenJDK V11** with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will fail, because the default `make` target is `exploded-image`. If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script. For more information, read this [issue](https://github.com/ibmruntimes/openj9-openjdk-jdk9/issues/34).

A binary for the full developer kit (jdk) is built and stored in the following directory:

- **build/linux-x86_64-normal-server-release/images/jdk**

    :whale: If you built your binaries in a Docker container, copy the binaries to the containers **/root/hostdir** directory so that you can access them on your local system. You'll find them in the directory you set for `<host_directory>` when you started your Docker container. See [Setting up your build environment with Docker](#setting-up-your-build-environment-with-docker).

    :pencil: On other architectures the **/jdk** directory is in **build/linux-ppc64le-normal-server-release/images** (Linux on 64-bit Power systems) and **build/linux-s390x-normal-server-release/images** (Linux on 64-bit z Systems).

    :pencil: If you want a binary for the runtime environment (jre), you must run `make legacy-jre-image`, which produces a jre build in the **build/linux-x86_64-normal-server-release/images/jre** directory.

### 5. Test
:penguin:
For a simple test, try running the `java -version` command.
Change to the /jdk directory:
```
cd build/linux-x86_64-normal-server-release/images/jdk
```
Run:
```
./bin/java -version
```

Here is some sample output:

```
openjdk version "11-internal" 2018-09-25
OpenJDK Runtime Environment (build 11-internal+0-adhoc..jdk11)
Eclipse OpenJ9 VM (build master-ee517c1, JRE 11 Linux amd64-64-Bit Compressed References 20180910_000000 (JIT enabled, AOT enabled)
OpenJ9   - ee517c1
OMR      - f29d158
JCL      - 98f2038 based on jdk-11+28)
```
:penguin: *Congratulations!* :tada:

----------------------------------

## AIX
:blue_book:

:construction:
This section is still under construction. Further contributions expected.

The following instructions guide you through the process of building an **OpenJDK V11** binary that contains Eclipse OpenJ9 on AIX 7.2.

### 1. Prepare your system
:blue_book:
You must install the following AIX Licensed Program Products (LPPs):
- [xlc/C++ 13.1.3](https://www.ibm.com/developerworks/downloads/r/xlcplusaix/)
- x11.adt.ext

You must also install the boot JDK: [Java10_AIX_PPC64](https://adoptopenjdk.net/releases.html?variant=openjdk10&jvmVariant=openj9#ppc64_aix).

A number of RPM packages are also required. The easiest method for installing these packages is to use `yum`, because `yum` takes care of any additional dependent packages for you.

Download the following file: [yum_install_aix-ppc64.txt](aix/jdk10/yum_install_aix-ppc64.txt)

This file contains a list of required RPM packages that you can install by specifying the following command:
```
yum shell yum_install_aix-ppc64.txt
```

It is important to take the list of package dependencies from this file because it is kept right up to date by our developers.

Download and setup freemarker.jar into your home directory by running the following commands:

```
cd /<my_home_dir>
wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz
tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2
rm -f freemarker.tgz
```

### 2. Get the source
:blue_book:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches. Run the following command:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk11.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk11
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:

```
bash get_source.sh
```
### 3. Configure
:blue_book:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure --with-freemarker-jar=/<my_home_dir>/freemarker.jar \
               --with-cups-include=<cups_include_path> \
               --disable-warnings-as-errors
```
where `<my_home_dir>` is the location where you stored **freemarker.jar** and `<cups_include_path>` is the absolute path to CUPS. For example `/opt/freeware/include`.

:pencil: If you require a heap size greater than 57GB, enable a noncompressedrefs build with the `--with-noncompressedrefs` option during this step.

### 4. build
:blue_book:
Now you're ready to build OpenJDK with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will fail, because the default `make` target is `exploded-image`. If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script. For more information, read this [issue](https://github.com/ibmruntimes/openj9-openjdk-jdk9/issues/34).

A binary for the full developer kit (jdk) is built and stored in the following directory:

- **build/aix-ppc64-normal-server-release/images/jdk**

    :pencil: If you want a binary for the runtime environment (jre), you must run `make legacy-jre-image`, which produces a jre build in the **build/aix-ppc64-normal-server-release/images/jre** directory.

### 5. Test
:blue_book:
For a simple test, try running the `java -version` command.
Change to the /jdk directory:
```
cd build/aix-ppc64-normal-server-release/images/jdk
```
Run:
```
./bin/java -version
```

Here is some sample output:

```
openjdk version "11-internal" 2018-09-25
OpenJDK Runtime Environment (build 11-internal+0-adhoc..openj9-openjdk-jdk11)
Eclipse OpenJ9 VM (build master-06905e2, JRE 11 AIX ppc64-64 Compressed References 20180726_000000 (JIT enabled, AOT enabled)
OpenJ9   - 06905e2
OMR      - 28139f2
JCL      - e5c64f5 based on jdk-11+21)
```
:blue_book: *Congratulations!* :tada:

----------------------------------

## Windows
:ledger:

The following instructions guide you through the process of building a Windows **OpenJDK V11** binary that contains Eclipse OpenJ9. This process can be used to build binaries for Windows 7, 8, and 10.

### 1. Prepare your system
:ledger:
You must install a number of software dependencies to create a suitable build environment on your system:

- [Cygwin](https://cygwin.com/install.html), which provides a Unix-style command line interface. Install all packages in the `Devel` category. Included in this package is mingw, a minimalist subset of GNU tools for Microsoft Windows. OpenJ9 uses the mingw/GCC compiler and is tested with version 6.4.0 of same. Other versions are expected to work also. In the `Archive` category, install the packages `zip` and `unzip`. In the `Utils` category, install the `cpio` package. Install any further package dependencies that are identified by the installer. More information about using Cygwin can be found [here](https://cygwin.com/docs.html).
- [Windows JDK 10](https://adoptopenjdk.net/releases.html?variant=openjdk10#x64_win), which is used as the boot JDK.
- [Microsoft Visual Studio 2017]( https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&rel=15), which is the default compiler level used by OpenJDK11; or [Microsoft Visual Studio 2013]( https://go.microsoft.com/fwlink/?LinkId=532495), which is chosen to compile if VS2017 isn't installed.
- [Freemarker V2.3.8](https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download)
- [mingw-w64](https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/installer/mingw-w64-install.exe)

Note:
mingw-w64 must be installed if compiling with VS2017 in that the current version of cygwin has not yet integrated the latest changes of mingw-w64 for the moment. Meanwhile, add the binary path of mingw-w64 to the `PATH` environment variable. e.g.
```
export PATH="/cygdrive/c/mingw-w64/x86_64-8.1.0-win32-seh-rt_v6-rev0/mingw64/bin/:$PATH" (in Cygwin)
```

Update your `LIB` and `INCLUDE` environment variables to provide a path to the Windows debugging tools with the following commands:

```
set INCLUDE=C:\Program Files\Debugging Tools for Windows (x64)\sdk\inc;%INCLUDE%
set LIB=C:\Program Files\Debugging Tools for Windows (x64)\sdk\lib;%LIB%
```

   You can download Visual Studio, Freemarker manually or obtain them using the [wget](http://www.gnu.org/software/wget/faq.html#download) utility. If you choose to use `wget`, follow these steps:

- Open a cygwin terminal and change to the `/temp` directory:
```
cd /cygdrive/c/temp
```

- Run the following commands:
```
wget https://go.microsoft.com/fwlink/?LinkId=532495 -O vs2013.exe
wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz
```
- Before installing Visual Studio, change the permissions on the installation file by running `chmod u+x vs2013.exe`.
- Install Visual Studio by running the file `vs2013.exe` (There is no special step required for downloading/installing VS2017. Please follow the guide of the downloaded installer to install all required components, especially for VC compiler).

Not all of the shared libraries that are included with Visual Studio are registered during installation.
In particular, the `msdia120.dll`(VS2013) or `msdia140.dll`(VS2017) libraries must be registered manually.
To do so, execute the following from a command prompt:

**VS2013**
```
regsvr32 "C:\Program Files (x86)\Microsoft Visual Studio 12.0\DIA SDK\bin\msdia120.dll"
regsvr32 "C:\Program Files (x86)\Microsoft Visual Studio 12.0\DIA SDK\bin\amd64\msdia120.dll"
```
**VS2017**
```
regsvr32 "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\DIA SDK\bin\msdia140.dll"
regsvr32 "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\DIA SDK\bin\amd64\msdia140.dll"
```

- To unpack the Freemarker and Freetype compressed files, run:
```
tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2
```

### 2. Get the source
:ledger:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches.

Run the following command in the Cygwin terminal:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk11.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk11
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:

```
bash get_source.sh
```
### 3. Configure
:ledger:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure --disable-warnings-as-errors \
               --with-toolchain-version=2013 or 2017 \
               --with-freemarker-jar=/cygdrive/c/temp/freemarker.jar
```
Note: there is no need to specify --with-toolchain-version for 2017 as it will be located automatically

:pencil: Modify the paths for freemarker if you manually downloaded and unpacked these dependencies into different directories. If Java 10 is not available on the path, add the `--with-boot-jdk=<path_to_jdk10>` configuration option.

:pencil: If you require a heap size greater than 57GB, enable a noncompressedrefs build with the `--with-noncompressedrefs` option during this step.

### 4. build
:ledger:
Now you're ready to build OpenJDK with OpenJ9:
```
make all
```

A binary for the full developer kit (jdk) is built and stored in the following directory:

- **build/windows-x86_64-normal-server-release/images/jdk**

    :pencil: If you want a binary for the runtime environment (jre), you must run `make legacy-jre-image`, which produces a jre build in the **build/windows-x86_64-normal-server-release/images/jre** directory.

### 5. Test
:ledger:
For a simple test, try running the `java -version` command.
Change to the /jdk directory:
```
cd build/windows-x86_64-normal-server-release/images/jdk
```
Run:
```
./bin/java -version
```

Here is some sample output:

```
openjdk version "11-internal" 2018-09-25
OpenJDK Runtime Environment (build 11-internal+0-adhoc.Administrator.openj9-openjdk-jdk11)
Eclipse OpenJ9 VM (build master-11410ac2, JRE 11 Windows 7 amd64-64-Bit Compressed References 20180724_000000 (JIT enabled, AOT enabled)
OpenJ9   - 11410ac2
OMR      - e2e4b67c
JCL      - a786f96b13 based on jdk-11+21)
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
