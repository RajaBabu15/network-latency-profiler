#pragma once

#include "common.hpp"
#include <atomic>

namespace udp_benchmark {

class CongestionController {
private:
    std::atomic<uint64_t> cwnd_;
    std::atomic<uint64_t> ssthresh_;
    std::atomic<uint64_t> inflight_;

    const uint64_t min_cwnd_;
    const uint64_t max_cwnd_;

public:
    explicit CongestionController(uint64_t initial_cwnd = 1000,
                                 uint64_t initial_ssthresh = 5000,
                                 uint64_t min_cwnd = config::MIN_CWND,
                                 uint64_t max_cwnd = config::MAX_CWND);


    uint64_t get_cwnd() const { return cwnd_.load(); }
    uint64_t get_ssthresh() const { return ssthresh_.load(); }
    uint64_t get_inflight() const { return inflight_.load(); }


    bool can_send() const;
    void packet_sent();
    void packet_acked();
    void packet_lost();


    void on_ack_received(bool has_loss = false);
    void on_timeout();
    void on_duplicate_ack();


    double get_utilization() const;
    void reset_stats();


    void set_min_cwnd(uint64_t min_cwnd);
    void set_max_cwnd(uint64_t max_cwnd);

protected:
    void increase_cwnd();
    void decrease_cwnd_on_loss();
    void enter_slow_start();
    void enter_congestion_avoidance();
};


struct CongestionStats {
    uint64_t total_acks = 0;
    uint64_t total_losses = 0;
    uint64_t total_timeouts = 0;
    uint64_t slow_start_events = 0;
    uint64_t congestion_avoidance_events = 0;

    timestamp_t last_reset_time = 0;

    void reset() {
        total_acks = 0;
        total_losses = 0;
        total_timeouts = 0;
        slow_start_events = 0;
        congestion_avoidance_events = 0;
        last_reset_time = get_timestamp_ns();
    }

    double get_loss_rate() const {
        uint64_t total_events = total_acks + total_losses;
        return total_events > 0 ? static_cast<double>(total_losses) / total_events : 0.0;
    }
};

class EnhancedCongestionController : public CongestionController {
private:
    CongestionStats stats_;
    bool verbose_logging_;

public:
    explicit EnhancedCongestionController(uint64_t initial_cwnd = 1000,
                                        uint64_t initial_ssthresh = 5000,
                                        bool verbose = false);


    void on_ack_received_with_stats(bool has_loss = false);
    void on_timeout_with_stats();


    const CongestionStats& get_stats() const { return stats_; }
    void reset_stats() { stats_.reset(); }


    void set_verbose_logging(bool verbose) { verbose_logging_ = verbose; }
    bool is_verbose_logging() const { return verbose_logging_; }
};

}