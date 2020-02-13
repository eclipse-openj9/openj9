/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

SETUP_LABEL = (params.SETUP_LABEL) ? params.SETUP_LABEL : 'worker'
echo "SETUP_LABEL: ${SETUP_LABEL}"
ARGS = ['SDK_VERSION', 'PLATFORM']
def BUILD_NAME
variableFile = ''
buildFile = ''

timestamps {
    timeout(time: 20, unit: 'HOURS') {
        node(SETUP_LABEL) {
            try{
                def gitConfig = scm.getUserRemoteConfigs().get(0)
                def remoteConfigParameters = [url: params.SCM_REPO]
                def scmBranch = params.SCM_BRANCH
                if (!scmBranch) {
                    scmBranch = scm.branches[0].name
                }

                if (gitConfig.getCredentialsId()) {
                    remoteConfigParameters.put("credentialsId", "${gitConfig.getCredentialsId()}")
                }

                if (params.SCM_REFSPEC) {
                    remoteConfigParameters.put("refspec", params.SCM_REFSPEC)
                }

                checkout changelog: false,
                        poll: false,
                        scm: [$class: 'GitSCM',
                        branches: [[name: "${scmBranch}"]],
                        doGenerateSubmoduleConfigurations: false,
                        extensions: [[$class: 'CloneOption',
                                      reference: "${HOME}/openjdk_cache"],
                                    [$class: 'SparseCheckoutPaths', sparseCheckoutPaths: [[path: 'buildenv/jenkins']]]],
                        userRemoteConfigs: [remoteConfigParameters]]

                variableFile = load 'buildenv/jenkins/common/variables-functions.groovy'
                variableFile.setup()
            } finally {
                // disableDeferredWipeout also requires deleteDirs. See https://issues.jenkins-ci.org/browse/JENKINS-54225
                cleanWs notFailBuild: true, disableDeferredWipeout: true, deleteDirs: true
            }
        }
        buildFile.build_all()
    }
}
