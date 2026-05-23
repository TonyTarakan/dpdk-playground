add_executable(ecpri_receiver main.cpp)

target_compile_options(ecpri_receiver PRIVATE
    ${DPDK_CFLAGS_OTHER}
    -march=native
    -O3
)

target_include_directories(ecpri_receiver PRIVATE
    ${DPDK_INCLUDE_DIRS}
)

find_package(kissfft REQUIRED)
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

target_link_libraries(ecpri_receiver PRIVATE
    ${DPDK_LIBRARIES}
    -Wl,--whole-archive
    -lrte_net_af_packet
    -lrte_net_tap
    -Wl,--no-whole-archive
    kissfft::kissfft
    sfml-graphics
    sfml-window
    sfml-system
    pthread dl numa m
)
