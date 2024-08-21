const fs = require('fs');
const path = require('node:path');

const tag = process.env.TAG_NAME;
const token = process.env.GITHUB_TOKEN;
const files = process.argv.slice(2);

if (!tag) {
    console.error("Must set TAG_NAME environment variable");
    process.exit(1);
}
if (!token) {
    console.error("Must set GITHUB_TOKEN environment variable");
    process.exit(1);
}
if (!files.length) {
    console.error("No files passed");
    process.exit(1);
}

const options = {
    headers: {
        "Authorization": `Bearer ${token}`,
        "X-GitHub-Api-Version": "2022-11-28",
        "Accept": "application/vnd.github+json",
        "Content-Type": "application/octet-stream",
    }
};

(async function script() {
    const response = await fetch(`https://api.github.com/repos/IBM/node-odbc/releases/tags/${tag}`);

    if (!response.ok) {
        console.error(`ERROR: Getting release failed: ${response.status}`)
        process.exit(1);
    }

    const release_obj = await response.json();

    // Returns a goofy URL "template", get rid of the parameters
    const upload_url = release_obj.upload_url.substr(0, release_obj.upload_url.indexOf('{'));;

    for (const file of files) {
        const name = path.basename(file);
        console.log(`Uploading ${file} -> ${name}`);

        const content = fs.readFileSync(file);

        const response = await fetch(`${upload_url}?name=${name}`, {
            method: "POST",
            body: content,
            ...options
        });

        if (!response.ok) {
            console.error(`ERROR: Upload of ${file} failed: ${response.status}`)
            process.exit(1);
        }
    }
})();
