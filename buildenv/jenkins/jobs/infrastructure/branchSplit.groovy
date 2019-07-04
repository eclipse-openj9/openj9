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

// Assumes OPENJ9_SHA, OMR_SHA, and NEW_BRANCH_NAME are passed in Jenkins parameters

HTTP = 'https://'
OMR_REPO = 'github.com/eclipse/openj9-omr.git'
OPENJ9_REPO = 'github.com/eclipse/openj9.git'

DEFAULT_SPLIT_OPENJ9 = 'remotes/origin/master'
DEFAULT_SPLIT_OMR = 'remotes/origin/openj9'

if (OPENJ9_SHA == '') {
    OPENJ9_SHA = DEFAULT_SPLIT_OPENJ9
}

if (OMR_SHA == '') {
    OMR_SHA = DEFAULT_SPLIT_OMR
}

def clone_branch_push(REPO, NEW_BRANCH, SPLIT_SHA) {
    timeout(time: 3, unit: 'HOURS') {
        node ('worker') {
            git "${HTTP}${REPO}"

            REPO_NAME = REPO.substring(REPO.lastIndexOf('/') +1, REPO.lastIndexOf('.git'))
            echo "REPO_NAME = '${REPO_NAME}'"

            // branch from sha/tag/branch
            sh "git checkout -b '${NEW_BRANCH}' '${SPLIT_SHA}'"

            withCredentials([usernamePassword(credentialsId: 'b6987280-6402-458f-bdd6-7affc2e360d4', usernameVariable: 'USERNAME', passwordVariable: 'PASSWORD')]) {
                sh "git push ${HTTP}${USERNAME}:${PASSWORD}@${REPO} '${NEW_BRANCH}'"
            }

            // Set the build description
            currentBuild.description += "<br/>${REPO_NAME}: ${SPLIT_SHA}"

            cleanWs()
        }
    }
}

branches = [:]

branches['OMR'] = { 
    stage('Branch OMR') {
        timestamps {
            clone_branch_push(OMR_REPO, NEW_BRANCH_NAME, OMR_SHA)
        }
    }
}

branches['OpenJ9'] = { 
    stage('Branch OpenJ9') {
        timestamps {
            clone_branch_push(OPENJ9_REPO, NEW_BRANCH_NAME, OPENJ9_SHA)
        }
    }
}

node ('worker') {
    if (NEW_BRANCH_NAME == '') {
        error "Must provide a NEW_BRANCH_NAME, stopping build"
    } else {
        currentBuild.description = "Branch: ${NEW_BRANCH_NAME}"
    }
}
parallel branches
