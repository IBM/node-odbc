class SimpleQueue {
  constructor() {
    this.fifo = [];
    this.executing = false;
  }

  push(fn) {
    this.fifo.push(fn);
    this.maybeNext();
  }

  maybeNext() {
    if (!this.executing) {
      this.next();
    }
  }

  next() {
    if (this.fifo.length) {
      const fn = this.fifo.shift();
      this.executing = true;

      fn(() => {
        this.executing = false;
        this.maybeNext();
      });
    }
  }
}

module.exports.SimpleQueue = SimpleQueue;
