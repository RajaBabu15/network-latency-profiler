#pragma once

#include <chrono>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <iostream>


#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define htobe16(x) OSSwapHostToBigInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#endif

namespace udp_benchmark {


using clock_ns = std::chrono::steady_clock;
using timestamp_t = uint64_t;
using sequence_t = uint64_t;


namespace config {
    constexpr int DEFAULT_BUFFER_SIZE = 4 * 1024 * 1024;
    constexpr int MIN_MESSAGE_SIZE = 16;
    constexpr int MAX_PACKET_SIZE = 2048;
    constexpr int DEFAULT_WINDOW_SIZE = 256;
    constexpr int DEFAULT_ACK_PERIOD = 1;
    constexpr uint64_t MIN_CWND = 10;
    constexpr uint64_t MAX_CWND = 10000;
}


struct Pending {
    sequence_t seq;
    timestamp_t send_ts_ns;
    int retransmits;

    Pending() : seq(0), send_ts_ns(0), retransmits(0) {}
    Pending(sequence_t s, timestamp_t ts, int rt = 0)
        : seq(s), send_ts_ns(ts), retransmits(rt) {}
};

struct PacketHeader {
    sequence_t seq;
    timestamp_t timestamp;
} __attribute__((packed));

struct AckHeader {
    sequence_t ack_seq;
    uint16_t bitmap_len;
} __attribute__((packed));


inline timestamp_t get_timestamp_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        clock_ns::now().time_since_epoch()
    ).count();
}

inline double timestamp_to_seconds(timestamp_t ts_ns) {
    return static_cast<double>(ts_ns) / 1e9;
}

inline double timestamp_diff_us(timestamp_t start_ns, timestamp_t end_ns) {
    return static_cast<double>(end_ns - start_ns) / 1000.0;
}


extern std::mutex g_log_mutex;

template<typename... Args>
void safe_log(Args&&... args) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    (std::cerr << ... << args);
    std::cerr.flush();
}

}