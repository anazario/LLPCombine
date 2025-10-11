#!/usr/bin/env python3
"""
ABCD Configuration Generator

Generates ABCD configuration files from parameterized templates with full flexibility
for bin definitions, cuts, and systematics.

Usage: python generate_abcd_config.py [options]
"""

import argparse
import yaml
import json
import os
import sys
from string import Template

def create_standard_systematics():
    """Create standard ABCD systematics structure"""
    return {
        "abcd_systematics": [
            {
                "name": "scale_${SYST_PREFIX}_B",
                "type": "rateParam", 
                "value": 1.0,
                "bins": ["auto_control_1"],
                "processes": ["backgrounds"]
            },
            {
                "name": "scale_${SYST_PREFIX}_C",
                "type": "rateParam",
                "value": 1.0, 
                "bins": ["auto_control_2"],
                "processes": ["backgrounds"]
            },
            {
                "name": "scale_${SYST_PREFIX}_D", 
                "type": "rateParam",
                "value": 1.0,
                "bins": ["auto_control_3"],
                "processes": ["backgrounds"]
            },
            {
                "name": "abcd_${SYST_PREFIX}_constraint",
                "type": "rateParam",
                "formula": "auto",
                "bins": ["auto_predicted"],
                "processes": ["backgrounds"]
            }
        ],
        "precision_systematics": [
            {
                "name": "${SYST_PREFIX}_precision_B",
                "type": "lnN",
                "value": "${PRECISION_VALUE}",
                "bins": ["auto_control_1"],
                "processes": ["backgrounds"]
            },
            {
                "name": "${SYST_PREFIX}_precision_C",
                "type": "lnN", 
                "value": "${PRECISION_VALUE}",
                "bins": ["auto_control_2"],
                "processes": ["backgrounds"]
            },
            {
                "name": "${SYST_PREFIX}_precision_D",
                "type": "lnN",
                "value": "${PRECISION_VALUE}",
                "bins": ["auto_control_3"], 
                "processes": ["backgrounds"]
            }
        ]
    }

def create_cuts(sv_type, var1_name, var1_low, var1_high, var2_name, var2_low, var2_high, 
                dxysig_low, dxysig_high, region_type):
    """Create cuts for a specific region"""
    
    # Base SV cut
    cuts = [f"SV_{sv_type} == 1"]
    
    # Variable 1 cuts (Ms or Rs)
    if var1_high is not None:
        cuts.extend([f"{var1_name} > {var1_low}", f"{var1_name} < {var1_high}"])
    else:
        cuts.append(f"{var1_name} > {var1_low}")
    
    # Variable 2 cuts (Rs or Ms) - optional
    if var2_name and var2_low is not None:
        if var2_high is not None:
            cuts.extend([f"{var2_name} > {var2_low}", f"{var2_name} < {var2_high}"])
        else:
            cuts.append(f"{var2_name} > {var2_low}")
    
    # dxySig cuts based on region
    sv_branch_name = "HadronicSV" if sv_type == "nHadronic" else "LeptonicSV"
    if region_type in ["A", "C"]:  # Low dxySig regions
        cuts.extend([f"{sv_branch_name}_dxySig[0] > {dxysig_low}", 
                    f"{sv_branch_name}_dxySig[0] < {dxysig_high}"])
    else:  # High dxySig regions (B, D)
        cuts.extend([f"{sv_branch_name}_dxySig[0] > {dxysig_high}",
                    f"{sv_branch_name}_dxySig[0] < 1000"])
    
    return cuts

def format_range_string(low, high):
    """Format range for descriptions"""
    if high is None:
        return f">{low}"
    else:
        return f"{low}-{high}"

def format_cuts_as_yaml(cuts_list):
    """Format cuts list as proper YAML list with indentation"""
    formatted_cuts = []
    for cut in cuts_list:
        formatted_cuts.append(f'      - "{cut}"')
    return "\n".join(formatted_cuts)

def main():
    parser = argparse.ArgumentParser(description="Generate ABCD configuration from template")
    
    # Analysis settings
    parser.add_argument("--name", required=True, help="Analysis name")
    parser.add_argument("--output", required=True, help="Output config file")
    parser.add_argument("--luminosity", type=float, default=30.0, help="Luminosity (fb^-1)")
    parser.add_argument("--output-dir", default="./json/", help="Output directory")
    
    # SV and variable settings
    parser.add_argument("--sv-type", choices=["nHadronic", "nLeptonic"], required=True, help="SV type")
    parser.add_argument("--var1-name", required=True, help="First variable name (e.g., rjr_Ms[1])")
    parser.add_argument("--var1-ranges", required=True, help="Var1 ranges: 'low1,high1;low2,high2' (use 'inf' for no upper bound)")
    parser.add_argument("--var2-name", help="Second variable name (optional)")
    parser.add_argument("--var2-ranges", help="Var2 ranges (optional): 'low1,high1;low2,high2'")
    
    # dxySig settings
    parser.add_argument("--dxysig-low", type=float, default=100, help="dxySig low boundary")
    parser.add_argument("--dxysig-mid", type=float, default=300, help="dxySig middle boundary") 
    
    # ABCD settings
    parser.add_argument("--predicted", choices=["region_A", "region_B", "region_C", "region_D"], 
                       default="region_A", help="Which region to predict")
    
    # Sample settings
    parser.add_argument("--backgrounds", default="[]", help="Background samples (JSON list)")
    parser.add_argument("--signals", default='["gogoZ"]', help="Signal samples (JSON list)")
    parser.add_argument("--signal-points", default='["gogoZ_1500_500_100_10"]', help="Signal points (JSON list)")
    parser.add_argument("--data", default='["DisplacedJet18"]', help="Data samples (JSON list)")
    
    # Systematics settings
    parser.add_argument("--syst-prefix", help="Systematics name prefix (default: derived from analysis name)")
    parser.add_argument("--precision-value", type=float, default=1.10, help="Precision systematic value")
    parser.add_argument("--custom-systematics", help="Custom systematics YAML file to use instead of standard")
    
    # Options
    parser.add_argument("--verbosity", type=int, default=2, help="Verbosity level")
    parser.add_argument("--template", default="config/abcd_template.yaml", help="Template file")
    
    args = parser.parse_args()
    
    # Parse variable ranges
    def parse_ranges(range_str):
        ranges = []
        for r in range_str.split(';'):
            low, high = r.split(',')
            low = float(low)
            high = None if high.lower() == 'inf' else float(high)
            ranges.append((low, high))
        return ranges
    
    var1_ranges = parse_ranges(args.var1_ranges)
    var2_ranges = parse_ranges(args.var2_ranges) if args.var2_ranges else [None, None]
    
    # Check we have 2x2 configuration
    if len(var1_ranges) != 2 or (args.var2_ranges and len(var2_ranges) != 2):
        print("Error: Must specify exactly 2 ranges for each variable to form 2x2 ABCD grid")
        return 1
    
    # Generate region names and descriptions
    var1_low_str = format_range_string(var1_ranges[0][0], var1_ranges[0][1])
    var1_high_str = format_range_string(var1_ranges[1][0], var1_ranges[1][1])
    
    if args.var2_ranges:
        var2_low_str = format_range_string(var2_ranges[0][0], var2_ranges[0][1])
        var2_high_str = format_range_string(var2_ranges[1][0], var2_ranges[1][1])
        base_name = f"{args.sv_type}_{args.var1_name.replace('[1]', '').replace('rjr_', '')}{var1_low_str}_{args.var2_name.replace('[1]', '').replace('rjr_', '')}"
    else:
        base_name = f"{args.sv_type}_{args.var1_name.replace('[1]', '').replace('rjr_', '')}"
    
    # Create region definitions
    regions = {}
    region_cuts = {}
    region_descs = {}
    
    # Region A: var1_high, dxySig_low (typically the signal region)
    region_a_name = f"{base_name}_A"
    regions["region_A"] = region_a_name
    region_cuts[region_a_name] = create_cuts(args.sv_type, args.var1_name, var1_ranges[1][0], var1_ranges[1][1],
                                           args.var2_name, var2_ranges[1][0] if args.var2_ranges else None, 
                                           var2_ranges[1][1] if args.var2_ranges else None,
                                           args.dxysig_low, args.dxysig_mid, "A")
    region_descs[region_a_name] = f"{args.sv_type}, Var1 {var1_high_str}, dxySig low - PREDICTED" if args.predicted == "region_A" else f"{args.sv_type}, Var1 {var1_high_str}, dxySig low"
    
    # Region B: var1_high, dxySig_high  
    region_b_name = f"{base_name}_B"
    regions["region_B"] = region_b_name
    region_cuts[region_b_name] = create_cuts(args.sv_type, args.var1_name, var1_ranges[1][0], var1_ranges[1][1],
                                           args.var2_name, var2_ranges[1][0] if args.var2_ranges else None,
                                           var2_ranges[1][1] if args.var2_ranges else None, 
                                           args.dxysig_low, args.dxysig_mid, "B")
    region_descs[region_b_name] = f"{args.sv_type}, Var1 {var1_high_str}, dxySig high - PREDICTED" if args.predicted == "region_B" else f"{args.sv_type}, Var1 {var1_high_str}, dxySig high"
    
    # Region C: var1_low, dxySig_low
    region_c_name = f"{base_name}_C" 
    regions["region_C"] = region_c_name
    region_cuts[region_c_name] = create_cuts(args.sv_type, args.var1_name, var1_ranges[0][0], var1_ranges[0][1],
                                           args.var2_name, var2_ranges[0][0] if args.var2_ranges else None,
                                           var2_ranges[0][1] if args.var2_ranges else None,
                                           args.dxysig_low, args.dxysig_mid, "C")
    region_descs[region_c_name] = f"{args.sv_type}, Var1 {var1_low_str}, dxySig low - PREDICTED" if args.predicted == "region_C" else f"{args.sv_type}, Var1 {var1_low_str}, dxySig low"
    
    # Region D: var1_low, dxySig_high
    region_d_name = f"{base_name}_D"
    regions["region_D"] = region_d_name  
    region_cuts[region_d_name] = create_cuts(args.sv_type, args.var1_name, var1_ranges[0][0], var1_ranges[0][1],
                                           args.var2_name, var2_ranges[0][0] if args.var2_ranges else None,
                                           var2_ranges[0][1] if args.var2_ranges else None,
                                           args.dxysig_low, args.dxysig_mid, "D")
    region_descs[region_d_name] = f"{args.sv_type}, Var1 {var1_low_str}, dxySig high - PREDICTED" if args.predicted == "region_D" else f"{args.sv_type}, Var1 {var1_low_str}, dxySig high"
    
    # Create systematics
    syst_prefix = args.syst_prefix or args.name.replace(" ", "_").lower()
    
    if args.custom_systematics:
        with open(args.custom_systematics) as f:
            systematics = yaml.safe_load(f)
    else:
        systematics = create_standard_systematics()
    
    # Create substitution dictionary
    substitutions = {
        "ANALYSIS_NAME": args.name,
        "OUTPUT_JSON": f"{args.name.replace(' ', '_').lower()}.json",
        "OUTPUT_DIR": args.output_dir,
        "LUMINOSITY": args.luminosity,
        "BACKGROUNDS": args.backgrounds,
        "SIGNALS": args.signals, 
        "SIGNAL_POINTS": args.signal_points,
        "DATA": args.data,
        "REGION_A_NAME": regions["region_A"],
        "REGION_B_NAME": regions["region_B"], 
        "REGION_C_NAME": regions["region_C"],
        "REGION_D_NAME": regions["region_D"],
        "PREDICTED_REGION": args.predicted,
        "ABCD_FORMULA": "(@0*@1/@2)",
        "GENERATE_DATACARDS": "true",
        "REGION_A_DESC": region_descs[regions["region_A"]],
        "REGION_B_DESC": region_descs[regions["region_B"]],
        "REGION_C_DESC": region_descs[regions["region_C"]],
        "REGION_D_DESC": region_descs[regions["region_D"]],
        "REGION_A_CUTS": format_cuts_as_yaml(region_cuts[regions["region_A"]]),
        "REGION_B_CUTS": format_cuts_as_yaml(region_cuts[regions["region_B"]]),
        "REGION_C_CUTS": format_cuts_as_yaml(region_cuts[regions["region_C"]]),
        "REGION_D_CUTS": format_cuts_as_yaml(region_cuts[regions["region_D"]]),
        "SYSTEMATICS_BLOCK": yaml.dump(systematics),
        "SYST_PREFIX": syst_prefix,
        "PRECISION_VALUE": args.precision_value,
        "VERBOSITY": args.verbosity,
        "PARALLEL": "false",
        "DRY_RUN": "false"
    }
    
    # Read template and substitute
    with open(args.template, 'r') as f:
        template_content = f.read()
    
    template = Template(template_content)
    
    # Substitute systematics block separately to handle nested substitutions
    systematics_yaml = yaml.dump(systematics, default_flow_style=False, indent=2)
    systematics_template = Template(systematics_yaml)
    systematics_substituted = systematics_template.safe_substitute(substitutions)
    
    # Indent the systematics block properly for YAML
    systematics_indented = "\n".join("  " + line if line.strip() else line 
                                    for line in systematics_substituted.split("\n"))
    substitutions["SYSTEMATICS_BLOCK"] = systematics_indented
    
    output_content = template.safe_substitute(substitutions)
    
    # Write output
    with open(args.output, 'w') as f:
        f.write(output_content)
    
    print(f"Generated ABCD configuration: {args.output}")
    print(f"Analysis: {args.name}")
    print(f"Regions: A={regions['region_A']}, B={regions['region_B']}, C={regions['region_C']}, D={regions['region_D']}")
    print(f"Predicted: {args.predicted}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())