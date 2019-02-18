/* eslint-env node */

const odbc = require('../../build/Release/odbc.node');
const { Database } = require('./Database.js');

class Pool {
  constructor(options) {
    Pool.count += 1;
    this.index = Pool.count;
    this.availablePool = {};
    this.usedPool = {};
    this.odbc = new odbc.ODBC();
    this.options = options || {};
    this.options.odbc = this.odbc;
  }

  open(connectionString, callback) {
    let db;

    // check to see if we already have a connection for this connection string
    if (this.availablePool[connectionString] && this.availablePool[connectionString].length) {
      db = this.availablePool[connectionString].shift();
      this.usedPool[connectionString].push(db);

      callback(null, db);
    } else {
      db = new Database(this.options);
      db.realClose = db.close;

      db.close = (cb) => {
        // call back early, we can do the rest of this stuff after the client thinks
        // that the connection is closed.
        cb(null);


        // close the connection for real
        // this will kill any temp tables or anything that might be a security issue.
        db.realClose(() => {
          // remove this db from the usedPool
          this.usedPool[connectionString].splice(this.usedPool[connectionString].indexOf(db), 1);

          // re-open the connection using the connection string
          db.open(connectionString, (error) => {
            if (error) {
              console.error(error);
              return;
            }

            // add this clean connection to the connection pool
            this.availablePool[connectionString] = this.availablePool[connectionString] || [];
            this.availablePool[connectionString].push(db);
            if (exports.debug) console.dir(this);
          });
        });
      };

      db.open(connectionString, (error) => {
        if (exports.debug) console.log('odbc.js : pool[%s] : pool.db.open callback()', this.index);

        this.usedPool[connectionString] = this.usedPool[connectionString] || [];
        this.usedPool[connectionString].push(db);

        callback(error, db);
      });
    }
  }

  close(callback) {
    let required = 0;
    let received = 0;
    let connections;
    let key;


    if (exports.debug) console.log('odbc.js : pool[%s] : pool.close()', this.index);
    // we set a timeout because a previous db.close() may
    // have caused the a behind the scenes db.open() to prepare
    // a new connection
    setTimeout(() => {
      // merge the available pool and the usedPool
      const pools = { ...this.availablePool, ...this.usedPool };

      if (exports.debug) { console.log('odbc.js : pool[%s] : pool.close() - setTimeout() callback', this.index); }
      if (exports.debug) { console.dir(pools); }

      if (Object.keys(pools).length === 0) {
        return callback();
      }

      const poolKeys = Object.keys(pools);
      poolKeys.forEach((poolKey) => {
        connections = pools[poolKey];
        required += connections.length;

        if (exports.debug) console.log('odbc.js : pool[%s] : pool.close() - processing pools %s - connections: %s', this.index, key, connections.length);

        const realCloseCallback = (x) => {
          if (exports.debug) console.log('odbc.js : pool[%s] : pool.close() - realClose() callback for connection #%s', this.index, x);
          received += 1;

          if (received === required) {
            callback();

            // prevent mem leaks
            connections = null;
            required = null;
            received = null;
          }
        };

        for (let x = 0; x < connections.length; x += 1) {
          if (exports.debug) console.log('odbc.js : pool[%s] : pool.close() - calling realClose() for connection #%s', this.index, x);

          connections[x].realClose(realCloseCallback.bind(x));
        }
      });
      return undefined;
    }, 2000);
  }
}

Pool.count = 0;
