const axios = require('axios');
const core = require('@actions/core');
const github = require('@actions/github');

async function run() {
    const sandboxIssueNumber = 1; // 19673
    const sandboxOwner = 'Tigers-X'; // eclipse-openj9
    const sandboxRepo = 'openj9';

    const issueNumber = process.env.ISSUE_NUMBER;
    let issueStatus = process.env.ISSUE_STATUS;
    issueStatus = issueStatus[0].toUpperCase() + issueStatus.slice(1);

    const input = {
        issue_title: process.env.ISSUE_TITLE,
        issue_description: process.env.ISSUE_DESCRIPTION,
    };

    const apiUrl = "http://140.211.168.122/recommendation";
    const octokit = new github.getOctokit(process.env.GITHUB_TOKEN);

    try {
        const response = await axios.post(apiUrl, JSON.stringify(input), {
            headers: {
                'Accept': 'application/json',
                'Content-Type': 'application/json'
            }
        });

        const issueComment = response.data;
        let predictedAssignees = issueComment.recommended_developers;
        let predictedLabels = issueComment.recommended_components;
        let resultString = `Issue Number: ${issueNumber}\n`;

        resultString += `Status: ${issueStatus}\n`;
        resultString += `Recommended Components: ${predictedLabels.join(', ')}\n`;
        resultString += `Recommended Assignees: ${predictedAssignees.join(', ')}\n`;

        await octokit.rest.issues.createComment({
            issue_number: sandboxIssueNumber,
            owner: sandboxOwner,
            repo: sandboxRepo,
            body: resultString
        });
    } catch (error) {
        core.setFailed(`Action failed with error: ${error}`);
        await octokit.rest.issues.createComment({
            issue_number: sandboxIssueNumber,
            owner: sandboxOwner,
            repo: sandboxRepo,
            body: `The TriagerX model is currently not responding to the issue ${issueNumber}. Please try again later.`
        });
    }
}
run();
