<!--
Copyright IBM Corp. and others 2022

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

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
-->

Building OpenJDK Version 21 with OpenJ9
=======================================

Building OpenJDK 21 with OpenJ9 will be familiar to anyone who has already built OpenJDK. The easiest method
involves the use of Docker and Dockerfiles to create a build environment that contains everything
you need to produce a Linux binary of OpenJDK V21 with the Eclipse OpenJ9 virtual machine. If this method
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
This build process provides detailed instructions for building a Linux x86-64 binary of **OpenJDK V21** with OpenJ9 on Ubuntu 20. The binary can be built directly on your system, in a virtual machine, or in a Docker container :whale:.

If you are using a different Linux distribution, you might have to review the list of libraries that are bundled with your distribution and/or modify the instructions to use equivalent commands to the Advanced Packaging Tool (APT). For example, for Centos, substitute the `apt-get` command with `yum`.

If you want to build a binary for Linux on a different architecture, such as Power Systems&trade; or z Systems&trade;, the process is very similar and any additional information for those architectures are included as Notes :pencil: as we go along.

### 1. Prepare your system
:penguin:
Instructions are provided for preparing your system with and without the use of Docker technology.

Obtain the [docker build script](https://github.com/eclipse-openj9/openj9/blob/master/buildenv/docker/mkdocker.sh) to determine the correct software pre-requisites for both.

Download the docker build script to your local system or copy and paste the following command:

```
wget https://raw.githubusercontent.com/eclipse-openj9/openj9/master/buildenv/docker/mkdocker.sh
```

Optionally, skip to [Setting up your build environment without Docker](#setting-up-your-build-environment-without-docker).

#### Setting up your build environment with Docker :whale:
If you want to build a binary by using a Docker container, follow these steps to prepare your system:

1. The first thing you need to do is install Docker. You can download the free Community edition from [here](https://docs.docker.com/engine/installation/), which also contains instructions for installing Docker on your system.  You should also read the [Getting started](https://docs.docker.com/get-started/) guide to familiarise yourself with the basic Docker concepts and terminology.

2. Next, run the following command to build a Docker image, called **openj9**:
```
bash mkdocker.sh --tag=openj9 --dist=ubuntu --version=22 --gitcache=no --jdk=21 --build
```

3. Start a Docker container from the **openj9** image with the following command, where `-v` maps any directory, `<host_directory>`,
on your local system to the containers `/root/hostdir` directory so that you can store the binaries, once they are built:
```
docker run -v <host_directory>:/root/hostdir -it openj9
```

:pencil: Depending on your [Docker system configuration](https://docs.docker.com/engine/reference/commandline/cli/#description), you might need to prefix the `docker` commands with `sudo`.

Now that you have the Docker image running, you are ready to move to the next step, [Get the source](#2-get-the-source).

#### Setting up your build environment without Docker

If you don't want to user Docker, you can still build directly on your Ubuntu system or in a Ubuntu virtual machine. Use the output of the following command like a recipe card to determine the software dependencies that must be installed on the system, plus a few configuration steps.

```
bash mkdocker.sh --tag=openj9 --dist=ubuntu --version=22 --gitcache=no --jdk=21 --print
```

1. Install the list of dependencies that can be obtained with the `apt-get` command from the following section of the Dockerfile:
```
apt-get update \
  && apt-get install -qq -y --no-install-recommends \
    ...
```

2. The previous step installed g++-11 and gcc-11 packages, which might be different
than the default version installed on your system. Export variables to set the
version used in the build.
```
export CC=gcc-11 CXX=g++-11
```

3. Download and setup the boot JDK using the latest AdoptOpenJDK v20 build.
```
cd <my_home_dir>
wget -O bootjdk20.tar.gz "https://api.adoptopenjdk.net/v3/binary/latest/20/ga/linux/x64/jdk/openj9/normal/adoptopenjdk"
tar -xzf bootjdk20.tar.gz
rm -f bootjdk20.tar.gz
mv $(ls | grep -i jdk-20) bootjdk20
```

### 2. Get the source
:penguin:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches. Run the following command:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk21.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk21
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:
```
bash get_source.sh
```

:pencil: **OpenSSL support:** If you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v3.x available locally, you must specify `-openssl-branch=<branch>` where `<branch>` is an OpenSSL branch (or tag) like `openssl-3.0.13`. If the specified version of OpenSSL is already available in the standard location (`SRC_DIR/openssl`), `get_source.sh` uses it. Otherwise, the script deletes the content and downloads the specified version of OpenSSL source to the standard location and builds it. If you already have the version of OpenSSL in the standard location but you want a fresh copy, you must delete your current copy.

### 3. Configure
:penguin:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure --with-boot-jdk=/home/jenkins/bootjdks/jdk20
```
:warning: The path in the example `--with-boot-jdk=` option is appropriate for the Docker installation. If you're not using the Docker environment, set the path that's appropriate for your setup, such as `<my_home_dir>/bootjdk20`.

:pencil: Configuring and building is not specific to OpenJ9 but uses the OpenJDK build infrastructure with OpenJ9 added.
Many other configuration options are available, including options to increase the verbosity of the build output to include command lines (`LOG=cmdlines`), more info or debug information.
For more information see [OpenJDK build troubleshooting](https://htmlpreview.github.io/?https://raw.githubusercontent.com/openjdk/jdk21u/master/doc/building.html#troubleshooting).

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
Now you're ready to build **OpenJDK V21** with OpenJ9:
```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

A binary for the full developer kit (jdk) is built and stored in the following directory:

- **build/linux-x86_64-server-release/images/jdk**

    :whale: If you built your binaries in a Docker container, copy the binaries to the containers **/root/hostdir** directory so that you can access them on your local system. You'll find them in the directory you set for `<host_directory>` when you started your Docker container. See [Setting up your build environment with Docker](#setting-up-your-build-environment-with-docker-whale).

    :pencil: On other architectures the **jdk** directory is in **build/linux-ppc64le-server-release/images** (Linux on 64-bit Power systems) or **build/linux-s390x-server-release/images** (Linux on 64-bit z Systems).

:pencil: If you want a binary for the runtime environment (jre), you must run `make legacy-jre-image`, which produces a jre build in the **build/linux-x86_64-server-release/images/jre** directory.

:pencil: One of the images created with `make all` is the `debug-image`. This directory contains files that provide debug information for executables and shared libraries when using native debuggers.
To use it, copy the contents of `debug-image` over the jdk before using the jdk with a native debugger.
Another image created is the `test` image, which contains executables and native libraries required when running some functional and OpenJDK testing.
For local testing set the NATIVE_TEST_LIBS environment variable to the test image location, see the [OpenJ9 test user guide](https://github.com/eclipse-openj9/openj9/blob/master/test/docs/OpenJ9TestUserGuide.md).

### 5. Test
:penguin:
For a simple test, try running the `java -version` command.
Change to the jdk directory:
```
cd build/linux-x86_64-server-release/images/jdk
```
Run:
```
./bin/java -version
```

Here is some sample output:

```
openjdk version "21-internal" 2023-09-19
OpenJDK Runtime Environment (build 21-internal-adhoc.jenkins.BuildJDK21x86-64linuxNightly)
Eclipse OpenJ9 VM (build master-5a7404ec286, JRE 21 Linux amd64-64-Bit Compressed References 20230825_48 (JIT enabled, AOT enabled)
OpenJ9   - 5a7404ec286
OMR      - 9659cbcdcef
JCL      - c8a5053e1e5 based on jdk-21+35)
```

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.x support, the following acknowledgments apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (https://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:penguin: *Congratulations!* :tada:

----------------------------------

## AIX
:blue_book:

The following instructions guide you through the process of building an **OpenJDK V21** binary that contains Eclipse OpenJ9 on AIX 7.2.

### 1. Prepare your system
:blue_book:
You must install the following AIX Licensed Program Products (LPPs):
- [xlc/C++ 16](https://www.ibm.com/developerworks/downloads/r/xlcplusaix/)
- x11.adt.ext

You must also install the boot JDK: [Java20_AIX_PPC64](https://api.adoptopenjdk.net/v3/binary/latest/20/ga/aix/ppc64/jdk/openj9/normal/adoptopenjdk).

A number of RPM packages are also required. The easiest method for installing these packages is to use `yum`, because `yum` takes care of any additional dependent packages for you.

Download the following file: [yum_install_aix-ppc64.txt](../../buildenv/aix/jdk21/yum_install_aix-ppc64.txt)

This file contains a list of required RPM packages that you can install by specifying the following command:
```
yum shell yum_install_aix-ppc64.txt
```

It is important to take the list of package dependencies from this file because it is kept up to date by our developers.

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
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk21.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk21
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:

```
bash get_source.sh
```

:pencil: **OpenSSL support:** If you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v3.x available locally, you must specify `-openssl-branch=<branch>` where `<branch>` is an OpenSSL branch (or tag) like `openssl-3.0.13`. If the specified version of OpenSSL is already available in the standard location (`SRC_DIR/openssl`), `get_source.sh` uses it. Otherwise, the script deletes the content and downloads the specified version of OpenSSL source to the standard location and builds it. If you already have the version of OpenSSL in the standard location but you want a fresh copy, you must delete your current copy.

### 3. Configure
:blue_book:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure \
    --with-boot-jdk=<path_to_boot_JDK20> \
    --with-cups-include=<cups_include_path> \
    --disable-warnings-as-errors
```
where `<cups_include_path>` is the absolute path to CUPS. For example, `/opt/freeware/include`.

:pencil: Configuring and building is not specific to OpenJ9 but uses the OpenJDK build infrastructure with OpenJ9 added.
Many other configuration options are available, including options to increase the verbosity of the build output to include command lines (`LOG=cmdlines`), more info or debug information.
For more information see [OpenJDK build troubleshooting](https://htmlpreview.github.io/?https://raw.githubusercontent.com/openjdk/jdk21u/master/doc/building.html#troubleshooting).

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

A binary for the full developer kit (jdk) is built and stored in the following directory:

- **build/aix-ppc64-server-release/images/jdk**

:pencil: If you want a binary for the runtime environment (jre), you must run `make legacy-jre-image`, which produces a jre build in the **build/aix-ppc64-server-release/images/jre** directory.

:pencil: One of the images created with `make all` is the `debug-image`. This directory contains files that provide debug information for executables and shared libraries when using native debuggers.
To use it, copy the contents of `debug-image` over the jdk before using the jdk with a native debugger.
Another image created is the `test` image, which contains executables and native libraries required when running some functional and OpenJDK testing.
For local testing set the NATIVE_TEST_LIBS environment variable to the test image location, see the [OpenJ9 test user guide](https://github.com/eclipse-openj9/openj9/blob/master/test/docs/OpenJ9TestUserGuide.md).

### 5. Test
:blue_book:
For a simple test, try running the `java -version` command.
Change to the /jdk directory:
```
cd build/aix-ppc64-server-release/images/jdk
```
Run:
```
./bin/java -version
```

Here is some sample output:

```
openjdk version "21-internal" 2023-09-19
OpenJDK Runtime Environment (build 21-internal-adhoc.jenkins.BuildJDK21ppc64aixNightly)
Eclipse OpenJ9 VM (build master-5a7404ec286, JRE 21 AIX ppc64-64-Bit Compressed References 20230825_48 (JIT enabled, AOT enabled)
OpenJ9   - 5a7404ec286
OMR      - 9659cbcdcef
JCL      - c8a5053e1e5 based on jdk-21+35)
```

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.x support, the following acknowledgments apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (https://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:blue_book: *Congratulations!* :tada:

----------------------------------

## Windows
:ledger:

The following instructions guide you through the process of building a Windows **OpenJDK V21** binary that contains Eclipse OpenJ9. This process can be used to build binaries for Windows.

### 1. Prepare your system
:ledger:
You must install a number of software dependencies to create a suitable build environment on your system:

- [Cygwin](https://cygwin.com/install.html), which provides a Unix-style command line interface. Install all packages in the `Devel` category. In the `Archive` category, install the packages `zip` and `unzip`. In the `Utils` category, install the `cpio` package. Install any further package dependencies that are identified by the installer. More information about using Cygwin can be found [here](https://cygwin.com/docs.html).
- [Windows JDK 20](https://api.adoptopenjdk.net/v3/binary/latest/20/ga/windows/x64/jdk/openj9/normal/adoptopenjdk), which is used as the boot JDK.
- [Microsoft Visual Studio 2022](https://aka.ms/vs/17/release/vs_community.exe), which is the default compiler level used by OpenJDK21.
- [Freemarker V2.3.8](https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download) - only when building with `--with-cmake=no`
- [LLVM/Clang](https://releases.llvm.org/7.0.0/LLVM-7.0.0-win64.exe)
- [NASM Assembler v2.13.03 or newer](https://www.nasm.us/pub/nasm/releasebuilds/?C=M;O=D)

Add the binary path of Clang to the `PATH` environment variable to override the older version of clang integrated in Cygwin. e.g.
```
export PATH="/cygdrive/c/Program Files/LLVM/bin:$PATH" (in Cygwin)
```

Add the path to `nasm.exe` to the `PATH` environment variable to override the older version of NASM installed in Cygwin. e.g.
```
export PATH="/cygdrive/c/Program Files/NASM:$PATH" (in Cygwin)
```

   You can download Visual Studio manually or obtain it using the [wget](https://www.gnu.org/software/wget/faq.html#download) utility. If you choose to use `wget`, follow these steps:

- Open a cygwin terminal and change to the `/temp` directory:
```
cd /cygdrive/c/temp
```

- Run the following command:
```
wget https://aka.ms/vs/17/release/vs_community.exe -O vs2022.exe
```

- Before installing Visual Studio, change the permissions on the installation file by running `chmod u+x vs2022.exe`.
- Install Visual Studio by running the file `vs2022.exe` (There is no special step required for installing. Please follow the guide of the installer to install all desired components, the C++ compiler is required).

Not all of the shared libraries that are included with Visual Studio are registered during installation.
In particular, the `msdia140.dll` libraries must be registered manually by running command prompt as administrator.  To do so, execute the following from a command prompt:

```
regsvr32 "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\DIA SDK\bin\msdia140.dll"
regsvr32 "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\DIA SDK\bin\amd64\msdia140.dll"
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
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk21.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk21
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:

```
bash get_source.sh
```
:pencil: Do not check out the source code in a path which contains spaces or has a long name or is nested many levels deep.

:pencil: Create the directory that is going to contain the OpenJDK clone by using the `mkdir` command in the Cygwin bash shell and not using Windows Explorer. This ensures that it will have proper Cygwin attributes, and that its children will inherit those attributes.

:pencil: **OpenSSL support:** If you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v3.x available locally, you must specify `-openssl-branch=<branch>` where `<branch>` is an OpenSSL branch (or tag) like `openssl-3.0.13`. If the specified version of OpenSSL is already available in the standard location (SRC_DIR/openssl), `get_source.sh` uses it. Otherwise, the script deletes the content and downloads the specified version of OpenSSL source to the standard location and builds it. If you already have the version of OpenSSL in the standard location but you want a fresh copy, you must delete your current copy.

### 3. Configure
:ledger:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.
```
bash configure \
    --with-boot-jdk=<path_to_boot_JDK20> \
    --disable-warnings-as-errors
```
Note: If you have multiple versions of Visual Studio installed, you can enforce a specific version to be used by setting `--with-toolchain-version`, i.e., by including `--with-toolchain-version=2022` option in the configure command.

:pencil: Configuring and building is not specific to OpenJ9 but uses the OpenJDK build infrastructure with OpenJ9 added.
Many other configuration options are available, including options to increase the verbosity of the build output to include command lines (`LOG=cmdlines`), more info or debug information.
For more information see [OpenJDK build troubleshooting](https://htmlpreview.github.io/?https://raw.githubusercontent.com/openjdk/jdk21u/master/doc/building.html#troubleshooting).

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

:pencil: **OpenSSL support:** If you want to build an OpenJDK that includes OpenSSL, you must specify `--with-openssl={fetched|path_to_library}`

  where:

  - `fetched` uses the OpenSSL source downloaded by `get-source.sh` in step **2. Get the source**.
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

A binary for the full developer kit (jdk) is built and stored in the following directory:

- **build/windows-x86_64-server-release/images/jdk**

:pencil: If you want a binary for the runtime environment (jre), you must run `make legacy-jre-image`, which produces a jre build in the **build/windows-x86_64-server-release/images/jre** directory.

:pencil: One of the images created with `make all` is the `debug-image`. This directory contains files that provide debug information for executables and shared libraries when using native debuggers.
To use it, copy the contents of `debug-image` over the jdk before using the jdk with a native debugger.
Another image created is the `test` image, which contains executables and native libraries required when running some functional and OpenJDK testing.
For local testing set the NATIVE_TEST_LIBS environment variable to the test image location, see the [OpenJ9 test user guide](https://github.com/eclipse-openj9/openj9/blob/master/test/docs/OpenJ9TestUserGuide.md).

### 5. Test
:ledger:
For a simple test, try running the `java -version` command.
Change to the jdk directory:
```
cd build/windows-x86_64-server-release/images/jdk
```
Run:
```
./bin/java -version
```

Here is some sample output:

```
openjdk version "21-internal" 2023-09-19
OpenJDK Runtime Environment (build 21-internal-adhoc.jenkins.buildjdk21x86-64windowsnightly)
Eclipse OpenJ9 VM (build master-5a7404ec286, JRE 21 Windows Server 2019 amd64-64-Bit Compressed References 20230825_49 (JIT enabled, AOT enabled)
OpenJ9   - 5a7404ec286
OMR      - 9659cbcdcef
JCL      - c8a5053e1e5 based on jdk-21+35)
```

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.x support, the following acknowledgments apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (https://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:ledger: *Congratulations!* :tada:

----------------------------------

## macOS
:apple:
The following instructions guide you through the process of building a macOS **OpenJDK V21** binary that contains Eclipse OpenJ9. This process can be used to build binaries for macOS 10.

### 1. Prepare your system
:apple:
You must install a number of software dependencies to create a suitable build environment on your system (the specified versions are minimums):

- [Xcode 10.3, use >= 11.4.1 to support code signing](https://developer.apple.com/download/more/) (requires an Apple account to log in).
- [macOS JDK 20](https://api.adoptopenjdk.net/v3/binary/latest/20/ga/mac/x64/jdk/openj9/normal/adoptopenjdk), which is used as the boot JDK.

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

Only when building with `--with-cmake=no`, [Freemarker V2.3.8](https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download) is also required, which can be obtained and installed with the following commands:

```
wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz
gtar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip-components=2
rm -f freemarker.tgz
```

Bash version 4 is required by the `get_source.sh` script that you will use in step 2, which is installed to `/usr/local/bin/bash`. To prevent problems during the build process, make Bash v4 your default shell by typing the following commands:

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
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk21.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk21
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:

```
bash get_source.sh
```

:pencil: **OpenSSL support:** If you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v3.x available locally, you must specify `-openssl-branch=<branch>` where `<branch>` is an OpenSSL branch (or tag) like `openssl-3.0.13`. If the specified version of OpenSSL is already available in the standard location (SRC_DIR/openssl), `get_source.sh` uses it. Otherwise, the script deletes the content and downloads the specified version of OpenSSL source to the standard location and builds it. If you already have the version of OpenSSL in the standard location but you want a fresh copy, you must delete your current copy.

### 3. Configure
:apple:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.

```
bash configure --with-boot-jdk=<path_to_boot_JDK20>
```

:pencil: Configuring and building is not specific to OpenJ9 but uses the OpenJDK build infrastructure with OpenJ9 added.
Many other configuration options are available, including options to increase the verbosity of the build output to include command lines (`LOG=cmdlines`), more info or debug information.
For more information see [OpenJDK build troubleshooting](https://htmlpreview.github.io/?https://raw.githubusercontent.com/openjdk/jdk21u/master/doc/building.html#troubleshooting).

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

:pencil: **AArch64 macOS only:**
  - Please specify `--with-noncompressedrefs` because compressed references are not supported on AArch64 macOS yet.
  - `--with-cmake=no` is not supported on AArch64 macOS.  Please use cmake.

:pencil: **OpenSSL support:** If you want to build an OpenJDK that includes OpenSSL, you must specify `--with-openssl={fetched|path_to_library}`

  where:

  - `fetched` uses the OpenSSL source downloaded by `get-source.sh` in step **2. Get the source**.
  - `path_to_library` uses a custom OpenSSL library that's already built.

  If you want to include the OpenSSL cryptographic library in the OpenJDK binary, you must include `--enable-openssl-bundling`.

:pencil: When building using `--with-cmake=no`, you must specify `freemarker.jar` with an absolute path, such as `--with-freemarker-jar=<path_to>/freemarker.jar`, where `<path_to>` is the location where you stored `freemarker.jar`.

### 4. build
:apple:
Now you're ready to build OpenJDK with OpenJ9:

```
make all
```
:warning: If you just type `make`, rather than `make all` your build will be incomplete, because the default `make` target is `exploded-image`.
If you want to specify `make` instead of `make all`, you must add `--default-make-target=images` when you run the configure script.

Two builds of OpenJDK with Eclipse OpenJ9 are built and stored in the following directories:

- **build/macosx-x86_64-server-release/images/jdk**
- **build/macosx-x86_64-server-release/images/jdk-bundle**

    :pencil: For running applications such as Eclipse, use the **-bundle** version.

:pencil: If you want a binary for the runtime environment (jre), you must run `make legacy-jre-image`, which produces a jre build in the **build/macosx-x86_64-server-release/images/jre** directory.

:pencil: One of the images created with `make all` is the `debug-image`. This directory contains files that provide debug information for executables and shared libraries when using native debuggers.
To use it, copy the contents of `debug-image` over the jdk before using the jdk with a native debugger.
Another image created is the `test` image, which contains executables and native libraries required when running some functional and OpenJDK testing.
For local testing set the NATIVE_TEST_LIBS environment variable to the test image location, see the [OpenJ9 test user guide](https://github.com/eclipse-openj9/openj9/blob/master/test/docs/OpenJ9TestUserGuide.md).

### 5. Test
:apple:
For a simple test, try running the `java -version` command. Change to the jdk directory:
```
cd build/macosx-x86_64-server-release/images/jdk
```
Run:
```
./bin/java -version
```

Here is some sample output:

```
openjdk version "21-internal" 2023-09-19
OpenJDK Runtime Environment (build 21-internal-adhoc.jenkins.BuildJDK21x86-64macNightly)
Eclipse OpenJ9 VM (build master-5a7404ec286, JRE 21 Mac OS X amd64-64-Bit Compressed References 20230825_48 (JIT enabled, AOT enabled)
OpenJ9   - 5a7404ec286
OMR      - 9659cbcdcef
JCL      - c8a5053e1e5 based on jdk-21+35)
```

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.x support, the following acknowledgments apply in accordance with the license terms:

- *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (https://www.openssl.org/).*
- *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:ledger: *Congratulations!* :tada:

----------------------------------

## AArch64

:penguin:
The following instructions guide you through the process of building an **OpenJDK V21** binary that contains Eclipse OpenJ9 for AArch64 (ARMv8 64-bit) Linux.

### 1. Prepare your system

The binary can be built on your AArch64 Linux system, or in a Docker container :whale: on x86-64 Linux.

### 2. Get the source
:penguin:
First you need to clone the Extensions for OpenJDK for OpenJ9 project. This repository is a git mirror of OpenJDK without the HotSpot JVM, but with an **openj9** branch that contains a few necessary patches. Run the following command:
```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk21.git
```
Cloning this repository can take a while because OpenJDK is a large project! When the process is complete, change directory into the cloned repository:
```
cd openj9-openjdk-jdk21
```
Now fetch additional sources from the Eclipse OpenJ9 project and its clone of Eclipse OMR:

```
bash get_source.sh
```

:pencil: **OpenSSL support:** On an AArch64 Linux system if you want to build an OpenJDK with OpenJ9 binary with OpenSSL support and you do not have a built version of OpenSSL v3.x available locally, you must specify `-openssl-branch=<branch>` where `<branch>` is an OpenSSL branch (or tag) like `openssl-3.0.13`. If the specified version of OpenSSL is already available in the standard location (SRC_DIR/openssl), `get_source.sh` uses it. Otherwise, the script deletes the content and downloads the specified version of OpenSSL source to the standard location and builds it. If you already have the version of OpenSSL in the standard location but you want a fresh copy, you must delete your current copy.

### 3. Prepare for build on AArch64 Linux

You must install a number of software dependencies to create a suitable build environment on your AArch64 Linux system:

- GNU C/C++ compiler 10.3 (The Docker image uses GCC 7.5)
- [AArch64 Linux JDK](https://api.adoptopenjdk.net/v3/binary/latest/20/ga/linux/aarch64/jdk/openj9/normal/adoptopenjdk), which is used as the boot JDK.
- [Freemarker V2.3.8](https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download) - Only when building with `--with-cmake=no`

See [Setting up your build environment without Docker](#setting-up-your-build-environment-without-docker) in [Linux section](#linux) for other dependencies to be installed.

### 4. Create the Docker image

If you build the binary on x86-64 Linux, run the following commands to build a Docker image for AArch64 cross-compilation, called **openj9aarch64**:
```
cd openj9/buildenv/docker/aarch64-linux_CC
docker build -t openj9aarch64 -f Dockerfile .
```

Start a Docker container from the **openj9aarch64** image with the following command, where `<host_directory>` is the directory that contains `openj9-openjdk-jdk21` in your local system:
```
docker run -v <host_directory>/openj9-openjdk-jdk21:/root/openj9-openjdk-jdk21 -it openj9aarch64
```

Then go to the `openj9-openjdk-jdk21` directory:
```
cd /root/openj9-openjdk-jdk21
```

### 5. Configure
:penguin:
When you have all the source files that you need, run the configure script, which detects how to build in the current build environment.

For building on AArch64 Linux:
```
bash configure --with-boot-jdk=<path_to_boot_JDK> \
               --disable-warnings-as-errors
```

For building in the Docker container:
```
bash configure --openjdk-target=${OPENJ9_CC_PREFIX} \
               --with-x=${OPENJ9_CC_DIR}/${OPENJ9_CC_PREFIX}/ \
               --with-freetype-include=${OPENJ9_CC_DIR}/${OPENJ9_CC_PREFIX}/libc/usr/include/freetype2 \
               --with-freetype-lib=${OPENJ9_CC_DIR}/${OPENJ9_CC_PREFIX}/libc/usr/lib \
               --with-boot-jdk=/root/bootjdk20 \
               --with-build-jdk=/root/bootjdk21 \
               --disable-warnings-as-errors \
               --disable-ddr
```

:pencil: Configuring and building is not specific to OpenJ9 but uses the OpenJDK build infrastructure with OpenJ9 added.
Many other configuration options are available, including options to increase the verbosity of the build output to include command lines (`LOG=cmdlines`), more info or debug information.
For more information see [OpenJDK build troubleshooting](https://htmlpreview.github.io/?https://raw.githubusercontent.com/openjdk/jdk21u/master/doc/building.html#troubleshooting).

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

:pencil: **OpenSSL support:** If you want to build an OpenJDK that uses OpenSSL, you must specify `--with-openssl={fetched|system|path_to_library}`

  where:

  - `fetched` uses the OpenSSL source downloaded by `get-source.sh` in step **2. Get the source**. Using `--with-openssl=fetched` will fail during the build in the Docker environment.
  - `system` uses the package installed OpenSSL library in the system.  Use this option when you build on your AArch64 Linux system.
  - `path_to_library` uses an OpenSSL v3.x library that's already built.  You can use `${OPENJ9_CC_DIR}/${OPENJ9_CC_PREFIX}/libc/usr` as `path_to_library` when you are configuring in the Docker container.

:pencil: **DDR support:** You can build DDR support only on AArch64 Linux.  If you are building in a cross-compilation environment, you need the `--disable-ddr` option.

:pencil: **CUDA support:** You can enable CUDA support if you are building on NVIDIA Jetson Developer Kit series.  Add `--enable-cuda --with-cuda=/usr/local/cuda` when you run `configure`.  The path `/usr/local/cuda` may be different depending on the version of JetPack.

:pencil: You may need to add `--disable-warnings-as-errors-openj9` depending on the toolchain version.

:pencil: When building using `--with-cmake=no`, you must specify `freemarker.jar` with an absolute path, such as `--with-freemarker-jar=<path_to>/freemarker.jar`, where `<path_to>` is the location where you stored `freemarker.jar`.

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

:pencil: One of the images created with `make all` is the `debug-image`. This directory contains files that provide debug information for executables and shared libraries when using native debuggers.
To use it, copy the contents of `debug-image` over the jdk before using the jdk with a native debugger.
Another image created is the `test` image, which contains executables and native libraries required when running some functional and OpenJDK testing.
For local testing set the NATIVE_TEST_LIBS environment variable to the test image location, see the [OpenJ9 test user guide](https://github.com/eclipse-openj9/openj9/blob/master/test/docs/OpenJ9TestUserGuide.md).

### 6. Test
:penguin:
For a simple test, try running the `java -version` command.
Change to your jdk directory on AArch64 Linux:
```
cd build/linux-aarch64-normal-server-release/images/jdk
```
Run:
```
./bin/java -version
```

Here is some sample output:

```
openjdk version "21-internal" 2023-09-19
OpenJDK Runtime Environment (build 21-internal-adhoc.jenkins.BuildJDK21aarch64linuxNightly)
Eclipse OpenJ9 VM (build master-5a7404ec286, JRE 21 Linux aarch64-64-Bit Compressed References 20230825_48 (JIT enabled, AOT enabled)
OpenJ9   - 5a7404ec286
OMR      - 9659cbcdcef
JCL      - c8a5053e1e5 based on jdk-21+35)
```

:pencil: **OpenSSL support:** If you built an OpenJDK with OpenJ9 that includes OpenSSL v1.x support, the following acknowledgments apply in accordance with the license terms:

  - *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (https://www.openssl.org/).*
  - *This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).*

:penguin: *Congratulations!* :tada:
