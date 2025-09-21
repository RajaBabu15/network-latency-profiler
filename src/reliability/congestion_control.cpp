#include "udp_benchmark/congestion_control.hpp"
#include <algorithm>
#include <iostream>

namespace udp_benchmark {


CongestionController::CongestionController(uint64_t initial_cwnd, uint64_t initial_ssthresh,
                                         uint64_t min_cwnd, uint64_t max_cwnd)
    : cwnd_(initial_cwnd), ssthresh_(initial_ssthresh), inflight_(0),
      min_cwnd_(min_cwnd), max_cwnd_(max_cwnd) {}

bool CongestionController::can_send() const {
    return inflight_.load() < cwnd_.load();
}

void CongestionController::packet_sent() {
    inflight_.fetch_add(1);
}

void CongestionController::packet_acked() {
    if (inflight_.load() > 0) {
        inflight_.fetch_sub(1);
    }
}

void CongestionController::packet_lost() {
    if (inflight_.load() > 0) {
        inflight_.fetch_sub(1);
    }
}

void CongestionController::on_ack_received(bool has_loss) {
    if (has_loss) {
        decrease_cwnd_on_loss();
    } else {
        increase_cwnd();
    }
}

void CongestionController::on_timeout() {

    decrease_cwnd_on_loss();
    enter_slow_start();
}

void CongestionController::on_duplicate_ack() {

    decrease_cwnd_on_loss();
}

double CongestionController::get_utilization() const {
    uint64_t cwnd = cwnd_.load();
    uint64_t inflight = inflight_.load();
    return cwnd > 0 ? static_cast<double>(inflight) / cwnd : 0.0;
}

void CongestionController::reset_stats() {
    inflight_.store(0);
}

void CongestionController::set_min_cwnd(uint64_t min_cwnd) {
    const_cast<uint64_t&>(min_cwnd_) = min_cwnd;
}

void CongestionController::set_max_cwnd(uint64_t max_cwnd) {
    const_cast<uint64_t&>(max_cwnd_) = max_cwnd;
}

void CongestionController::increase_cwnd() {
    uint64_t current_cwnd = cwnd_.load();
    uint64_t current_ssthresh = ssthresh_.load();
    uint64_t new_cwnd;

    if (current_cwnd < current_ssthresh) {

        new_cwnd = std::min(current_cwnd * 2, max_cwnd_);
    } else {

        new_cwnd = std::min(current_cwnd + 1, max_cwnd_);
    }

    cwnd_.store(new_cwnd);
}

void CongestionController::decrease_cwnd_on_loss() {
    uint64_t current_cwnd = cwnd_.load();
    uint64_t new_cwnd = std::max(current_cwnd / 2, min_cwnd_);
    uint64_t new_ssthresh = std::max(current_cwnd / 2, min_cwnd_);

    cwnd_.store(new_cwnd);
    ssthresh_.store(new_ssthresh);
}

void CongestionController::enter_slow_start() {
    cwnd_.store(min_cwnd_);
}

void CongestionController::enter_congestion_avoidance() {
    uint64_t current_cwnd = cwnd_.load();
    ssthresh_.store(current_cwnd);
}


EnhancedCongestionController::EnhancedCongestionController(uint64_t initial_cwnd,
                                                         uint64_t initial_ssthresh,
                                                         bool verbose)
    : CongestionController(initial_cwnd, initial_ssthresh), verbose_logging_(verbose) {
    stats_.reset();
}

void EnhancedCongestionController::on_ack_received_with_stats(bool has_loss) {
    stats_.total_acks++;

    if (has_loss) {
        stats_.total_losses++;
        if (verbose_logging_) {
            uint64_t current_cwnd = get_cwnd();
            safe_log("LOSS event: cwnd=", current_cwnd, " -> ");
        }
        decrease_cwnd_on_loss();
        if (verbose_logging_) {
            safe_log(get_cwnd(), " (loss rate: ",
                    static_cast<int>(stats_.get_loss_rate() * 100), "%)\n");
        }
    } else {
        uint64_t old_cwnd = get_cwnd();
        if (old_cwnd < get_ssthresh()) {
            stats_.slow_start_events++;
        } else {
            stats_.congestion_avoidance_events++;
        }

        increase_cwnd();

        if (verbose_logging_) {
            uint64_t new_cwnd = get_cwnd();
            if (new_cwnd != old_cwnd) {
                safe_log("CWND increase: ", old_cwnd, " -> ", new_cwnd, "\n");
            }
        }
    }


    CongestionController::on_ack_received(has_loss);
}

void EnhancedCongestionController::on_timeout_with_stats() {
    stats_.total_timeouts++;

    if (verbose_logging_) {
        uint64_t old_cwnd = get_cwnd();
        safe_log("TIMEOUT event: cwnd=", old_cwnd, " -> ");
    }

    on_timeout();

    if (verbose_logging_) {
        safe_log(get_cwnd(), " (entering slow start)\n");
    }
}

}