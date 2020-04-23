---
name: odbc_issues
about: Notify us about a bug in the odbc Node.js package
title: "[BUG]"
labels: ''
assignees: ''

---

**Before you write a report...**
* Read [the documentation](https://github.com/markdirish/node-odbc/blob/master/README.md)
* Looked through [other issues](https://github.com/markdirish/node-odbc/issues?q=) for solutions
* Rebuild the `odbc` binary with `DEBUG` to produce detailed information
  1. Change the contents of `node_modules/odbc/binding.gyp` to define `DEBUG`:
```
...
'defines' : [
  'NAPI_EXPERIMENTAL', 'DEBUG'
],
...
```
  2. Rebuild the `odbc` binary:
```
cd node_modules/odbc && npm install
```

**Describe your system**
* ODBC Driver:
* Database Name:
* Database Version:
* Database OS:
* Node.js Version:
* Node.js OS:

**Describe the bug**
A clear and concise description of what the bug is.

**Expected behavior**
A clear and concise description of what you expected to happen.

**To Reproduce**
Steps to reproduce the behavior:
1. Connect to database with options '...'
2. Create database table '...'
3. Run query '...'

**Code**
If applicable, add snippets of code to show:

* The offending code:

* Any DEBUG information printed to the terminal:

* Any error information returned from a function call:

**Additional context**
Add any other context about the problem here.
