# Network Latency Profiler

Enterprise-grade UDP network latency profiling tool with advanced congestion control and microsecond-precision measurement capabilities.

## Quick Start

```bash
make help
```

```
(base) rajababu@Rajas-MacBook-Air network-latency-profiler % make help
Network Latency Profiler - Available Commands:

Setup & Build:
  make setup      - Full setup: create env, install deps, build
  make build      - Build C++ programs only
  make clean      - Clean all build artifacts and temp files

Testing & Benchmarking:
  make test       - Quick test (500 messages)
  make run        - Standard demo (10,000 messages)
  make benchmark  - Comprehensive benchmark (25,000 messages)
  make benchmark-intensive - Intensive test (100,000 messages)
  make debug      - Debug test with verbose output (200 messages)

Analysis & Monitoring:
  make status     - Show system status and running processes
  make monitor    - Monitor running UDP processes
  make kill-all   - Kill all UDP processes
  make verify     - Verify installation and dependencies

Advanced:
  make pgo        - Profile-guided optimization build
  make deps       - Install Python dependencies
  make deps-check - Check Python dependencies

Example usage:
  make setup && make run
```

### clean the project before running 

```bash
make clean
make kill-all
```

## Components

- udp_sender.cpp - UDP client with AIMD congestion control
- udp_receiver.cpp - UDP server with ACK mechanism
- analyze.py - Statistical analysis and percentile calculation

## Setup

This is setup the whole project 

```bash
make setup
```

### If you secifically want to build only the cpp file better use the the build command

```bash
make build
```

Or manually:
```bash
++ -O3 -std=c++17 -Wall -Wextra -march=native -mtune=native -Iinclude src/udp_sender.cpp src/core/common.cpp src/network/packet.cpp src/network/network_utils.cpp src/utils/stats.cpp src/reliability/congestion_control.cpp src/reliability/reliability.cpp -o udp_sender -pthread
g++ -O3 -std=c++17 -Wall -Wextra -march=native -mtune=native -Iinclude src/udp_receiver.cpp src/core/common.cpp src/network/packet.cpp src/network/network_utils.cpp src/utils/stats.cpp src/reliability/congestion_control.cpp src/reliability/reliability.cpp -o udp_receiver -pthread
```

## Run

```bash
make run
```

Or manually:
```bash
g++ -O3 -std=c++17 -Wall -Wextra -march=native -mtune=native -Iinclude src/udp_sender.cpp src/core/common.cpp src/network/packet.cpp src/network/network_utils.cpp src/utils/stats.cpp src/reliability/congestion_control.cpp src/reliability/reliability.cpp -o udp_sender -pthread
g++ -O3 -std=c++17 -Wall -Wextra -march=native -mtune=native -Iinclude src/udp_receiver.cpp src/core/common.cpp src/network/packet.cpp src/network/network_utils.cpp src/utils/stats.cpp src/reliability/congestion_control.cpp src/reliability/reliability.cpp -o udp_receiver -pthread
```

## Parameters

UDP Sender: receiver_ip port msg_size rate_msgs/s total_msgs log.csv
- receiver_ip: Target IP address
- port: UDP port (e.g., 9000)
- msg_size: Message size in bytes (min 16)
- rate_msgs/s: Target rate in messages per second
- total_msgs: Total messages to send
- log.csv: Output CSV file

UDP Receiver: listen_port logfile.csv
- listen_port: UDP port to listen on
- logfile.csv: Output CSV file

## Benchmark Results

```

All messages sent! Waiting for final ACKs...
Sender finished. Sent 10000 messages.
Check demo_send.csv for results.

=== Final Statistics ===

Throughput Statistics:
  Duration: 4.12 seconds
  Packet rate: 2427.82 pps
  Throughput: 2.49 Mbps
  Loss rate: 0.00%
Demo Results Summary:
  Packet loss rate (unique recv): 0.9900%
  Median (p50): 23.9 μs
  p99: 69.6 μs
  p99.9: 247.9 μs
  Median (p50): 105.0 μs
  p99: 223.5 μs
```

## Dependencies

- C++17 compiler (g++ or clang)
- Python 3 with pandas and numpy
- macOS or Linux

Install Python packages:
```bash
pip3 install pandas numpy matplotlib
```


## Files

- Programs: udp_sender, udp_receiver
- Scripts: run_benchmark.sh, analyze.py, setup.sh
- Analysis: benchmark results in results/ directory
