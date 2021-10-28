module.exports = {
  dataType: 'VARBINARY',
  configurations: [
    {
      size: '(16)',
      options: null,
      tests: [
        {
          name: 'Buffer -> BINARY',
          values: {
            in: {
              value: Buffer.from('7468697320697320612074c3a97374', 'hex'),
              expected: Buffer.from('7468697320697320612074c3a97374', 'hex'),
            },
            inout: {
              value: Buffer.from('00112233445566778899aabbccddeeff', 'hex'),
              expected: Buffer.from('7468697320697320612074c3a97374', 'hex'),
            },
            out: {
              value: null,
              expected: Buffer.from('00112233445566778899aabbccddeeff', 'hex'),
            },
          },
        },
        {
          name: 'Buffer -> BINARY [empty buffer]',
          values: {
            in: {
              value: Buffer.alloc(0),
              expected: Buffer.alloc(0),
            },
            inout: {
              value: Buffer.alloc(0),
              expected: Buffer.alloc(0),
            },
            out: {
              value: null,
              expected: Buffer.alloc(0),
            },
          },
        },
        {
          name: 'Buffer -> BINARY [null bytes]',
          values: {
            in: {
              value: Buffer.alloc(16),
              expected: Buffer.alloc(16),
            },
            inout: {
              value: Buffer.alloc(16),
              expected: Buffer.alloc(16),
            },
            out: {
              value: null,
              expected: Buffer.alloc(16),
            },
          },
        },
      ],
    },
  ],
};
