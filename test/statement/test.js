/* eslint-env node, mocha */
/* eslint-disable global-require */

describe.only('Statement', () => {
  require('./prepare.js');
  require('./bind.js');
  require('./execute.js');
  require('./close.js');
});
