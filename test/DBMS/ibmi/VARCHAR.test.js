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
              expected: '',
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
    },
    {
      size: '(16)',
      options: 'CCSID 278',
      tests: [
        {
          name: 'string -> VARCHAR (empty values)',
          values: {
            in: {
              value: '',
              expected: '',
            },
            inout: {
              value: ' ',
              expected: '',
            },
            out: {
              value: null,
              expected: ' ',
            },
          },
        },
        {
          name: 'string -> VARCHAR (partially full values)',
          values: {
            in: {
              value: 'Åßçð1234',
              expected: 'Åßçð1234',
            },
            inout: {
              value: '¦ Ü » ± ',
              expected: 'Åßçð1234',
            },
            out: {
              value: null,
              expected: '¦ Ü » ± ',
            },
          },
        },
        {
          name: 'string -> VARCHAR (full values)',
          values: {
            in: {
              value: 'Åßçð1234Æþ§Ð5678',
              expected: 'Åßçð1234Æþ§Ð5678',
            },
            inout: {
              value: '¦ Ü » ± 9 8 7 6 ',
              expected: 'Åßçð1234Æþ§Ð5678',
            },
            out: {
              value: null,
              expected: '¦ Ü » ± 9 8 7 6 ',
            },
          },
        },
      ],
    },
    {
      size: '(16)',
      options: 'CCSID 838',
      tests: [
        {
          name: 'string -> VARCHAR (empty values)',
          values: {
            in: {
              value: '',
              expected: '',
            },
            inout: {
              value: ' ',
              expected: '',
            },
            out: {
              value: null,
              expected: ' ',
            },
          },
        },
        {
          name: 'string -> VARCHAR (partially full values)',
          values: {
            in: {
              value: 'ทดฒฝ1234',
              expected: 'ทดฒฝ1234',
            },
            inout: {
              value: '๕ ๔ ๓ ๛ ',
              expected: 'ทดฒฝ1234',
            },
            out: {
              value: null,
              expected: '๕ ๔ ๓ ๛ ',
            },
          },
        },
        {
          name: 'string -> VARCHAR (full values)',
          values: {
            in: {
              value: 'ทดฒฝ1234ทดฒฝ1234',
              expected: 'ทดฒฝ1234ทดฒฝ1234',
            },
            inout: {
              value: '๕ ๔ ๓ ๛   8 7 6 ',
              expected: 'ทดฒฝ1234ทดฒฝ1234',
            },
            out: {
              value: null,
              expected: '๕ ๔ ๓ ๛   8 7 6 ',
            },
          },
        },
      ],
    },
  ],
};
