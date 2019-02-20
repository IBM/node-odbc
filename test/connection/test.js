/* eslint-env node, mocha */
/* eslint-disable global-require */

describe('Connection', () => {
  require('./constructor.js');
  require('./close.js');
  require('./query.js');
  require('./beginTransaction.js');
  require('./commit.js');
  require('./rollback.js');
  require('./columns.js');
  require('./tables.js');
});
