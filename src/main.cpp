#include "iq_source.h"

#include <gnuradio/top_block.h>
#include <gnuradio/blocks/file_sink.h>

#include <thread>
#include <chrono>

int main()
{
    const auto tb = gr::make_top_block("iq_generator");
    const auto src = iq_source::make(1e6f);
    const auto sink = gr::blocks::file_sink::make(sizeof(gr_complex), "iq_samples.cfile");

    tb->connect(src, 0, sink, 0);
    tb->start();

    std::this_thread::sleep_for(std::chrono::seconds(5l));

    tb->stop();
    tb->wait();

    return 0;
}