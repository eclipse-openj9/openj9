<!--
Copyright (c) 2018, 2020 IBM Corp. and others

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

Building OpenJDK 11 with OpenJ9 will be familiar to anyone who has already built OpenJDK. The easiest method
involves the use of Docker and Dockerfiles to create a build environment that contains everything
you need to produce a Linux binary of OpenJDK V11 with the Eclipse OpenJ9 virtual machine. If this method
sounds ideal for you, go straight to the [Linux :penguin:](#linux) section.

Build instructions are available for the following platforms:

- [Linux :penguin:](#linux)
- [AIX :blue_book:](#aix)
- [Windows :ledger:](#windows)
- [macOS :apple:](#macOS)
- [ARM :iphone:](#arm)
- [AArch64](#aarch64)
- [Riscv64 :rocket:](#riscv64)

User documentation for the latest release of Eclipse OpenJ9 is available at the [Eclipse Foundation](https://www.eclipse.org/openj9/docs).
If you build a binary from the current OpenJ9 source, new features and changes might be in place for the next release of OpenJ9. Draft user
documentation for the next release of OpenJ9 can be found [here](https://eclipse.github.io/openj9-docs/).

----------------------------------

## Linux
:penguin:
This build process provides detailed instructions for building a Linux x86-64 binary of **OpenJDK V11** with OpenJ9 on Ubuntu 16.04. The binary can be built directly on your system, in a virtual machine, or in a Docker container :whale:.

If you are using a different Linux distribution, you might have to review the list of libraries that are bundled with your distribution and/or modify the instructions to use equivalent commands to the Advanced Packaging Tool (APT). For example, for Centos, substitute the `apt-get` command with `yum`.

If you want to build a binary for Linux on a different architecture, such as Power Systems&trade; or z Systems&trade;, the process is very similar and any additional information for those architectures are included as Notes :pencil: as we go along.
See [AArch64 section](#aarch64) for building for AArch64 Linux.

### 1. Prepare your system
:penguin:
Instructions are provided for preparing your system with and without the use of Docker technology.

Skip to [Setting up your build environment without Docker](#setting-up-your-build-environment-without-docker).

#### Setting up your build environment with Docker :whale:
If you want to build a binary by using a Docker container, follow these steps to prepare your system:

1. The first thing you need to do is install Docker. You can download the free Community edition from [here](https://docs.docker.com/engine/installation/), which also contains instructions for installing Docker on your system.  You should also read the [Getting started](https://docs.docker.com/get-started/) guide to familiarise yourself with the basic Docker concepts and terminology.

2. Obtain the [docker build script](https://github.com/eclipse/openj9/blob/master/buildenv/docker/mkdocker.sh) to build and run a container that has all the correct software pre-requisites.

Download the docker build script to your local system or copy and paste the following command:```

```
wget https://raw.githubusercontent.com/eclipse/openj9/master/buildenv/docker/mkdocker.sh
```

3. Next, run the following command to build a Docker image, called **openj9**:
```
bash mkdocker.sh --tag=openj9 --dist=ubuntu --version=16.04 --build
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
bash mkdocker.sh --tag=openj9 --dist=ubuntu --version=16.04 --print
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

3. Download and setup **freemarker.jar** into a directory.
```
cd /<my_home_dir>
wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz
tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2
rm -f freemarker.tgz
```

4. Download and setup the boot JDK using the latest AdoptOpenJDK v11 build.
```
cd /<my_home_dir>
wget -O bootjdk11.tar.gz "https://api.adoptopenjdk.net/v3/binary/latest/11/ga/linux/x64/jdk/openj9/normal/adoptopenjdk"
tar -xzf bootjdk11.tar.gz
rm -f bootjdk11.tar.gz
mv $(ls | grep -i jdk-11) bootjdk11
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

:pencil: **OpenSSL support:** If you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v1.1.x available locally, you must specify `--openssl-version=<version>` where `<version>` is an OpenSSL level like 1.1.0 or 1.1.1. If the specified version of OpenSSL is already available in the standard location (SRC_DIR/openssl), `get_source.sh` uses it. Otherwise, the script deletes the content and downloads the specified version of OpenSSL source to the standard location and builds it. If you already have the version of OpenSSL in the standard location but you want a fresh copy, you must delete your current copy.

### 3. Configure
:penguin:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure --with-freemarker-jar=/<my_home_dir>/freemarker.jar --with-boot-jdk=/usr/lib/jvm/adoptojdk-java-11
```
:warning: You must give an absolute path to freemarker.jar

:warning: The path in the example --with-boot-jdk= option is appropriate for the Docker installation. If not using the Docker environment, set the path appropriate for your setup, such as "/<my_home_dir>/bootjdk11" as setup in the previous instructions.

:pencil: **Non-compressed references support:** If you require a heap size greater than 57GB, enable a noncompressedrefs build with the `--with-noncompressedrefs` option during this step.

:pencil: **OpenSSL support:** If you want to build an OpenJDK that includes OpenSSL, you must specify `--with-openssl={fetched|system|path_to_library}`

  where:

  - `fetched` uses the OpenSSL source downloaded by `get-source.sh` in step **2. Get the source**.
  - `system` uses the package installed OpenSSL library in the system.
  - `path_to_library` uses a custom OpenSSL library that's already built.

  If you want to include the OpenSSL cryptographic library in the OpenJDK binary, you must include `--enable-openssl-bundling`.

### 4. Build
:penguin:
Now you're ready to build **OpenJDK V11** with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

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

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.1.x support, the following acknowledgements apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

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

You must also install the boot JDK: [Java11_AIX_PPC64](https://api.adoptopenjdk.net/v3/binary/latest/11/ga/aix/ppc64/jdk/openj9/normal/adoptopenjdk).

A number of RPM packages are also required. The easiest method for installing these packages is to use `yum`, because `yum` takes care of any additional dependent packages for you.

Download the following file: [yum_install_aix-ppc64.txt](../../buildenv/aix/jdk11/yum_install_aix-ppc64.txt)

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

:pencil: **OpenSSL support:** If you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v1.1.x available locally, you must specify `--openssl-version=<version>` where `<version>` is an OpenSSL level like 1.1.0 or 1.1.1. If the specified version of OpenSSL is already available in the standard location (SRC_DIR/openssl), `get_source.sh` uses it. Otherwise, the script deletes the content and downloads the specified version of OpenSSL source to the standard location and builds it. If you already have the version of OpenSSL in the standard location but you want a fresh copy, you must delete your current copy.

### 3. Configure
:blue_book:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure --with-freemarker-jar=/<my_home_dir>/freemarker.jar \
               --with-cups-include=<cups_include_path> \
               --disable-warnings-as-errors
```
where `<my_home_dir>` is the location where you stored **freemarker.jar** and `<cups_include_path>` is the absolute path to CUPS. For example, `/opt/freeware/include`.

:pencil: **Non-compressed references support:** If you require a heap size greater than 57GB, enable a noncompressedrefs build with the `--with-noncompressedrefs` option during this step.

:pencil: **OpenSSL support:** If you want to build an OpenJDK that includes OpenSSL, you must specify `--with-openssl={fetched|system|path_to_library}`

where:

- `fetched` uses the OpenSSL source downloaded by `get-source.sh` in step **2. Get the source**.
- `system` uses the package installed OpenSSL library in the system.
- `path_to_library` uses a custom OpenSSL library that's already built.

  If you want to include the OpenSSL cryptographic library in the OpenJDK binary, you must include `--enable-openssl-bundling`.

### 4. build
:blue_book:
Now you're ready to build OpenJDK with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

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

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.1.x support, the following acknowledgements apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:blue_book: *Congratulations!* :tada:

----------------------------------

## Windows
:ledger:

The following instructions guide you through the process of building a Windows **OpenJDK V11** binary that contains Eclipse OpenJ9. This process can be used to build binaries for Windows.

### 1. Prepare your system
:ledger:
You must install a number of software dependencies to create a suitable build environment on your system:

- [Cygwin](https://cygwin.com/install.html), which provides a Unix-style command line interface. Install all packages in the `Devel` category. In the `Archive` category, install the packages `zip` and `unzip`. In the `Utils` category, install the `cpio` package. Install any further package dependencies that are identified by the installer. More information about using Cygwin can be found [here](https://cygwin.com/docs.html).
- [Windows JDK 11](https://api.adoptopenjdk.net/v3/binary/latest/11/ga/windows/x64/jdk/openj9/normal/adoptopenjdk), which is used as the boot JDK.
- [Microsoft Visual Studio 2017]( https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&rel=15), which is the default compiler level used by OpenJDK11; or [Microsoft Visual Studio 2013]( https://go.microsoft.com/fwlink/?LinkId=532495), which is chosen to compile if VS2017 isn't installed.
- [Freemarker V2.3.8](https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download)
- [LLVM/Clang](http://releases.llvm.org/7.0.0/LLVM-7.0.0-win64.exe)
- [NASM Assembler v2.13.03 or newer](https://www.nasm.us/pub/nasm/releasebuilds/?C=M;O=D)

Add the binary path of Clang to the `PATH` environment variable to override the older version of clang integrated in Cygwin. e.g.
```
export PATH="/cygdrive/c/LLVM/bin:$PATH" (in Cygwin)
```

Add the path to `nasm.exe` to the `PATH` environment variable to override the older version of NASM installed in Cygwin. e.g.
```
export PATH="/cygdrive/c/NASM:$PATH" (in Cygwin)
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

:pencil: **OpenSSL support:** If you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v1.1.x available locally, you must obtain a prebuilt OpenSSL v1.1.x binary.

### 3. Configure
:ledger:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure --disable-warnings-as-errors \
               --with-toolchain-version=2013 or 2017 \
               --with-freemarker-jar=/cygdrive/c/temp/freemarker.jar
```
Note: there is no need to specify --with-toolchain-version for 2017 as it will be located automatically

:pencil: Modify the paths for freemarker if you manually downloaded and unpacked these dependencies into different directories. If Java 11 is not available on the path, add the `--with-boot-jdk=<path_to_jdk11>` configuration option.

:pencil: **Non-compressed references support:** If you require a heap size greater than 57GB, enable a noncompressedrefs build with the `--with-noncompressedrefs` option during this step.

:pencil: **OpenSSL support:** If you want to build an OpenJDK that includes OpenSSL, you must specify `--with-openssl=path_to_library`, where `path_to_library` specifies the path to the prebuilt OpenSSL library that you obtained in **2. Get the source**. If you want to include the OpenSSL cryptographic library in the OpenJDK binary, you must also include `--enable-openssl-bundling`.

### 4. build
:ledger:
Now you're ready to build OpenJDK with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

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

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.1.x support, the following acknowledgements apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:ledger: *Congratulations!* :tada:

----------------------------------

## macOS
:apple:
The following instructions guide you through the process of building a macOS **OpenJDK V11** binary that contains Eclipse OpenJ9. This process can be used to build binaries for macOS 10.

### 1. Prepare your system
:apple:
You must install a number of software dependencies to create a suitable build environment on your system:

- [Xcode >= 11.4](https://developer.apple.com/download/more/) (requires an Apple account to log in).
- [macOS OpenJDK 11](https://api.adoptopenjdk.net/v3/binary/latest/11/ga/mac/x64/jdk/openj9/normal/adoptopenjdk), which is used as the boot JDK.

The following dependencies can be installed by using [Homebrew](https://brew.sh/) (the specified versions are minimums):

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

:pencil: **OpenSSL support:** If you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v1.1.x available locally, you must obtain a prebuilt OpenSSL v1.1.x binary.

### 3. Configure
:apple:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.

```
bash configure --with-boot-jdk=<path_to_boot_JDK11>
```

:pencil: Modify the path for the macOS boot JDK that you installed in step 1. If `configure` is unable to detect Freetype, add the option `--with-freetype=<path to freetype>`, where `<path to freetype>` is typically `/usr/local/Cellar/freetype/2.9.1/`.

:pencil: **Non-compressed references support:** If you require a heap size greater than 57GB, enable a noncompressedrefs build with the `--with-noncompressedrefs` option during this step.

:pencil: **OpenSSL support:** If you want to build an OpenJDK that includes OpenSSL, you must specify `--with-openssl=path_to_library`, where `path_to_library` specifies the path to the prebuilt OpenSSL library that you obtained in **2. Get the source**. If you want to include the OpenSSL cryptographic library in the OpenJDK binary, you must also include `--enable-openssl-bundling`.

### 4. build
:apple:
Now you're ready to build OpenJDK with OpenJ9:

```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

Two builds of OpenJDK with Eclipse OpenJ9 are built and stored in the following directories:

- **build/macosx-x86_64-normal-server-release/images/jdk**
- **build/macosx-x86_64-normal-server-release/images/jdk-bundle**

    :pencil: For running applications such as Eclipse, use the **-bundle** version.

    :pencil: If you want a binary for the runtime environment (jre), you must run `make legacy-jre-image`, which produces a jre build in the **build/macosx-x86_64-normal-server-release/images/jre** directory.

### 5. Test
:apple:
For a simple test, try running the `java -version` command.
Change to the /jdk directory:
```
cd build/macosx-x86_64-normal-server-release/images/jdk
```
Run:
```
./bin/java -version
```

Here is some sample output:

```
openjdk version "11-internal" 2018-09-25
OpenJDK Runtime Environment (build 11-internal+0-adhoc.heidinga.openj9-openjdk-jdk11)
Eclipse OpenJ9 VM (build djh/libjava-72338d7a1, JRE 11 Mac OS X amd64-64-Bit Compressed References 20181104_000000 (JIT enabled, AOT enabled)
OpenJ9   - 72338d7a1
OMR      - d4cd7c31
JCL      - 9da99f8b97 based on jdk-11+28)
```

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.1.x support, the following acknowledgements apply in accordance with the license terms:

- *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/).*
- *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:ledger: *Congratulations!* :tada:

----------------------------------

## ARM
:iphone:

:construction: We haven't created a full build process for ARM yet? Watch this space!

----------------------------------

## AArch64

:penguin:
The following instructions guide you through the process of building an **OpenJDK V11** binary that contains Eclipse OpenJ9 for AArch64 (ARMv8 64-bit) Linux.

### 1. Prepare your system

The binary can be built on your AArch64 Linux system, or in a Docker container :whale: on x86-64 Linux.

Note: Building on x86-64 without Docker is not tested.

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

### 3. Prepare for build on AArch64 Linux

You must install a number of software dependencies to create a suitable build environment on your AArch64 Linux system:

- GNU C/C++ compiler (The Docker image uses GCC 7.5)
- [AArch64 Linux JDK](https://api.adoptopenjdk.net/v3/binary/latest/11/ga/linux/aarch64/jdk/openj9/normal/adoptopenjdk), which is used as the boot JDK.
- [Freemarker V2.3.8](https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download)

See [Setting up your build environment without Docker](#setting-up-your-build-environment-without-docker) in [Linux section](#linux) for other dependencies to be installed.

### 4. Create the Docker image

If you build the binary on x86-64 Linux, run the following commands to build a Docker image for AArch64 cross-compilation, called **openj9aarch64**:
```
cd openj9/buildenv/docker/aarch64-linux_CC
docker build -t openj9aarch64 -f Dockerfile .
```

Start a Docker container from the **openj9aarch64** image with the following command, where `<host_directory>` is the directory that contains `openj9-openjdk-jdk11` in your local system:
```
docker run -v /<host_directory>/openj9-openjdk-jdk11:/root/openj9-openjdk-jdk11 -it openj9aarch64
```

Then go to the `openj9-openjdk-jdk11` directory:
```
cd /root/openj9-openjdk-jdk11
```

### 5. Configure
:penguin:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.

For building on AArch64 Linux:
```
bash configure --with-freemarker-jar=/<path_to>/freemarker.jar \
               --with-boot-jdk=/<path_to_boot_JDK> \
               --disable-warnings-as-errors
```

For building in the Docker container:
```
bash configure --openjdk-target=${OPENJ9_CC_PREFIX} \
               --with-x=${OPENJ9_CC_DIR}/${OPENJ9_CC_PREFIX}/ \
               --with-freetype-include=${OPENJ9_CC_DIR}/${OPENJ9_CC_PREFIX}/libc/usr/include/freetype2 \
               --with-freetype-lib=${OPENJ9_CC_DIR}/${OPENJ9_CC_PREFIX}/libc/usr/lib \
               --with-freemarker-jar=/root/freemarker.jar \
               --with-boot-jdk=/root/bootjdk11 \
               --with-build-jdk=/root/bootjdk11 \
               --with-cmake=no \
               --disable-warnings-as-errors \
               --disable-ddr
```

:pencil: **Non-compressed references support:** If you require a heap size greater than 57GB, enable a noncompressedrefs build with the `--with-noncompressedrefs` option during this step.

:pencil: **OpenSSL support:** If you want to build an OpenJDK that uses OpenSSL, you must specify `--with-openssl={system|path_to_library}`

  where:

  - `system` uses the package installed OpenSSL library in the system.  Use this option when you build on your AArch64 Linux system.
  - `path_to_library` uses an OpenSSL v1.1.x library that's already built.  You can use `${OPENJ9_CC_DIR}/${OPENJ9_CC_PREFIX}/libc/usr` as `path_to_library` when you are configuring in the Docker container.
  - Using `--with-openssl=fetched` will fail during the build in the Docker environment.

:pencil: **DDR support:** You can build DDR support only on AArch64 Linux.  If you are building in a cross-compilation environment, you need the `--disable-ddr` option.

:pencil: **CUDA support:** You can enable CUDA support if you are building on NVIDIA Jetson Developer Kit series.  Add `--enable-cuda --with-cuda=/usr/local/cuda` when you run `configure`.  The path `/usr/local/cuda` may be different depending on the version of JetPack.

:pencil: You may need to add `--disable-warnings-as-errors-openj9` depending on the toolchain version.

### 6. Build
:penguin:
Now you're ready to build OpenJDK with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

A binary for the full developer kit (jdk) is built and stored in the following directory:

- **build/linux-aarch64-normal-server-release/images/jdk**

Copy its contents to your AArch64 Linux device.

:pencil: If you want a binary for the runtime environment (jre), you must run `make legacy-jre-image`, which produces a jre build in the **build/linux-aarch64-normal-server-release/images/jre** directory.

### 6. Test
:penguin:
For a simple test, try running the `java -version` command.
Change to your jdk directory on AArch64 Linux:
```
cd jdk
```
Run:
```
bin/java -version
```

Here is some sample output:

```
openjdk version "11.0.6-internal" 2020-01-14
OpenJDK Runtime Environment (build 11.0.6-internal+0-adhoc..openj9-openjdk-jdk11)
Eclipse OpenJ9 VM (build master-83baf0b, JRE 11 Linux aarch64-64-Bit 20191204_000000 (JIT enabled, AOT enabled)
OpenJ9   - 83baf0b
OMR      - 7b2e5df
JCL      - d247952 based on jdk-11.0.6+6)
```

:construction: AArch64 JIT compiler is not fully optimized at the time of writing this, compared with other platforms.

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.1.x support, the following acknowledgements apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:penguin: *Congratulations!* :tada:

----------------------------------

## Riscv64
:rocket:
The following instructions guide you through the process of building an **OpenJDK V11** binary that contains Eclipse OpenJ9 for Riscv64 (RISC-V 64-bit) Linux.

:bulb:
It is assumed that the Ubuntu version is at least 16.04 (gcc & g++ >= 7) is the default host system for the cross-compilation.

### 1. Prepare your system for cross-compilation
:rocket:
A number of software packages/dependencies must be installed on the host system to create a suitable cross-compiling environment:

- [The GNU cross-toolchain](https://github.com/riscv/riscv-gnu-toolchain), which contains the source code of the C/C++ cross-compiler intended for RISC-V
- [bootJDK_OpenJ9](https://api.adoptopenjdk.net/v3/binary/latest/11/ga/linux/x64/jdk/openj9/normal/adoptopenjdk), which is only used to generate a build JDK for the cross-compilation on the host system
- [QEMU](https://www.qemu.org/download/#source), which is an open source emulator that converts the RISC-V instructions to the opcode on the host system
- [Fedora_Stage4](https://fedorapeople.org/groups/risc-v/disk-images/), which contains the 1st Fedora bootstrap image on RISC-V and the corresponding BBL (Berkeley Boot Loader) binary file
- [Fedora_Developer_Rawhide](https://dl.fedoraproject.org/pub/alt/risc-v/repo/virt-builder-images/images/), which contains the latest Fedora disk images for development and the corresponding firmware image files
- [Freemarker V2.3.8](https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download)

:bulb:
There are two types of Fedora/RISC-V images for downloading, both of which work good when booted via QEMU. As the 1st Fedora/RISC-V image, the bootstrap process of `Fedora_Stage4` is much faster than that of `Fedora_Developer_Rawhide` which comes with quite a lot new features (needs over 30 minutes in bootstrap). Thus, it is recommended to use `Fedora_Stage4` if your preference is to verify/test the generated JDK on the emulator. `Fedora_Developer_Rawhide` is required for whoever working on both the emulator and the real hardware given that Fedora_Stage4 is obsolete (only used for archive on the Fedora/RISC-V website) in which case there is no more technical support on this image (please contact developers involved via #fedora-riscv/IRC for help if any issue).

The following sections cover the whole installation & building steps for all required software packages.

### 2. Install software packages for the cross-compilation
:rocket:
For the cross-compilation, the first thing is to install all required software packages on your system before building the JDK. When it comes to Fedora/RISC-V, you need to boot the OS image via `QEMU` to login the target system for installation.

[1] First of all, download & compile the latest version of `QEMU` source on your host system as follows:
```
wget https://download.qemu.org/qemu-5.0.0.tar.xz
tar xvJf qemu-5.0.0.tar.xz
cd qemu-5.0.0
./configure
make
make install
```

Note:
For the moment, the `QEMU` package targeted for RISC-V is not ready & unavailable even on Ubuntu 19.10, in which case there is no way to directly install the package for use. In addition, please avoid using qemu 4.2 as there are bugs of qemu 4.2 with floating points on Linux in which case SSH doesn't work properly.

[2] Download the Fedora OS image & the corresponding kernel related file for bootstrap

(1) **Fedora_Stage4**

Install `e2fsck` if it doesn't exist on your host system:
```
wget http://downloads.sourceforge.net/project/e2fsprogs/e2fsprogs/v1.43.1/e2fsprogs-1.43.1.tar.gz
tar xzf e2fsprogs-1.43.1.tar.gz
cd e2fsprogs-1.43.1
./configure # <== If this step fails, please check the config.log file.
                  It could be the missing "libc6-dev" package on your system.
make
make install
```

Download the Fedora OS image `stage4-disk.img.xz` and `bbl` (the boot loader) from
https://fedorapeople.org/groups/risc-v/disk-images/ (4GB in size by default)
and expand it appropriately for more space as follows:
e.g.
```
unxz --keep stage4-disk.img.xz
truncate -s 15G stage4-disk.img
e2fsck -fp stage4-disk.img
resize2fs stage4-disk.img
```

(2) **Fedora_Developer_Rawhide**

Install virt-builder & configure the downloading environment
```
sudo apt install libguestfs-tools

mkdir -p ~/.config/virt-builder/repos.d/
cat <<EOF >~/.config/virt-builder/repos.d/fedora-riscv.conf
[fedora-riscv]
uri=https://dl.fedoraproject.org/pub/alt/risc-v/repo/virt-builder-images/images/index
EOF
```
Download the Fedora developer image using virt-builder (please refer to the instructions at https://fedoraproject.org/wiki/Architectures/RISC-V/Installing#Download_using_virt-builder for details) and expand it appropriately for more space (the minimal size is 10GB by default) as follows:
e.g.
```
$ sudo virt-builder
     --hostname openj9riscv.fedoraproject.org  \
     --root-password password:riscv  \
     --arch riscv64  \
     --size 15G  \
     --format raw  \
     --output Fedora-Developer-Rawhide-20200108.n.0-sda.raw fedora-rawhide-developer-20200108.n.0
```

Note:
virt-builder only works good on the Ubuntu version >= 18.

Download the corresponding firmware image with wget:
e.g.
```
wget https://dl.fedoraproject.org/pub/alt/risc-v/repo/virt-builder-images/images/Fedora-Developer-Rawhide-20200108.n.0-fw_payload-uboot-qemu-virt-smode.elf
```

[3] Boot the Fedora OS image via QEMU

(1) **Fedora_Stage4** (please refer to https://wiki.qemu.org/Documentation/Platforms/RISCV for details)
e.g.
```
  qemu-system-riscv64 \
   -nographic \
   -machine virt \
   -smp 4 \
   -m 4G \   #the memory can be adjusted depending on the capability of your host system
   -kernel bbl \
   -object rng-random,filename=/dev/urandom,id=rng0 \
   -device virtio-rng-device,rng=rng0 \
   -append "console=ttyS0 ro root=/dev/vda" \
   -device virtio-blk-device,drive=hd0 \
   -drive file=stage4-disk.img,format=raw,id=hd0 \
   -device virtio-net-device,netdev=usernet \
   -netdev user,id=usernet,hostfwd=tcp::10000-:22  #the remote port number is 10000 for SSH to login
```
Login: `root`
Password: `riscv`

The following screen messages show up after logging in the system with the root account
e.g. 
```
Welcome to the Fedora/RISC-V stage4 disk image
https://fedoraproject.org/wiki/Architectures/RISC-V

Build date: 2018-04-06 08:24
Kernel 4.19.0-rc8 on an riscv64 (ttyS0)
The root password is 'riscv'.
To install new packages use 'dnf install ...'
...
stage4 login: root
Password: riscv
Last login: Fri Oct  4 21:21:28 from 10.0.2.2

[root@stage4 ~]# uname -a
Linux stage4.fedoraproject.org 4.19.0-rc8 #1
SMP Wed Oct 17 15:11:25 UTC 2018 riscv64 riscv64 riscv64 GNU/Linux
```

:bulb:
To establish another session to communicate with the target system,
run the command via any terminal software (e.g. putty) as follows:
```
ssh -p 10000 root@localhost
root@localhost's password: riscv
```

(2) **Fedora_Developer_Rawhide** (please refer to https://fedoraproject.org/wiki/Architectures/RISC-V/Installing#Boot_under_QEMU for details)
e.g.
```
  qemu-system-riscv64 \
   -nographic \
   -machine virt \
   -smp 8 \
   -m 8G \   #the memory can be adjusted depending on the capability of your host system
   -kernel Fedora-Developer-Rawhide-20200108.n.0-fw_payload-uboot-qemu-virt-smode.elf \
   -object rng-random,filename=/dev/urandom,id=rng0 \
   -device virtio-rng-device,rng=rng0 \
   -device virtio-blk-device,drive=hd0 \
   -drive file=Fedora-Developer-Rawhide-20200108.n.0-sda.raw,format=raw,id=hd0 \
   -device virtio-net-device,netdev=usernet \
   -netdev user,id=usernet,hostfwd=tcp::10000-:22  #the remote port number is 10000 for SSH to login
```
Login: `root`
Password: `riscv`

The following screen messages show up after logging in the system with the root account
e.g. 
```
Welcome to the Fedora/RISC-V disk image
https://fedoraproject.org/wiki/Architectures/RISC-V

Build date: Wed Jan  8 10:28:16 UTC 2020

Kernel 5.5.0-0.rc5.git0.1.1.riscv64.fc32.riscv64 on an riscv64 (ttyS0)
...
To install new packages use 'dnf install ...'
...
For updates and latest information read:
https://fedoraproject.org/wiki/Architectures/RISC-V

Fedora/RISC-V
Koji:               http://fedora.riscv.rocks/koji/
SCM:                http://fedora.riscv.rocks:3000/
Distribution rep.:  http://fedora.riscv.rocks/repos-dist/
Koji internal rep.: http://fedora.riscv.rocks/repos/

openj9riscv login: root
Password: riscv
Last login: Thu Apr 23 11:57:54 on ttyS0

[root@openj9riscv ~]# uname -a
Linux openj9riscv.fedoraproject.org 5.5.0-0.rc5.git0.1.1.riscv64.fc32.riscv64 #1
SMP Mon Jan 6 17:31:22 UTC 2020 riscv64 riscv64 riscv64 GNU/Linux
```

:bulb:
Given that the root account is rejected in login remotely via SSH, you need
to create a user account on `Fedora_Developer_Rawhide` to establish another 
session to communicate with the target system as follows:

Create a user account on `Fedora_Developer_Rawhide`:
e.g.
```
sudo adduser user_riscv
sudo passwd  riscv
```

Run the command via any terminal software (e.g. putty) with the non-root account as follows:
e.g.
```
ssh -p 10000 user_riscv@localhost
user_riscv@localhost's password: riscv
```

[4] Install all X11/development related software packages required in building the JDK on Fedora/QEMU.
```
dnf install libX11-devel libXtst-devel libXt-devel libXrender-devel libXrandr-devel libXi-devel libXext-devel
dnf install cups-devel fontconfig-devel alsa-lib-devel freetype-devel  #freetype might be skipped if already installed
dnf install libdwarf-devel libstdc++-static #optional if the DDR is enabled for compilation on the target system
dnf install wget git autoconf automake  #mostly used in the compilation on the target system
dnf install openssl-devel  #optional if the OpenSSL support is required
```

:bulb:
If there is anything else missing during the cross-compilation on your host system, please check the link at https://secondary.fedoraproject.org/pub/alt/risc-v/RPMS/riscv64 to see whether it is available for downloading. Some of them might go upstream to the main repository.

[5] Power off the Fedora OS as follows after all installations above are finished.
```
poweroff
```

:bulb:
It is advised that you prepare two Fodora OS images with the same packages installed for development. One image is exclusive for QEMU while another is only used in the cross-compilation, in which case there is no need to frequently power on/off the OS image via QEMU.

[6] Mount the Fedora OS image to your host system for the cross-compilation.

(1) **Fedora_Stage4**
```
mkdir <your_fedora_mount_directory>
sudo mount -o loop  stage4-disk.img  <your_fedora_mount_directory>

<your_fedora_mount_directory># ls
bin   dev  home  lib64       media  opt   root  sbin  sys  usr
boot  etc  lib   lost+found  mnt    proc  run   srv   tmp  var
```

(2) **Fedora_Developer_Rawhide**
```
<1> sudo  partx -v -a  Fedora-Developer-Rawhide-20200108.n.0-sda.raw
partition: none, disk: Fedora-Developer-Rawhide-20200108.n.0-sda.raw, lower: 0, upper: 0
Trying to use '/dev/loop14' for the loop device
/dev/loop14: partition table type 'gpt' detected
range recount: max partno=4, lower=0, upper=0
/dev/loop14: partition #1 added
/dev/loop14: partition #2 added
/dev/loop14: partition #3 added
/dev/loop14: partition #4 added

<2> sudo fdisk -l /dev/loop14
Disk /dev/loop14: 15 GiB, 16106127360 bytes, 31457280 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: gpt
Disk identifier: 54A47910-70D3-4FB5-AF40-DAD63952C97D

Device          Start      End  Sectors  Size Type
/dev/loop14p1    2048  1435647  1433600  700M Linux filesystem
/dev/loop14p2 1435648  1435711       64   32K HiFive Unleashed FSBL
/dev/loop14p3 1435776  1452159    16384    8M HiFive Unleashed BBL
/dev/loop14p4 1452160 31454591 30002432 14.3G Linux filesystem   #the file system to be mounted

<3> mkdir <your_fedora_mount_directory>

<4> sudo mount /dev/loop14p4  <your_fedora_mount_directory>

<your_fedora_mount_directory> ls
bin  boot  dev  etc  home  lib  lib64  lost+found  media
mnt  opt  proc  root  run  sbin  srv  sys  tmp  usr  var
```

### 3. Compile/Install the cross-toolchain for RISC-V
:rocket:
The GNU cross-toolchain project at https://github.com/riscv/riscv-gnu-toolchain contains everything required during compilation except the OS specific headers/libraries which are already installed previously on the Fedora OS image.

[1] Run the following command to get the source of the cross-toolchain:
```
$ git clone --recursive https://github.com/riscv/riscv-gnu-toolchain.git
```

[2] When the process is complete, install all required software packages on your host system:
```
$ sudo apt-get install \
    autoconf \
    automake \
    autotools-dev \
    curl \
    libmpc-dev \
    libmpfr-dev \
    libgmp-dev \
    gawk \
    build-essential \
    bison \
    flex \
    texinfo \
    gperf \
    libtool \
    patchutils \
    bc \
    zlib1g-dev \
    libexpat-dev
```

[3] Set up the installation path and build the cross-toolchain on your system:
```
cd riscv-gnu-toolchain
./configure --prefix=/opt/riscv_toolchain_linux  #the path to install the cross-toolchain
make linux
```

:bulb:
Please refer to the latest instructions at https://github.com/riscv/riscv-gnu-toolchain for details if
you are working on other host systems or if any issue is detected when compiling the cross-toolchain.

[4] Run the RISC-V specific gcc command to ensure the cross-compiler works good as expected:
```
$ riscv64-unknown-linux-gnu-gcc --version
riscv64-unknown-linux-gnu-gcc (GCC) 9.2.0
Copyright (C) 2019 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.
There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

Note:
In addition to the cross-toolchain compiled from the source, the cross-toolchain package targeted for RISC-V is also available for installation (please check to see whether it is ready on your Ubuntu system)
e.g.
```
$ sudo apt-cache  search   riscv
g++-9-riscv64-linux-gnu - GNU C++ compiler (cross compiler for riscv64 architecture)
g++-riscv64-linux-gnu - GNU C++ compiler for the riscv64 architecture
gcc-9-riscv64-linux-gnu - GNU C compiler (cross compiler for riscv64 architecture)
gcc-riscv64-linux-gnu - GNU C compiler for the riscv64 architecture
libgcc-9-dev-riscv64-cross - GCC support library (development files)
libstdc++-9-dev-riscv64-cross - GNU Standard C++ Library v3 (development files) (riscv64)
binutils-riscv64-linux-gnu - GNU binary utilities, for riscv64-linux-gnu target
...

sudo apt-get install  gcc-riscv64-linux-gnu g++-riscv64-linux-gnu gcc-9-riscv64-linux-gnu g++-9-riscv64-linux-gnu libstdc++-9-dev-riscv64-cross linux-libc-dev-riscv64-cross binutils-riscv64-linux-gnu

$ riscv64-linux-gnu-gcc --version
riscv64-linux-gnu-gcc (Ubuntu 9.2.1-9ubuntu1) 9.2.1 20191008
Copyright (C) 2019 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```
:bulb:
The cross-built JDK compiled by the installed cross-compiler (with GLIBC >= 2.28)  doesn't work on Fedora_stage4 (only support `GLIBC_2.27`).

### 4. Get the source for building the JDK
:rocket: You need to clone the Extensions for OpenJDK for OpenJ9 project, which is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches. Run the following command:
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

### 5. Generate the build JDK for the cross-compilation
:rocket:
When you have all the source files on the host system, run the following configure command to set up the compilation environment as follows:
```
export JAVA_HOME=/<path_to_build_JDK>  # the build JDK here is downloaded from https://api.vm.net/v3/binary/latest/11/ga/linux/x64/jdk/openj9/normal/adoptopenjdk
export PATH="$JAVA_HOME/bin:$PATH"

bash configure --disable-warnings-as-errors --with-freemarker-jar=/<path_to>/freemarker.jar
```

Generate the build-JDK intended for later cross-compilation:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

:bulb:
Please move the generated build-JDK at `build/linux-x86_64-normal-server-release/images` to an appropriate directory as the `build` directory will be removed for cross-compilation in the following step.

### 6. Configure the cross-compiling environment
:rocket:
Run the following configure command to set up the cross-compilation environment in the same `openj9-openjdk-jdk11` directory where the `build-JDK` is created at [step 5](#5-generate-the-build-jdk-for-the-cross-compilation) after removing the existing `build` directory:
```
export RISCV64=/<path_to_gnu_cross_toolchain>  #e.g. /opt/riscv_toolchain_linux
export PATH="$RISCV64/bin:$PATH"

bash configure --disable-warnings-as-errors \ 
               --disable-ddr \
               --with-boot-jdk=/<path_to_build_JDK_for_cross_compilation> \   #the `build-JDK` created at step 5
               --with-build-jdk=/<path_to_build_JDK_for_cross_compilation> \  #the `build-JDK` created at step 5
               --openjdk-target=riscv64-unknown-linux-gnu \                   #the prefix for the compiled cross-toolchain
               --with-sysroot=/<path_to_your_fedora_mount_directory> \
               --with-freemarker-jar=/<path_to>/freemarker.jar
```

:bulb:
For installed cross-toolchain package, run the following configure command to set up the cross-compilation environment:
```
export RISCV_TOOLCHAIN_TYPE=install  #specify the install type to use the installed cross-toolchain for the cross-compilation

bash configure --disable-warnings-as-errors \ 
               --disable-ddr \
               --with-boot-jdk=/<path_to_build_JDK_for_cross_compilation> \   #the `build-JDK` created at step 5
               --with-build-jdk=/<path_to_build_JDK_for_cross_compilation> \  #the `build-JDK` created at step 5
               --openjdk-target=riscv64-linux-gnu \                           #the prefix for the installed cross-toolchain
               --with-sysroot=/<path_to_your_fedora_mount_directory> \
               --with-freemarker-jar=/<path_to>/freemarker.jar
```

:pencil:
If you want to build an OpenJDK that uses OpenSSL, you must specify `--with-openssl=<path_to_library>`
 where:
  - `path_to_library` uses an OpenSSL v1 library installed on the mounted Fedora OS image
e.g.
```
--with-openssl=/<path_to_your_fedora_mount_directory>/usr
```

You can check the version of OpenSSL on your target system as follows:
e.g.
```
<your_fedora_mount_directory>/usr/bin# file openssl
openssl: ELF 64-bit LSB executable, UCB RISC-V, version 1 (SYSV), 
dynamically linked, interpreter /lib/ld-linux-riscv64-lp64d.so.1, 
for GNU/Linux 4.15.0, BuildID[sha1]=715cfde5a3bdc6ff31b6cc2e449f06df1b3c465a, not stripped
```

### 7. Cross-build the JDK
:rocket:
Now you're ready to cross-build OpenJDK with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

A binary for the full developer kit (JDK without DDR support) is built and stored in the following directory:

- **build/linux-riscv64-normal-server-release/images/jdk**

### 8. Test the JDK on Fedora/QEMU

[1] Unmount the Fedora OS image after the cross-compilation is complete.
```
sudo umount /<path_to_your_fedora_mount_directory>
```

[2] Boot Fedora via QEMU with the command `qemu-system-riscv64` at [step 2](#2-install-software-packages-for-the-cross-compilation):
```
stage4 login: root
Password: riscv
Last login: Fri Oct  4 21:21:28 from 10.0.2.2
```

:bulb:
There is no need to unmount & boot up this Fedora image if your have another one already booted via QEMU.

[3] Upload the zipped JDK onto Fedora via the `scp` command as follows:
```
[your_user_name@riscv_qemu new_dir]# scp <user>@<the_IP_address_on_your_host_system>:/<path_to>/jdk.zip .
[your_user_name@riscv_qemu new_dir]# unzip -qq jdk.zip
```

[4] Change to your jdk directory on Fedora and try running the `java -version` command for a simple test.
```
[your_user_name@riscv_qemu new_dir]# cd /home/<user_name>/new_dir/jdk
[your_user_name@riscv_qemu jdk]#./bin/java -version
```

Here is some sample output:
```
$ ./bin/java -version
openjdk version "11.0.5-internal" 2019-11-15
OpenJDK Runtime Environment (build 11.0.5-internal+0-adhoc.root.openj9-openjdk-jdk11)
Eclipse OpenJ9 VM (build master-41621b6b6, JRE 11 Linux riscv64-64-Bit Compressed References 20191119_000000 (JIT disabled, AOT disabled)
OpenJ9   - 41621b6b6
OMR      - 92c14ce2
JCL      - e5937725d3 based on jdk-11.0.5+10)
```

:bulb:
The JIT compiler for Riscv64 is not supported at the time of writing, in which case the JDK ends up with the same output by default, whether or not the `-Xint` option is specified on the command line.

### 9. Build the JDK on Fedora (Optionally if you want to enable the DDR support in the jdk)
:rocket:
The following building steps are only used with the DDR support enabled in compilation on Fedora.

[1] Follow the steps at **4** to get all source required for building the JDK.
e.g.
```
[root@fedora_riscv <build_jdk_dir>] git clone https://github.com/ibmruntimes/openj9-openjdk-jdk11.git
[root@fedora_riscv <build_jdk_dir>] cd openj9-openjdk-jdk11
[root@fedora_riscv <build_jdk_dir>] bash get_source.sh
```

[2] Run the following configure script to set up the building environment on Fedora.
```
export JAVA_HOME=/<path_to>/<the_cross_built_jdk>
export PATH="$JAVA_HOME/bin:$PATH"
```
:bulb:
Please remove `/usr/lib64/ccache` from the PATH environment variable as `/usr/lib64/ccache/gcc` is not supported in the compilation of OpenJDK.

The boot JDK set up in `JAVA_HOME` is the cross-built jdk uploaded to Fedora after cross-compiled on your host system.
```
bash configure --disable-warnings-as-errors --with-freemarker-jar=/<path_to>/freemarker.jar
```

:pencil:
If you want to build an OpenJDK that uses OpenSSL,  you must specify `--with-openssl=system` where`system` uses the OpenSSL library already installed on Fedora.

:bulb:
Given that there is no JIT support for now, you might need to accelerate the compilation for parallel execution of building jobs by specifying `--with-jobs=<number_of_jobs>`, e.g. `--with-jobs=32`.

### 10. Test the JDK on the hardware (Optionally if hardware is available for test)
:rocket:
(this section is based on verification result from HiFive U540 dev board / to be updated)


:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.1.x support, the following acknowledgements apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:rocket: *Congratulations!* :tada:
