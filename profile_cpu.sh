#!/usr/bin/env bash
# profile_cpu.sh - CPU profiling and performance monitoring

set -e

# Configuration
PORT=9000
MSG_SIZE=128
RATE=10000
TOTAL=50000
DURATION=10

echo "=== CPU Performance Profiling ==="
echo "Duration: $DURATION seconds"
echo ""

# Start receiver with monitoring
echo "Starting receiver with monitoring..."
./udp_receiver $PORT profile_recv.csv &
RECV_PID=$!

# Monitor receiver CPU
(while kill -0 $RECV_PID 2>/dev/null; do
    ps -p $RECV_PID -o %cpu,rss,vsz,comm | tail -1
    sleep 1
done) > receiver_cpu.log &

echo "Receiver PID: $RECV_PID"
sleep 1

# Start sender with monitoring
echo "Starting sender..."
./udp_sender_simple 127.0.0.1 $PORT $MSG_SIZE $RATE $TOTAL profile_send.csv &
SEND_PID=$!

# Monitor sender CPU
(while kill -0 $SEND_PID 2>/dev/null; do
    ps -p $SEND_PID -o %cpu,rss,vsz,comm | tail -1
    sleep 1
done) > sender_cpu.log &

echo "Sender PID: $SEND_PID"

# Wait for completion
wait $SEND_PID

# Kill receiver
sleep 2
kill $RECV_PID 2>/dev/null || true

# Generate report
echo ""
echo "=== CPU Usage Summary ==="
echo "Sender CPU usage:"
awk '{sum+=$1; n++} END {print "  Average: " sum/n "%"}' sender_cpu.log

echo "Receiver CPU usage:"
awk '{sum+=$1; n++} END {print "  Average: " sum/n "%"}' receiver_cpu.log

# Analyze latency
echo ""
echo "=== Latency Results ==="
python3 analyze.py profile_send.csv profile_recv.csv 2>/dev/null | grep -E "Median|p99|Throughput"

# Cleanup
rm -f profile_*.csv sender_cpu.log receiver_cpu.log

echo ""
echo "✅ Profiling complete"