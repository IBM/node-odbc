module.exports.ibmi = [
  require('./BIGINT.js'),
  require('./BINARY.js'),
  require('./CHARACTER.js'),
  require('./DECIMAL.js'),
  require('./INTEGER.js'),
  require('./NUMERIC.js'),
  require('./SMALLINT.js'),
  require('./VARBINARY.js'),
  require('./VARCHAR.js'),
  require('./XML.js'),
];

// const berp = [
//   // { // TODO: Blob doesnt currently work
//   //   dataType: 'BLOB',
//   //   size: 16,
//   //   tests: [
//   //     {
//   //       name: 'Buffer -> BLOB',
//   //       values: {
//   //         in: {
//   //           value: Buffer.from('7468697320697320612074c3a97374', 'hex'),
//   //           expected: Buffer.from('7468697320697320612074c3a97374', 'hex'),
//   //         },
//   //         inout: {
//   //           value: Buffer.from('00112233445566778899aabbccddeeff', 'hex'),
//   //           expected: Buffer.from('7468697320697320612074c3a97374', 'hex'),
//   //         },
//   //         out: {
//   //           value: null,
//   //           expected: Buffer.from('00112233445566778899aabbccddeeff', 'hex'),
//   //         },
//   //       },
//   //     },
//   //     {
//   //       name: 'Buffer -> BLOB [empty buffer]',
//   //       values: {
//   //         in: {
//   //           value: Buffer.alloc(0),
//   //           expected: Buffer.alloc(0),
//   //         },
//   //         inout: {
//   //           value: Buffer.alloc(0),
//   //           expected: Buffer.alloc(16),
//   //         },
//   //         out: {
//   //           value: null,
//   //           expected: Buffer.alloc(16),
//   //         },
//   //       },
//   //     },
//   //     {
//   //       name: 'Buffer -> BLOB [null bytes]',
//   //       values: {
//   //         in: {
//   //           value: Buffer.alloc(16),
//   //           expected: Buffer.alloc(16),
//   //         },
//   //         inout: {
//   //           value: Buffer.alloc(16),
//   //           expected: Buffer.alloc(16),
//   //         },
//   //         out: {
//   //           value: null,
//   //           expected: Buffer.alloc(16),
//   //         },
//   //       },
//   //     },
//   //   ],
//   // },
//   // {
//   //   dataType: 'ROWID', // TODO: doesn't work in driver
//   //   size: null,
//   //   tests: [
//   //     {
//   //       name: 'Number -> ROWID',
//   //       values: {
//   //         in: {
//   //           value: 1,
//   //           expected: 1,
//   //         },
//   //         inout: {
//   //           value: 2,
//   //           expected: 1,
//   //         },
//   //         out: {
//   //           value: null,
//   //           expected: 2,
//   //         },
//   //       },
//   //     },
//   //   ],
//   // },
//   {
//     dataType: 'XML',
//     configurations: [
//       {
//         size: null,
//         options: null,
//         tests: [
//           {
//             name: 'string -> XML [empty]',
//             values: {
//               in: {
//                 value: '',
//                 expected: '',
//               },
//               inout: {
//                 value: ' ',
//                 expected: '',
//               },
//               out: {
//                 value: null,
//                 expected: ' ',
//               },
//             },
//           },
//           {
//             name: 'string -> VARCHAR (full)',
//             values: {
//               in: {
//                 value: `<note>
//                 <to>Tove</to>
//                 <from>Jani</from>
//                 <heading>Reminder</heading>
//                 <body>Don't forget me this weekend!</body>
//                 </note>`,
//                 expected: `<note>
//                 <to>Tove</to>
//                 <from>Jani</from>
//                 <heading>Reminder</heading>
//                 <body>Don't forget me this weekend!</body>
//                 </note>`,
//               },
//               inout: {
//                 value: `<note>
//                 <to>Tove</to>
//                 <from>Jani</from>
//                 <heading>Reminder</heading>
//                 <body>Don't forget me this weekend!</body>
//                 </note>`,
//                 expected: `<note>
//                 <to>Tove</to>
//                 <from>Jani</from>
//                 <heading>Reminder</heading>
//                 <body>Don't forget me this weekend!</body>
//                 </note>`,
//               },
//               out: {
//                 value: null,
//                 expected: `<note>
//                 <to>Tove</to>
//                 <from>Jani</from>
//                 <heading>Reminder</heading>
//                 <body>Don't forget me this weekend!</body>
//                 </note>`,
//               },
//             },
//           },
//         ],
//       },
//     ],
//   },
// ];