#!/bin/bash

# Simple wrapper for ABCD closure summary script

if [ $# -eq 0 ]; then
    echo "Usage: $0 <datacard_directory> [options]"
    echo ""
    echo "Generate a clear summary of ABCD closure test results"
    echo ""
    echo "Example: $0 datacards"
    echo "         $0 datacards -v  # verbose output"
    exit 1
fi

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Run the Python script
python3 "${SCRIPT_DIR}/summarize_abcd_closure.py" "$@"