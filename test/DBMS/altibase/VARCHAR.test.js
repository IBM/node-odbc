module.exports = {
  dataType: 'VARCHAR',
  configurations: [
    {
      size: '(16)',
      options: null,
      tests: [
        {
          name: 'string -> VARCHAR [empty]',
          values: {
            in: {
              value: '',
              expected: '',
            },
            inout: {
              value: ' ',
              expected: null,
            },
            out: {
              value: null,
              expected: ' ',
            },
          },
        },
        {
          name: 'string -> VARCHAR (full)',
          values: {
            in: {
              value: 'ABCD1234EFGH5678',
              expected: 'ABCD1234EFGH5678',
            },
            inout: {
              value: 'Z Y X W 9 8 7 6 ',
              expected: 'ABCD1234EFGH5678',
            },
            out: {
              value: null,
              expected: 'Z Y X W 9 8 7 6 ',
            },
          },
        },
      ],
    }
  ],
};
