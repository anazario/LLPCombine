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
    """Identify which bins correspond to ABCD regions A, B, C, D"""
    # Look for bins with scale parameters
    scale_bins = []
    for param_name in params:
        if param_name.startswith("scale_") and param_name != "scale_r":
            bin_name = param_name.replace("scale_", "")
            if bin_name in data_yields:
                scale_bins.append(bin_name)
    
    # Sort bins to identify regions consistently
    # Usually the predicted region (A) is the one NOT in the scale parameters list
    all_bins = set(data_yields.keys())
    control_bins = set(scale_bins)
    predicted_bins = all_bins - control_bins
    
    if len(predicted_bins) == 1:
        region_A = list(predicted_bins)[0]
        regions_BCD = sorted(list(control_bins))
        
        if len(regions_BCD) >= 3:
            return {
                "A": region_A,
                "B": regions_BCD[0], 
                "C": regions_BCD[1],
                "D": regions_BCD[2]
            }
    
    return None

def calculate_closure_metrics(abcd_info):
    """Calculate ABCD closure metrics"""
    params = abcd_info["params"]
    data_yields = abcd_info["data_yields"]
    
    # Identify regions
    regions = identify_abcd_regions(data_yields, params)
    if not regions:
        return None
    
    # Get observed yields
    yield_A_obs = data_yields.get(regions["A"], 0)
    yield_B = data_yields.get(regions["B"], 0)
    yield_C = data_yields.get(regions["C"], 0) 
    yield_D = data_yields.get(regions["D"], 0)
    
    # Get scale factors from fit
    scale_B = params.get(f"scale_{regions['B']}", {"value": 1.0, "error": 0.0})
    scale_C = params.get(f"scale_{regions['C']}", {"value": 1.0, "error": 0.0})
    scale_D = params.get(f"scale_{regions['D']}", {"value": 1.0, "error": 0.0})
    
    # Calculate predictions
    naive_pred = (yield_B * yield_C / yield_D) if yield_D > 0 else 0
    fitted_pred = (yield_B * scale_B["value"] * yield_C * scale_C["value"] / 
                   (yield_D * scale_D["value"])) if yield_D > 0 else 0
    
    # Calculate closure metrics
    closure_ratio = fitted_pred / yield_A_obs if yield_A_obs > 0 else 0
    naive_ratio = naive_pred / yield_A_obs if yield_A_obs > 0 else 0
    
    # Calculate pull (fitted prediction vs observed, in units of statistical uncertainty)
    stat_error_A = sqrt(yield_A_obs) if yield_A_obs > 0 else 1
    pull = (fitted_pred - yield_A_obs) / stat_error_A
    
    return {
        "regions": regions,
        "yields": {
            "A_observed": yield_A_obs,
            "B": yield_B,
            "C": yield_C, 
            "D": yield_D
        },
        "scale_factors": {
            "B": scale_B,
            "C": scale_C,
            "D": scale_D
        },
        "predictions": {
            "naive": naive_pred,
            "fitted": fitted_pred
        },
        "closure_metrics": {
            "closure_ratio": closure_ratio,
            "naive_ratio": naive_ratio,
            "pull": pull,
            "stat_error": stat_error_A
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
    print(f"   Region A (predicted): {regions['A']}")
    print(f"   Region B (control):   {regions['B']}")
    print(f"   Region C (control):   {regions['C']}")
    print(f"   Region D (control):   {regions['D']}")
    
    print(f"\n📈 OBSERVED YIELDS:")
    print(f"   A (observed):  {yields['A_observed']:8.1f}")
    print(f"   B (control):   {yields['B']:8.1f}")
    print(f"   C (control):   {yields['C']:8.1f}")
    print(f"   D (control):   {yields['D']:8.1f}")
    
    print(f"\n⚖️  FITTED SCALE FACTORS:")
    print(f"   Region B: {scales['B']['value']:6.3f} ± {scales['B']['error']:5.3f}")
    print(f"   Region C: {scales['C']['value']:6.3f} ± {scales['C']['error']:5.3f}")
    print(f"   Region D: {scales['D']['value']:6.3f} ± {scales['D']['error']:5.3f}")
    
    print(f"\n🎯 ABCD PREDICTIONS:")
    print(f"   Naive (B×C/D):         {preds['naive']:8.1f}")
    print(f"   Fitted (scaled):       {preds['fitted']:8.1f}")
    print(f"   Observed (A):          {yields['A_observed']:8.1f}")
    
    print(f"\n📏 CLOSURE PERFORMANCE:")
    print(f"   Naive ratio (pred/obs):    {metrics['naive_ratio']:6.3f}")
    print(f"   Fitted ratio (pred/obs):   {metrics['closure_ratio']:6.3f}")
    print(f"   Statistical error on A:    {metrics['stat_error']:6.1f}")
    print(f"   Pull (fitted-obs)/σ:       {metrics['pull']:+6.2f}")
    
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