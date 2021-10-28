module.exports = {
  dataType: 'BIGINT',
  configurations: [
    {
      size: null,
      options: null,
      tests: [
        // These tests only run if the N-API version supports BigInt
        ...(parseInt(process.versions.napi) >= 5 ? [
          {
            name: 'BigInt -> BIGINT (min value)',
            values: {
              in: {
                value: -9223372036854775807n,
                expected: -9223372036854775807n,
              },
              inout: {
                value: -9223372036854775808n,
                expected: -9223372036854775807n,
              },
              out: {
                value: null,
                expected: -9223372036854775808n,
              },
            }
          },
          {
            name: 'BigInt -> BIGINT (max value)',
            values: {
              in: {
                value: 9223372036854775806n,
                expected: 9223372036854775806n,
              },
              inout: {
                value: 9223372036854775807n,
                expected: 9223372036854775806n,
              },
              out: {
                value: null,
                expected: 9223372036854775807n,
              },
            },
          },
        ] : []),
        {
          name: 'Number -> BIGINT [MIN_SAFE_INTEGER]',
          values: {
            in: {
              value: Number.MIN_SAFE_INTEGER + 1,
              expected: Number.MIN_SAFE_INTEGER + 1,
            },
            inout: {
              value: Number.MIN_SAFE_INTEGER,
              expected: Number.MIN_SAFE_INTEGER + 1,
            },
            out: {
              value: null,
              expected: BigInt(Number.MIN_SAFE_INTEGER),
            },
          },
        },
        {
          name: 'Number -> BIGINT [MAX_SAFE_INTEGER]',
          values: {
            in: {
              value: Number.MAX_SAFE_INTEGER - 1,
              expected: Number.MAX_SAFE_INTEGER - 1,
            },
            inout: {
              value: Number.MAX_SAFE_INTEGER,
              expected: Number.MAX_SAFE_INTEGER - 1,
            },
            out: {
              value: null,
              expected: BigInt(Number.MAX_SAFE_INTEGER),
            },
          },
        },
        {
          name: 'String -> BIGINT (min value)',
          values: {
            in: {
              value: "-9223372036854775807",
              expected: "-9223372036854775807",
            },
            inout: {
              value: "-9223372036854775808",
              expected: "-9223372036854775807",
            },
            out: {
              value: null,
              expected: -9223372036854775808n,
            },
          },
        },
        {
          name: 'String -> BIGINT (max value)',
          values: {
            in: {
              value: "9223372036854775806",
              expected: "9223372036854775806",
            },
            inout: {
              value: "9223372036854775807",
              expected: "9223372036854775806",
            },
            out: {
              value: null,
              expected: 9223372036854775807n,
            },
          },
        },
      ],
    },
  ],
};
