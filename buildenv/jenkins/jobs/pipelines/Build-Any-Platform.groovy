/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

ARGS = ['SDK_VERSION', 'PLATFORM']

SETUP_LABEL = params.SETUP_LABEL
if (!SETUP_LABEL) {
    SETUP_LABEL = 'worker'
}

timeout(time: 10, unit: 'HOURS') {
    timestamps {
        node(SETUP_LABEL) {
            try{
                checkout scm
                variableFile = load 'buildenv/jenkins/common/variables-functions.groovy'
                variableFile.set_job_variables('build')
                buildFile = load 'buildenv/jenkins/common/build.groovy'
            } finally {
                // disableDeferredWipeout also requires deleteDirs. See https://issues.jenkins-ci.org/browse/JENKINS-54225
                cleanWs notFailBuild: true, disableDeferredWipeout: true, deleteDirs: true
            }
        }
    }
    stage ('Queue') {
        node("${NODE}") {
            buildFile.build_all()
        }
    }
}
