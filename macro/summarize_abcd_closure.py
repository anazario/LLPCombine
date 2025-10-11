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

def extract_abcd_info(fit_file):
    """Extract ABCD closure information from fit diagnostics file"""
    f = ROOT.TFile.Open(fit_file, "READ")
    if not f or f.IsZombie():
        print(f"Error: Could not open {fit_file}")
        return None
    
    # Get the fit result
    fit_b = f.Get("fit_b")
    if not fit_b:
        print(f"Error: No fit_b result found in {fit_file}")
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
    if shapes_prefit:
        # Look for data histograms in each bin
        for key in shapes_prefit.GetListOfKeys():
            bin_name = key.GetName()
            bin_dir = shapes_prefit.GetDirectory(bin_name)
            if bin_dir:
                data_hist = bin_dir.Get("data")
                if data_hist:
                    data_yields[bin_name] = data_hist.Integral()
    
    f.Close()
    
    return {
        "params": params,
        "data_yields": data_yields,
        "fit_file": fit_file
    }

def identify_abcd_regions(data_yields, params):
    """Identify which bins correspond to ABCD regions and which one is predicted"""
    all_bins = list(data_yields.keys())
    
    # Find the predicted bin by looking for the one with error = 0 (fixed by formula)
    # or no scale parameter (controlled by ABCD constraint)
    predicted_bin = None
    for bin_name in all_bins:
        scale_param = f"scale_{bin_name}"
        if scale_param in params:
            if params[scale_param]["error"] == 0.0:
                predicted_bin = bin_name
                break
        else:
            # Bin without scale parameter is likely the predicted one
            predicted_bin = bin_name
            break
    
    # If no clear predicted bin found, look for naming patterns
    if not predicted_bin:
        for bin_name in all_bins:
            bin_lower = bin_name.lower()
            if 'sr' in bin_lower or 'signal' in bin_lower:
                predicted_bin = bin_name
                break
    
    # If still not found, use the bin with smallest scale factor error as fallback
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
        predicted_bin = all_bins[0]  # Ultimate fallback
    
    # Get the three control bins
    control_bins = [b for b in all_bins if b != predicted_bin]
    control_bins.sort()  # Sort for consistent assignment
    
    # Create mapping with predicted bin as "PREDICTED" and controls as generic labels
    region_mapping = {
        "predicted": predicted_bin,
        "control_1": control_bins[0] if len(control_bins) > 0 else None,
        "control_2": control_bins[1] if len(control_bins) > 1 else None,
        "control_3": control_bins[2] if len(control_bins) > 2 else None
    }
    
    return region_mapping

def calculate_closure_metrics(abcd_info):
    """Calculate ABCD closure metrics"""
    params = abcd_info["params"]
    data_yields = abcd_info["data_yields"]
    
    # Identify regions
    regions = identify_abcd_regions(data_yields, params)
    if not regions or not regions.get("predicted"):
        return None
    
    # Get yields - predicted region and three control regions
    predicted_bin = regions["predicted"]
    control_bins = [regions["control_1"], regions["control_2"], regions["control_3"]]
    control_bins = [b for b in control_bins if b is not None]
    
    if len(control_bins) < 3:
        return None  # Need at least 3 control regions for ABCD
    
    yield_predicted_true = data_yields.get(predicted_bin, 0)  # TRUE yield in predicted region
    yield_ctrl1 = data_yields.get(control_bins[0], 0)
    yield_ctrl2 = data_yields.get(control_bins[1], 0)
    yield_ctrl3 = data_yields.get(control_bins[2], 0)
    
    # Get scale factors from fit
    scale_predicted = params.get(f"scale_{predicted_bin}", {"value": 1.0, "error": 0.0})
    scale_ctrl1 = params.get(f"scale_{control_bins[0]}", {"value": 1.0, "error": 0.0})
    scale_ctrl2 = params.get(f"scale_{control_bins[1]}", {"value": 1.0, "error": 0.0})
    scale_ctrl3 = params.get(f"scale_{control_bins[2]}", {"value": 1.0, "error": 0.0})
    
    # Calculate ABCD predictions: Predicted = Control1 * Control2 / Control3
    # (The specific formula depends on which region is predicted)
    naive_pred = (yield_ctrl1 * yield_ctrl2 / yield_ctrl3) if yield_ctrl3 > 0 else 0
    fitted_pred = (yield_ctrl1 * scale_ctrl1["value"] * yield_ctrl2 * scale_ctrl2["value"] / 
                   (yield_ctrl3 * scale_ctrl3["value"])) if yield_ctrl3 > 0 else 0
    
    # Calculate closure metrics (how well does prediction match truth)
    closure_ratio = fitted_pred / yield_predicted_true if yield_predicted_true > 0 else 0
    naive_ratio = naive_pred / yield_predicted_true if yield_predicted_true > 0 else 0
    
    # Calculate pull (fitted prediction vs true yield, in units of statistical uncertainty)
    stat_error_predicted = sqrt(yield_predicted_true) if yield_predicted_true > 0 else 1
    pull = (fitted_pred - yield_predicted_true) / stat_error_predicted
    
    return {
        "regions": regions,
        "yields": {
            "predicted_true": yield_predicted_true,  # TRUE yield in predicted region
            "control_1": yield_ctrl1,
            "control_2": yield_ctrl2, 
            "control_3": yield_ctrl3
        },
        "scale_factors": {
            "predicted": scale_predicted,
            "control_1": scale_ctrl1,
            "control_2": scale_ctrl2,
            "control_3": scale_ctrl3
        },
        "predictions": {
            "naive": naive_pred,
            "fitted": fitted_pred
        },
        "closure_metrics": {
            "closure_ratio": closure_ratio,
            "naive_ratio": naive_ratio,
            "pull": pull,
            "stat_error": stat_error_predicted
        }
    }

def print_closure_summary(closure_data, signal_name):
    """Print a formatted summary of the closure test"""
    if not closure_data:
        print(f"❌ Could not extract closure data for {signal_name}")
        return
    
    regions = closure_data["regions"]
    yields = closure_data["yields"]
    scales = closure_data["scale_factors"]
    preds = closure_data["predictions"]
    metrics = closure_data["closure_metrics"]
    
    print(f"\n{'='*60}")
    print(f"ABCD CLOSURE TEST SUMMARY: {signal_name}")
    print(f"{'='*60}")
    
    print(f"\n📊 REGION MAPPING:")
    print(f"   Predicted region: {regions['predicted']}")
    print(f"   Control region 1: {regions['control_1']}")
    print(f"   Control region 2: {regions['control_2']}")
    print(f"   Control region 3: {regions['control_3']}")
    
    print(f"\n📈 TRUE YIELDS:")
    print(f"   Predicted (true): {yields['predicted_true']:8.1f}")
    print(f"   Control 1:        {yields['control_1']:8.1f}")
    print(f"   Control 2:        {yields['control_2']:8.1f}")
    print(f"   Control 3:        {yields['control_3']:8.1f}")
    
    print(f"\n⚖️  FITTED SCALE FACTORS:")
    print(f"   Predicted: {scales['predicted']['value']:6.3f} ± {scales['predicted']['error']:5.3f}")
    print(f"   Control 1: {scales['control_1']['value']:6.3f} ± {scales['control_1']['error']:5.3f}")
    print(f"   Control 2: {scales['control_2']['value']:6.3f} ± {scales['control_2']['error']:5.3f}")
    print(f"   Control 3: {scales['control_3']['value']:6.3f} ± {scales['control_3']['error']:5.3f}")
    
    print(f"\n🎯 ABCD PREDICTIONS:")
    print(f"   Naive (C1×C2/C3):      {preds['naive']:8.1f}")
    print(f"   Fitted (scaled):       {preds['fitted']:8.1f}")
    print(f"   True (predicted):      {yields['predicted_true']:8.1f}")
    
    print(f"\n📏 CLOSURE PERFORMANCE:")
    print(f"   Naive ratio (pred/true):   {metrics['naive_ratio']:6.3f}")
    print(f"   Fitted ratio (pred/true):  {metrics['closure_ratio']:6.3f}")
    print(f"   Statistical error:         {metrics['stat_error']:6.1f}")
    print(f"   Pull (fitted-true)/σ:      {metrics['pull']:+6.2f}")
    
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

def main():
    parser = argparse.ArgumentParser(description="Summarize ABCD closure test results")
    parser.add_argument("datacard_dir", help="Directory containing fit results")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    
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
        abcd_info = extract_abcd_info(fit_file)
        if not abcd_info:
            continue
            
        # Calculate closure metrics
        closure_data = calculate_closure_metrics(abcd_info)
        
        # Print summary
        print_closure_summary(closure_data, signal_name)
    
    return 0

if __name__ == "__main__":
    sys.exit(main())