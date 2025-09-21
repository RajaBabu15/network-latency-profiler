#!/usr/bin/env bash
# run_benchmark.sh - Run UDP latency benchmarks with various network conditions
# Usage: ./run_benchmark.sh [receiver_ip] [port]

set -e

# Configuration
RECV_IP=${1:-127.0.0.1}
PORT=${2:-9000}
MSG_SIZE=${3:-128}
RATE=${4:-10000}  # msgs/sec
TOTAL=${5:-100000}

# Output directories
RESULTS_DIR="results/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

# Log files
SENDER_LOG="$RESULTS_DIR/sender_log.csv"
RECV_LOG="$RESULTS_DIR/recv_log.csv"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}UDP Latency Benchmark${NC}"
echo "======================================"
echo "Configuration:"
echo "  Receiver IP: $RECV_IP"
echo "  Port: $PORT"
echo "  Message Size: $MSG_SIZE bytes"
echo "  Rate: $RATE msgs/sec"
echo "  Total Messages: $TOTAL"
echo "  Results Directory: $RESULTS_DIR"
echo "======================================"

# Function to check if we're on macOS or Linux
detect_os() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    else
        echo "unknown"
    fi
}

OS=$(detect_os)

# Function to run benchmark
run_test() {
    local test_name=$1
    local netem_cmd=$2
    
    echo -e "\n${YELLOW}Running test: $test_name${NC}"
    
    # Create test-specific directory
    TEST_DIR="$RESULTS_DIR/$test_name"
    mkdir -p "$TEST_DIR"
    
    # Apply network emulation (Linux only)
    if [[ "$OS" == "linux" && -n "$netem_cmd" ]]; then
        echo "Applying netem: $netem_cmd"
        sudo tc qdisc del dev lo root 2>/dev/null || true
        if [[ "$netem_cmd" != "none" ]]; then
            sudo tc qdisc add dev lo root $netem_cmd
        fi
    fi
    
    # Start receiver in background
    echo "Starting receiver..."
    ./udp_receiver $PORT "$TEST_DIR/recv_log.csv" &
    RECV_PID=$!
    echo "Receiver PID: $RECV_PID"
    sleep 1
    
    # CPU pinning (Linux only)
    if [[ "$OS" == "linux" ]]; then
        # Pin receiver to core 2
        taskset -cp 2 $RECV_PID 2>/dev/null || echo "Note: CPU pinning not available"
    fi
    
    # Run sender
    echo "Starting sender..."
    if [[ "$OS" == "linux" ]]; then
        # Pin sender to core 3
        taskset -c 3 ./udp_sender $RECV_IP $PORT $MSG_SIZE $RATE $TOTAL "$TEST_DIR/sender_log.csv"
    else
        ./udp_sender $RECV_IP $PORT $MSG_SIZE $RATE $TOTAL "$TEST_DIR/sender_log.csv"
    fi
    
    # Wait for receiver to process final messages
    sleep 2
    
    # Kill receiver
    kill $RECV_PID 2>/dev/null || true
    wait $RECV_PID 2>/dev/null || true
    
    # Clean up netem (Linux only)
    if [[ "$OS" == "linux" && -n "$netem_cmd" ]]; then
        sudo tc qdisc del dev lo root 2>/dev/null || true
    fi
    
    echo -e "${GREEN}Test $test_name completed${NC}"
    
    # Run analysis on this test
    if [ -f analyze.py ]; then
        echo "Analyzing results..."
        python3 analyze.py "$TEST_DIR/sender_log.csv" "$TEST_DIR/recv_log.csv" > "$TEST_DIR/analysis.txt" 2>&1 || true
        cat "$TEST_DIR/analysis.txt"
    fi
}

# Build the programs if not already built
if [ ! -f udp_sender ] || [ ! -f udp_receiver ]; then
    echo -e "${YELLOW}Building programs...${NC}"
    make clean
    make
fi

# Run test scenarios
echo -e "\n${GREEN}Starting benchmark suite...${NC}"

if [[ "$OS" == "linux" ]]; then
    # Full test suite for Linux
    run_test "01_baseline" "none"
    run_test "02_delay_1ms" "netem delay 1ms 0.2ms"
    run_test "03_loss_0.1pct" "netem loss 0.1%"
    run_test "04_burst_loss" "netem loss 1% 25%"
    run_test "05_delay_loss" "netem delay 2ms 1ms loss 0.2%"
    run_test "06_congestion_spike" "netem delay 5ms 2ms loss 0.5%"
elif [[ "$OS" == "macos" ]]; then
    # Limited test suite for macOS (no netem)
    echo -e "${YELLOW}Note: Running on macOS - network emulation (netem) not available${NC}"
    echo "Only baseline test will be performed."
    run_test "01_baseline" ""
    
    echo -e "\n${YELLOW}For full testing with network emulation:${NC}"
    echo "  - Run on Linux with tc/netem installed"
    echo "  - Or use Network Link Conditioner on macOS (manual setup)"
    echo "  - Or run in Docker/VM with Linux"
else
    echo -e "${RED}Unknown OS: $OS${NC}"
    run_test "01_baseline" ""
fi

# Generate summary report
echo -e "\n${GREEN}Generating summary report...${NC}"
cat > "$RESULTS_DIR/summary.txt" <<EOF
UDP Latency Benchmark Summary
=============================
Date: $(date)
Configuration:
  - Message Size: $MSG_SIZE bytes
  - Target Rate: $RATE msgs/sec
  - Total Messages: $TOTAL
  - Receiver: $RECV_IP:$PORT

Tests Performed:
EOF

for dir in "$RESULTS_DIR"/*/; do
    if [ -d "$dir" ]; then
        test_name=$(basename "$dir")
        echo "  - $test_name" >> "$RESULTS_DIR/summary.txt"
        if [ -f "$dir/analysis.txt" ]; then
            echo "    Results:" >> "$RESULTS_DIR/summary.txt"
            grep -E "median|p95|p99|Retransmit" "$dir/analysis.txt" | sed 's/^/      /' >> "$RESULTS_DIR/summary.txt" || true
        fi
    fi
done

echo -e "\n${GREEN}Benchmark suite completed!${NC}"
echo "Results saved to: $RESULTS_DIR"
echo "Summary: $RESULTS_DIR/summary.txt"