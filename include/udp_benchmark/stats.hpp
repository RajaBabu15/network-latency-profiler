#pragma once

#include "common.hpp"
#include <string>
#include <fstream>
#include <vector>
#include <mutex>
#include <memory>

namespace udp_benchmark {


class LatencyLogger {
private:
    std::ofstream file_;
    std::mutex file_mutex_;
    std::string filename_;
    bool header_written_ = false;

public:
    explicit LatencyLogger(const std::string& filename);
    ~LatencyLogger();

    bool is_open() const { return file_.is_open(); }


    void log_sender_data(sequence_t seq, timestamp_t send_ts,
                        timestamp_t ack_recv_ts, int retransmits);


    void log_receiver_data(sequence_t seq, timestamp_t recv_ts,
                          timestamp_t send_ts);


    void log_csv_row(const std::vector<std::string>& values);

    void flush();
    void close();

private:
    void write_sender_header();
    void write_receiver_header();
};


struct LatencyStats {
    uint64_t packet_count = 0;
    uint64_t total_latency_ns = 0;
    uint64_t min_latency_ns = UINT64_MAX;
    uint64_t max_latency_ns = 0;

    std::vector<uint64_t> latencies;

    void add_latency(uint64_t latency_ns) {
        packet_count++;
        total_latency_ns += latency_ns;
        min_latency_ns = std::min(min_latency_ns, latency_ns);
        max_latency_ns = std::max(max_latency_ns, latency_ns);
        latencies.push_back(latency_ns);
    }

    double get_mean_latency_us() const {
        return packet_count > 0 ?
            static_cast<double>(total_latency_ns) / (packet_count * 1000.0) : 0.0;
    }

    double get_min_latency_us() const {
        return min_latency_ns != UINT64_MAX ?
            static_cast<double>(min_latency_ns) / 1000.0 : 0.0;
    }

    double get_max_latency_us() const {
        return static_cast<double>(max_latency_ns) / 1000.0;
    }

    uint64_t get_percentile_latency_ns(double percentile) const;
    double get_percentile_latency_us(double percentile) const;

    void reset() {
        packet_count = 0;
        total_latency_ns = 0;
        min_latency_ns = UINT64_MAX;
        max_latency_ns = 0;
        latencies.clear();
    }
};

struct ThroughputStats {
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    timestamp_t start_time = 0;
    timestamp_t end_time = 0;

    void start() { start_time = get_timestamp_ns(); }
    void end() { end_time = get_timestamp_ns(); }

    double get_duration_seconds() const {
        return end_time > start_time ?
            static_cast<double>(end_time - start_time) / 1e9 : 0.0;
    }

    double get_packet_rate() const {
        double duration = get_duration_seconds();
        return duration > 0 ? packets_sent / duration : 0.0;
    }

    double get_throughput_mbps() const {
        double duration = get_duration_seconds();
        return duration > 0 ? (bytes_sent * 8.0) / (duration * 1e6) : 0.0;
    }

    double get_loss_rate() const {
        return packets_sent > 0 ?
            static_cast<double>(packets_sent - packets_received) / packets_sent : 0.0;
    }

    void reset() {
        packets_sent = 0;
        packets_received = 0;
        bytes_sent = 0;
        bytes_received = 0;
        start_time = 0;
        end_time = 0;
    }
};

class StatsCollector {
private:
    LatencyStats latency_stats_;
    ThroughputStats throughput_stats_;
    mutable std::mutex stats_mutex_;


    uint64_t progress_interval_ = 1000;
    uint64_t last_progress_count_ = 0;
    timestamp_t last_progress_time_ = 0;

public:
    StatsCollector() = default;


    void add_latency_measurement(timestamp_t send_ts, timestamp_t recv_ts);
    void add_packet_sent(size_t bytes);
    void add_packet_received(size_t bytes);


    LatencyStats get_latency_stats() const;
    ThroughputStats get_throughput_stats() const;


    void set_progress_interval(uint64_t interval) { progress_interval_ = interval; }
    bool should_report_progress();
    void print_progress_summary();


    void start_collection();
    void end_collection();
    void reset();


    void print_final_summary() const;
    void print_latency_distribution() const;
};


class RateLimiter {
private:
    double target_rate_;
    double interval_us_;
    timestamp_t last_send_time_ = 0;

public:
    explicit RateLimiter(double rate_msgs_per_sec);

    void set_rate(double rate_msgs_per_sec);
    double get_rate() const { return target_rate_; }


    bool can_send();


    void wait_for_next_send();


    void mark_sent();
};


class ProgressReporter {
private:
    uint64_t total_work_;
    uint64_t completed_work_ = 0;
    timestamp_t start_time_;
    timestamp_t last_report_time_;
    uint64_t report_interval_;

public:
    ProgressReporter(uint64_t total_work, uint64_t report_interval = 1000);

    void update(uint64_t completed);
    void increment(uint64_t amount = 1) { update(completed_work_ + amount); }

    double get_progress_percentage() const;
    double get_estimated_remaining_seconds() const;
    void print_progress();

    bool is_complete() const { return completed_work_ >= total_work_; }
    void finish();
};

}