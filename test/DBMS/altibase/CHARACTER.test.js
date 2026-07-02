module.exports = {
  dataType: 'CHAR',
  configurations: [
    {
      size: '(16)',
      options: null,
      tests: [
        {
          name: 'string -> CHARACTER [empty]',
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
              expected: '                ',
            },
          },
        },
        {
          name: 'string -> CHAR (partially full values)',
          values: {
            in: {
              value: 'ABCD1234',
              expected: 'ABCD1234',
            },
            inout: {
              value: 'Z Y X W 9',
              expected: 'ABCD1234        ',
            },
            out: {
              value: null,
              expected: 'Z Y X W 9       ',
            },
          },
        },
        {
          name: 'string -> CHARACTER (full)',
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
