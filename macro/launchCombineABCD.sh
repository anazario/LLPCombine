#!/bin/bash

# User-friendly ABCD Combine analysis script
# Based on the structure of launchCombine.sh but tailored for ABCD analysis

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS] <datacard_directory1> [datacard_directory2] ..."
    echo ""
    echo "Run CMS Combine ABCD analysis on generated datacards"
    echo ""
    echo "Arguments:"
    echo "  <datacard_directory>    One or more directories containing ABCD datacards"
    echo "                          Supports glob patterns like datacards_Predict*"
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
    echo "  $0 datacards                          # Basic ABCD analysis"
    echo "  $0 -v -j 4 datacards_abcd           # Verbose with 4 parallel jobs"
    echo "  $0 datacards_PredictA datacards_PredictB   # Multiple directories"
    echo "  $0 datacards_Predict*               # Using glob pattern"
    echo "  $0 --t2w-only datacards             # Only create workspaces"
    echo "  $0 --fit-only datacards             # Only run fit (workspaces must exist)"
}

# Default values
dcdirs=()  # Array to hold multiple datacard directories
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
            # Add datacard directory to array
            dcdirs+=("$1")
            shift
            ;;
    esac
done

# Check if at least one datacard directory was provided
if [ ${#dcdirs[@]} -eq 0 ]; then
    echo "Error: No datacard directory specified"
    show_usage
    exit 1
fi

# Check if all datacard directories exist and contain .txt files
valid_dirs=()
for dcdir in "${dcdirs[@]}"; do
    if [ ! -d "${dcdir}" ]; then
        echo "Warning: Datacard directory '${dcdir}' not found, skipping"
        continue
    fi
    
    if ! ls ${dcdir}/*/*.txt 1> /dev/null 2>&1; then
        echo "Warning: No datacard files (*.txt) found in ${dcdir}/*/, skipping"
        continue
    fi
    
    valid_dirs+=("${dcdir}")
done

# Check if we have any valid directories
if [ ${#valid_dirs[@]} -eq 0 ]; then
    echo "Error: No valid datacard directories found"
    echo "Make sure ABCD datacards were generated properly using BF.x"
    exit 1
fi

echo "=== CMS Combine ABCD Analysis ==="
echo "Processing ${#valid_dirs[@]} datacard directories:"
for dcdir in "${valid_dirs[@]}"; do
    echo "  - ${dcdir}"
done
echo "Parallel jobs: ${jobs}"
echo "Use toy data: ${use_toys}"
echo "Verbose: ${verbose}"
echo ""

if [ "$verbose" = true ]; then
    echo "Found signal directories in each datacard directory:"
    for dcdir in "${valid_dirs[@]}"; do
        echo "  ${dcdir}:"
        ls -d ${dcdir}/*/
    done
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
    
    # Build the input pattern for all directories
    input_pattern=""
    for dcdir in "${valid_dirs[@]}"; do
        if [ -n "$input_pattern" ]; then
            input_pattern="${input_pattern} ${dcdir}/*/*.txt"
        else
            input_pattern="${dcdir}/*/*.txt"
        fi
    done
    
    if [ "$verbose" = true ]; then
        echo "Running: combineTool.py -M T2W -i ${input_pattern} -o ws.root --parallel ${jobs} ${verbose_opts}"
    fi
    
    combineTool.py -M T2W -i ${input_pattern} -o ws.root --parallel ${jobs} ${verbose_opts}
    
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
    
    # Build the workspace pattern for all directories
    workspace_pattern=""
    for dcdir in "${valid_dirs[@]}"; do
        if [ -n "$workspace_pattern" ]; then
            workspace_pattern="${workspace_pattern} ${dcdir}/*/ws.root"
        else
            workspace_pattern="${dcdir}/*/ws.root"
        fi
    done
    
    if [ "$verbose" = true ]; then
        echo "Running: combineTool.py -M FitDiagnostics --saveShapes --saveWithUncertainties ${toy_opts} -d ${workspace_pattern} --there --parallel ${jobs} ${verbose_opts}"
    fi
    
    combineTool.py -M FitDiagnostics --saveShapes --saveWithUncertainties --robustFit 1 ${toy_opts} -d ${workspace_pattern} --there --parallel ${jobs} ${verbose_opts}
    
    if [ $? -ne 0 ]; then
        echo "Error: Fit diagnostics failed"
        exit 1
    fi
    
    echo "✓ Fit diagnostics completed successfully"
    echo ""
fi

echo "=== ABCD Analysis Complete ==="
echo "Results are located in the individual signal directories under:"
for dcdir in "${valid_dirs[@]}"; do
    echo "  - ${dcdir}/"
done
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
