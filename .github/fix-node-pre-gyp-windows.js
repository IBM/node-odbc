// Patch node-pre-gyp to work around unresolved issue on Windows
// See https://github.com/mapbox/node-pre-gyp/issues/715
// Need to set shell: true on the spawn API on Windows now
const fs = require('node:fs/promises');

const path = 'node_modules/@mapbox/node-pre-gyp/lib/util/compile.js';

const before = "  const cmd = cp.spawn(shell_cmd, final_args, { cwd: undefined, env: process.env, stdio: [0, 1, 2] });";
const after = "  const cmd = cp.spawn(shell_cmd, final_args, { cwd: undefined, env: process.env, stdio: [0, 1, 2], shell: process.platform === 'win32' });";

(async function script() {
    const fh = await fs.open(path, 'r+');

    let i = 0;
    let out = '';
    for await (let line of fh.readLines({autoClose: false})) {
        i++;;

        if (i == 80) {
            if (line != before) {
                console.error("Patch failed!\n");
                process.exit(1);
            }
            line = after;
        }

        out += line + "\n";
    }

    await fh.write(out, 0);

    await fh.close();
})();

