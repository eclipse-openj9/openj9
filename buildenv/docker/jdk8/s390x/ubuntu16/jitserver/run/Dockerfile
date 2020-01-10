# Copyright (c) 2019, 2020 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0

# To use this docker file:
# 1.Depends on your OS platform, build the base openj9 docker
#   image from the Dockerfile in buildenv/docker/jdk<version>/
#   `docker build -f buildenv/docker/jdk<version>/<platform>/ubuntu<version>/Dockerfile -t=openj9 .`
#
# 2.Build the jitserver docker image from buildenv/docker/jitserver/.../build/ to fetch dependencies, source code and build jitserver binaries
#   `docker build -f \
#    buildenv/docker/jitserver/jdk<version>/<platform>/ubuntu<version>/build/Dockerfile \
#    --build-arg openj9_repo=<your-openj9-repo> \
#    --build-arg openj9_branch=<your-openj9-branch> \
#    --build-arg omr_repo=<your-omr-repo> \
#    --build-arg omr_branch=<your-omr-branch> \
#    -t=openj9-jitserver-build .`
#
# 3.Build the jitserver server docker image from buildenv/docker/jitserver/.../run/
#   `docker build -f \
#   buildenv/docker/jitserver/jdk<version>/<platform>/ubuntu<version>/run/Dockerfile \
#   -t=openj9-jitserver-run .`
#

FROM openj9-jitserver-build:latest

ENTRYPOINT ["/root/j2sdk-image/jre/bin/jitserver", "-Xjit:verbose={JITServer}"]

EXPOSE 38400
