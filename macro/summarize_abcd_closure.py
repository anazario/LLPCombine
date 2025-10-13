#!/usr/bin/env python3
"""
ABCD Closure Test Summary Script

Reads CMS Combine fit diagnostics output and provides a clear summary
of ABCD closure performance focusing on yield predictions rather than signal analysis.

Usage: python summarize_abcd_closure.py <datacard_directory>
"""

import sys
import os
import glob
import ROOT
import json
import argparse
from math import sqrt

def find_fit_files(datacard_dir):
    """Find fitDiagnostics ROOT files in the datacard directory"""
    pattern = os.path.join(datacard_dir, "*", "fitDiagnostics*.root")
    files = glob.glob(pattern)
    return files

def extract_observations_from_datacard(datacard_path):
    """Extract observed data from datacard file"""
    data_yields = {}
    try:
        with open(datacard_path, 'r') as f:
            lines = f.readlines()
        
        
        # Find the observation line
        observations = []
        for line in lines:
            line = line.strip()
            if line.startswith('observation'):
                parts = line.split()
                if len(parts) >= 2:
                    observations = [float(x) for x in parts[1:]]
                    break
        
        if not observations:
            return {}
        
        # Find the bin names from the first bin line after the separator
        bin_names = []
        found_separator = False
        for line in lines:
            line = line.strip()
            if '----' in line:
                found_separator = True
                continue
            if found_separator and line.startswith('bin'):
                parts = line.split()
                if len(parts) >= 2:
                    bin_names = parts[1:]
                    break
        
        if not bin_names:
            return {}
        
        # Get unique bin names (the line may have duplicates)
        unique_bins = []
        for bin_name in bin_names:
            if bin_name not in unique_bins:
                unique_bins.append(bin_name)
        
        
        # Match observations to bin names
        if len(observations) == len(unique_bins):
            for bin_name, obs in zip(unique_bins, observations):
                data_yields[bin_name] = int(obs) if obs == int(obs) else obs
                
    except Exception as e:
        print(f"ERROR: Failed to parse datacard {datacard_path}: {e}")
        import traceback
        traceback.print_exc()
        return {}
    
    return data_yields

def extract_abcd_info(fit_file, verbose=False):
    """Extract ABCD closure information from fit diagnostics file"""
    f = ROOT.TFile.Open(fit_file, "READ")
    if not f or f.IsZombie():
        print(f"Error: Could not open {fit_file}")
        return None
    
    # Get the fit result
    fit_b = f.Get("fit_b")
    if not fit_b:
        print(f"Error: No fit_b result found in {fit_file}")
        print("Available objects in ROOT file:")
        for key in f.GetListOfKeys():
            print(f"  - {key.GetName()} ({key.GetClassName()})")
        f.Close()
        return None
    
    # Get parameter values
    params = {}
    for i in range(fit_b.floatParsFinal().getSize()):
        param = fit_b.floatParsFinal().at(i)
        name = param.GetName()
        value = param.getVal()
        error = param.getError()
        params[name] = {"value": value, "error": error}
    
    # Get constant parameters too
    for i in range(fit_b.constPars().getSize()):
        param = fit_b.constPars().at(i)
        name = param.GetName()
        value = param.getVal()
        params[name] = {"value": value, "error": 0.0}
    
    # Get pre-fit and post-fit shapes for data yields
    shapes_prefit = f.Get("shapes_prefit")
    shapes_postfit = f.Get("shapes_fit_b")
    
    data_yields = {}
    
    # Get the original observed data from the RooWorkspace 
    workspace = f.Get("w")
    if workspace:
        # Debug: print all variables in workspace to see what's available
        if verbose:
            var_iter = workspace.allVars().createIterator()
            var = var_iter.Next()
            while var:
                print(f"  - {var.GetName()} = {var.getVal()}")
                var = var_iter.Next()
        
        # Try different naming patterns for observed data
        all_bins = []
        if shapes_prefit:
            for key in shapes_prefit.GetListOfKeys():
                all_bins.append(key.GetName())
        
        for bin_name in all_bins:
            found = False
            # Try various naming conventions for observed data in workspace
            for pattern in [f"n_obs_{bin_name}", f"n_obs_bin{bin_name}", f"obs_{bin_name}", 
                           f"{bin_name}_obs", f"data_obs_{bin_name}"]:
                obs_var = workspace.var(pattern)
                if obs_var:
                    data_yields[bin_name] = obs_var.getVal()
                    found = True
                    break
            
            if not found:
                print(f"WARNING: Could not find observed data for {bin_name} in workspace")
    
    # If no workspace or no data found, try to extract from datacard
    if not data_yields:
        print("INFO: Attempting to extract observed data from datacard...")
        # Try common datacard names
        fit_dir = os.path.dirname(fit_file)
        
        for datacard_name in ["datacard.txt", "combine_datacard.txt", "*.txt"]:
            if "*" in datacard_name:
                import glob
                matches = glob.glob(os.path.join(fit_dir, datacard_name))
                if matches:
                    datacard_path = matches[0]
                    break
            else:
                datacard_path = os.path.join(fit_dir, datacard_name)
                if os.path.exists(datacard_path):
                    break
        else:
            datacard_path = None
            
        if datacard_path and os.path.exists(datacard_path):
            data_yields = extract_observations_from_datacard(datacard_path)
            if data_yields:
                print(f"SUCCESS: Extracted observed data from {datacard_path}")
            else:
                print("ERROR: Could not extract observations from datacard")
        else:
            print(f"ERROR: No datacard found in {fit_dir}")
    
    # Final fallback with proper error handling
    if not data_yields:
        print("ERROR: Could not find original observed data anywhere.")
        print("       Please ensure the datacard contains 'observation' line with the true data.")
    
    f.Close()
    
    return {
        "params": params,
        "data_yields": data_yields,
        "fit_file": fit_file
    }

def identify_abcd_regions(data_yields, params):
    """Identify ABCD regions and determine correct formula based on region structure"""
    all_bins = list(data_yields.keys())
    
    # Find the predicted bin by looking for the one with error = 0 (fixed by formula)
    predicted_bin = None
    for bin_name in all_bins:
        scale_param = f"scale_{bin_name}"
        if scale_param in params:
            if params[scale_param]["error"] == 0.0:
                predicted_bin = bin_name
                break
        else:
            predicted_bin = bin_name
            break
    
    # Fallback methods if not found
    if not predicted_bin:
        for bin_name in all_bins:
            bin_lower = bin_name.lower()
            if 'sr' in bin_lower or 'signal' in bin_lower or 'cra' in bin_lower:
                predicted_bin = bin_name
                break
    
    if not predicted_bin:
        min_rel_error = float('inf')
        for bin_name in all_bins:
            scale_param = f"scale_{bin_name}"
            if scale_param in params and params[scale_param]["value"] != 0:
                rel_error = params[scale_param]["error"] / abs(params[scale_param]["value"])
                if rel_error < min_rel_error:
                    min_rel_error = rel_error
                    predicted_bin = bin_name
    
    if not predicted_bin and len(all_bins) > 0:
        predicted_bin = all_bins[0]
    
    # Now identify the proper ABCD structure
    control_bins = [b for b in all_bins if b != predicted_bin]
    
    # Try to identify ABCD structure from naming patterns
    abcd_mapping = {}
    for bin_name in all_bins:
        bin_lower = bin_name.lower()
        if 'cra' in bin_lower or (predicted_bin == bin_name):
            abcd_mapping['A'] = bin_name
        elif 'crb' in bin_lower:
            abcd_mapping['B'] = bin_name  
        elif 'crc' in bin_lower:
            abcd_mapping['C'] = bin_name
        elif 'crd' in bin_lower:
            abcd_mapping['D'] = bin_name
    
    # If no clear CR pattern, try lepton/hadron pattern for 2x2 ABCD
    if len(abcd_mapping) < 4:
        abcd_mapping = {}
        lep_bins = [b for b in all_bins if 'lep' in b.lower()]
        had_bins = [b for b in all_bins if 'had' in b.lower()]
        
        if len(lep_bins) == 2 and len(had_bins) == 2:
            # Sort each group to get consistent assignment
            lep_bins.sort()
            had_bins.sort() 
            
            # Standard 2x2 ABCD assignment
            abcd_mapping['A'] = lep_bins[0]  # nLep1CRA
            abcd_mapping['B'] = lep_bins[1]  # nLep1CRB  
            abcd_mapping['C'] = had_bins[0]  # nHad1CRC
            abcd_mapping['D'] = had_bins[1]  # nHad1CRD
    
    # Final fallback: alphabetical assignment
    if len(abcd_mapping) < 4 and len(all_bins) >= 4:
        sorted_bins = sorted(all_bins)
        abcd_mapping = {
            'A': sorted_bins[0],
            'B': sorted_bins[1], 
            'C': sorted_bins[2],
            'D': sorted_bins[3]
        }
    elif len(all_bins) < 4:
        print(f"ERROR: Need at least 4 bins for ABCD analysis, found only {len(all_bins)}")
        return {}
    
    # Determine which region is predicted and return appropriate mapping
    predicted_label = None
    for label, bin_name in abcd_mapping.items():
        if bin_name == predicted_bin:
            predicted_label = label
            break
    
    return {
        "abcd_mapping": abcd_mapping,
        "predicted_region": predicted_label,
        "predicted_bin": predicted_bin
    }

def calculate_closure_metrics(abcd_info):
    """Calculate ABCD closure metrics using proper formula"""
    params = abcd_info["params"]
    data_yields = abcd_info["data_yields"]
    
    # Use existing ABCD mapping from config if available, otherwise auto-detect
    if 'abcd_mapping' in abcd_info and 'predicted_region' in abcd_info:
        # Use mapping from config file
        abcd_mapping = abcd_info['abcd_mapping']
        predicted_region = abcd_info['predicted_region']
        regions = {
            "abcd_mapping": abcd_mapping,
            "predicted_region": predicted_region,
            "predicted_bin": abcd_mapping.get(predicted_region)
        }
    else:
        # Fall back to auto-detection
        regions = identify_abcd_regions(data_yields, params)
        if not regions or not regions.get("predicted_region"):
            return None
        abcd_mapping = regions["abcd_mapping"] 
        predicted_region = regions["predicted_region"]
    
    # Get yields for all ABCD regions
    yield_A = data_yields.get(abcd_mapping["A"], 0)
    yield_B = data_yields.get(abcd_mapping["B"], 0) 
    yield_C = data_yields.get(abcd_mapping["C"], 0)
    yield_D = data_yields.get(abcd_mapping["D"], 0)
    
    # Get scale factors
    scale_A = params.get(f"scale_{abcd_mapping['A']}", {"value": 1.0, "error": 0.0})
    scale_B = params.get(f"scale_{abcd_mapping['B']}", {"value": 1.0, "error": 0.0})
    scale_C = params.get(f"scale_{abcd_mapping['C']}", {"value": 1.0, "error": 0.0})
    scale_D = params.get(f"scale_{abcd_mapping['D']}", {"value": 1.0, "error": 0.0})
    
    # Calculate predictions using correct ABCD formula based on which region is predicted
    if predicted_region == "A":
        # A = B * C / D (standard ABCD)
        naive_pred = (yield_B * yield_C / yield_D) if yield_D > 0 else 0
        fitted_pred = (yield_B * scale_B["value"] * yield_C * scale_C["value"] / 
                      (yield_D * scale_D["value"])) if yield_D > 0 else 0
        predicted_true = yield_A
        formula_str = "B×C/D"
        
    elif predicted_region == "B": 
        # B = A * D / C
        naive_pred = (yield_A * yield_D / yield_C) if yield_C > 0 else 0
        fitted_pred = (yield_A * scale_A["value"] * yield_D * scale_D["value"] / 
                      (yield_C * scale_C["value"])) if yield_C > 0 else 0
        predicted_true = yield_B
        formula_str = "A×D/C"
        
    elif predicted_region == "C":
        # C = A * D / B  
        naive_pred = (yield_A * yield_D / yield_B) if yield_B > 0 else 0
        fitted_pred = (yield_A * scale_A["value"] * yield_D * scale_D["value"] / 
                      (yield_B * scale_B["value"])) if yield_B > 0 else 0
        predicted_true = yield_C
        formula_str = "A×D/B"
        
    elif predicted_region == "D":
        # D = B * C / A
        naive_pred = (yield_B * yield_C / yield_A) if yield_A > 0 else 0
        fitted_pred = (yield_B * scale_B["value"] * yield_C * scale_C["value"] / 
                      (yield_A * scale_A["value"])) if yield_A > 0 else 0
        predicted_true = yield_D
        formula_str = "B×C/A"
    else:
        return None
    
    # Calculate closure metrics
    closure_ratio = fitted_pred / predicted_true if predicted_true > 0 else 0
    naive_ratio = naive_pred / predicted_true if predicted_true > 0 else 0
    
    # Calculate pull
    stat_error = sqrt(predicted_true) if predicted_true > 0 else 1
    pull = (fitted_pred - predicted_true) / stat_error
    
    return {
        "regions": regions,
        "yields": {
            "A": yield_A,
            "B": yield_B,
            "C": yield_C,
            "D": yield_D,
            "predicted_true": predicted_true
        },
        "scale_factors": {
            "A": scale_A,
            "B": scale_B, 
            "C": scale_C,
            "D": scale_D
        },
        "predictions": {
            "naive": naive_pred,
            "fitted": fitted_pred,
            "formula": formula_str
        },
        "closure_metrics": {
            "closure_ratio": closure_ratio,
            "naive_ratio": naive_ratio,
            "pull": pull,
            "stat_error": stat_error
        }
    }

def print_closure_summary(closure_data, signal_name, region_cuts=None):
    """Print a formatted summary of the closure test"""
    if not closure_data:
        print(f"❌ Could not extract closure data for {signal_name}")
        return
    
    regions = closure_data["regions"]
    abcd_mapping = regions["abcd_mapping"]
    predicted_region = regions["predicted_region"]
    yields = closure_data["yields"]
    scales = closure_data["scale_factors"]
    preds = closure_data["predictions"]
    metrics = closure_data["closure_metrics"]
    
    print(f"\n{'='*60}")
    print(f"ABCD CLOSURE TEST SUMMARY: {signal_name}")
    print(f"{'='*60}")
    
    print(f"\n📊 REGION MAPPING:")
    print(f"   Region A: {abcd_mapping['A']} {'← PREDICTED' if predicted_region == 'A' else ''}")
    print(f"   Region B: {abcd_mapping['B']} {'← PREDICTED' if predicted_region == 'B' else ''}")  
    print(f"   Region C: {abcd_mapping['C']} {'← PREDICTED' if predicted_region == 'C' else ''}")
    print(f"   Region D: {abcd_mapping['D']} {'← PREDICTED' if predicted_region == 'D' else ''}")
    
    # Display region cuts if available
    if region_cuts:
        print(f"\n🔍 REGION CUTS:")
        for region in ['A', 'B', 'C', 'D']:
            if region in region_cuts:
                print(f"   Region {region} ({abcd_mapping[region]}):")
                for i, cut in enumerate(region_cuts[region], 1):
                    print(f"      {i}. {cut}")
                print()  # Add spacing between regions
    
    print(f"\n📈 TRUE YIELDS:")
    print(f"   Region A: {yields['A']:8.1f}")
    print(f"   Region B: {yields['B']:8.1f}")
    print(f"   Region C: {yields['C']:8.1f}")
    print(f"   Region D: {yields['D']:8.1f}")
    
    print(f"\n⚖️  FITTED SCALE FACTORS:")
    print(f"   Region A: {scales['A']['value']:8.5f} ± {scales['A']['error']:7.5f}")
    print(f"   Region B: {scales['B']['value']:8.5f} ± {scales['B']['error']:7.5f}")
    print(f"   Region C: {scales['C']['value']:8.5f} ± {scales['C']['error']:7.5f}")
    print(f"   Region D: {scales['D']['value']:8.5f} ± {scales['D']['error']:7.5f}")
    
    print(f"\n🎯 ABCD PREDICTIONS:")
    print(f"   Naive ({preds['formula']}):       {preds['naive']:10.4f}")
    print(f"   Fitted (scaled):       {preds['fitted']:10.4f}")
    print(f"   True ({predicted_region}):              {yields['predicted_true']:10.4f}")
    
    print(f"\n📏 CLOSURE PERFORMANCE:")
    print(f"   Naive ratio (pred/true):   {metrics['naive_ratio']:8.5f}")
    print(f"   Fitted ratio (pred/true):  {metrics['closure_ratio']:8.5f}")
    print(f"   Statistical error:         {metrics['stat_error']:8.4f}")
    print(f"   Pull (fitted-true)/σ:      {metrics['pull']:+8.4f}")
    
    # Interpret results
    print(f"\n🔍 INTERPRETATION:")
    if abs(metrics['pull']) < 1:
        print(f"   ✅ EXCELLENT closure: |pull| < 1σ")
    elif abs(metrics['pull']) < 2:
        print(f"   ✅ GOOD closure: |pull| < 2σ")
    elif abs(metrics['pull']) < 3:
        print(f"   ⚠️  MARGINAL closure: |pull| < 3σ")
    else:
        print(f"   ❌ POOR closure: |pull| > 3σ")
    
    if 0.9 <= metrics['closure_ratio'] <= 1.1:
        print(f"   ✅ Good normalization: within 10% of expectation")
    elif 0.8 <= metrics['closure_ratio'] <= 1.2:
        print(f"   ⚠️  Moderate bias: within 20% of expectation")
    else:
        print(f"   ❌ Significant bias: >20% deviation from expectation")

def extract_abcd_mapping_from_config(config_file):
    """Extract ABCD region mapping from config file"""
    try:
        import yaml
        with open(config_file, 'r') as f:
            config = yaml.safe_load(f)
        
        if 'abcd' in config and 'regions' in config['abcd']:
            regions = config['abcd']['regions']
            # Convert region_A -> A, etc.
            mapping = {}
            for region_key, bin_name in regions.items():
                if region_key.startswith('region_'):
                    letter = region_key.split('_')[1]  # region_A -> A
                    mapping[letter] = bin_name
            
            predicted_region = config['abcd'].get('predicted_region', 'region_A')
            predicted_letter = predicted_region.split('_')[1] if predicted_region.startswith('region_') else 'A'
            
            return mapping, predicted_letter
        
        return None, None
    except Exception as e:
        print(f"Warning: Could not extract ABCD mapping from config file: {e}")
        return None, None

def extract_region_cuts(config_file, abcd_mapping):
    """Extract region cuts from ABCD config file"""
    try:
        import yaml
        with open(config_file, 'r') as f:
            config = yaml.safe_load(f)
        
        region_cuts = {}
        if 'bins' in config:
            for region_letter, bin_name in abcd_mapping.items():
                if bin_name in config['bins'] and 'cuts' in config['bins'][bin_name]:
                    cuts = config['bins'][bin_name]['cuts']
                    # Handle YAML anchors by flattening the list
                    flattened_cuts = []
                    for cut in cuts:
                        if isinstance(cut, list):
                            flattened_cuts.extend(cut)
                        else:
                            flattened_cuts.append(cut)
                    region_cuts[region_letter] = flattened_cuts
        
        return region_cuts
    except Exception as e:
        print(f"Warning: Could not extract cuts from config file: {e}")
        return {}

def main():
    parser = argparse.ArgumentParser(description="Summarize ABCD closure test results")
    parser.add_argument("datacard_dir", help="Directory containing fit results")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("-c", "--config", help="ABCD config file (optional, to display region cuts)")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.datacard_dir):
        print(f"Error: Directory {args.datacard_dir} does not exist")
        return 1
    
    # Find fit files
    fit_files = find_fit_files(args.datacard_dir)
    if not fit_files:
        print(f"Error: No fitDiagnostics*.root files found in {args.datacard_dir}")
        return 1
    
    print(f"Found {len(fit_files)} fit result file(s)")
    
    # Process each fit file
    for fit_file in fit_files:
        # Extract signal name from path
        signal_name = os.path.basename(os.path.dirname(fit_file))
        
        if args.verbose:
            print(f"\nProcessing: {fit_file}")
        
        # Extract ABCD information
        abcd_info = extract_abcd_info(fit_file, args.verbose)
        if not abcd_info:
            continue
        
        # Override ABCD mapping with config file if provided
        if args.config and os.path.exists(args.config):
            config_mapping, config_predicted = extract_abcd_mapping_from_config(args.config)
            if config_mapping:
                print(f"Using ABCD mapping from config file: {args.config}")
                print(f"DEBUG: Config mapping: {config_mapping}")
                print(f"DEBUG: Config predicted: {config_predicted}")
                print(f"DEBUG: abcd_info keys: {abcd_info.keys()}")
                if 'abcd_mapping' in abcd_info:
                    print(f"DEBUG: Old mapping: {abcd_info['abcd_mapping']}")
                else:
                    print(f"DEBUG: No 'abcd_mapping' key found, creating new one")
                abcd_info['abcd_mapping'] = config_mapping
                abcd_info['predicted_region'] = config_predicted
                print(f"DEBUG: New mapping: {abcd_info['abcd_mapping']}")
            else:
                print(f"DEBUG: Failed to extract mapping from config file")
        
        # Calculate closure metrics
        closure_data = calculate_closure_metrics(abcd_info)
        
        # Extract region cuts if config file provided
        region_cuts = None
        if args.config and os.path.exists(args.config):
            # Use the mapping that will be used in the closure calculation
            mapping_for_cuts = abcd_info.get('abcd_mapping')
            if mapping_for_cuts:
                region_cuts = extract_region_cuts(args.config, mapping_for_cuts)
        
        # Print summary
        print_closure_summary(closure_data, signal_name, region_cuts)
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
