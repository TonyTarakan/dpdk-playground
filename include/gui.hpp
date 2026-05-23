#pragma once

#include "dsp.hpp"

#include <SFML/Graphics.hpp>
#include <rte_ring.h>

#include <algorithm>
#include <cstdio>

namespace gui {

constexpr int WIN_W    = 1280;
constexpr int WIN_H    = 600;
constexpr int MARGIN_L = 10;
constexpr int MARGIN_R = 10;
constexpr int MARGIN_T = 10;
constexpr int MARGIN_B = 10;

class SpectrumView final {
public:
    SpectrumView() {
        window_.create(sf::VideoMode(WIN_W, WIN_H), "eCPRI IQ Spectrum");
        window_.setFramerateLimit(60);
        bars_.setPrimitiveType(sf::LineStrip);
        bars_.resize(dsp::FFT_SIZE);
    }

    bool is_open() const {
        return window_.isOpen();
    }

    void update(dsp::DspCore& dsp, rte_ring* frame_ring) {
        sf::Event ev{};
        while (window_.pollEvent(ev)) {
            if (ev.type == sf::Event::Closed) {
                window_.close();
            }
        }

        void* ptr = nullptr;
        if (rte_ring_dequeue(frame_ring, &ptr) == RTE_FLOW_OP_SUCCESS) {
            if (current_) {
                dsp.recycle_frame(current_);
            }
            current_ = static_cast<dsp::SpectrumFrame*>(ptr);
        }

        window_.clear(sf::Color(16, 16, 16));
        if (current_) {
            draw_bars(*current_);
        }
        window_.display();
    }

private:
    void draw_bars(const dsp::SpectrumFrame& f) {
        // Mirror
        constexpr int N = dsp::FFT_SIZE / 2;

        float max_val = std::numeric_limits<float>::lowest();
        for (int k = 0; k < N; ++k) {
            max_val = std::max(max_val, f.bins[k]);
        }

        constexpr auto plot_w = static_cast<float>(WIN_W - MARGIN_L - MARGIN_R);
        constexpr auto plot_h = static_cast<float>(WIN_H - MARGIN_T - MARGIN_B);
        constexpr auto bottom = static_cast<float>(WIN_H - MARGIN_B);
        constexpr float dx = plot_w / static_cast<float>(N);

        for (int k = 0; k < N; ++k) {
            const float norm = f.bins[k] / max_val;
            const float h = norm * plot_h;
            const float x = MARGIN_L + static_cast<float>(k) * dx;

            bars_[2 * k + 0].position = {x, bottom - h};
            bars_[2 * k + 1].position = {x, bottom};
            bars_[2 * k + 0].color = sf::Color(220, 180, 40);
            bars_[2 * k + 1].color = sf::Color(220, 180, 40);
        }
        window_.draw(bars_);
    }

    sf::RenderWindow    window_;
    sf::VertexArray     bars_;
    dsp::SpectrumFrame* current_{nullptr};
};

} // namespace gui
