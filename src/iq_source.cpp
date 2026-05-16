#include "iq_source.h"

#include <gnuradio/io_signature.h>

#include <cmath>

iq_source::sptr iq_source::make(float sample_rate)
{
    return std::make_shared<iq_source>(sample_rate);
}

iq_source::iq_source(float sample_rate)
    :
    gr::sync_block("iq_source",
        gr::io_signature::make(0, 0, 0),
        gr::io_signature::make(1, 1, sizeof(gr_complex))),
    fs_(sample_rate)
{
}

int iq_source::work(int out_cnt, gr_vector_const_void_star&, gr_vector_void_star& out_data)
{
    auto* out = static_cast<gr_complex*>(out_data[0]);

    for (int i = 0; i < out_cnt; ++i) {

        const float t = static_cast<float>(sample_index_) / fs_;

        const float signal = std::sin(2.0f * std::numbers::pi * 1e3f * t) +
            0.5f * std::sin(2.0f * std::numbers::pi * 5e3f * t) +
            0.25f * std::sin(2.0f * std::numbers::pi * 15e3f * t);

        // complex baseband
        out[i] = gr_complex(signal, 0.0f);

        ++sample_index_;
    }

    return out_cnt;
}