/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

// TAG_NAME and TAG_ANNOTATION are passed in Jenkins parameters (Mandatory)
// OPENJ9_SHA, OMR_SHA default to master/openj9 respectively if not set on Jenkins

HTTP = 'https://'
OMR_REPO = 'github.com/eclipse/openj9-omr.git'
OPENJ9_REPO = 'github.com/eclipse/openj9.git'


def clone_branch_push(REPO, TAG_NAME, TAG_ANNOTATION, TAG_POINT, POINT_TYPE) {
    timeout(time: 3, unit: 'HOURS') {
        node ('worker') {
            git "${HTTP}${REPO}"

            def REPO_NAME = REPO.substring(REPO.lastIndexOf('/') +1, REPO.lastIndexOf('.git'))
            echo "REPO_NAME: ${REPO_NAME}"

            // Prefix branch names with ref remote since we don't create local refs for branches
            def POINT_TYPE_PREFIX = ''
            if (POINT_TYPE == 'BRANCH') {
                POINT_TYPE_PREFIX = 'refs/remotes/origin/'
            }

            def SHA = sh (
                script: "git rev-parse --short ${POINT_TYPE_PREFIX}${TAG_POINT}",
                returnStdout: true
            )
            echo "${REPO} SHA: ${SHA}"

            // Set the build description
            currentBuild.description += "<br/>${REPO_NAME}: ${SHA}"

            // Tag SHA
            sh "git tag -a '${TAG_NAME}' -m '${TAG_ANNOTATION}' ${POINT_TYPE_PREFIX}${TAG_POINT}"
            sh "git show ${TAG_NAME}"

            withCredentials([usernamePassword(credentialsId: 'b6987280-6402-458f-bdd6-7affc2e360d4', usernameVariable: 'USERNAME', passwordVariable: 'PASSWORD')]) {
                sh "git push ${HTTP}${USERNAME}:${PASSWORD}@${REPO} 'refs/tags/${TAG_NAME}'"
            }
            cleanWs()
        }
    }
}

branches = [:]

branches['OMR'] = {
    stage('Tag OMR') {
        timestamps {
            clone_branch_push(OMR_REPO, TAG_NAME, TAG_ANNOTATION, OMR_SHA, OMR_TAG_POINT_TYPE)
        }
    }
}

branches['OpenJ9'] = {
    stage('Tag OpenJ9') {
        timestamps {
            clone_branch_push(OPENJ9_REPO, TAG_NAME, TAG_ANNOTATION, OPENJ9_SHA, OPENJ9_TAG_POINT_TYPE)
        }
    }
}

node ('worker') {
    if ((TAG_NAME == '') || (TAG_ANNOTATION == '')) {
        error "Must provide a TAG_NAME and TAG_ANNOTATION, stopping build"
    } else {
        currentBuild.description = "Tag: ${TAG_NAME}<br/>Annotation: ${TAG_ANNOTATION}"
    }
    if (OPENJ9_SHA == '') {
        OPENJ9_SHA = 'master'
        OPENJ9_TAG_POINT_TYPE = 'BRANCH'
    }
    if (OMR_SHA == '') {
        OMR_SHA = 'openj9'
        OMR_TAG_POINT_TYPE = 'BRANCH'
    }
}
parallel branches
