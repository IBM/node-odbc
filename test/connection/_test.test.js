/* eslint-env node, mocha */
/* eslint-disable global-require */

describe('Connection', () => {
  require('./constructor.test.js');
  require('./close.test.js');
  require('./query.test.js');
  require('./beginTransaction.test.js');
  require('./commit.test.js');
  require('./rollback.test.js');
  require('./columns.test.js');
  require('./primaryKeys.test.js');
  require('./foreignkeys.test.js');
  require('./tables.test.js');
  require('./callProcedure.test.js');
  require('./setIsolationLevel.test.js');
});
