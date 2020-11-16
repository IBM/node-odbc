/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert').strict;
const odbc = require('../../');

const dbms = process.env.DBMS;

if (dbms) {
  const procedureDataTypes = require('../DBMS/test')[dbms];
  if (!procedureDataTypes) {
    console.error('DBMS was invalid!');
  } else {

    async function runTest(connection, procedureName, test) {
      const { values } = test;
      const parameters = [values.in.value, values.inout.value, values.out.value];
      const expectedParams = [values.in.expected, values.inout.expected, values.out.expected];
      const results = await connection.callProcedure(null, process.env.DB_SCHEMA, procedureName, parameters);
      assert.deepEqual(results.parameters, expectedParams);
    }

    describe.only('...testing IN, INOUT, and OUT parameters...', () => {
      procedureDataTypes.forEach((dataTypeInfo) => {
        const { dataType } = dataTypeInfo;
        describe(`...for ${dataType}...`, () => {
          dataTypeInfo.configurations.forEach((dataTypeConfiguration, index) => {
            const { size, options } = dataTypeConfiguration;
            const fieldConfiguration = `${size ? `${size} ` : ''}${options ? `${options} ` : ''}`;
            describe(`...with ${fieldConfiguration ? `${fieldConfiguration}` : 'default field configuration...'}`, () => {
              const procedureName = `${dataType}${index}_IN_INOUT_OUT`;
              let connection;
              before(async () => {
                try {
                  connection = await odbc.connect(process.env.CONNECTION_STRING);
                  // await connection.beginTransaction();
                  const procedureQuery = `CREATE OR REPLACE PROCEDURE ${process.env.DB_SCHEMA}.${procedureName} (
                    IN IN_${dataType} ${dataType} ${size ? `${size} ` : ' '}${options ? `${options}` : ''},
                    INOUT INOUT_${dataType} ${dataType} ${size ? `${size} ` : ' '}${options ? `${options}` : ''},
                    OUT OUT_${dataType} ${dataType} ${size ? `${size} ` : ' '}${options ? `${options}` : ''}
                          )
                      LANGUAGE SQL
                      MODIFIES SQL DATA
                      PROGRAM TYPE SUB
                      CONCURRENT ACCESS RESOLUTION DEFAULT
                      DYNAMIC RESULT SETS 0
                      OLD SAVEPOINT LEVEL
                      COMMIT ON RETURN NO
                    BEGIN
                    SET OUT_${dataType} = INOUT_${dataType};
                    SET INOUT_${dataType} = IN_${dataType};
                    END`;
                  await connection.query(procedureQuery);
                  // await connection.commit();
                } catch (error) {
                  console.error(error);
                }
              });
              dataTypeConfiguration.tests.forEach((test) => {
                it(`...testing ${test.name}.`, async () => {
                  await runTest(connection, procedureName, test);
                });
              });
              after(async () => {
                try {
                  connection.query(`DROP PROCEDURE ${process.env.DB_SCHEMA}.${procedureName}`);
                  // await connection.commit();
                  await connection.close();
                } catch (error) {
                  console.log("ERROR123");
                  console.error(error);
                  throw(error);
                }
              });
            });
          });
        });
      });
    });
  }
}

describe('.callProcedure(procedureName, parameters, [callback])...', () => {
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
      const array = [undefined, undefined];
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
        assert.deepEqual(error1, null);
        connection.callProcedure(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_STOREDPROCEDURE2}`, array, (error2, result2) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result2, null);
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
