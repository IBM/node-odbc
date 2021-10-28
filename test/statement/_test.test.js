/* eslint-env node, mocha */
/* eslint-disable global-require */

describe('Statement', () => {
  require('./prepare.test.js');
  require('./bind.test.js');
  require('./execute.test.js');
  require('./close.test.js');
});
