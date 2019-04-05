const { Connection } = require('./Connection');
const { Pool } = require('./Pool');
const legacy = require('./legacy/legacy'); // v1.x behavior

module.exports = {
  Pool,
  Connection,
  legacy,
};
