<!--
Copyright (c) 2019, 2019 IBM Corp. and others

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

# How to build only the Compiler component

## Motivation
When developing on the compiler, developers very rarely need to build
the entire JVM. Additionally, building only the compiler component is 
much faster than building the entire JVM. Therefore, this document 
outlines the process for speeding compiler development by only building
the compiler component. 

Note, this approach only works if the changes are limited to the compiler
component; cross component work will require a full JVM rebuild. 

## Steps

First, build OpenJ9 as per the instructions 
in https://github.com/eclipse/openj9/tree/master/doc/build-instructions.
Then, to build only the Compiler component, set up your environment as 
follows:

```
EXTENSIONS_DIR=<path to openj9-openjdk extensions repo>
JAVA_BASE=$EXTENSIONS_DIR/build/linux-x86_64-normal-server-release

# Go to top level directory
cd $EXTENSIONS_DIR/..

# Set up directory structure
ln -s $EXTENSIONS_DIR/openj9/runtime/compiler
ln -s $EXTENSIONS_DIR/omr

# Set up env vars
export TRHOME=$PWD
export J9SRC=$JAVA_BASE/vm
export PATH=$J9SRC:$PATH

# If using a clean openj9/omr repo, regenerate the tracegen files
pushd .
cd $TRHOME/compiler/env
tracegen -treatWarningAsError -generatecfiles -threshold 1 -file j9jit.tdf
echo ""
popd

# Compiler Makefile variables
export JIT_SRCBASE=$TRHOME
export JIT_OBJBASE=$TRHOME/objs
export JIT_DLL_DIR=$TRHOME
```

Then to build:
```
make -C $TRHOME/compiler -f compiler.mk BUILD_CONFIG=prod -j<num threads> J9_VERSION=29
```
To clean:
```
rm -rf $JIT_OBJBASE
```
