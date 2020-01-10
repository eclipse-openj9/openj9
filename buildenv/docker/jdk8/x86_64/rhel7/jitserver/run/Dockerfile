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
# 
# 1. Make sure you have a j2sdk-image under the current directory and execute the following command
#   `docker build -f \
#    buildenv/docker/jdk8/x86_64/rhel7/jitserver/run/Dockerfile \
#    -t=openj9-jitserver-run .`
#

FROM registry.access.redhat.com/ubi7/ubi:latest

# Bring the sdk into the image
WORKDIR /opt
COPY j2sdk-image /opt/sdk-ubi

# Create user
RUN adduser --uid 10000 --gid 0 default
USER 10000

ENTRYPOINT ["/opt/sdk-ubi/jre/bin/jitserver", "-Xjit:verbose"]
EXPOSE 38400
