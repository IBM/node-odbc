{
  'targets' : [
    {
      'target_name' : 'odbc',
      'sources' : [ 
        'src/odbc.cpp',
        'src/odbc_connection.cpp',
        'src/odbc_statement.cpp',
        'src/odbc_result.cpp',
        'src/dynodbc.cpp'
      ],
      'ldflags': [
      '-Wl,-brtl,-bnoquiet /QopenSys/pkgs/lib/libodbc.so'
      ],
      'cflags' : ['-std=c++0x', '-DNAPI_DISABLE_CPP_EXCEPTIONS', '-Wall', '-Wextra', '-Wno-unused-parameter', '-I/QOpenSys/usr/include', '-I/QOpenSys/pkgs/include'],
      'include_dirs': [
        '<!@(node -p "require(\'node-addon-api\').include")',
      ],
      'defines' : [
      ],
      'conditions' : [
        [ 'OS == "linux"', {
          'libraries' : [ 
            '-lodbc',
            '-I/QOpenSys/usr/include'
          ],
          'cflags' : [
            '-g'
          ]
        }],
        [ 'OS == "mac"', {
          'include_dirs': [
            '<!@(node -p "require(\'node-addon-api\').include")',
            '/usr/local/include'
          ],
          'libraries' : [
            '-L/usr/local/lib',
            '-lodbc' 
          ]
        }],
        [ 'OS=="win"', {
          'sources' : [
            'src/strptime.c',
            'src/odbc.cpp'
          ],
          'libraries' : [ 
            '-lodbccp32.lib' 
          ]
        }],
        [ 'OS=="os400"', {
          
        }]
      ]
    }
  ]
}
