<!--
Copyright IBM Corp. and others 2017

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
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

Building OpenJDK Version 8 with OpenJ9
======================================

Building OpenJDK 8 with OpenJ9 will be familiar to anyone who has already built OpenJDK. The easiest method
involves the use of Docker and Dockerfiles to create a build environment that contains everything
you need to produce a Linux binary of OpenJDK V8 with the Eclipse OpenJ9 virtual machine. If this method
sounds ideal for you, go straight to the [Linux :penguin:](#linux) section.

Build instructions are available for the following platforms:

- [Linux :penguin:](#linux)
- [AIX :blue_book:](#aix)
- [Windows :ledger:](#windows)
- [macOS :apple:](#macOS)
- [AArch64](#aarch64)

User documentation for the latest release of Eclipse OpenJ9 is available at the [Eclipse Foundation](https://www.eclipse.org/openj9/docs).
If you build a binary from the current OpenJ9 source, new features and changes might be in place for the next release of OpenJ9. Draft user
documentation for the next release of OpenJ9 can be found [here](https://eclipse-openj9.github.io/openj9-docs/).

----------------------------------

## Linux
:penguin:
This build process provides detailed instructions for building a Linux x86-64 binary of OpenJDK V8 with OpenJ9 on Ubuntu 16.04. The binary can be built directly on your system, in a virtual
machine, or in a Docker container :whale:.

If you are using a different Linux distribution, you might have to review the list of libraries that are bundled with your distribution and/or modify the instructions to use equivalent commands to the Advanced Packaging Tool (APT). For example, for Centos, substitute the `apt-get` command with `yum`.

If you want to build a binary for Linux on a different architecture, such as Power Systems&trade; or z Systems&trade;, the process is very similar and any additional information for those architectures are included as Notes :pencil: as we go along.
See also [AArch64 section](#aarch64) for building for AArch64 Linux.

### 1. Prepare your system
:penguin:
Instructions are provided for preparing your system with and without the use of Docker technology.

Skip to [Setting up your build environment without Docker](#setting-up-your-build-environment-without-docker).

#### Setting up your build environment with Docker :whale:
If you want to build a binary by using a Docker container, follow these steps to prepare your system:

1. The first thing you need to do is install Docker. You can download the free Community edition from [here](https://docs.docker.com/engine/installation/), which also contains instructions for installing Docker on your system.  You should also read the [Getting started](https://docs.docker.com/get-started/) guide to familiarise yourself with the basic Docker concepts and terminology.

2. Obtain the [docker build script](https://github.com/eclipse-openj9/openj9/blob/master/buildenv/docker/mkdocker.sh) to build and run a container that has all the correct software pre-requisites.

Download the docker build script to your local system or copy and paste the following command:

```
wget https://raw.githubusercontent.com/eclipse-openj9/openj9/master/buildenv/docker/mkdocker.sh
```

3. Next, run the following command to build a Docker image, called **openj9**:
```
bash mkdocker.sh --tag=openj9 --dist=ubuntu --version=16.04 --gitcache=no --jdk=8 --build
```

4. Start a Docker container from the **openj9** image with the following command, where `-v` maps any directory, `<host_directory>`,
on your local system to the containers `/root/hostdir` directory so that you can store the binaries, once they are built:
```
docker run -v <host_directory>:/root/hostdir -it openj9
```

:pencil: Depending on your [Docker system configuration](https://docs.docker.com/engine/reference/commandline/cli/#description), you might need to prefix the `docker` commands with `sudo`.

Now that you have the Docker image running, you are ready to move to the next step, [Get the source](#2-get-the-source).

#### Setting up your build environment without Docker

If you don't want to user Docker, you can still build directly on your Ubuntu system or in a Ubuntu virtual machine. Use the output of the following command like a recipe card to determine the software dependencies that must be installed on the system, plus a few configuration steps.

```
bash mkdocker.sh --tag=openj9 --dist=ubuntu --version=16.04 --gitcache=no --jdk=8 --print
```

1. Install the list of dependencies that can be obtained with the `apt-get` command from the following section of the Dockerfile:

```
apt-get update \
 && apt-get install -qq -y --no-install-recommends \
   ...
```

2. The previous step installed g++-7 and gcc-7 packages, which might be different
than the default version installed on your system. Export variables to set the
version used in the build.
```
export CC=gcc-7 CXX=g++-7
```

3. Only when building with `--with-cmake=no`, download and setup `freemarker.jar` into a directory. The example commands use `/root` to be consistent with the Docker instructions. If you aren't using Docker, you probably want to store the `freemarker.jar` in your home directory.
```
cd /root
wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz
tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip-components=2
rm -f freemarker.tgz
```

4. Download and setup the boot JDK using the latest AdoptOpenJDK v8 build.
```
cd <my_home_dir>
wget -O bootjdk8.tar.gz "https://api.adoptopenjdk.net/v3/binary/latest/8/ga/linux/x64/jdk/openj9/normal/adoptopenjdk"
tar -xzf bootjdk8.tar.gz
rm -f bootjdk8.tar.gz
mv $(ls | grep -i jdk8) bootjdk8
```

### 2. Get the source
:penguin:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches. Run the following command:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk8.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk8
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:
```
bash get_source.sh
```

:pencil: **OpenSSL support:** If you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v1.1.x available locally, you must specify `--openssl-version=<version>` where `<version>` is an OpenSSL level like 1.1.0 or 1.1.1. If the specified version of OpenSSL is already available in the standard location (SRC_DIR/openssl), `get_source.sh` uses it. Otherwise, the script deletes the content and downloads the specified version of OpenSSL source to the standard location and builds it. If you already have the version of OpenSSL in the standard location but you want a fresh copy, you must delete your current copy.

### 3. Configure
:penguin:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure --with-boot-jdk=/home/jenkins/bootjdks/jdk8
```
:warning: The path in the example --with-boot-jdk= option is appropriate for the Docker installation. If not using the Docker environment, set the path appropriate for your setup, such as "<my_home_dir>/bootjdk8" as setup in the previous instructions.

:pencil: Configuring and building is not specific to OpenJ9 but uses the OpenJDK build infrastructure with OpenJ9 added.
Many other configuration options are available, including options to increase the verbosity of the build output to include command lines (`LOG=cmdlines`), more info or debug information.
For more information see [OpenJDK build troubleshooting](https://htmlpreview.github.io/?https://raw.githubusercontent.com/openjdk/jdk8u/master/README-builds.html#troubleshooting).

:pencil: **Mixed and compressed references support:** Different types of 64-bit builds can be created:
- [compressed references](https://www.eclipse.org/openj9/docs/gc_overview/#compressed-references) (only)
- non-compressed references (only)
- mixed references, either compressed or non-compressed references is selected when starting Java

Mixed references is the default to build when no options are specified. _Note that `--with-cmake=no` cannot be used to build mixed references._ `configure` options include:
- `--with-mixedrefs` create a mixed references static build (equivalent to `--with-mixedrefs=static`)
- `--with-mixedrefs=no` create a build supporting compressed references only
- `--with-mixedrefs=dynamic` create a mixed references build that uses runtime checks
- `--with-mixedrefs=static` (this is the default) create a mixed references build which avoids runtime checks by compiling source twice
- `--with-noncompressedrefs` create a build supporting non-compressed references only

:pencil: **OpenSSL support:** If you want to build an OpenJDK that includes OpenSSL, you must specify `--with-openssl={fetched|system|path_to_library}`

  where:

  - `fetched` uses the OpenSSL source downloaded by `get-source.sh` in step **2. Get the source**.
  - `system` uses the package installed OpenSSL library in the system.
  - `path_to_library` uses a custom OpenSSL library that's already built.

  If you want to include the OpenSSL cryptographic library in the OpenJDK binary, you must include `--enable-openssl-bundling`.

:pencil: When building using `--with-cmake=no`, you must specify `freemarker.jar` with an absolute path, such as `--with-freemarker-jar=/root/freemarker.jar`.

### 4. Build
:penguin:
Now you're ready to build OpenJDK V8 with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

Two Java builds are produced: a full developer kit (jdk) and a runtime environment (jre)
- **build/linux-x86_64-normal-server-release/images/j2sdk-image**
- **build/linux-x86_64-normal-server-release/images/j2re-image**

    :whale: If you built your binaries in a Docker container, copy the binaries to the containers **/root/hostdir** directory so that you can access them on your local system. You'll find them in the directory you set for `<host_directory>` when you started your Docker container. See [Setting up your build environment with Docker](#setting-up-your-build-environment-with-docker).

    :pencil: On other architectures the **/j2sdk-image** and **/j2re-image** directories are in **build/linux-ppc64le-normal-server-release/images** (Linux on 64-bit Power systems) and **build/linux-s390x-normal-server-release/images** (Linux on 64-bit z Systems)

:pencil: One of the images created with `make all` is the `debug-image`. This directory contains files that provide debug information for executables and shared libraries when using native debuggers.
To use it, copy the contents of `debug-image` over the jdk `jre` directory before using the jdk with a native debugger.
Another image created is the `test` image, which contains executables and native libraries required when running some functional and OpenJDK testing.
For local testing set the NATIVE_TEST_LIBS environment variable to the test image location, see the [OpenJ9 test user guide](https://github.com/eclipse-openj9/openj9/blob/master/test/docs/OpenJ9TestUserGuide.md).

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

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.1.x support, the following acknowledgements apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:penguin: *Congratulations!* :tada:

----------------------------------

## AIX
:blue_book:

The following instructions guide you through the process of building an OpenJDK V8 binary that contains Eclipse OpenJ9 on AIX 7.2.

### 1. Prepare your system
:blue_book:
You must install the following AIX Licensed Program Products (LPPs):

- [Java7_64](https://developer.ibm.com/javasdk/support/aix-download-service/)
- [xlc/C++ 13.1.3](https://www.ibm.com/developerworks/downloads/r/xlcplusaix/)
- x11.adt.ext

A number of RPM packages are also required. The easiest method for installing these packages is to use `yum`, because `yum` takes care of any additional dependent packages for you.

Download the following file: [yum_install_aix-ppc64.txt](../../buildenv/aix/jdk8/yum_install_aix-ppc64.txt)

This file contains a list of required RPM packages that you can install by specifying the following command:
```
yum shell yum_install_aix-ppc64.txt
```

It is important to take the list of package dependencies from this file because it is kept right up to date by our developers.

Only when building with `--with-cmake=no`, download and setup `freemarker.jar` into your home directory by running the following commands:

```
cd <my_home_dir>
wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz
tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip-components=2
rm -f freemarker.tgz
```

### 2. Get the source
:blue_book:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches. Run the following command:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk8.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk8
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:

```
bash get_source.sh
```
:pencil: **OpenSSL support:** If you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v1.1.x available locally, you must specify `--openssl-version=<version>` where `<version>` is an OpenSSL level like 1.1.0 or 1.1.1. If the specified version of OpenSSL is already available in the standard location (SRC_DIR/openssl), `get_source.sh` uses it. Otherwise, the script deletes the content and downloads the specified version of OpenSSL source to the standard location and builds it. If you already have the version of OpenSSL in the standard location but you want a fresh copy, you must delete your current copy.

### 3. Configure
:blue_book:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure --with-cups-include=<cups_include_path>
```
where `<cups_include_path>` is the absolute path to CUPS. For example, `/opt/freeware/include`.

:pencil: Configuring and building is not specific to OpenJ9 but uses the OpenJDK build infrastructure with OpenJ9 added.
Many other configuration options are available, including options to increase the verbosity of the build output to include command lines (`LOG=cmdlines`), more info or debug information.
For more information see [OpenJDK build troubleshooting](https://htmlpreview.github.io/?https://raw.githubusercontent.com/openjdk/jdk8u/master/README-builds.html#troubleshooting).

:pencil: **Mixed and compressed references support:** Different types of 64-bit builds can be created:
- [compressed references](https://www.eclipse.org/openj9/docs/gc_overview/#compressed-references) (only)
- non-compressed references (only)
- mixed references, either compressed or non-compressed references is selected when starting Java

Mixed references is the default to build when no options are specified. _Note that `--with-cmake=no` cannot be used to build mixed references._ `configure` options include:
- `--with-mixedrefs` create a mixed references static build (equivalent to `--with-mixedrefs=static`)
- `--with-mixedrefs=no` create a build supporting compressed references only
- `--with-mixedrefs=dynamic` create a mixed references build that uses runtime checks
- `--with-mixedrefs=static` (this is the default) create a mixed references build which avoids runtime checks by compiling source twice
- `--with-noncompressedrefs` create a build supporting non-compressed references only

:pencil: **OpenSSL support:** If you want to build an OpenJDK that includes OpenSSL, you must specify `--with-openssl={fetched|system|path_to_library}`

  where:

  - `fetched` uses the OpenSSL source downloaded by `get-source.sh` in step **2. Get the source**.
  - `system` uses the package installed OpenSSL library in the system.
  - `path_to_library` uses a custom OpenSSL library that's already built.

    If you want to include the OpenSSL cryptographic library in the OpenJDK binary, you must include `--enable-openssl-bundling`.

:pencil: When building using `--with-cmake=no`, you must specify `freemarker.jar` with an absolute path, such as `--with-freemarker-jar=<my_home_dir>/freemarker.jar`, where `<my_home_dir>` is the location where you stored `freemarker.jar`.

### 4. build
:blue_book:
Now you're ready to build OpenJDK with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

Two Java builds are produced: a full developer kit (jdk) and a runtime environment (jre)
- **build/aix-ppc64-normal-server-release/images/j2sdk-image**
- **build/aix-ppc64-normal-server-release/images/j2re-image**

:pencil: One of the images created with `make all` is the `debug-image`. This directory contains files that provide debug information for executables and shared libraries when using native debuggers.
To use it, copy the contents of `debug-image` over the jdk `jre` directory before using the jdk with a native debugger.
Another image created is the `test` image, which contains executables and native libraries required when running some functional and OpenJDK testing.
For local testing set the NATIVE_TEST_LIBS environment variable to the test image location, see the [OpenJ9 test user guide](https://github.com/eclipse-openj9/openj9/blob/master/test/docs/OpenJ9TestUserGuide.md).

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

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.1.x support, the following acknowledgements apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:blue_book: *Congratulations!* :tada:

----------------------------------

## Windows
:ledger:
The following instructions guide you through the process of building a Windows 64-bit OpenJDK V8 binary that contains Eclipse OpenJ9. This process can be used to build binaries for Windows.

### 1. Prepare your system
:ledger:
You must install a number of software dependencies to create a suitable build environment on your system:

- [Cygwin for 64-bit versions of Windows](https://cygwin.com/install.html), which provides a Unix-style command line interface. Install all packages in the `Devel` category. In the `Archive` category, install the packages `zip` and `unzip`. In the `Utils` category, install the `cpio` package. Install any further package dependencies that are identified by the installer. More information about using Cygwin can be found [here](https://cygwin.com/docs.html).
- [Windows JDK 8](https://api.adoptopenjdk.net/v3/binary/latest/8/ga/windows/x64/jdk/openj9/normal/adoptopenjdk), which is used as the boot JDK.
- [Windows SDK 7 Debugging tools](https://www.microsoft.com/download/confirmation.aspx?id=8279).
- [Microsoft Visual Studio 2013 Professional](https://www.visualstudio.com/vs/older-downloads/) OR
- [Microsoft Visual Studio 2017](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&rel=15), the OpenJ9 project is using this level.
- [Freemarker V2.3.8](https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download) - only when building with `--with-cmake=no`
- [Freetype2 V2.5.3](https://www.freetype.org/)
- [LLVM/Clang 64bit](http://releases.llvm.org/7.0.0/LLVM-7.0.0-win64.exe) or [LLVM/Clang 32bit](http://releases.llvm.org/7.0.0/LLVM-7.0.0-win32.exe)
- [NASM Assembler v2.13.03 or newer](https://www.nasm.us/pub/nasm/releasebuilds/?C=M;O=D)

Add the binary path of Clang to the `PATH` environment variable to override the older version of clang integrated in Cygwin. e.g.
```
export PATH="/cygdrive/c/Program Files/LLVM/bin:$PATH" (in Cygwin for 64bit)
or
export PATH="/cygdrive/c/Program Files/LLVM_32/bin:$PATH" (in Cygwin for 32bit)
```

Add the path to `nasm.exe` to the `PATH` environment variable to override the older version of NASM installed in Cygwin. e.g.
```
export PATH="/cygdrive/c/Program Files/NASM:$PATH" (in Cygwin)
```

Update your `LIB` and `INCLUDE` environment variables to provide a path to the Windows debugging tools with the following commands:

```
set INCLUDE=C:\Program Files\Debugging Tools for Windows (x64)\sdk\inc;%INCLUDE%
set LIB=C:\Program Files\Debugging Tools for Windows (x64)\sdk\lib;%LIB%
```

Not all of the shared libraries that are included with Visual Studio 2013 or 2017 are registered during installation.
In particular, the `msdia120.dll` or `msdia140.dll` libraries must be registered manually by running command prompt as administrator.  To do so, execute the following from a command prompt:

VS2013:
```
regsvr32 "C:\Program Files (x86)\Microsoft Visual Studio 12.0\DIA SDK\bin\msdia120.dll"
regsvr32 "C:\Program Files (x86)\Microsoft Visual Studio 12.0\DIA SDK\bin\amd64\msdia120.dll"
```
VS2017:
```
regsvr32 "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\DIA SDK\bin\msdia140.dll"
regsvr32 "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\DIA SDK\bin\amd64\msdia140.dll"
```

You can download Freetype manually or obtain it using the [wget](http://www.gnu.org/software/wget/faq.html#download) utility. If you choose to use `wget`, follow these steps:

- Open a cygwin64 terminal and change to the `/temp` directory:
```
cd /cygdrive/c/temp
```

- Run the following command:
```
wget http://download.savannah.gnu.org/releases/freetype/freetype-2.5.3.tar.gz
```

- To unpack the Freetype archive, run:
```
tar --one-top-level=/cygdrive/c/temp/freetype --strip-components=1 -xzf freetype-2.5.3.tar.gz
```

- When building with `--with-cmake=no`, unpack the Freemarker archive:
```
tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip-components=2
```

### 2. Get the source
:ledger:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches.

Run the following command in the Cygwin terminal:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk8.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk8
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:

```
bash get_source.sh
```
:pencil: Do not check out the source code in a path which contains spaces or has a long name or is nested many levels deep.

:pencil: Create the directory that is going to contain the OpenJDK clone by using the `mkdir` command in the Cygwin bash shell and not using Windows Explorer. This ensures that it will have proper Cygwin attributes, and that its children will inherit those attributes.

:pencil: **OpenSSL support:** If you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v1.1.x available locally, you must specify `--openssl-version=<version>` where `<version>` is an OpenSSL level like 1.1.0 or 1.1.1. If the specified version of OpenSSL is already available in the standard location (SRC_DIR/openssl), `get_source.sh` uses it. Otherwise, the script deletes the content and downloads the specified version of OpenSSL source to the standard location and builds it. If you already have the version of OpenSSL in the standard location but you want a fresh copy, you must delete your current copy.

### 3. Configure
:ledger:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
1) Win 64-bit
```
bash configure --disable-ccache \
               --with-boot-jdk=/cygdrive/c/<path to_jdk8>
```

2) Win 32-bit
```
bash configure --disable-ccache \
               --with-boot-jdk=/cygdrive/c/<path_to_jdk8> \
               --with-target-bits=32
```
Note: If you have multiple versions of Visual Studio installed, you can enforce a specific version to be used by setting `--with-toolchain-version`, i.e., by including `--with-toolchain-version=2013` option in the configure command.

:pencil: Modify the path for freetype if you manually downloaded and unpacked this dependency into a different directory.

:pencil: Configuring and building is not specific to OpenJ9 but uses the OpenJDK build infrastructure with OpenJ9 added.
Many other configuration options are available, including options to increase the verbosity of the build output to include command lines (`LOG=cmdlines`), more info or debug information.
For more information see [OpenJDK build troubleshooting](https://htmlpreview.github.io/?https://raw.githubusercontent.com/openjdk/jdk8u/master/README-builds.html#troubleshooting).

:pencil: **Mixed and compressed references support:** Different types of 64-bit builds can be created:
- [compressed references](https://www.eclipse.org/openj9/docs/gc_overview/#compressed-references) (only)
- non-compressed references (only)
- mixed references, either compressed or non-compressed references is selected when starting Java

Mixed references is the default to build when no options are specified. _Note that `--with-cmake=no` cannot be used to build mixed references._ `configure` options include:
- `--with-mixedrefs` create a mixed references static build (equivalent to `--with-mixedrefs=static`)
- `--with-mixedrefs=no` create a build supporting compressed references only
- `--with-mixedrefs=dynamic` create a mixed references build that uses runtime checks
- `--with-mixedrefs=static` (this is the default) create a mixed references build which avoids runtime checks by compiling source twice
- `--with-noncompressedrefs` create a build supporting non-compressed references only

:pencil: **OpenSSL support:** If you want to build an OpenJDK that includes OpenSSL, you must specify `--with-openssl={fetched|system|path_to_library}`

  where:

  - `fetched` uses the OpenSSL source downloaded by `get-source.sh` in step **2. Get the source**.
  - `system` uses the package installed OpenSSL library in the system.
  - `path_to_library` uses a custom OpenSSL library that's already built.

  If you want to include the OpenSSL cryptographic library in the OpenJDK binary, you must include `--enable-openssl-bundling`.

:pencil: When building using `--with-cmake=no`, you must specify `freemarker.jar` with an absolute path, such as `--with-freemarker-jar=/cygdrive/c/temp/freemarker.jar`.

### 4. build
:ledger:
Now you're ready to build OpenJDK with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

Two Java builds are produced: a full developer kit (jdk) and a runtime environment (jre)
1) Win 64-bit
- **build/windows-x86_64-normal-server-release/images/j2sdk-image**
- **build/windows-x86_64-normal-server-release/images/j2re-image**

2) Win 32-bit
- **build/windows-x86-normal-server-release/images/j2sdk-image**
- **build/windows-x86-normal-server-release/images/j2re-image**

:pencil: One of the images created with `make all` is the `debug-image`. This directory contains files that provide debug information for executables and shared libraries when using native debuggers.
To use it, copy the contents of `debug-image` over the jdk `jre` directory before using the jdk with a native debugger.
Another image created is the `test` image, which contains executables and native libraries required when running some functional and OpenJDK testing.
For local testing set the NATIVE_TEST_LIBS environment variable to the test image location, see the [OpenJ9 test user guide](https://github.com/eclipse-openj9/openj9/blob/master/test/docs/OpenJ9TestUserGuide.md).

### 5. Test
:ledger:
For a simple test, try running the `java -version` command.
Change to the **/j2sdk-image** directory:

1) Win 64-bit
```
cd build/windows-x86_64-normal-server-release/images/j2sdk-image
```
Run:
```
./bin/java -version
```
Here is some sample output:
```
openjdk version "1.8.0_172-internal"
OpenJDK Runtime Environment (build 1.8.0_172-internal-administrator_2018_05_07_15_35-b00)
Eclipse OpenJ9 VM (build master-9329b7b9, JRE 1.8.0 Windows 7 amd64-64-Bit Compressed References 20180507_000000 (JIT enabled, AOT enabled)
OpenJ9   - 9329b7b9
OMR      - 884959f4
JCL      - 7f27c537a8 based on jdk8u172-b11)
```

2) Win 32-bit
```
cd build/windows-x86-normal-server-release/images/j2sdk-image
```
Run:
```
./jre/bin/java -version
```
Here is some sample output:
```
openjdk version "1.8.0_172-internal"
OpenJDK Runtime Environment (build 1.8.0_172-internal-administrator_2018_05_11_07_22-b00)
Eclipse OpenJ9 VM (build master-9f924a1a, JRE 1.8.0 Windows 7 x86-32-Bit 20180511_000000 (JIT enabled, AOT enabled)
OpenJ9   - 9f924a1a
OMR      - e5db96ba
JCL      - 7f27c537a8 based on jdk8u172-b11)
```

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.1.x support, the following acknowledgements apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:ledger: *Congratulations!* :tada:

----------------------------------

## macOS
:apple:
The following instructions guide you through the process of building a macOS **OpenJDK V8** binary that contains Eclipse OpenJ9. This process can be used to build binaries to run on macOS 10.9.0 or later.

### 1. Prepare your system
:apple:
You must install a number of software dependencies to create a suitable build environment on your system (the specified versions are minimums):

- [Xcode 10.3](https://developer.apple.com/download/more/) (requires an Apple account to log in).
- [macOS OpenJDK 8](https://api.adoptopenjdk.net/v3/binary/latest/8/ga/mac/x64/jdk/openj9/normal/adoptopenjdk), which is used as the boot JDK.

The following dependencies can be installed by using [Homebrew](https://brew.sh/):

- [autoconf 2.6.9](https://formulae.brew.sh/formula/autoconf)
- [bash 4.4.23](https://formulae.brew.sh/formula/bash)
- [binutils 2.32](https://formulae.brew.sh/formula/binutils)
- [cmake 3.4](https://formulae.brew.sh/formula/cmake)
- [freetype 2.9.1](https://formulae.brew.sh/formula/freetype)
- [git 2.19.2](https://formulae.brew.sh/formula/git)
- [gnu-tar 1.3](https://formulae.brew.sh/formula/gnu-tar)
- [nasm 2.13.03](https://formulae.brew.sh/formula/nasm)
- [pkg-config 0.29.2](https://formulae.brew.sh/formula/pkg-config)
- [wget 1.19.5](https://formulae.brew.sh/formula/wget)

Only when building with `--with-cmake=no`, [Freemarker V2.3.8](https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download) is also required, which can be obtained and installed with the following commands:

```
wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz
gtar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip-components=2
rm -f freemarker.tgz
```

Bash version 4 is required by the `./get_source.sh` script that you will use in step 2, which is installed to `/usr/local/bin/bash`. To prevent problems during the build process, make Bash v4 your default shell by typing the following commands:

```
# Find the <CURRENT_SHELL> for <USERNAME>
dscl . -read <USERNAME> UserShell

# Change the shell to Bash version 4 for <USERNAME>
dscl . -change <USERNAME> UserShell <CURRENT_SHELL> /usr/local/bin/bash

# Verify that the shell has been changed
dscl . -read <USERNAME> UserShell
```

### 2. Get the source
:apple:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches.

Run the following command:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk8.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk8
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:

```
bash get_source.sh
```

:pencil: **OpenSSL support:** If you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v1.1.x available locally, you must obtain a prebuilt OpenSSL v1.1.x binary.

### 3. Configure
:apple:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.

```
bash configure \
    TAR=gtar \
    --with-boot-jdk=<path_to_boot_JDK8> \
    --with-toolchain-type=clang
```

:pencil: Configuring and building is not specific to OpenJ9 but uses the OpenJDK build infrastructure with OpenJ9 added.
Many other configuration options are available, including options to increase the verbosity of the build output to include command lines (`LOG=cmdlines`), more info or debug information.
For more information see [OpenJDK build troubleshooting](https://htmlpreview.github.io/?https://raw.githubusercontent.com/openjdk/jdk8u/master/README-builds.html#troubleshooting).

:pencil: **Mixed and compressed references support:** Different types of 64-bit builds can be created:
- [compressed references](https://www.eclipse.org/openj9/docs/gc_overview/#compressed-references) (only)
- non-compressed references (only)
- mixed references, either compressed or non-compressed references is selected when starting Java

Mixed references is the default to build when no options are specified. _Note that `--with-cmake=no` cannot be used to build mixed references._ `configure` options include:
- `--with-mixedrefs` create a mixed references static build (equivalent to `--with-mixedrefs=static`)
- `--with-mixedrefs=no` create a build supporting compressed references only
- `--with-mixedrefs=dynamic` create a mixed references build that uses runtime checks
- `--with-mixedrefs=static` (this is the default) create a mixed references build which avoids runtime checks by compiling source twice
- `--with-noncompressedrefs` create a build supporting non-compressed references only

:pencil: **OpenSSL support:** If you want to build an OpenJDK that includes OpenSSL, you must specify `--with-openssl=path_to_library`, where `path_to_library` specifies the path to the prebuilt OpenSSL library that you obtained in **2. Get the source**. If you want to include the OpenSSL cryptographic library in the OpenJDK binary, you must also include `--enable-openssl-bundling`.

:pencil: When building using `--with-cmake=no`, you must specify `freemarker.jar` with an absolute path, such as `--with-freemarker-jar=<path_to>/freemarker.jar`, where `<path_to>` is the location where you stored `freemarker.jar`.

### 4. build
:apple:
Now you're ready to build OpenJDK with OpenJ9.

```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

:pencil: Because `make all` does not provide sufficient details for debugging, a more verbose build can be produced by running the command `make LOG=trace all 2>&1 | tee make_all.log`.

Four Java builds are produced, which include two full developer kits (jdk) and two runtime environments (jre):

- **build/macosx-x86_64-normal-server-release/images/j2sdk-image**
- **build/macosx-x86_64-normal-server-release/images/j2re-image**
- **build/macosx-x86_64-normal-server-release/images/j2sdk-bundle**
- **build/macosx-x86_64-normal-server-release/images/j2re-bundle**

:pencil:For running applications such as Eclipse, use the **-bundle** versions.

:pencil: One of the images created with `make all` is the `debug-image`. This directory contains files that provide debug information for executables and shared libraries when using native debuggers.
To use it, copy the contents of `debug-image` over the jdk `jre` directory before using the jdk with a native debugger.
Another image created is the `test` image, which contains executables and native libraries required when running some functional and OpenJDK testing.
For local testing set the NATIVE_TEST_LIBS environment variable to the test image location, see the [OpenJ9 test user guide](https://github.com/eclipse-openj9/openj9/blob/master/test/docs/OpenJ9TestUserGuide.md).

### 5. Test
:apple:
For a simple test, try running the `java -version` command. Change to the j2re-image directory:
```
cd build/macosx-x86_64-normal-server-release/images/j2re-image
```
Run:
```
./bin/java -version
```

Here is some sample output:
```
openjdk version "1.8.0_192-internal"
OpenJDK Runtime Environment (build 1.8.0_192-internal-jenkins_2018_10_17_11_24-b00)
Eclipse OpenJ9 VM (build master-2c817c52c, JRE 1.8.0 Mac OS X amd64-64-Bit Compressed References 20181017_000000 (JIT enabled, AOT enabled)
OpenJ9   - 2c817c52c
OMR      - 4d96857a
JCL      - fcd436bf56 based on jdk8u192-b03)
```

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.1.x support, the following acknowledgements apply in accordance with the license terms:

- *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/).*
- *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:ledger: *Congratulations!* :tada:

----------------------------------

## AArch64

:penguin:
The following instructions guide you through the process of building an OpenJDK V8 binary that contains Eclipse OpenJ9 for AArch64 (ARMv8 64-bit) Linux.

The binary can be built on your AArch64 Linux system.  Cross-building on x86-64 Linux is not supported yet.

### 1. Get the source
:penguin:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches. Run the following command:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk8.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk8
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:
```
bash get_source.sh
```

### 2. Prepare your system

You must install a number of software dependencies to create a suitable build environment on your AArch64 Linux system:

- GNU C/C++ compiler
- [AArch64 Linux JDK](https://api.adoptopenjdk.net/v3/binary/latest/8/ga/linux/aarch64/jdk/hotspot/normal/adoptopenjdk), which is used as the boot JDK.
- [Freemarker V2.3.8](https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download) - Only when building with `--with-cmake=no`

See [Setting up your build environment without Docker](#setting-up-your-build-environment-without-docker) in [Linux section](#linux) for other dependencies to be installed.

### 3. Configure
:penguin:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure --with-boot-jdk=<path_to_boot_JDK>
```
:pencil: Configuring and building is not specific to OpenJ9 but uses the OpenJDK build infrastructure with OpenJ9 added.
Many other configuration options are available, including options to increase the verbosity of the build output to include command lines (`LOG=cmdlines`), more info or debug information.
For more information see [OpenJDK build troubleshooting](https://htmlpreview.github.io/?https://raw.githubusercontent.com/openjdk/jdk8u/master/README-builds.html#troubleshooting).

:pencil: **Mixed and compressed references support:** Different types of 64-bit builds can be created:
- [compressed references](https://www.eclipse.org/openj9/docs/gc_overview/#compressed-references) (only)
- non-compressed references (only)
- mixed references, either compressed or non-compressed references is selected when starting Java

Mixed references is the default to build when no options are specified. _Note that `--with-cmake=no` cannot be used to build mixed references._ `configure` options include:
- `--with-mixedrefs` create a mixed references static build (equivalent to `--with-mixedrefs=static`)
- `--with-mixedrefs=no` create a build supporting compressed references only
- `--with-mixedrefs=dynamic` create a mixed references build that uses runtime checks
- `--with-mixedrefs=static` (this is the default) create a mixed references build which avoids runtime checks by compiling source twice
- `--with-noncompressedrefs` create a build supporting non-compressed references only

:pencil: **OpenSSL support:** If you want to build an OpenJDK that uses OpenSSL, you must specify `--with-openssl={system|path_to_library}`

  where:

  - `system` uses the package installed OpenSSL library in the system.
  - `path_to_library` uses a custom OpenSSL library that's already built.

  If you want to include the OpenSSL cryptographic library in the OpenJDK binary, you must include `--enable-openssl-bundling`.

:pencil: When building using `--with-cmake=no`, you must specify `freemarker.jar` with an absolute path, such as `--with-freemarker-jar=<path_to>/freemarker.jar`, where `<path_to>` is the location where you stored `freemarker.jar`.

### 6. Build
:penguin:
Now you're ready to build OpenJDK V8 with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

Two Java builds are produced: a full developer kit (jdk) and a runtime environment (jre):
- **build/linux-aarch64-normal-server-release/images/j2sdk-image**
- **build/linux-aarch64-normal-server-release/images/j2re-image**

:pencil: One of the images created with `make all` is the `debug-image`. This directory contains files that provide debug information for executables and shared libraries when using native debuggers.
To use it, copy the contents of `debug-image` over the jdk `jre` directory before using the jdk with a native debugger.
Another image created is the `test` image, which contains executables and native libraries required when running some functional and OpenJDK testing.
For local testing set the NATIVE_TEST_LIBS environment variable to the test image location, see the [OpenJ9 test user guide](https://github.com/eclipse-openj9/openj9/blob/master/test/docs/OpenJ9TestUserGuide.md).

### 6. Test
:penguin:
For a simple test, try running the `java -version` command.
Change to your **j2re-image** directory:
```
cd build/linux-aarch64-normal-server-release/images/j2re-image
```
Run:
```
bin/java -version
```

Here is some sample output:

```
openjdk version "1.8.0_265-internal"
OpenJDK Runtime Environment (build 1.8.0_265-internal-ubuntu_2020_07_28_13_28-b00)
Eclipse OpenJ9 VM (build master-e724f249c, JRE 1.8.0 Linux aarch64-64-Bit Compressed References 20200728_000000 (JIT enabled, AOT enabled)
OpenJ9   - e724f249c
OMR      - 8124c1385
JCL      - 28815f64 based on jdk8u265-b01)
```

:construction: AArch64 JIT compiler is not fully optimized at the time of writing this, compared with other platforms.

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.1.x support, the following acknowledgements apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:penguin: *Congratulations!* :tada:
