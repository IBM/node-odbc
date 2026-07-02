{
  'targets' : [
    {
      'target_name' : 'odbc',
      'sources' : [
        'src/odbc.cpp',
        'src/odbc_connection.cpp',
        'src/odbc_statement.cpp',
        'src/odbc_cursor.cpp',
        'src/dynodbc.cpp'
      ],
      'cflags' : ['-Wall', '-Wextra', '-Wno-unused-parameter', '-DNAPI_DISABLE_CPP_EXCEPTIONS'],
      'include_dirs': [
        '<!@(node -p "require(\'node-addon-api\').include")'
      ],
      'defines' : [
        'NAPI_VERSION=<(napi_build_version)'
      ],
      'conditions' : [
        [ 'OS == "linux"', {
          'libraries' : [
            '-lodbc'
          ],
          'cflags' : [
            '-g'
          ]
        }],
        [ 'OS == "mac"', {
          'conditions': [
            [ 'target_arch=="arm64"', {
              'include_dirs': [
                '/opt/homebrew/include'
              ],
              'libraries' : [
                '-L/opt/homebrew/lib',
                '-lodbc'
              ],
            }], ['target_arch=="x64"', {
              'include_dirs': [
                '/usr/local/include',
              ],
              'libraries' : [
                '-L/usr/local/lib',
                '-lodbc'
              ],
            }],
          ],
          'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ]
        }],
        [ 'OS == "freebsd"', {
          'include_dirs': [
            '/usr/local/include'
          ],
          'libraries' : [
            '-L/usr/local/lib',
            '-lodbc'
          ],
          'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ]
        }],
        [ 'OS=="win"', {
          'sources' : [
            'src/odbc.cpp'
          ],
          'libraries' : [
            '-lodbccp32.lib'
          ],
          'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS', 'UNICODE' ]
        }],
        [ 'OS=="aix"', {
          'variables': {
            'os_name': '<!(uname -s)',
          },
          'conditions': [
             [ '"<(os_name)"=="OS400"', {
               'ldflags': [
                  '-Wl,-brtl,-blibpath:/QOpenSys/pkgs/lib,-lodbc'
                ],
                'cflags' : ['-std=c++0x', '-DNAPI_DISABLE_CPP_EXCEPTIONS', '-Wall', '-Wextra', '-Wno-unused-parameter', '-I/QOpenSys/usr/include', '-I/QOpenSys/pkgs/include']
             }]
          ]
        }],
        [ 'OS=="os400"', {
          'ldflags': ['-Wl,-blibpath:/QOpenSys/pkgs/lib', '-lodbc'],
          'cflags' : ['-std=c++0x', '-DNAPI_DISABLE_CPP_EXCEPTIONS', '-Wall', '-Wextra', '-Wno-unused-parameter', '-I/QOpenSys/usr/include', '-I/QOpenSys/pkgs/include']
        }]
      ]
    },
    {
      "target_name": "action_after_build",
      "type": "none",
      "dependencies": [ "<(module_name)" ],
      "copies": [
        {
          "files": [ "<(PRODUCT_DIR)/<(module_name).node" ],
          "destination": "<(module_path)"
        }
      ]
    }
  ]
}
