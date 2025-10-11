#!/bin/bash

# Quick ABCD Configuration Generator
# Wrapper for common use cases

show_help() {
    echo "Quick ABCD Configuration Generator"
    echo ""
    echo "Usage: $0 <preset> <name> [options]"
    echo ""
    echo "Presets:"
    echo "  ms-analysis    - Ms vs dxySig analysis (nHad or nLep)"
    echo "  rs-analysis    - Rs vs dxySig analysis (nHad or nLep)"  
    echo "  custom         - Custom variable analysis"
    echo ""
    echo "Examples:"
    echo "  $0 ms-analysis nHad_Ms1k2k --sv-type nHadronic --ms-ranges '1000,2000;2000,inf'"
    echo "  $0 rs-analysis nLep_Rs15-30 --sv-type nLeptonic --rs-ranges '0.15,0.3;0.3,inf'"
    echo "  $0 custom MyAnalysis --var1-name 'rjr_Ms[1]' --var1-ranges '1000,2000;2000,3000'"
    echo ""
    echo "Options:"
    echo "  --sv-type       - nHadronic or nLeptonic (required)"
    echo "  --predicted     - region_A, region_B, region_C, or region_D (default: region_A)"
    echo "  --ms-ranges     - Ms ranges: 'low1,high1;low2,high2'"
    echo "  --rs-ranges     - Rs ranges: 'low1,high1;low2,high2'"
    echo "  --precision     - Precision systematic value (default: 1.10)"
    echo "  --output        - Output file (default: config/[name].yaml)"
}

if [ $# -lt 2 ]; then
    show_help
    exit 1
fi

preset=$1
name=$2
shift 2

# Default values
sv_type=""
predicted="region_A"
precision="1.10"
output="config/${name}.yaml"
ms_ranges=""
rs_ranges=""

# Parse options
while [[ $# -gt 0 ]]; do
    case $1 in
        --sv-type)
            sv_type="$2"
            shift 2
            ;;
        --predicted)
            predicted="$2"
            shift 2
            ;;
        --ms-ranges)
            ms_ranges="$2"
            shift 2
            ;;
        --rs-ranges) 
            rs_ranges="$2"
            shift 2
            ;;
        --precision)
            precision="$2"
            shift 2
            ;;
        --output)
            output="$2"
            shift 2
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Validate required parameters
if [ -z "$sv_type" ]; then
    echo "Error: --sv-type is required"
    exit 1
fi

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Generate config based on preset
case $preset in
    ms-analysis)
        if [ -z "$ms_ranges" ]; then
            echo "Error: --ms-ranges required for ms-analysis preset"
            exit 1
        fi
        
        echo "Generating Ms vs dxySig ABCD analysis..."
        python3 "${SCRIPT_DIR}/generate_abcd_config.py" \
            --name "$name" \
            --output "$output" \
            --sv-type "$sv_type" \
            --var1-name "rjr_Ms[1]" \
            --var1-ranges "$ms_ranges" \
            --predicted "$predicted" \
            --precision-value "$precision"
        ;;
        
    rs-analysis)
        if [ -z "$rs_ranges" ]; then
            echo "Error: --rs-ranges required for rs-analysis preset"
            exit 1
        fi
        
        echo "Generating Rs vs dxySig ABCD analysis..."
        python3 "${SCRIPT_DIR}/generate_abcd_config.py" \
            --name "$name" \
            --output "$output" \
            --sv-type "$sv_type" \
            --var1-name "rjr_Rs[1]" \
            --var1-ranges "$rs_ranges" \
            --predicted "$predicted" \
            --precision-value "$precision"
        ;;
        
    custom)
        echo "For custom analysis, use generate_abcd_config.py directly:"
        echo "python3 ${SCRIPT_DIR}/generate_abcd_config.py --help"
        exit 1
        ;;
        
    *)
        echo "Error: Unknown preset '$preset'"
        show_help
        exit 1
        ;;
esac

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Configuration generated: $output"
    echo ""
    echo "Next steps:"
    echo "  1. Review the configuration file"
    echo "  2. Run analysis: ./LLPCombine.x $output"
    echo "  3. Generate datacards: ./BF.x ./json/[output].json $output -v"
    echo "  4. Run ABCD analysis: ./macro/launchCombineABCD.sh datacards"
    echo "  5. Summarize results: ./macro/abcd_summary.sh datacards"
fi