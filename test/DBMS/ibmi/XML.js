module.exports = {
  dataType: 'XML',
  configurations: [
    {
      size: null,
      options: null,
      tests: [
        {
          name: 'string -> XML [empty]',
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
          name: 'string -> XML (full)',
          values: {
            in: {
              value: `<note>
              <to>Tove</to>
              <from>Jani</from>
              <heading>Reminder</heading>
              <body>Don't forget me this weekend!</body>
              </note>`,
              expected: `<note>
              <to>Tove</to>
              <from>Jani</from>
              <heading>Reminder</heading>
              <body>Don't forget me this weekend!</body>
              </note>`,
            },
            inout: {
              value: `<note>
              <to>Tove</to>
              <from>Jani</from>
              <heading>Reminder</heading>
              <body>Don't forget me this weekend!</body>
              </note>`,
              expected: `<note>
              <to>Tove</to>
              <from>Jani</from>
              <heading>Reminder</heading>
              <body>Don't forget me this weekend!</body>
              </note>`,
            },
            out: {
              value: null,
              expected: `<note>
              <to>Tove</to>
              <from>Jani</from>
              <heading>Reminder</heading>
              <body>Don't forget me this weekend!</body>
              </note>`,
            },
          },
        },
      ],
    },
  ],
};
