project(
    'dpdk-playground',
    'cpp',
    version : '0.1',
    default_options : [
        'cpp_std=c++26',
        'buildtype=debugoptimized'
    ]
)

dpdk = dependency('libdpdk')
gnuradio = dependency('gnuradio-runtime')
gnuradio_blocks = dependency('gnuradio-blocks')
fmt = dependency('fmt')
quill = dependency('quill')

executable(
    'dpdk-playground',
    [
        'src/main.cpp',
    ],
    dependencies : [
        gnuradio,
        gnuradio_blocks,
        dpdk,
        fmt,
        quill,
    ],
    cpp_args : [
        '-march=native',
        '-Wall',
        '-Wextra'
    ]
)