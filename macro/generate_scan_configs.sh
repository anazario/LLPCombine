#!/bin/bash

# Generate ABCD scan configurations for systematic studies
# Tests the same phase space with different predicted regions

echo "Generating ABCD scan configurations..."

# Base parameters
SV_TYPE="nHadronic"
MS_RANGES="1000,2000;2000,3000"
PRECISION="1.12"
BASE_NAME="nHad_Ms1k3k_Scan"

# Generate configs for each predicted region
for REGION in region_A region_B region_C region_D; do
    SUFFIX=$(echo $REGION | cut -d'_' -f2)
    CONFIG_NAME="${BASE_NAME}_Predict${SUFFIX}"
    OUTPUT_FILE="config/${CONFIG_NAME}.yaml"
    
    echo "  Generating: $CONFIG_NAME (predict $REGION)"
    
    python3 macro/generate_abcd_config.py \
        --name "$CONFIG_NAME" \
        --output "$OUTPUT_FILE" \
        --sv-type "$SV_TYPE" \
        --var1-name "rjr_Ms[1]" \
        --var1-ranges "$MS_RANGES" \
        --predicted "$REGION" \
        --precision-value "$PRECISION" \
        --syst-prefix "scan_${SUFFIX,,}" \
        --signal-points '["gogoZ_1500_500_100_10"]'
        
    if [ $? -eq 0 ]; then
        echo "    ✅ Generated: $OUTPUT_FILE"
    else
        echo "    ❌ Failed: $OUTPUT_FILE"
    fi
done

echo ""
echo "📊 Scan configurations generated!"
echo "Run all analyses with:"
echo "  for config in config/nHad_Ms1k3k_Scan_*.yaml; do"
echo "    echo \"Processing: \$config\""
echo "    ./LLPCombine.x \"\$config\""
echo "    ./BF.x ./json/\$(basename \$config .yaml).json \"\$config\" -v"
echo "  done"