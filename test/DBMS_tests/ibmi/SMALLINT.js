module.exports = {
  dataType: 'SMALLINT',
  configurations: [
    {
      size: null,
      options: null,
      tests: [
        {
          name: 'Number -> SMALLINT (min value)',
          values: {
            in: {
              value: -32768,
              expected: -32768,
            },
            inout: {
              value: -32767,
              expected: -32768,
            },
            out: {
              value: null,
              expected: -32767,
            },
          },
        },
        {
          name: 'Number -> SMALLINT (max value)',
          values: {
            in: {
              value: 32767,
              expected: 32767,
            },
            inout: {
              value: 32766,
              expected: 32767,
            },
            out: {
              value: null,
              expected: 32766,
            },
          },
        },
        {
          name: 'BigInt -> SMALLINT (min value)',
          values: {
            in: {
              value: -32768n,
              expected: -32768n,
            },
            inout: {
              value: -32767n,
              expected: -32768n,
            },
            out: {
              value: null,
              expected: -32767,
            },
          },
        },
        {
          name: 'BigInt -> SMALLINT (max value)',
          values: {
            in: {
              value: 32767n,
              expected: 32767n,
            },
            inout: {
              value: 32766n,
              expected: 32767n,
            },
            out: {
              value: null,
              expected: 32766,
            },
          },
        },
        {
          name: 'String -> SMALLINT (min value)',
          values: {
            in: {
              value: "-32768",
              expected: "-32768",
            },
            inout: {
              value: "-32767",
              expected: "-32768",
            },
            out: {
              value: null,
              expected: -32767,
            },
          },
        },
        {
          name: 'String -> SMALLINT (max value)',
          values: {
            in: {
              value: "32767",
              expected: "32767",
            },
            inout: {
              value: "32766",
              expected: "32767",
            },
            out: {
              value: null,
              expected: 32766,
            },
          },
        },
      ],
    },
  ],
};
