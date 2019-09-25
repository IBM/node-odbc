/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');

describe('.callProcedure(catalog, schema, procedureName, parameters, [callback])...', () => {
  describe('...with callbacks...', () => {
    it('...should place correct result in an out parameter.', (done) => {
      const array = [undefined];
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
        assert.deepEqual(error1, null);
        connection.callProcedure(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_STOREDPROCEDURE}`, array, (error2, result2) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result2, null);
          done();
        });
      });
    });
    it('...should place correct result in an multiple out parameters.', (done) => {
      const array = [
        undefined, undefined, undefined, undefined, undefined,
        undefined, undefined, undefined, undefined, undefined,
        undefined, undefined, undefined, undefined, undefined,
      ];
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
        assert.deepEqual(error1, null);
        connection.callProcedure(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_STOREDPROCEDURE3}`, array, (error2, result2) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result2, null);
          done();
        });
      });
    });
    it('...should place correct result in an multiple in/out parameters.', (done) => {
      const array = [
        'firstParam', 'secondParam',
      ];
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
        assert.deepEqual(error1, null);
        connection.callProcedure(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_STOREDPROCEDURE4}`, array, (error2, result2) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result2, null);
          done();
        });
      });
    });
    // Procedure has 9 parameters:
    // 1. IN_CHAR_FIELD
    // 2. IN_VARCHAR_FIELD
    // 3. IN_BINARY_FIELD
    // 4. INOUT_CHAR_FIELD
    // 5. INOUT_VARCHAR_FIELD
    // 6. INOUT_BINARY_FIELD
    // 7. OUT_CHAR_FIELD
    // 8. OUT_VARCHAR_FIELD
    // 9. OUT_BINARY_FIELD
    // Procedure takes INOUT fields and places the value in OUT fields, then
    // takes IN fields and places the values in INOUT fields
    it.only('...should handle all IN/INOUT/OUT parameters.', (done) => {
      const inBuffer = Buffer.from('inBuffer');
      const inOutBuffer = Buffer.from('inOutBuffer');
      const array = [
        'inChar', 'inVarchar', BigInt(9007199254740991),
        'inOutChar', 'inOutVarchar', BigInt(9007199254740992),
        undefined, undefined, undefined,
        // TIME
        '12.34.56', '01.22.33', undefined,
        // TIMESTAMP
        '2019-09-25 11:35:33.157582', '2019-09-26 01:00:00.123456', undefined,
      ];
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
        assert.deepEqual(error1, null);
        connection.callProcedure(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_STOREDPROCEDURE5}`, array, (error2, result2) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result2, null);
          console.log(result2);
          done();
        });
      });
    });
    it.skip('...should return SQL_LONGVARCHAR data.', (done) => {
      const array = [undefined];
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
        assert.deepEqual(error1, null);
        connection.callProcedure(null, 'MIRISH', 'CLOBPROC', array, (error2, result2) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result2, null);
          done();
        });
      });
    });
  });
});
