#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <rte_byteorder.h>
#include <arpa/inet.h>

namespace ecpri {

// eCPRI Common Header (4 bytes)
struct __attribute__((packed)) CommonHeader {
    uint8_t  revision_c;   // bits[7:4]=revision(0x1), bits[3:1]=reserved, bit[0]=concat
    uint8_t  msg_type;     // 0x00 = IQ Data
    uint16_t payload_size; // size of payload in bytes (big-endian), EXCLUDING this header

    static void make(CommonHeader& h, uint16_t payload_bytes) {
        h.revision_c  = 0x1 << 4;
        h.msg_type    = 0x00;
        h.payload_size = rte_cpu_to_be_16(payload_bytes);
    }

    [[nodiscard]] uint16_t revision_c_ntoh() const {
        return rte_be_to_cpu_16(revision_c);
    }
    [[nodiscard]] uint16_t msg_type_ntoh() const {
        return rte_be_to_cpu_16(msg_type);
    }
    [[nodiscard]] uint16_t payload_size_ntoh() const {
        return rte_be_to_cpu_16(payload_size);
    }
};
static_assert(sizeof(CommonHeader) == 4);

// eCPRI IQ Data Payload Header (4 bytes before the IQ samples)
struct __attribute__((packed)) IQDataHeader {
    uint16_t pc_id;        // PC_ID  – identifies the stream
    uint16_t seq_id;       // SEQ_ID – rolling sequence number (big-endian)

    static void make(IQDataHeader& h, uint16_t pc, uint16_t seq) {
        h.pc_id  = rte_cpu_to_be_16(pc);
        h.seq_id = rte_cpu_to_be_16(seq);
    }

    [[nodiscard]] int16_t pc_id_ntoh() const {
        return rte_be_to_cpu_16(pc_id);
    }
    [[nodiscard]] int16_t seq_id_ntoh() const {
        return rte_be_to_cpu_16(seq_id);
    }
};
static_assert(sizeof(IQDataHeader) == 4);

// IQ sample
struct __attribute__((packed)) IQSample {
    int16_t i_be;
    int16_t q_be;
};
static_assert(sizeof(IQSample) == 4);

inline IQSample sample_hton(const IQSample& s) {
    return {
        static_cast<int16_t>(rte_cpu_to_be_16(s.i_be)),
        static_cast<int16_t>(rte_cpu_to_be_16(s.q_be))
    };
}

constexpr uint16_t SAMPS_PER_PKT = 256;
constexpr uint16_t PAYLOAD_SIZE = sizeof(IQDataHeader) + SAMPS_PER_PKT * sizeof(IQSample);

struct __attribute__((packed)) IQFrame {
    CommonHeader common;
    IQDataHeader iq_hdr;
    IQSample samples[SAMPS_PER_PKT];

    static void make_hdr(IQFrame& f, uint16_t pc_id, uint16_t seq) {
        CommonHeader::make(f.common, PAYLOAD_SIZE);
        IQDataHeader::make(f.iq_hdr, pc_id, seq);
    }
};

} // namespace ecpri
