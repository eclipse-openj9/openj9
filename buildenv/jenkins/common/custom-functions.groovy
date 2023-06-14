/******************************************************************************
 * Licensed Materials - Property of IBM
 * "Restricted Materials of IBM"
 *
 * (c) Copyright IBM Corp. 2020, 2023 All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 ******************************************************************************/

def set_extra_options() {
    set_gskit()
    set_healthcenter()
}

def set_gskit() {
    if (EXTRA_GETSOURCE_OPTIONS.contains("-openjceplus-repo") && (!EXTRA_CONFIGURE_OPTIONS.contains("--with-gskit"))) {
        // required by OpenJCEPlus for IBM Java 11 builds
		println("Fetch gskit")

        def gskitSDKDownloadUrl = buildspec.getScalarField("gskit.sdk", SDK_VERSION)
        def gskitLibDownloadUrl = buildspec.getScalarField("gskit.lib", SDK_VERSION)

        if  (gskitSDKDownloadUrl && gskitLibDownloadUrl) {
            def gskitDir = "${WORKSPACE}/gskit"
            def gskitLibDir = "${gskitDir}/lib/"

            curl_artifactory(gskitSDKDownloadUrl, 'gskit_sdk.tar')
            curl_artifactory(gskitLibDownloadUrl, 'gskit_lib.tar')

            def extractCommand = "tar -xf"
            def extractSDKCmdOpts = "-d gskit"

            if (SPEC.contains('zos')) {
                // to extract tarball, use pax for z/OS to preserve uid/gid permssions
                extractCommand = "pax -rf"
                extractSDKCmdOpts = "-s#jgsk_sdk#gskit/#"
            }

            // extract gskit_sdk.tar into gskit directory
            echo "Install GSKIT in ${gskitDir}"
            sh "${extractCommand} gskit_sdk.tar ${extractSDKCmdOpts}"

            // extract gskit_lib.tar into gskit/lib
            dir(gskitLibDir) {
                sh "${extractCommand} ${WORKSPACE}/gskit_lib.tar libjgsk8iccs_64.so N"
            }

            if (SPEC.contains('zos')) {
                dir (gskitDir) {
                    // copy gskit/libjgsk8iccs_64.x to gskit/lib/
                    fileOperations([fileCopyOperation(excludes: '', flattenFiles: true, includes: 'libjgsk8iccs_64.x', targetLocation: gskitLibDir)])
                }
            }

            // add --with-gskit configure option
            EXTRA_CONFIGURE_OPTIONS += " --with-gskit=${gskitDir}"

            fileOperations([fileDeleteOperation(excludes: '', includes: 'gskit_*.tar')])
        }
    }
}

def set_healthcenter() {
    def hcAgentDownloadUrl = buildspec.getScalarField("healthcenter.agent", SDK_VERSION)

    if (hcAgentDownloadUrl) {
        def hcAgentArchName = hcAgentDownloadUrl.tokenize('/').getAt(-1)

        curl_artifactory(hcAgentDownloadUrl)

        if (fileExists("${WORKSPACE}/${hcAgentArchName}")) {
            // add --with-healthcenter configure option
            EXTRA_CONFIGURE_OPTIONS += " --with-healthcenter=${WORKSPACE}/${hcAgentArchName}"
        }
    }
}

def curl_artifactory(downloadURL, outputFile = null) {
    // env.ARTIFACTORY_SERVER should be already set in build.groovy
    // see set_basic_artifactory_config() in variables-functions.groovy
    def server = Artifactory.server env.ARTIFACTORY_SERVER
    def artifactoryCredentialsID = server.getCredentialsId()

    def outputOption = '-O'
    if (outputFile) {
        outputOption = "-o ${outputFile}"
    }
    withCredentials([usernamePassword(credentialsId: "${artifactoryCredentialsID}", usernameVariable: "ARTIFACTORY_USERNAME", passwordVariable: "ARTIFACTORY_TOKEN")]) {
        sh "_ENCODE_FILE_NEW=UNTAGGED curl -sSkL -u \$ARTIFACTORY_USERNAME:\$ARTIFACTORY_TOKEN ${downloadURL} ${outputOption}"
    }
}

return this
