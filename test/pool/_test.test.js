/* eslint-env node, mocha */
/* eslint-disable global-require */

describe('Pool', () => {
  require('./constructor.test.js');
  require('./connect.test.js');
  require('./query.test.js');
  require('./close.test.js');
});
