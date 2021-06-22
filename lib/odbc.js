const binary = require('@mapbox/node-pre-gyp');
const path = require('path');

const bindingPath = binary.find(path.resolve(path.join(__dirname, '../package.json')));

const nativeOdbc = require(bindingPath);

const { Connection } = require('./Connection');
const { Pool } = require('./Pool');

function connect(connectionString, callback) {
  if (typeof callback !== 'function') {
    return new Promise((resolve, reject) => {
      nativeOdbc.connect(connectionString, (error, odbcConnection) => {
        if (error) {
          reject(error);
        } else {
          const connection = new Connection(odbcConnection);
          resolve(connection);
        }
      });
    });
  }

  return nativeOdbc.connect(connectionString, (error, odbcConnection) => {
    if (!error) {
      return callback(error, new Connection(odbcConnection));
    }
    return callback(error, null);
  });
}

function pool(options, callback) {
  const poolObj = new Pool(options);

  if (typeof callback !== 'function') {
    return new Promise(async (resolve, reject) => {
      try {
        await poolObj.init();
        resolve(poolObj);
      } catch(e) {
        reject(e);
      }
    });
  }

  return poolObj.init((error) => {
    callback(error, poolObj);
  });
}

module.exports = {
  pool,
  connect,
  ...nativeOdbc.odbcConstants,
};
