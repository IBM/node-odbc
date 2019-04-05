class ODBCResult {
  constructor(result) {
    this.result = result;
    this.index = 0;
  }

  fetchAll(callback) {
    callback(null, this.result);
  }

  fetch(callback) {
    if (this.moreResultsSync()) {
      const result = this.result[this.index];
      this.index += 1;
      callback(null, result);
    } else {
      callback(Error('No more results'), null);
    }
    return undefined;
  }

  moreResultsSync() {
    if (this.result.length < this.index) {
      return false;
    }
    return true;
  }

  // eslint-disable-next-line class-methods-use-this
  closeSync() {
    // statement already closed, do nothing
  }

  fetchSync() {
    if (this.moreResultsSync()) {
      return this.result[this.index];
    }
    return undefined;
  }

  fetchAllSync() {
    return this.result;
  }

  getColumnNamesSync() {
    return this.result.columns.map(a => a.name);
  }

  getRowCountSync() {
    return this.result.count;
  }
}

module.exports.ODBCResult = ODBCResult;
