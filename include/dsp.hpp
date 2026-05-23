#pragma once

#include "ecpri.hpp"
#include "dpdk_port.hpp"

#include <rte_ring.h>
#include <rte_mbuf.h>
#include <rte_lcore.h>

#include <kissfft/kiss_fft.h>

#include <atomic>
#include <array>
#include <cmath>
#include <stdexcept>

namespace dsp {

constexpr int FFT_SIZE = 4096;
constexpr int FRAME_POOL_SIZE = 8;

struct SpectrumFrame {
    float bins[FFT_SIZE];
};

class DspCore final {
public:
    DspCore(rte_ring* iq_ring, rte_ring* frame_ring) : iq_ring_(iq_ring), frame_ring_(frame_ring)
    {
        cfg_ = kiss_fft_alloc(FFT_SIZE, 0, nullptr, nullptr);
        if (!cfg_) throw std::runtime_error("kiss_fft_alloc failed");

        recycle_ = rte_ring_create("dsp_recycle", FRAME_POOL_SIZE * 2,
                                    rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (!recycle_) throw std::runtime_error("rte_ring_create dsp_recycle failed");
        for (int i = 0; i < FRAME_POOL_SIZE; ++i)
            rte_ring_enqueue(recycle_, &pool_[i]);
    }

    ~DspCore() {
        kiss_fft_free(cfg_);
        rte_ring_free(recycle_);
    }

    static int lcore_entry(void* arg) {
        return static_cast<DspCore*>(arg)->run();
    }

    void request_stop() {
        stop_.store(true, std::memory_order_relaxed);
    }

    void recycle_frame(SpectrumFrame* f) const {
        rte_ring_enqueue(recycle_, f);
    }

private:
    int run() {
        int fill = 0;

        std::printf("[dsp] lcore %u started\n", rte_lcore_id());

        while (!stop_.load(std::memory_order_relaxed)) {
            rte_mbuf* mbufs[dpdk::BURST_SIZE];
            const uint16_t n = rte_ring_dequeue_burst(
                iq_ring_, reinterpret_cast<void**>(mbufs),
                dpdk::BURST_SIZE, nullptr);

            if (n == 0) { rte_pause(); continue; }

            for (uint16_t i = 0; i < n; ++i) {
                rte_mbuf* m = mbufs[i];

                const char* base = rte_pktmbuf_mtod(m, const char*);
                const auto* ch   = reinterpret_cast<const ecpri::CommonHeader*>(
                    base + sizeof(dpdk::EthHeader));
                const auto* ws   = reinterpret_cast<const ecpri::IQSample*>(
                    base + sizeof(dpdk::EthHeader)
                         + sizeof(ecpri::CommonHeader)
                         + sizeof(ecpri::IQDataHeader));

                const uint32_t ns =
                    (ch->payload_size_ntoh() - static_cast<uint16_t>(sizeof(ecpri::IQDataHeader)))
                    / static_cast<uint32_t>(sizeof(ecpri::IQSample));

                for (uint32_t k = 0; k < ns && fill < FFT_SIZE; ++k, ++fill) {
                    fin_[fill].r = rte_be_to_cpu_16(ws[k].i_be) * (1.0f / 32768.0f);
                    fin_[fill].i = rte_be_to_cpu_16(ws[k].q_be) * (1.0f / 32768.0f);
                }

                rte_pktmbuf_free(m);

                if (fill >= FFT_SIZE) {
                    publish();
                    fill = 0;
                }
            }
        }

        std::printf("[dsp] lcore %u stopped\n", rte_lcore_id());
        return 0;
    }

    void publish() {
        kiss_fft(cfg_, fin_, fout_);

        void* ptr = nullptr;
        if (rte_ring_dequeue(recycle_, &ptr) != 0) return;
        auto* frame = static_cast<SpectrumFrame*>(ptr);

        for (int k = 0; k < FFT_SIZE; ++k) {
            // kinda spectrum
            frame->bins[k] = fout_[k].r * fout_[k].r + fout_[k].i * fout_[k].i;
        }

        if (rte_ring_enqueue(frame_ring_, frame) != 0) {
            void* old = nullptr;
            if (rte_ring_dequeue(frame_ring_, &old) == 0)
                rte_ring_enqueue(recycle_, old);
            rte_ring_enqueue(frame_ring_, frame);
        }
    }

    rte_ring*     iq_ring_;
    rte_ring*     frame_ring_;
    rte_ring*     recycle_{};

    kiss_fft_cfg  cfg_{};
    kiss_fft_cpx  fin_[FFT_SIZE]{};
    kiss_fft_cpx  fout_[FFT_SIZE]{};

    std::array<SpectrumFrame, FRAME_POOL_SIZE> pool_{};
    std::atomic<bool> stop_{false};
};

} // namespace dsp
