/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const { Connection } = require('../../');

describe('.callProcedure(procedureName, parameters, [callback])...', () => {
  describe('...with callbacks...', () => {
    it('...should return information about all columns of a table.', (done) => {
      const connection = new Connection(`${process.env.CONNECTION_STRING}`);
      connection.callProcedure('MARK.MAXBAL', (error1, result1) => {
        if (error1) { console.error(error1); }
        assert.deepEqual(error1, null);
        assert.notDeepEqual(result1, null);
        done();
      });
    });
  });
});
