
#!/bin/bash

# Check if datacard directory was provided as argument
if [ $# -eq 0 ]; then
    echo "Usage: $0 <datacard_directory>"
    echo "Example: $0 datacards_eos"
    exit 1
fi

dcdir=$1

# Check if the datacard directory exists
if [ ! -d "${dcdir}" ]; then
    echo "Error: Datacard directory '${dcdir}' not found"
    exit 1
fi

# Check if there are any signal subdirectories with .txt files
if ! ls ${dcdir}/*/*.txt 1> /dev/null 2>&1; then
    echo "Error: No datacard files (*.txt) found in ${dcdir}/*/."
    echo "Make sure datacards were generated properly."
    exit 1
fi

echo "Running Combine analysis on datacards in: ${dcdir}"
echo "Found signal directories:"
ls -d ${dcdir}/*/

# Process each signal directory individually
for signal_dir in ${dcdir}/*/; do
    if [ -d "$signal_dir" ]; then
        signal_name=$(basename "$signal_dir")
        echo "Processing signal: $signal_name"
        
        # Check if there are datacard files in this directory
        if ls "$signal_dir"*.txt 1> /dev/null 2>&1; then
            echo "  Running AsymptoticLimits for $signal_name..."
            pushd "$signal_dir" > /dev/null
            combine -M AsymptoticLimits -d *.txt -n .limit
            popd > /dev/null
            
            echo "  Running Significance test for $signal_name..."
            pushd "$signal_dir" > /dev/null
            combine -t -1 --expectSignal=1 -M Significance -d *.txt -n .Test
            popd > /dev/null
        else
            echo "  Warning: No datacard files found in $signal_dir"
        fi
    fi
done

echo "Combine analysis completed!"
echo "Results should be in the individual signal directories under ${dcdir}/"
