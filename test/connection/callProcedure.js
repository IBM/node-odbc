/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const { Connection } = require('../../');

describe.skip('.callProcedure(procedureName, parameters, [callback])...', () => {
  describe('...with callbacks...', () => {
    it('...should place correct result in and out parameter.', (done) => {
      const array = [undefined];
      const connection = new Connection(`${process.env.CONNECTION_STRING}`);
      connection.callProcedure(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_STOREDPROCEDURE}`, array, (error1, result1) => {
        if (error1) { console.error(error1); }
        console.log(result1);
        assert.deepEqual(error1, null);
        assert.notDeepEqual(result1, null);
        done();
      });
    });
  });
});
