module.exports = {
  dataType: 'INTEGER',
  configurations: [
    {
      size: null,
      options: null,
      tests: [
        {
          name: 'Number -> INTEGER (min value)',
          values: {
            in: {
              value: -2147483648,
              expected: -2147483648,
            },
            inout: {
              value: -2147483647,
              expected: -2147483648,
            },
            out: {
              value: null,
              expected: -2147483647,
            },
          },
        },
        {
          name: 'Number -> INTEGER (max value)',
          values: {
            in: {
              value: 2147483647,
              expected: 2147483647,
            },
            inout: {
              value: 2147483646,
              expected: 2147483647,
            },
            out: {
              value: null,
              expected: 2147483646,
            },
          },
        },
        {
          name: 'BigInt -> INTEGER (min value)',
          values: {
            in: {
              value: -2147483648n,
              expected: -2147483648n,
            },
            inout: {
              value: -2147483647n,
              expected: -2147483648n,
            },
            out: {
              value: null,
              expected: -2147483647,
            },
          },
        },
        {
          name: 'BigInt -> INTEGER (max value)',
          values: {
            in: {
              value: 2147483647n,
              expected: 2147483647n,
            },
            inout: {
              value: 2147483646n,
              expected: 2147483647n,
            },
            out: {
              value: null,
              expected: 2147483646,
            },
          },
        },
        {
          name: 'String -> INTEGER (min value)',
          values: {
            in: {
              value: "-2147483648",
              expected: "-2147483648",
            },
            inout: {
              value: "-2147483647",
              expected: "-2147483648",
            },
            out: {
              value: null,
              expected: -2147483647,
            },
          },
        },
        {
          name: 'String -> INTEGER (max value)',
          values: {
            in: {
              value: "2147483647",
              expected: "2147483647",
            },
            inout: {
              value: "2147483646",
              expected: "2147483647",
            },
            out: {
              value: null,
              expected: 2147483646,
            },
          },
        },
      ],
    }
  ]
};
