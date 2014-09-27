{
  'includes': ['common.gypi'],
	'conditions': [
      ['OS == "mac"',{
        'make_global_settings': [
          ['CC','/opt/local/bin/clang++-mp-3.6'],
          ['CXX','/opt/local/bin/clang++-mp-3.6'],
          ['LINK','/opt/local/bin/clang++-mp-3.6'],
        ],
      }],
	],
  'target_defaults': {
    'msvs_settings': {
#     'msvs_precompiled_header': '../mordor/pch.h',
#     'msvs_precompiled_source': '../mordor/pch.cpp',
      'VCCLCompilerTool': {
        'WarningLevel': '4', # /W4
      },
    },
    'xcode_settings': {
      'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
      'GCC_PREFIX_HEADER': '../mordor/pch.h',
      'GCC_PRECOMPILE_PREFIX_HEADER': 'YES',
      'CLANG_CXX_LANGUAGE_STANDARD': 'c++11',
      'MACOSX_DEPLOYMENT_TARGET': '10.8', # OS X Deployment Target: 10.8
      'CLANG_CXX_LIBRARY': 'libc++', # libc++ requires OS X 10.7 or later
    },
    'conditions': [
      ['OS == "mac"',{
        "cflags": [ "<!@(llvm-config-mp-3.6 --cxxflags)" ],
        "cflags!": ['-funsigned-char'],
        "cflags_cc!": ['-funsigned-char'],
        "link_settings": {
          "ldflags": [ "<!@(llvm-config-mp-3.6 --ldflags)" ],
          "libraries": [
            '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          ],
        },
      }],
      ['OS == "linux"',{
        "link_settings": {
          "libraries": [
            '-lpthread',
          ],
        },
      }],
      ['OS != "win"',{
        "link_settings": {
          "libraries": [
#            '-L /usr/local/lib',
#            '-lssl',
#            '-lcrypto',
#            '-lboost_thread',
#            '-lboost_system',
#            '-llzma',
#            '-lz',
#            '-lpthread',
          ],
        },
        'cflags': ['-include ../mordor/pch.h'],
        'include_dirs': [
          '..',
          '../deps',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'mordor_pch',
      'product_name': 'pch.h.gch',
      'type': 'none',
      'cflags': ['-x c++-header'],
      'sources': [
        '../mordor/pch.h',
        '../mordor/pch.cpp',
      ]
    },
    {
      'target_name': 'mordor_base',
      'product_name': 'mordor_base',
      'type': 'static_library',
      'dependencies': [
        'mordor_pch',
      ],
      'sources': [
        '../mordor/assert.cpp',
        '../mordor/config.cpp',
        '../mordor/cxa_exception.cpp',
        '../mordor/date_time.cpp',
        '../mordor/string.cpp',
        '../mordor/statistics.cpp',
        '../mordor/sleep.cpp',
        '../mordor/fiber.cpp',
        '../mordor/fibersynchronization.cpp',
        '../mordor/workerpool.cpp',
        '../mordor/main.cpp',
        '../mordor/exception.cpp',
        '../mordor/semaphore.cpp',
        '../mordor/scheduler.cpp',
        '../mordor/socket.cpp',
        '../mordor/thread.cpp',
        '../mordor/type_name.cpp',
        '../mordor/timer.cpp',
        '../mordor/util.cpp',
        '../mordor/parallel.cpp',
        '../mordor/log.cpp',
        '../mordor/streams/buffer.cpp',
        '../mordor/streams/buffered.cpp',
        '../mordor/streams/cat.cpp',
        '../mordor/streams/counter.cpp',
        '../mordor/streams/stream.cpp',
        '../mordor/streams/std.cpp',
        '../mordor/streams/fd.cpp',
        '../mordor/streams/file.cpp',
        '../mordor/streams/filter.cpp',
        '../mordor/streams/limited.cpp',
        '../mordor/streams/memory.cpp',
        '../mordor/streams/null.cpp',
        '../mordor/streams/temp.cpp',
        '../mordor/streams/timeout.cpp',
        '../mordor/streams/singleplex.cpp',
        '../mordor/streams/socket_stream.cpp',
        '../mordor/streams/pipe.cpp',
        '../mordor/streams/transfer.cpp',
        '../mordor/streams/throttle.cpp',
        '../mordor/streams/test.cpp',
        '../mordor/streams/zero.cpp',
      ],
      'conditions': [
        ['OS == "linux"', {
          'sources':[
            '../mordor/iomanager_epoll.cpp',
          ]
        }],
        ['OS == "mac"', {
          'sources':[
            '../mordor/iomanager_kqueue.cpp',
          ]
        }],
        ['OS == "win"', {
          'sources':[
            '../mordor/eventloop.cpp',
            '../mordor/runtime_linking.cpp',
            '../mordor/iomanager_iocp.cpp',
            '../mordor/streams/handle.cpp',
            '../mordor/streams/efs.cpp',
            '../mordor/streams/namedpipe.cpp',
          ]
        }],
      ],
    },
    {
      'target_name': 'mordor_test',
      'product_name': 'md_test',
      'type': 'static_library',
      'dependencies': [
        'mordor_base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../mordor/test/stdoutlistener.cpp',
        '../mordor/test/antxmllistener.cpp',
        '../mordor/test/test.cpp',
        '../mordor/test/compoundlistener.cpp',
      ],
      'xcode_settings': {
      },
    }, # mordor_test
    {
      'target_name': 'tests_base',
      'product_name': 'tests_base',
      'type': 'executable',
      'dependencies': [
        'mordor_base',
        'mordor_test',
      ],
      'sources': [
        '../mordor/tests/atomic.cpp',
        '../mordor/tests/buffer.cpp',
        '../mordor/tests/config.cpp',
        '../mordor/tests/coroutine.cpp',
        '../mordor/tests/endian.cpp',
        '../mordor/tests/fibers.cpp',
        '../mordor/tests/fibersync.cpp',
        '../mordor/tests/fls.cpp',
        '../mordor/tests/future.cpp',
        '../mordor/tests/log.cpp',
        '../mordor/tests/iomanager.cpp',
        '../mordor/tests/scheduler.cpp',
        '../mordor/tests/string.cpp',
        '../mordor/tests/statistics.cpp',
        '../mordor/tests/thread.cpp',
        '../mordor/tests/timer.cpp',
        '../mordor/tests/unicode.cpp',
        '../mordor/tests/util.cpp',
        '../mordor/tests/socket.cpp',
        '../mordor/tests/stream.cpp',
        '../mordor/tests/buffered_stream.cpp',
        '../mordor/tests/counter_stream.cpp',
        '../mordor/tests/file_stream.cpp',
        '../mordor/tests/memory_stream.cpp',
#        '../mordor/tests/notify_stream.cpp',
        '../mordor/tests/pipe_stream.cpp',
        '../mordor/tests/temp_stream.cpp',
#        '../mordor/tests/timeout_stream.cpp',
        '../mordor/tests/transfer_stream.cpp',
        '../mordor/tests/run_tests.cpp',
      ],
    }, # tests
    {
      'target_name': 'cat',
      'product_name': 'cat',
      'type': 'executable',
      'dependencies': [
        'mordor_base',
      ],
      'sources': [
        '../mordor/examples/cat.cpp',
      ],
    }, # cat
  ] # targets
}
