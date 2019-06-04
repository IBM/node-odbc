const odbc = require('../build/Release/odbc.node');
const { Connection } = require('./Connection');
const { Pool } = require('./Pool');

function connect(connectionString, callback) {
  if (typeof callback !== 'function') {
    return new Promise((resolve, reject) => {
      odbc.connect(connectionString, (error, odbcConnection) => {
        if (error) {
          reject(error);
        } else {
          const connection = new Connection(odbcConnection);
          resolve(connection);
        }
      });
    });
  }

  return odbc.connect(connectionString, (error, odbcConnection) => {
    callback(error, new Connection(odbcConnection));
  });
}

function pool(options, callback) {
  const poolObj = new Pool(options);

  if (typeof callback !== 'function') {
    return new Promise((resolve, reject) => {
      poolObj.init((error) => {
        if (error) {
          reject(error);
        } else {
          resolve(poolObj);
        }
      });
    });
  }

  return poolObj.init((error) => {
    callback(error, poolObj);
  });
}

module.exports = {
  pool,
  connect,
};
