#pragma once

#include <gnuradio/sync_block.h>

class iq_source : public gr::sync_block {
public:
    using sptr = std::shared_ptr<iq_source>;
    explicit iq_source(float sample_rate);
    static sptr make(float sample_rate);
    int work(int out_cnt, gr_vector_const_void_star& in_data, gr_vector_void_star& out_data) override;
private:
    float fs_;
    uint64_t sample_index_ = 0;
};