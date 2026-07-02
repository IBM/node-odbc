module.exports = {
  dataType: 'CLOB',
  configurations: [
    {
      size: '(16)',
      options: null,
      tests: [
        {
          name: 'string -> CLOB [full]',
          values: {
            in: {
              value: 'ABCD1234EFGH5678',
              expected: 'ABCD1234EFGH5678',
            },
            inout: {
              value: ' A B C D E F G H',
              expected: 'ABCD1234EFGH5678',
            },
            out: {
              value: null,
              expected: ' A B C D E F G H',
            },
          },
        },
        {
          name: 'Buffer -> CLOB (partially full values)',
          values: {
            in: {
              value: Buffer.from('7468697320697320612074c3a9737400', 'hex'),
              expected: Buffer.from('7468697320697320612074c3a9737400', 'hex'),
            },
            inout: {
              value: Buffer.from('c8c5d3d3d65a', 'hex'),
              expected: Buffer.from('7468697320697320612074c3a9737400', 'hex'),
            },
            out: {
              value: null,
              expected: 'HELLO!',
            },
          },
        }
      ],
    },
  ],
};
