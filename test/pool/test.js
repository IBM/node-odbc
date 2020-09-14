/* eslint-env node, mocha */
/* eslint-disable global-require */

describe.only('Pool', () => {
  require('./constructor.js');
  require('./connect.js');
  require('./query.js');
  require('./close.js');
});
