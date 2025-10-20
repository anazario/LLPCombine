#!/usr/bin/env python3
"""
Explicit ABCD Configuration Generator

Generates ABCD configuration files using the new explicit x_axis/y_axis format.
Automatically detects variable names and generates descriptions from cut analysis.

Usage: python generate_explicit_abcd_config.py [options]
"""

import argparse
import yaml
import json
import os
import sys
import re
from string import Template

def create_abcd_systematics_auto(syst_prefix, precision_value):
    """Create ABCD systematics using auto placeholders (works for any predicted region)"""
    
    systematics_yaml = f"""abcd_systematics:
    # Rate parameters for control regions (auto-mapped at runtime)
    - name: "scale_{syst_prefix}_control1"
      type: "rateParam"
      value: 1.0
      bins: ["auto_control_1"]
      processes: ["backgrounds"]
    
    - name: "scale_{syst_prefix}_control2"
      type: "rateParam"
      value: 1.0
      bins: ["auto_control_2"]
      processes: ["backgrounds"]
    
    - name: "scale_{syst_prefix}_control3"
      type: "rateParam"
      value: 1.0
      bins: ["auto_control_3"]
      processes: ["backgrounds"]

    # ABCD constraint (formula auto-generated based on predicted region)
    - name: "abcd_{syst_prefix}_closure_constraint"
      type: "rateParam"
      formula: "auto"
      bins: ["auto_predicted"]
      processes: ["backgrounds"]

precision_systematics:
    # Precision systematics for control regions only
    - name: "{syst_prefix}_precision_control1"
      type: "lnN"
      value: {precision_value}
      bins: ["auto_control_1"]
      processes: ["backgrounds"]
    
    - name: "{syst_prefix}_precision_control2"
      type: "lnN"
      value: {precision_value}
      bins: ["auto_control_2"]
      processes: ["backgrounds"]
    
    - name: "{syst_prefix}_precision_control3"
      type: "lnN"
      value: {precision_value}
      bins: ["auto_control_3"]
      processes: ["backgrounds"]"""
    
    return systematics_yaml

def get_variable_shorthand(var_name):
    """Convert variable name to shorthand notation"""
    var_map = {
        'rjr_Ms': 'Ms',
        'rjr_Rs': 'Rs', 
        'SV_nHadronic': 'nHad',
        'SV_nLeptonic': 'nLep',
        'HadronicSV_dxySig': 'Sxy',
        'LeptonicSV_dxySig': 'Sxy'
    }
    
    # Handle array indices like rjr_Ms[1] -> Ms
    clean_var = re.sub(r'\[\d+\]', '', var_name)
    return var_map.get(clean_var, clean_var)

def extract_variables_from_cuts(cuts_list):
    """Extract variables and their ranges from a list of cuts"""
    variables = {}
    
    for cut in cuts_list:
        # Match patterns like "variable > value", "variable < value", "variable == value"
        match = re.search(r'(\w+(?:\[\d+\])?)\s*([<>=]+)\s*([0-9.]+)', cut)
        if match:
            var_name = match.group(1)
            operator = match.group(2)
            value = float(match.group(3))
            
            clean_var = re.sub(r'\[\d+\]', '', var_name)
            shorthand = get_variable_shorthand(clean_var)
            
            if shorthand not in variables:
                variables[shorthand] = {'ranges': [], 'equalities': [], 'original_vars': set()}
            
            variables[shorthand]['original_vars'].add(clean_var)
            
            if operator == '==':
                variables[shorthand]['equalities'].append(value)
            elif operator in ['>', '<']:
                variables[shorthand]['ranges'].append((operator, value))
    
    return variables

def generate_axis_name(x_vars, y_vars):
    """Generate axis names based on detected variables"""
    def format_vars(vars_dict):
        if not vars_dict:
            return "Unknown"
        
        # Special case: if only Sxy with different SV types, call it just Sxy
        if len(vars_dict) == 1 and 'Sxy' in vars_dict:
            sxy_vars = vars_dict['Sxy']['original_vars']
            if len(sxy_vars) > 1:  # Both Hadronic and Leptonic
                return "Sxy"
        
        # Special case: if we have SV type selection (nHad/nLep), call it Ntype regardless of other variables
        if ('nHad' in vars_dict or 'nLep' in vars_dict):
            return "Ntype"
        
        # General case: concatenate variable names
        var_names = sorted(vars_dict.keys())
        return "_".join(var_names)
    
    x_name = format_vars(x_vars)
    y_name = format_vars(y_vars) 
    
    return x_name, y_name

def generate_description(cuts_list, axis_type):
    """Generate human-readable description from cuts"""
    if not cuts_list:
        return f"{axis_type} region"
    
    variables = extract_variables_from_cuts(cuts_list)
    desc_parts = []
    
    for var, info in variables.items():
        if info['equalities']:
            for eq_val in info['equalities']:
                desc_parts.append(f"{var}={int(eq_val)}")
        
        if info['ranges']:
            # Try to extract range bounds
            lower_bound = None
            upper_bound = None
            
            for op, val in info['ranges']:
                if op == '>':
                    lower_bound = val
                elif op == '<':
                    upper_bound = val
            
            if lower_bound is not None and upper_bound is not None:
                desc_parts.append(f"{var}[{lower_bound},{upper_bound}]")
            elif lower_bound is not None:
                desc_parts.append(f"{var}>{lower_bound}")
            elif upper_bound is not None:
                desc_parts.append(f"{var}<{upper_bound}")
    
    return ", ".join(desc_parts) if desc_parts else f"{axis_type} region"

def main():
    parser = argparse.ArgumentParser(description="Generate explicit ABCD configuration")
    
    # Analysis settings
    parser.add_argument("--name", required=True, help="Analysis name")
    parser.add_argument("--output", help="Output config file (default: auto-generated)")
    parser.add_argument("--luminosity", type=float, default=30.0, help="Luminosity (fb^-1)")
    parser.add_argument("--output-dir", default="./json/", help="Output directory")
    
    # Explicit axis cut definitions
    parser.add_argument("--x-low-cuts", required=True, help="X-axis low cuts (JSON list)")
    parser.add_argument("--x-high-cuts", required=True, help="X-axis high cuts (JSON list)")
    parser.add_argument("--y-low-cuts", required=True, help="Y-axis low cuts (JSON list)")
    parser.add_argument("--y-high-cuts", required=True, help="Y-axis high cuts (JSON list)")
    
    # Common cuts
    parser.add_argument("--common-cuts", default="[]", help="Common cuts applied to all regions (JSON list)")
    
    
    # Sample settings
    parser.add_argument("--backgrounds", default="[]", help="Background samples (JSON list)")
    parser.add_argument("--signals", default='["gogoZ"]', help="Signal samples (JSON list)")
    parser.add_argument("--signal-points", default='["gogoZ_1500_500_100_10"]', help="Signal points (JSON list)")
    parser.add_argument("--data", default='["MET18"]', help="Data samples (JSON list)")
    
    # Systematics settings
    parser.add_argument("--syst-prefix", help="Systematics name prefix (default: derived from analysis name)")
    parser.add_argument("--precision-value", type=float, default=1.10, help="Precision systematic value")
    
    # Options
    parser.add_argument("--verbosity", type=int, default=2, help="Verbosity level")
    parser.add_argument("--data-as-background", action="store_true", help="Treat data samples as background processes (for data-driven ABCD)")
    parser.add_argument("--no-trigger-cuts", action="store_true", help="Disable default HLT trigger cuts")
    
    args = parser.parse_args()
    
    # Parse JSON arguments
    try:
        x_low_cuts = json.loads(args.x_low_cuts)
        x_high_cuts = json.loads(args.x_high_cuts)
        y_low_cuts = json.loads(args.y_low_cuts)
        y_high_cuts = json.loads(args.y_high_cuts)
        common_cuts = json.loads(args.common_cuts)
        backgrounds = json.loads(args.backgrounds)
        signals = json.loads(args.signals)
        signal_points = json.loads(args.signal_points)
        data = json.loads(args.data)
    except json.JSONDecodeError as e:
        print(f"Error parsing JSON arguments: {e}")
        return 1
    
    # Analyze cuts to determine axis names
    x_low_vars = extract_variables_from_cuts(x_low_cuts)
    x_high_vars = extract_variables_from_cuts(x_high_cuts)
    y_low_vars = extract_variables_from_cuts(y_low_cuts)
    y_high_vars = extract_variables_from_cuts(y_high_cuts)
    
    # Combine x and y variables for axis naming
    all_x_vars = {}
    for var_dict in [x_low_vars, x_high_vars]:
        for var, info in var_dict.items():
            if var not in all_x_vars:
                all_x_vars[var] = {'ranges': [], 'equalities': [], 'original_vars': set()}
            all_x_vars[var]['ranges'].extend(info['ranges'])
            all_x_vars[var]['equalities'].extend(info['equalities'])
            all_x_vars[var]['original_vars'].update(info['original_vars'])
    
    all_y_vars = {}
    for var_dict in [y_low_vars, y_high_vars]:
        for var, info in var_dict.items():
            if var not in all_y_vars:
                all_y_vars[var] = {'ranges': [], 'equalities': [], 'original_vars': set()}
            all_y_vars[var]['ranges'].extend(info['ranges'])
            all_y_vars[var]['equalities'].extend(info['equalities'])
            all_y_vars[var]['original_vars'].update(info['original_vars'])
    
    x_axis_name, y_axis_name = generate_axis_name(all_x_vars, all_y_vars)
    
    # Generate descriptions
    x_low_desc = generate_description(x_low_cuts, "x_low")
    x_high_desc = generate_description(x_high_cuts, "x_high")
    y_low_desc = generate_description(y_low_cuts, "y_low")
    y_high_desc = generate_description(y_high_cuts, "y_high")
    
    # Auto-generate output filename if not provided
    if not args.output:
        clean_name = args.name.replace(' ', '_').replace('-', '_')
        args.output = f"{clean_name}.yaml"
    
    # Create systematics
    syst_prefix = args.syst_prefix or args.name.replace(" ", "_").lower()
    systematics = create_abcd_systematics_auto(syst_prefix, args.precision_value)
    
    # Format cuts as YAML list with proper indentation
    def format_cuts_as_yaml(cuts_list, indent=6):
        """Format cuts list as proper YAML list with given indentation (spaces)."""
        if not cuts_list:
            return ' ' * indent + "[]\n"
        return "".join(f"{' ' * indent}- \"{cut}\"\n" for cut in cuts_list)


    
    # Create substitution dictionary
    substitutions = {
        "ANALYSIS_NAME": args.name,
        "OUTPUT_JSON": f"{args.name.replace(' ', '_').lower()}.json",
        "OUTPUT_DIR": args.output_dir,
        "LUMINOSITY": args.luminosity,
        "BACKGROUNDS": json.dumps(backgrounds),
        "SIGNALS": json.dumps(signals), 
        "SIGNAL_POINTS": json.dumps(signal_points),
        "DATA": json.dumps(data),
        "X_AXIS_NAME": x_axis_name,
        "X_LOW_DESC": x_low_desc,
        "X_LOW_CUTS": format_cuts_as_yaml(x_low_cuts,6),
        "X_HIGH_DESC": x_high_desc,
        "X_HIGH_CUTS": format_cuts_as_yaml(x_high_cuts,6),
        "Y_AXIS_NAME": y_axis_name,
        "Y_LOW_DESC": y_low_desc,
        "Y_LOW_CUTS": format_cuts_as_yaml(y_low_cuts,6),
        "Y_HIGH_DESC": y_high_desc,
        "Y_HIGH_CUTS": format_cuts_as_yaml(y_high_cuts,6),
        "COMMON_CUTS": format_cuts_as_yaml(common_cuts),
        "ABCD_FORMULA": "(@0*@1/@2)",
        "GENERATE_DATACARDS": "true",
        "DATA_AS_BACKGROUND": "true" if args.data_as_background else "false",
        "APPLY_TRIGGER_CUTS": "false" if args.no_trigger_cuts else "true",
        "VERBOSITY": args.verbosity,
        "PARALLEL": "false",
        "DRY_RUN": "false"
    }
    
    # Handle systematics format
    systematics_template = Template(systematics)
    systematics_substituted = systematics_template.safe_substitute(substitutions)
    systematics_indented = "\n".join("  " + line if line.strip() else line 
                                    for line in systematics_substituted.split("\n"))
    substitutions["SYSTEMATICS_BLOCK"] = systematics_indented
    
    # Read template and substitute
    with open("config/explicit_abcd_template.yaml", 'r') as f:
        template_content = f.read()
    
    template = Template(template_content)
    output_content = template.safe_substitute(substitutions)
    
    # Write output
    with open(args.output, 'w') as f:
        f.write(output_content)
    
    print(f"Generated explicit ABCD configuration: {args.output}")
    print(f"Analysis: {args.name}")
    print(f"X-axis: {x_axis_name} ({x_low_desc} vs {x_high_desc})")
    print(f"Y-axis: {y_axis_name} ({y_low_desc} vs {y_high_desc})")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
