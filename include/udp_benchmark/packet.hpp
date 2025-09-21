#pragma once

#include "common.hpp"
#include <vector>
#include <cstring>

namespace udp_benchmark {

class Packet {
private:
    std::vector<uint8_t> data_;

public:
    explicit Packet(size_t size = config::MAX_PACKET_SIZE);


    uint8_t* data() { return data_.data(); }
    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    size_t capacity() const { return data_.capacity(); }
    void resize(size_t new_size) { data_.resize(new_size); }


    void set_sequence(sequence_t seq);
    void set_timestamp(timestamp_t ts);
    sequence_t get_sequence() const;
    timestamp_t get_timestamp() const;


    bool has_valid_header() const;
    static size_t header_size() { return sizeof(PacketHeader); }
};

class AckPacket {
private:
    std::vector<uint8_t> data_;

public:
    explicit AckPacket(size_t bitmap_bytes = config::DEFAULT_WINDOW_SIZE / 8);


    uint8_t* data() { return data_.data(); }
    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }


    void set_ack_sequence(sequence_t ack_seq);
    void set_bitmap_length(uint16_t len);
    void set_bitmap_bit(size_t index, bool value);

    sequence_t get_ack_sequence() const;
    uint16_t get_bitmap_length() const;
    bool get_bitmap_bit(size_t index) const;
    uint8_t* get_bitmap_data() { return data_.data() + sizeof(AckHeader); }
    const uint8_t* get_bitmap_data() const { return data_.data() + sizeof(AckHeader); }


    void clear_bitmap();
    static size_t header_size() { return sizeof(AckHeader); }
};

class PacketHandler {
public:

    static Packet create_data_packet(sequence_t seq, timestamp_t ts, size_t total_size);
    static AckPacket create_ack_packet(sequence_t ack_seq,
                                      const std::vector<sequence_t>& missing_seqs,
                                      size_t window_size = config::DEFAULT_WINDOW_SIZE);


    static bool parse_data_packet(const uint8_t* data, size_t size,
                                 sequence_t& seq, timestamp_t& ts);
    static bool parse_ack_packet(const uint8_t* data, size_t size,
                                sequence_t& ack_seq, std::vector<sequence_t>& missing_seqs);


    static bool is_valid_packet_size(size_t size);
    static bool is_valid_ack_size(size_t size);
};

}