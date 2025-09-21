#include "udp_benchmark/stats.hpp"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <thread>

namespace udp_benchmark {


LatencyLogger::LatencyLogger(const std::string& filename) : filename_(filename) {
    file_.open(filename_);
    if (!file_.is_open()) {
        std::cerr << "Failed to open log file: " << filename_ << std::endl;
    }
}

LatencyLogger::~LatencyLogger() {
    close();
}

void LatencyLogger::log_sender_data(sequence_t seq, timestamp_t send_ts,
                                   timestamp_t ack_recv_ts, int retransmits) {
    std::lock_guard<std::mutex> lock(file_mutex_);
    if (!header_written_) {
        write_sender_header();
        header_written_ = true;
    }

    file_ << seq << "," << send_ts << "," << ack_recv_ts << "," << retransmits << "\n";
}

void LatencyLogger::log_receiver_data(sequence_t seq, timestamp_t recv_ts,
                                     timestamp_t send_ts) {
    std::lock_guard<std::mutex> lock(file_mutex_);
    if (!header_written_) {
        write_receiver_header();
        header_written_ = true;
    }

    file_ << seq << "," << recv_ts << "," << send_ts << "\n";
}

void LatencyLogger::write_sender_header() {
    file_ << "seq,send_ts_ns,ack_recv_ts_ns,retransmits\n";
}

void LatencyLogger::write_receiver_header() {
    file_ << "seq,recv_ts_ns,send_ts_ns\n";
}

void LatencyLogger::flush() {
    std::lock_guard<std::mutex> lock(file_mutex_);
    file_.flush();
}

void LatencyLogger::close() {
    if (file_.is_open()) {
        file_.close();
    }
}


uint64_t LatencyStats::get_percentile_latency_ns(double percentile) const {
    if (latencies.empty()) return 0;

    std::vector<uint64_t> sorted_latencies = latencies;
    std::sort(sorted_latencies.begin(), sorted_latencies.end());

    size_t index = static_cast<size_t>(percentile * (sorted_latencies.size() - 1) / 100.0);
    return sorted_latencies[index];
}

double LatencyStats::get_percentile_latency_us(double percentile) const {
    return static_cast<double>(get_percentile_latency_ns(percentile)) / 1000.0;
}


void StatsCollector::start_collection() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    throughput_stats_.start();
    last_progress_time_ = get_timestamp_ns();
}

void StatsCollector::end_collection() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    throughput_stats_.end();
}

void StatsCollector::add_latency_measurement(timestamp_t send_ts, timestamp_t recv_ts) {
    if (recv_ts > send_ts) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        latency_stats_.add_latency(recv_ts - send_ts);
    }
}

void StatsCollector::add_packet_sent(size_t bytes) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    throughput_stats_.packets_sent++;
    throughput_stats_.bytes_sent += bytes;
}

void StatsCollector::add_packet_received(size_t bytes) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    throughput_stats_.packets_received++;
    throughput_stats_.bytes_received += bytes;
}

LatencyStats StatsCollector::get_latency_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return latency_stats_;
}

ThroughputStats StatsCollector::get_throughput_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return throughput_stats_;
}

bool StatsCollector::should_report_progress() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    timestamp_t now = get_timestamp_ns();
    if (throughput_stats_.packets_received % progress_interval_ == 0 || 
        (now - last_progress_time_) > 1000000000) {
        last_progress_time_ = now;
        return true;
    }
    return false;
}

void StatsCollector::reset() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    latency_stats_.reset();
    throughput_stats_.reset();
}

void StatsCollector::print_final_summary() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    std::cout << "\n=== Final Statistics ===\n";
    std::cout << std::fixed << std::setprecision(2);
    
    if (latency_stats_.packet_count > 0) {
        std::cout << "Latency Statistics:\n";
        std::cout << "  Packets: " << latency_stats_.packet_count << "\n";
        std::cout << "  Mean: " << latency_stats_.get_mean_latency_us() << " μs\n";
        std::cout << "  Min: " << latency_stats_.get_min_latency_us() << " μs\n";
        std::cout << "  Max: " << latency_stats_.get_max_latency_us() << " μs\n";
        std::cout << "  p50: " << latency_stats_.get_percentile_latency_us(50.0) << " μs\n";
        std::cout << "  p99: " << latency_stats_.get_percentile_latency_us(99.0) << " μs\n";
    }
    
    std::cout << "\nThroughput Statistics:\n";
    std::cout << "  Duration: " << throughput_stats_.get_duration_seconds() << " seconds\n";
    std::cout << "  Packet rate: " << throughput_stats_.get_packet_rate() << " pps\n";
    std::cout << "  Throughput: " << throughput_stats_.get_throughput_mbps() << " Mbps\n";
    std::cout << "  Loss rate: " << (throughput_stats_.get_loss_rate() * 100) << "%\n";
}

void RateLimiter::wait_for_next_send() {
    while (!can_send()) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    mark_sent();
}

ProgressReporter::ProgressReporter(uint64_t total_work, uint64_t report_interval) 
    : total_work_(total_work), report_interval_(report_interval) {
    start_time_ = get_timestamp_ns();
    last_report_time_ = start_time_;
}

void ProgressReporter::update(uint64_t completed) {
    completed_work_ = completed;
}

double ProgressReporter::get_progress_percentage() const {
    return total_work_ > 0 ? (static_cast<double>(completed_work_) * 100.0 / total_work_) : 0.0;
}

void ProgressReporter::print_progress() {
    timestamp_t now = get_timestamp_ns();
    double elapsed_sec = static_cast<double>(now - start_time_) / 1e9;
    double rate = elapsed_sec > 0 ? completed_work_ / elapsed_sec : 0;
    
    std::cout << "\rProgress: " << completed_work_ << "/" << total_work_ 
              << " (" << static_cast<int>(get_progress_percentage()) << "%)"
              << " Rate: " << static_cast<int>(rate) << " msgs/sec" << std::flush;
    
    last_report_time_ = now;
}

void ProgressReporter::finish() {
    completed_work_ = total_work_;
    print_progress();
    std::cout << "\n";
}


RateLimiter::RateLimiter(double rate_msgs_per_sec) {
    set_rate(rate_msgs_per_sec);
}

void RateLimiter::set_rate(double rate_msgs_per_sec) {
    target_rate_ = rate_msgs_per_sec;
    interval_us_ = rate_msgs_per_sec > 0 ? 1000000.0 / rate_msgs_per_sec : 0;
}

bool RateLimiter::can_send() {
    timestamp_t now = get_timestamp_ns();
    timestamp_t elapsed_us = (now - last_send_time_) / 1000;
    return elapsed_us >= interval_us_;
}

void RateLimiter::mark_sent() {
    last_send_time_ = get_timestamp_ns();
}

}