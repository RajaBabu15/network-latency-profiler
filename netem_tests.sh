set -e
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

INTERFACE=${1:-lo}  
PORT=9000
MSG_SIZE=128
RATE=10000
TOTAL=50000
RESULTS_DIR="netem_results/$(date +%Y%m%d_%H%M%S)"

if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo -e "${RED}Error: This script requires Linux with tc/netem support${NC}"
    echo "On macOS, use Network Link Conditioner instead"
    exit 1
fi

if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Please run with sudo for tc commands${NC}"
    exit 1
fi

mkdir -p "$RESULTS_DIR"

echo -e "${GREEN}Network Emulation Test Suite${NC}"
echo "Interface: $INTERFACE"
echo "Results: $RESULTS_DIR"
echo ""

run_netem_test() {
    local test_name=$1
    local netem_cmd=$2
    
    echo -e "${YELLOW}Running: $test_name${NC}"
    echo "Netem: $netem_cmd"
    
    tc qdisc del dev $INTERFACE root 2>/dev/null || true
    
    if [ -n "$netem_cmd" ] && [ "$netem_cmd" != "none" ]; then
        tc qdisc add dev $INTERFACE root $netem_cmd
    fi
    
    TEST_DIR="$RESULTS_DIR/$test_name"
    mkdir -p "$TEST_DIR"
    
    ./udp_receiver $PORT "$TEST_DIR/recv.csv" > /dev/null 2>&1 &
    RECV_PID=$!
    sleep 1
    
    taskset -cp 2 $RECV_PID 2>/dev/null || true
    
    taskset -c 3 ./udp_sender_simple 127.0.0.1 $PORT $MSG_SIZE $RATE $TOTAL "$TEST_DIR/send.csv" 2>/dev/null || \
        ./udp_sender_simple 127.0.0.1 $PORT $MSG_SIZE $RATE $TOTAL "$TEST_DIR/send.csv"
        
    kill $RECV_PID 2>/dev/null || true
    wait $RECV_PID 2>/dev/null || true
    
    # Clear netem
    tc qdisc del dev $INTERFACE root 2>/dev/null || true
    
    # Analyze
    echo "Analyzing..."
    python3 analyze.py "$TEST_DIR/send.csv" "$TEST_DIR/recv.csv" > "$TEST_DIR/analysis.txt" 2>&1
    
    # Extract key metrics
    grep -E "Median|p99|loss rate" "$TEST_DIR/analysis.txt" | head -5
    
    echo -e "${GREEN}✓ Completed${NC}"
    echo ""
}

echo -e "${GREEN}Starting test suite...${NC}"
echo ""

# 1. Baseline
run_netem_test "01_baseline" "none"

# 2. Delay with jitter
run_netem_test "02_delay_1ms" "netem delay 1ms 0.2ms"

# 3. Random loss
run_netem_test "03_loss_0.1pct" "netem loss 0.1%"

# 4. Burst loss
run_netem_test "04_burst_loss" "netem loss 1% 25%"

# 5. Delay + loss
run_netem_test "05_delay_loss" "netem delay 2ms 1ms loss 0.2%"

# 6. Congestion spike
run_netem_test "06_congestion" "netem delay 5ms 2ms loss 0.5%"

# Generate summary
echo -e "${GREEN}Generating summary report...${NC}"
cat > "$RESULTS_DIR/summary.txt" <<EOF
Network Emulation Test Results
===============================
Date: $(date)
Interface: $INTERFACE
Message size: $MSG_SIZE bytes
Rate: $RATE msgs/sec
Total messages: $TOTAL

Test Results:
EOF

for dir in "$RESULTS_DIR"/*/; do
    if [ -d "$dir" ]; then
        test=$(basename "$dir")
        echo -e "\n$test:" >> "$RESULTS_DIR/summary.txt"
        if [ -f "$dir/analysis.txt" ]; then
            grep -E "Median|p99|loss rate" "$dir/analysis.txt" | sed 's/^/  /' >> "$RESULTS_DIR/summary.txt"
        fi
    fi
done

echo -e "${GREEN}✅ All tests complete!${NC}"
echo "Results saved to: $RESULTS_DIR"
echo "Summary: $RESULTS_DIR/summary.txt"