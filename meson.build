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
fmt = dependency('fmt')
quill = dependency('quill')

inc = include_directories('include')

executable(
    'ecpri-generator',
    [
        'src/generator/main.cpp',
    ],
    include_directories : inc,
    dependencies : [
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