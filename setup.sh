#!/usr/bin/env bash
# setup.sh - Complete setup script for UDP Latency Benchmark

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  UDP Latency Benchmark - Setup Script${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""

# Check OS
OS="unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
fi

echo -e "${GREEN}✓${NC} Detected OS: ${OS}"

# Check for required tools
echo ""
echo -e "${YELLOW}Checking requirements...${NC}"

# Check C++ compiler
if command -v g++ &> /dev/null; then
    echo -e "${GREEN}✓${NC} g++ found: $(g++ --version | head -1)"
else
    echo -e "${RED}✗${NC} g++ not found. Please install:"
    if [[ "$OS" == "macos" ]]; then
        echo "  brew install gcc"
    else
        echo "  sudo apt-get install g++ build-essential"
    fi
    exit 1
fi

# Check Python
if command -v python3 &> /dev/null; then
    echo -e "${GREEN}✓${NC} Python3 found: $(python3 --version)"
else
    echo -e "${RED}✗${NC} Python3 not found. Please install Python 3.7+"
    exit 1
fi

# Check for conda (optional)
if command -v conda &> /dev/null; then
    echo -e "${GREEN}✓${NC} Conda found: $(conda --version)"
    USE_CONDA=true
else
    echo -e "${YELLOW}!${NC} Conda not found - using pip instead"
    USE_CONDA=false
fi

# Install Python dependencies
echo ""
echo -e "${YELLOW}Installing Python dependencies...${NC}"
pip3 install -q --upgrade pip
pip3 install -q pandas numpy matplotlib seaborn
echo -e "${GREEN}✓${NC} Python packages installed"

# Build the programs
echo ""
echo -e "${YELLOW}Building C++ programs...${NC}"
make clean > /dev/null 2>&1
make build
echo -e "${GREEN}✓${NC} Programs built successfully"

# Create results directory
mkdir -p results
echo -e "${GREEN}✓${NC} Created results directory"

# Make scripts executable
chmod +x *.sh *.py 2>/dev/null || true
echo -e "${GREEN}✓${NC} Scripts made executable"

# Verify installation
echo ""
echo -e "${YELLOW}Verifying installation...${NC}"
if [ -f udp_sender ] && [ -f udp_receiver ] && [ -f udp_sender_simple ]; then
    echo -e "${GREEN}✓${NC} All programs built successfully"
else
    echo -e "${RED}✗${NC} Some programs failed to build"
    exit 1
fi

# Test Python imports
python3 -c "import pandas, numpy, matplotlib" 2>/dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓${NC} Python packages working"
else
    echo -e "${RED}✗${NC} Python packages not properly installed"
    exit 1
fi

echo ""
echo -e "${GREEN}================================================${NC}"
echo -e "${GREEN}  Setup Complete! 🎉${NC}"
echo -e "${GREEN}================================================${NC}"
echo ""
echo "Next steps:"
echo "  1. Run a quick test:     make test"
echo "  2. Run a demo:           make run"
echo "  3. Run full benchmark:   make benchmark"
echo "  4. View help:            make help"
echo ""
echo "Example manual usage:"
echo "  Terminal 1: ./udp_receiver 9000 recv.csv"
echo "  Terminal 2: ./udp_sender_simple 127.0.0.1 9000 128 10000 50000 send.csv"
echo "  Analyze:    python3 analyze.py send.csv recv.csv"
echo ""