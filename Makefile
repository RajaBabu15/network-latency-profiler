PROJECT_NAME = network-latency-profiler
CONDA_ENV = network_latency_env
PYTHON_VERSION = 3.9

CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -march=native -mtune=native -Iinclude
LDFLAGS = -pthread
TARGETS = udp_sender udp_receiver
SCRIPTS = run_benchmark.sh analyze.py

CONDA_BASE := $(shell conda info --base 2>/dev/null || echo "")
CONDA_EXISTS := $(shell which conda 2>/dev/null)
all: clean build

setup: env-create deps clean build

build: clean-quiet $(TARGETS) scripts

clean-quiet:
	@rm -f $(TARGETS) *.o *.a *.so *.dylib 2>/dev/null || true
	@find . -name "*.o" -type f -delete 2>/dev/null || true
	@find . -name "*.a" -type f -delete 2>/dev/null || true
	@find . -name "*.so" -type f -delete 2>/dev/null || true
	@find . -name "*.dylib" -type f -delete 2>/dev/null || true
	@find . -name "*.profraw" -type f -delete 2>/dev/null || true
	@find . -type d -empty -delete 2>/dev/null || true
	@rm -f *.log *.csv 2>/dev/null || true
	@rm -rf __pycache__ .pytest_cache 2>/dev/null || true
	@rm -f *.pyc *.pyo 2>/dev/null || true
	
# Source files
SOURCE_FILES = src/core/common.cpp src/network/packet.cpp src/network/network_utils.cpp src/utils/stats.cpp src/reliability/congestion_control.cpp src/reliability/reliability.cpp

udp_sender: src/udp_sender.cpp $(SOURCE_FILES)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

udp_receiver: src/udp_receiver.cpp $(SOURCE_FILES)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	
scripts:
	@chmod +x $(SCRIPTS) 2>/dev/null || true
	
env-create:
	@if [ -z "$(CONDA_EXISTS)" ]; then \
		exit 1; \
	fi
	@conda create -n $(CONDA_ENV) python=$(PYTHON_VERSION) -y 2>/dev/null || true

env-remove:
	@conda env remove -n $(CONDA_ENV) -y 2>/dev/null || true

env-info:
	@conda info --envs | grep $(CONDA_ENV) || true
	
clean:
	@rm -f $(TARGETS) *.o *.a *.so *.dylib 2>/dev/null || true
	@find . -name "*.o" -type f -delete 2>/dev/null || true
	@find . -name "*.a" -type f -delete 2>/dev/null || true
	@find . -name "*.so" -type f -delete 2>/dev/null || true
	@find . -name "*.dylib" -type f -delete 2>/dev/null || true
	@find . -name "*.profraw" -type f -delete 2>/dev/null || true
	@find . -type d -empty -delete 2>/dev/null || true
	@rm -f *.csv *.log 2>/dev/null || true
	@rm -rf results/ __pycache__/ .pytest_cache/ 2>/dev/null || true
	@rm -f *.pyc *.pyo 2>/dev/null || true
	@rm -f core.* *.core 2>/dev/null || true
	@rm -f *.gcda *.gcno *.gcov 2>/dev/null || true
	
distclean: clean env-remove
	@rm -rf results/ *.csv *.txt *.pcap *.gcda
	@rm -rf .pytest_cache/ .coverage htmlcov/
	
test: clean build
	@pkill -f udp_receiver 2>/dev/null || true
	@echo "Starting quick test (500 messages)..."
	@./udp_receiver 9000 quick_test_recv.csv > /dev/null 2>&1 &
	@sleep 0.5
	@./udp_sender 127.0.0.1 9000 128 5000 500 quick_test_send.csv
	@pkill -f udp_receiver 2>/dev/null || true
	@sleep 0.5
	@echo "Analyzing results..."
	@python3 analyze.py quick_test_send.csv quick_test_recv.csv || true
	@rm -f quick_test_*.csv
	
benchmark: clean build
	@pkill -f udp_receiver 2>/dev/null || true
	@TIMESTAMP=$$(date +%Y%m%d_%H%M%S); \
	echo "Starting benchmark (25,000 messages)..."; \
	echo "Using timestamp: $$TIMESTAMP"; \
	mkdir -p results/$$TIMESTAMP; \
	./udp_receiver 9000 results/$$TIMESTAMP/bench_recv.csv > /dev/null 2>&1 & \
	RECEIVER_PID=$$!; \
	sleep 1; \
	echo "Sender writing to: results/$$TIMESTAMP/bench_send.csv"; \
	./udp_sender 127.0.0.1 9000 128 6000 25000 results/$$TIMESTAMP/bench_send.csv; \
	sleep 2; \
	kill $$RECEIVER_PID 2>/dev/null || true; \
	wait $$RECEIVER_PID 2>/dev/null || true; \
	sleep 1; \
	echo "Analyzing benchmark results..."; \
	if [ -f results/$$TIMESTAMP/bench_send.csv ] && [ -f results/$$TIMESTAMP/bench_recv.csv ]; then \
		python3 analyze.py results/$$TIMESTAMP/bench_send.csv results/$$TIMESTAMP/bench_recv.csv; \
	else \
		echo "Error: missing file(s)."; \
		echo "Expected files:"; \
		echo "  results/$$TIMESTAMP/bench_send.csv"; \
		echo "  results/$$TIMESTAMP/bench_recv.csv"; \
		echo "Actual files:"; \
		ls -la results/ 2>/dev/null || true; \
		ls -la results/$$TIMESTAMP/ 2>/dev/null || true; \
		exit 1; \
	fi
	
benchmark-intensive: clean build
	@pkill -f udp_receiver 2>/dev/null || true
	@echo "WARNING: Intensive benchmark may experience congestion on localhost"
	@TIMESTAMP=$$(date +%Y%m%d_%H%M%S); \
	echo "Starting intensive benchmark (100,000 messages)..."; \
	echo "Using timestamp: $$TIMESTAMP"; \
	mkdir -p results/$$TIMESTAMP; \
	./udp_receiver 9000 results/$$TIMESTAMP/bench_recv.csv > /dev/null 2>&1 & \
	RECEIVER_PID=$$!; \
	sleep 1; \
	echo "Sender writing to: results/$$TIMESTAMP/bench_send.csv"; \
	./udp_sender 127.0.0.1 9000 128 10000 100000 results/$$TIMESTAMP/bench_send.csv; \
	sleep 3; \
	kill $$RECEIVER_PID 2>/dev/null || true; \
	wait $$RECEIVER_PID 2>/dev/null || true; \
	sleep 1; \
	echo "Analyzing intensive benchmark results..."; \
	if [ -f results/$$TIMESTAMP/bench_send.csv ] && [ -f results/$$TIMESTAMP/bench_recv.csv ]; then \
		python3 analyze.py results/$$TIMESTAMP/bench_send.csv results/$$TIMESTAMP/bench_recv.csv; \
	else \
		echo "Error: missing file(s)."; \
		echo "Expected files:"; \
		echo "  results/$$TIMESTAMP/bench_send.csv"; \
		echo "  results/$$TIMESTAMP/bench_recv.csv"; \
		echo "Actual files:"; \
		ls -la results/ 2>/dev/null || true; \
		ls -la results/$$TIMESTAMP/ 2>/dev/null || true; \
		exit 1; \
	fi
	
run: clean build
	@pkill -f udp_receiver 2>/dev/null || true
	@echo "Starting demo (10,000 messages)..."
	@./udp_receiver 9000 demo_recv.csv > /dev/null 2>&1 &
	@sleep 1
	@./udp_sender 127.0.0.1 9000 128 5000 10000 demo_send.csv
	@pkill -f udp_receiver 2>/dev/null || true
	@sleep 1
	@echo "Demo Results Summary:"
	@python3 analyze.py demo_send.csv demo_recv.csv | grep -E "Median|p99|Throughput|loss rate" || python3 analyze.py demo_send.csv demo_recv.csv
	@rm -f demo_*.csv
	
deps:
	@if [ -n "$(CONDA_EXISTS)" ] && conda env list | grep -q $(CONDA_ENV); then \
		conda run -n $(CONDA_ENV) pip install pandas numpy matplotlib seaborn --upgrade; \
	else \
		pip3 install pandas numpy matplotlib seaborn --upgrade; \
	fi

deps-check:
	@python3 -c "import pandas; print(f'✓ pandas {pandas.__version__}')" 2>/dev/null || true
	@python3 -c "import numpy; print(f'✓ numpy {numpy.__version__}')" 2>/dev/null || true
	@python3 -c "import matplotlib; print(f'✓ matplotlib {matplotlib.__version__}')" 2>/dev/null || true
	
pgo: clean
	# Step 1: Build with profiling (create profile directory)
	@mkdir -p profile_data
	$(CXX) -O3 -std=c++17 -march=native -Iinclude -fprofile-generate=profile_data src/udp_sender.cpp $(SOURCE_FILES) -o udp_sender $(LDFLAGS)
	$(CXX) -O3 -std=c++17 -march=native -Iinclude -fprofile-generate=profile_data src/udp_receiver.cpp $(SOURCE_FILES) -o udp_receiver $(LDFLAGS)
	# Step 2: Run training workload with proper synchronization
	@echo "Running profile training workload..."
	@./udp_receiver 9000 train_recv.csv > /dev/null 2>&1 & \
	RECEIVER_PID=$$!; \
	sleep 2; \
	./udp_sender 127.0.0.1 9000 128 10000 50000 train_send.csv; \
	sleep 2; \
	kill $$RECEIVER_PID 2>/dev/null || true; \
	wait $$RECEIVER_PID 2>/dev/null || true
	@echo "Profile data collection completed"
	# Step 3: Build with profile data
	@if [ -d profile_data ] && [ -n "$$(ls -A profile_data 2>/dev/null)" ]; then \
		echo "Building with profile-guided optimization..."; \
		$(CXX) -O3 -std=c++17 -march=native -Iinclude -fprofile-use=profile_data src/udp_sender.cpp $(SOURCE_FILES) -o udp_sender $(LDFLAGS); \
		$(CXX) -O3 -std=c++17 -march=native -Iinclude -fprofile-use=profile_data src/udp_receiver.cpp $(SOURCE_FILES) -o udp_receiver $(LDFLAGS);
		echo "PGO build completed successfully"; \
	else \
		echo "Warning: No profile data found, falling back to regular build"; \
		$(CXX) -O3 -std=c++17 -march=native -Iinclude src/udp_sender.cpp $(SOURCE_FILES) -o udp_sender $(LDFLAGS); \
		$(CXX) -O3 -std=c++17 -march=native -Iinclude src/udp_receiver.cpp $(SOURCE_FILES) -o udp_receiver $(LDFLAGS);
	fi
	# Clean up training files
	@rm -f train_*.csv
	@rm -rf profile_data
	
monitor:
	@ps aux | grep udp_receiver | grep -v grep || true
	@ps aux | grep udp_sender | grep -v grep || true
	@lsof -i :9000 2>/dev/null || true

kill-all:
	@pkill -f udp_receiver 2>/dev/null || true
	@pkill -f udp_sender 2>/dev/null || true
	
verify: deps-check
	@which g++ && g++ --version | head -1 || true
	@which python3 && python3 --version || true
	@ls -la udp_sender 2>/dev/null || true
	@ls -la udp_receiver 2>/dev/null || true
	
debug: clean build
	@pkill -f udp_receiver 2>/dev/null || true
	@echo "Starting debug test with frequent progress updates..."
	@./udp_receiver 9000 debug_recv.csv > debug_receiver.log 2>&1 &
	@sleep 0.5
	@./udp_sender 127.0.0.1 9000 128 2000 200 debug_send.csv
	@pkill -f udp_receiver 2>/dev/null || true
	@sleep 0.5
	@echo "Debug Results:"
	@python3 analyze.py debug_send.csv debug_recv.csv || true
	@echo "Receiver log (last 20 lines):"
	@tail -20 debug_receiver.log 2>/dev/null || echo "No receiver log found"
	@rm -f debug_*.csv debug_*.log
	
status:
	@echo "=== Network Latency Profiler Status ==="
	@echo "Build status:"
	@ls -la udp_sender udp_receiver 2>/dev/null || echo "Programs not built"
	@echo "\nRunning processes:"
	@ps aux | grep udp_ | grep -v grep || echo "No UDP processes running"
	@echo "\nPort 9000 usage:"
	@lsof -i :9000 2>/dev/null || echo "Port 9000 is free"
	@echo "\nRecent CSV files:"
	@ls -la *.csv 2>/dev/null || echo "No CSV files found"
	@echo "\nResults directory:"
	@ls -la results/ 2>/dev/null || echo "No results directory"
	
help:
	@echo "Network Latency Profiler - Available Commands:"
	@echo ""
	@echo "Setup & Build:"
	@echo "  make setup      - Full setup: create env, install deps, build"
	@echo "  make build      - Build C++ programs only"
	@echo "  make clean      - Clean all build artifacts and temp files"
	@echo ""
	@echo "Testing & Benchmarking:"
	@echo "  make test       - Quick test (500 messages)"
	@echo "  make run        - Standard demo (10,000 messages)"
	@echo "  make benchmark  - Comprehensive benchmark (25,000 messages)"
	@echo "  make benchmark-intensive - Intensive test (100,000 messages)"
	@echo "  make debug      - Debug test with verbose output (200 messages)"
	@echo ""
	@echo "Analysis & Monitoring:"
	@echo "  make status     - Show system status and running processes"
	@echo "  make monitor    - Monitor running UDP processes"
	@echo "  make kill-all   - Kill all UDP processes"
	@echo "  make verify     - Verify installation and dependencies"
	@echo ""
	@echo "Advanced:"
	@echo "  make pgo        - Profile-guided optimization build"
	@echo "  make deps       - Install Python dependencies"
	@echo "  make deps-check - Check Python dependencies"
	@echo ""
	@echo "Example usage:"
	@echo "  make setup && make run"

.PHONY: all setup build clean clean-quiet distclean test benchmark benchmark-intensive run debug status deps deps-check env-create env-remove env-info monitor kill-all verify pgo help scripts
