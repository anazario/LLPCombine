#!/bin/bash

echo "Generating all 5 ABCD configs in explicit format..."

# 1. Hybrid_MsLow_RsLowToMed
echo "1/5: Generating Hybrid_MsLow_RsLowToMed..."
python3 macro/generate_explicit_abcd_config.py \
  --name "Hybrid_MsLow_RsLowToMed" \
  --x-low-cuts '["SV_nLeptonic == 1", "LeptonicSV_dxySig[0] > 100.0", "LeptonicSV_dxySig[0] < 1000.0"]' \
  --x-high-cuts '["SV_nHadronic == 1", "HadronicSV_dxySig[0] > 100.0", "HadronicSV_dxySig[0] < 1000.0"]' \
  --y-low-cuts '["rjr_Rs[1] > 0.15", "rjr_Rs[1] < 0.3"]' \
  --y-high-cuts '["rjr_Rs[1] > 0.3", "rjr_Rs[1] < 0.4"]' \
  --common-cuts '["rjr_Ms[1] > 1000", "rjr_Ms[1] < 2000"]' \
  --output config/Hybrid_MsLow_RsLowToMed_explicit.yaml

# 2. Hybrid_NtypeVsSxy_SingleMsRsBin
echo "2/5: Generating Hybrid_NtypeVsSxy_SingleMsRsBin..."
python3 macro/generate_explicit_abcd_config.py \
  --name "Hybrid_NtypeVsSxy_SingleMsRsBin" \
  --x-low-cuts '["SV_nLeptonic == 1"]' \
  --x-high-cuts '["SV_nHadronic == 1"]' \
  --y-low-cuts '["LeptonicSV_dxySig[0] > 100.0", "LeptonicSV_dxySig[0] < 300.0", "HadronicSV_dxySig[0] > 100.0", "HadronicSV_dxySig[0] < 300.0"]' \
  --y-high-cuts '["LeptonicSV_dxySig[0] > 300.0", "LeptonicSV_dxySig[0] < 1000.0", "HadronicSV_dxySig[0] > 300.0", "HadronicSV_dxySig[0] < 1000.0"]' \
  --common-cuts '["rjr_Ms[1] > 1000", "rjr_Ms[1] < 2000", "rjr_Rs[1] > 0.15", "rjr_Rs[1] < 0.3"]' \
  --output config/Hybrid_NtypeVsSxy_SingleMsRsBin_explicit.yaml

# 3. nHad_MsRs_Sxy_config1
echo "3/5: Generating nHad_MsRs_Sxy_config1..."
python3 macro/generate_explicit_abcd_config.py \
  --name "nHad_MsRs_Sxy_config1" \
  --x-low-cuts '["HadronicSV_dxySig[0] > 100.0", "HadronicSV_dxySig[0] < 300.0"]' \
  --x-high-cuts '["HadronicSV_dxySig[0] > 300.0", "HadronicSV_dxySig[0] < 1000.0"]' \
  --y-low-cuts '["rjr_Rs[1] > 0.15", "rjr_Rs[1] < 0.3"]' \
  --y-high-cuts '["rjr_Rs[1] > 0.3", "rjr_Rs[1] < 0.4"]' \
  --common-cuts '["rjr_Ms[1] > 1000", "SV_nHadronic == 1"]' \
  --output config/nHad_MsRs_Sxy_config1_explicit.yaml

# 4. nHad_MsRs_Sxy_config2
echo "4/5: Generating nHad_MsRs_Sxy_config2..."
python3 macro/generate_explicit_abcd_config.py \
  --name "nHad_MsRs_Sxy_config2" \
  --x-low-cuts '["HadronicSV_dxySig[0] > 100.0", "HadronicSV_dxySig[0] < 300.0"]' \
  --x-high-cuts '["HadronicSV_dxySig[0] > 300.0", "HadronicSV_dxySig[0] < 1000.0"]' \
  --y-low-cuts '["rjr_Rs[1] > 0.15", "rjr_Rs[1] < 0.3"]' \
  --y-high-cuts '["rjr_Rs[1] > 0.3", "rjr_Rs[1] < 0.4"]' \
  --common-cuts '["SV_nHadronic == 1", "rjr_Ms[1] > 1000", "rjr_Ms[1] < 2000"]' \
  --output config/nHad_MsRs_Sxy_config2_explicit.yaml

# 5. nHad_MsRs_Sxy_config3
echo "5/5: Generating nHad_MsRs_Sxy_config3..."
python3 macro/generate_explicit_abcd_config.py \
  --name "nHad_MsRs_Sxy_config3" \
  --x-low-cuts '["rjr_Ms[1] > 1000", "rjr_Ms[1] < 2000"]' \
  --x-high-cuts '["rjr_Ms[1] > 2000", "rjr_Ms[1] < 3000"]' \
  --y-low-cuts '["HadronicSV_dxySig[0] > 100.0", "HadronicSV_dxySig[0] < 300.0"]' \
  --y-high-cuts '["HadronicSV_dxySig[0] > 300.0", "HadronicSV_dxySig[0] < 1000.0"]' \
  --common-cuts '["SV_nHadronic == 1", "rjr_Rs[1] > 0.15", "rjr_Rs[1] < 0.3"]' \
  --output config/nHad_MsRs_Sxy_config3_explicit.yaml

echo ""
echo "✅ All 5 explicit ABCD configs generated successfully!"
echo "Generated files:"
echo "  - config/Hybrid_MsLow_RsLowToMed_explicit.yaml"
echo "  - config/Hybrid_NtypeVsSxy_SingleMsRsBin_explicit.yaml"
echo "  - config/nHad_MsRs_Sxy_config1_explicit.yaml"
echo "  - config/nHad_MsRs_Sxy_config2_explicit.yaml"
echo "  - config/nHad_MsRs_Sxy_config3_explicit.yaml"
echo ""
echo "These configs are now ready to use with the updated ABCD summary script!"