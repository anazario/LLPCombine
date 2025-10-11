#!/bin/bash

# User-friendly ABCD Combine analysis script
# Based on the structure of launchCombine.sh but tailored for ABCD analysis

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS] <datacard_directory>"
    echo ""
    echo "Run CMS Combine ABCD analysis on generated datacards"
    echo ""
    echo "Arguments:"
    echo "  <datacard_directory>    Directory containing ABCD datacards"
    echo ""
    echo "Options:"
    echo "  -h, --help             Show this help message"
    echo "  -v, --verbose          Enable verbose output"
    echo "  -t, --toy-data         Use toy data (Asimov dataset)"
    echo "  -j, --jobs N           Number of parallel jobs (default: 1)"
    echo "  --t2w-only             Only run text2workspace (skip fit)"
    echo "  --fit-only             Only run fit diagnostics (skip t2w)"
    echo ""
    echo "Examples:"
    echo "  $0 datacards                    # Basic ABCD analysis"
    echo "  $0 -v -j 4 datacards_abcd     # Verbose with 4 parallel jobs"
    echo "  $0 --t2w-only datacards       # Only create workspaces"
    echo "  $0 --fit-only datacards       # Only run fit (workspaces must exist)"
}

# Default values
dcdir=""
verbose=false
use_toys=false
jobs=1
t2w_only=false
fit_only=false

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_usage
            exit 0
            ;;
        -v|--verbose)
            verbose=true
            shift
            ;;
        -t|--toy-data)
            use_toys=true
            shift
            ;;
        -j|--jobs)
            jobs="$2"
            shift 2
            ;;
        --t2w-only)
            t2w_only=true
            shift
            ;;
        --fit-only)
            fit_only=true
            shift
            ;;
        -*)
            echo "Error: Unknown option $1"
            show_usage
            exit 1
            ;;
        *)
            if [ -z "$dcdir" ]; then
                dcdir="$1"
            else
                echo "Error: Multiple datacard directories specified"
                show_usage
                exit 1
            fi
            shift
            ;;
    esac
done

# Check if datacard directory was provided
if [ -z "$dcdir" ]; then
    echo "Error: No datacard directory specified"
    show_usage
    exit 1
fi

# Check if the datacard directory exists
if [ ! -d "${dcdir}" ]; then
    echo "Error: Datacard directory '${dcdir}' not found"
    exit 1
fi

# Check if there are any signal subdirectories with .txt files
if ! ls ${dcdir}/*/*.txt 1> /dev/null 2>&1; then
    echo "Error: No datacard files (*.txt) found in ${dcdir}/*/."
    echo "Make sure ABCD datacards were generated properly using BF.x"
    exit 1
fi

echo "=== CMS Combine ABCD Analysis ==="
echo "Datacard directory: ${dcdir}"
echo "Parallel jobs: ${jobs}"
echo "Use toy data: ${use_toys}"
echo "Verbose: ${verbose}"
echo ""

if [ "$verbose" = true ]; then
    echo "Found signal directories:"
    ls -d ${dcdir}/*/
    echo ""
fi

# Prepare toy data options
toy_opts=""
if [ "$use_toys" = true ]; then
    toy_opts="-t -1"
fi

# Prepare verbose options
verbose_opts=""
if [ "$verbose" = true ]; then
    verbose_opts="--verbose 2"
fi

# Step 1: Text2Workspace conversion
if [ "$fit_only" = false ]; then
    echo "Step 1: Converting datacards to workspaces..."
    if [ "$verbose" = true ]; then
        echo "Running: combineTool.py -M T2W -i ${dcdir}/*/*.txt -o ws.root --parallel ${jobs} ${verbose_opts}"
    fi
    
    combineTool.py -M T2W -i ${dcdir}/*/*.txt -o ws.root --parallel ${jobs} ${verbose_opts}
    
    if [ $? -ne 0 ]; then
        echo "Error: Text2Workspace conversion failed"
        exit 1
    fi
    
    echo "✓ Workspaces created successfully"
    echo ""
fi

# Step 2: Fit Diagnostics
if [ "$t2w_only" = false ]; then
    echo "Step 2: Running fit diagnostics..."
    if [ "$verbose" = true ]; then
        echo "Running: combineTool.py -M FitDiagnostics --saveShapes --saveWithUncertainties ${toy_opts} -d ${dcdir}/*/ws.root --there --parallel ${jobs} ${verbose_opts}"
    fi
    
    combineTool.py -M FitDiagnostics --saveShapes --saveWithUncertainties --robustFit 1 ${toy_opts} -d ${dcdir}/*/ws.root --there --parallel ${jobs} ${verbose_opts}
    
    if [ $? -ne 0 ]; then
        echo "Error: Fit diagnostics failed"
        exit 1
    fi
    
    echo "✓ Fit diagnostics completed successfully"
    echo ""
fi

echo "=== ABCD Analysis Complete ==="
echo "Results are located in the individual signal directories under ${dcdir}/"
echo ""

if [ "$verbose" = true ]; then
    echo "Output files in each signal directory:"
    echo "  - ws.root                    : RooWorkspace file"
    echo "  - fitDiagnostics*.root       : Fit results and uncertainties"
    echo "  - shapes/                    : Post-fit shapes (if available)"
    echo ""
fi

echo "To inspect results:"
echo "  - Use combine plotting tools on fitDiagnostics*.root files"
echo "  - Check fit convergence and pull distributions"
echo "  - Examine ABCD systematic uncertainties in the output"