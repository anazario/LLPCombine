#!/usr/bin/env python3
"""
ABCD Closure Results Table Generator

Processes JSON closure results from the summary script and generates professional
LaTeX tables for publication. Creates both comprehensive and prediction-specific tables.

Usage: python generate_closure_tables.py <results_directory>
"""

import os
import sys
import json
import glob
import argparse
from pathlib import Path
import subprocess
from collections import defaultdict
import re
from concurrent.futures import ThreadPoolExecutor
import multiprocessing


def extract_variables_from_cuts(region_cuts):
    """Extract X and Y variables from region cuts"""
    if not region_cuts:
        return "Unknown", "Unknown"
    
    x_var, y_var = "Unknown", "Unknown"
    
    # Look through all regions to find variable patterns
    all_cuts = []
    for cuts in region_cuts.values():
        all_cuts.extend(cuts)
    
    # Find variables that appear in range cuts (> and < conditions)
    range_vars = set()
    for cut in all_cuts:
        # Match patterns like "variable > value" or "variable[0] < value"
        match = re.search(r'(\w+(?:\[\d+\])?)\s*[<>]', cut)
        if match:
            var = match.group(1).replace('[0]', '').replace('[1]', '')
            range_vars.add(var)
    
    # Convert to sorted list for consistency
    var_list = sorted(list(range_vars))
    
    if len(var_list) >= 2:
        x_var = format_variable_name(var_list[0])
        y_var = format_variable_name(var_list[1])
    elif len(var_list) == 1:
        x_var = format_variable_name(var_list[0])
    
    return x_var, y_var

def shorten_bin_name(bin_name):
    """Create shortened versions of bin names for table display"""
    # Remove common prefixes/suffixes
    shortened = bin_name
    shortened = re.sub(r'^n(Hadronic|Leptonic)_', '', shortened)
    shortened = re.sub(r'_[ABCD]$', '', shortened)
    shortened = re.sub(r'HadronicSV_dxySig_Rs', 'SxyRs', shortened)
    shortened = re.sub(r'_xLow_yHigh', '', shortened)
    shortened = re.sub(r'_xHigh_yHigh', '', shortened)
    shortened = re.sub(r'_xLow_yLow', '', shortened)  
    shortened = re.sub(r'_xHigh_yLow', '', shortened)
    
    # Limit length
    if len(shortened) > 15:
        shortened = shortened[:15] + "..."
    
    return shortened

def extract_base_config_name(result):
    """Extract base configuration name from result data"""
    # Try to get from config_file first (more reliable)
    config_file = result.get('analysis_info', {}).get('config_file', '')
    if config_file:
        # Remove .yaml extension and PredictA/B/C/D suffix
        base = config_file.replace('.yaml', '')
        base = re.sub(r'_Predict[ABCD]$', '', base)
        # Clean up directory prefixes
        base = base.replace('config/', '').replace('firstLookConfigs/', '')
        return base
    
    # Fallback to signal_name
    signal_name = result.get('analysis_info', {}).get('signal_name', '')
    match = re.search(r'(.+)_Predict[ABCD]$', signal_name)
    if match:
        base = match.group(1)
        base = re.sub(r'^gogoZ_\d+_\d+_\d+_\d+_', '', base)
        return base
    return signal_name

def organize_results_by_groups(results):
    """Organize results into ABCD groups and assign sequential numbering"""
    # Group by base configuration name
    groups = defaultdict(list)
    
    for result in results:
        base_name = extract_base_config_name(result)
        predicted_region = result['predicted_region']
        
        groups[base_name].append({
            'result': result,
            'predicted_region': predicted_region,
            'base_name': base_name
        })
    
    # Sort each group by predicted region (A, B, C, D)
    region_order = {'A': 0, 'B': 1, 'C': 2, 'D': 3}
    for base_name in groups:
        groups[base_name].sort(key=lambda x: region_order.get(x['predicted_region'], 4))
    
    # Assign sequential group numbers
    sorted_group_names = sorted(groups.keys())
    numbered_groups = {}
    
    for i, base_name in enumerate(sorted_group_names, 1):
        numbered_groups[i] = {
            'base_name': base_name,
            'results': groups[base_name]
        }
    
    return numbered_groups

def load_closure_results(results_dir):
    """Load all closure result JSON files from directory"""
    results = []
    
    json_files = glob.glob(os.path.join(results_dir, "*_closure_results.json"))
    
    for json_file in json_files:
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
                results.append(data)
        except Exception as e:
            print(f"Warning: Failed to load {json_file}: {e}")
    
    return results

def format_scale_factors(scale_factors, predicted_region):
    """Format scale factors with predicted region showing 1.000 ± 0.000"""
    formatted = []
    for region in ['A', 'B', 'C', 'D']:
        if region == predicted_region:
            formatted.append(r"1.000 $\pm$ 0.000")
        else:
            sf = scale_factors[region]
            formatted.append(f"{sf['value']:.3f} $\\pm$ {sf['error']:.3f}")
    return " / ".join(formatted)

def generate_comprehensive_table(grouped_results, output_file):
    """Generate comprehensive table with all results"""
    
    latex_content = r"""
\documentclass[10pt]{article}
\usepackage[margin=0.5in, landscape]{geometry}
\usepackage{booktabs}
\usepackage{array}
\usepackage{longtable}
\usepackage{amsmath}
\usepackage{textcomp}
\usepackage{siunitx}

\sisetup{
    round-mode=places,
    round-precision=3
}

\begin{document}
\begin{center}
{\Large \bf ABCD Closure Test Results - Comprehensive Summary}
\end{center}

\begin{longtable}{lcccccc}
\toprule
Config & Observed & Predicted & Scale Factors & Ratio & Pull \\
 & & & A / B / C / D & Pred/Obs & $\sigma$ \\
\midrule
\endfirsthead

\toprule
Config & Observed & Predicted & Scale Factors & Ratio & Pull \\
 & & & A / B / C / D & Pred/Obs & $\sigma$ \\
\midrule
\endhead

\bottomrule
\endfoot

"""
    
    # Process each numbered group
    sorted_groups = sorted(grouped_results.keys())
    for i, group_num in enumerate(sorted_groups):
        group_data = grouped_results[group_num]
        
        for item in group_data['results']:
            result = item['result']
            predicted_region = item['predicted_region']
            
            # Configuration label
            config_label = f"{group_num}{predicted_region}"
            
            # Extract metrics
            observed = result['yields']['predicted_true']
            predicted = result['predictions']['fitted']
            ratio = result['closure_metrics']['closure_ratio']
            pull = result['closure_metrics']['pull']
            
            # Format scale factors
            scale_factors_str = format_scale_factors(result['scale_factors'], predicted_region)
            
            # Format the table row
            latex_content += f"{config_label} & {observed:.1f} & {predicted:.3f} & "
            latex_content += f"{scale_factors_str} & {ratio:.3f} & {pull:+.2f} \\\\\n"
        
        # Add thin divider after each group (except the last one)
        if i < len(sorted_groups) - 1:
            latex_content += "\\midrule\n"
    
    latex_content += r"""
\end{longtable}

\vspace{1cm}

\begin{center}
{\bf Notes:} \\
Config: Configuration number + predicted region (see supplementary table for details) \\
Observed: True yield in predicted region \\
Predicted: ABCD formula prediction after fit \\
Scale Factors: Fitted scale factors for regions A/B/C/D (predicted region fixed at 1) \\
Ratio: Predicted/Observed (ideal = 1.0) \\
Pull: $(Predicted - Observed)/\sigma_{\mathrm{stat}}$ (ideal = 0)
\end{center}

\end{document}
"""
    
    with open(output_file, 'w') as f:
        f.write(latex_content)

def generate_group_tables(grouped_results, output_dir):
    """Generate separate tables for each ABCD group"""
    
    for group_num, group_data in grouped_results.items():
        base_name = group_data['base_name']
        output_file = os.path.join(output_dir, f"abcd_closure_group_{group_num}.tex")
        
        latex_content = f"""
\\documentclass[10pt]{{article}}
\\usepackage[margin=0.5in]{{geometry}}
\\usepackage{{booktabs}}
\\usepackage{{array}}
\\usepackage{{amsmath}}
\\usepackage{{textcomp}}
\\usepackage{{siunitx}}

\\sisetup{{
    round-mode=places,
    round-precision=3
}}

\\begin{{document}}
\\begin{{center}}
{{\\Large \\bf ABCD Closure Test Results - Configuration {group_num}}}
\\end{{center}}

\\begin{{table}}[htbp]
\\centering
\\begin{{tabular}}{{lcccccc}}
\\toprule
Config & Observed & Predicted & Scale Factors & Ratio & Pull \\\\
 & & & A / B / C / D & Pred/Obs & $\\sigma$ \\\\
\\midrule
"""
        
        for item in group_data['results']:
            result = item['result']
            predicted_region = item['predicted_region']
            
            # Configuration label
            config_label = f"{group_num}{predicted_region}"
            
            # Extract metrics
            observed = result['yields']['predicted_true']
            predicted = result['predictions']['fitted']
            ratio = result['closure_metrics']['closure_ratio']
            pull = result['closure_metrics']['pull']
            
            # Format scale factors
            scale_factors_str = format_scale_factors(result['scale_factors'], predicted_region)
            
            # Format row
            latex_content += f"{config_label} & {observed:.1f} & {predicted:.3f} & "
            latex_content += f"{scale_factors_str} & {ratio:.3f} & {pull:+.2f} \\\\\n"
        
        latex_content += f"""
\\bottomrule
\\end{{tabular}}
\\caption{{ABCD closure test results for configuration {group_num}. Each row shows results 
when predicting the indicated region (A, B, C, or D) using the ABCD method.}}
\\label{{tab:abcd_closure_group_{group_num}}}
\\end{{table}}

\\vspace{{1cm}}

\\begin{{center}}
{{\\bf Notes:}} \\\\
Config: {group_num} + predicted region (A, B, C, or D) \\\\
Observed: True yield in predicted region \\\\
Predicted: ABCD formula prediction after fit \\\\
Scale Factors: Fitted scale factors for regions A/B/C/D (predicted region fixed at 1) \\\\
Ratio: Predicted/Observed (ideal = 1.0) \\\\
Pull: $(Predicted - Observed)/\\sigma_{{\\mathrm{{stat}}}}$ (ideal = 0)
\\end{{center}}

\\end{{document}}
"""
        
        with open(output_file, 'w') as f:
            f.write(latex_content)

def generate_config_definitions_table(grouped_results, output_file):
    """Generate supplementary table with configuration definitions"""
    
    latex_content = r"""
\documentclass[10pt]{article}
\usepackage[margin=0.5in]{geometry}
\usepackage{booktabs}
\usepackage{array}
\usepackage{amsmath}

\begin{document}
\begin{center}
{\Large \bf ABCD Configuration Definitions}
\end{center}

\begin{table}[htbp]
\centering
\begin{tabular}{ll}
\toprule
Config & Description \\
\midrule
"""
    
    # Process each numbered group
    for group_num in sorted(grouped_results.keys()):
        group_data = grouped_results[group_num]
        base_name = group_data['base_name']
        
        # Clean up the base name for display
        display_name = base_name.replace('_', ' ').replace('gogoZ 1500 500 100 10 ', '')
        
        latex_content += f"{group_num} & {display_name} \\\\\n"
    
    latex_content += r"""
\bottomrule
\end{tabular}
\caption{Configuration definitions for ABCD closure test results. Each configuration 
is tested with four different predicted regions (A, B, C, D), resulting in entries 
like 1A, 1B, 1C, 1D for configuration 1.}
\label{tab:abcd_config_definitions}
\end{table}

\end{document}
"""
    
    with open(output_file, 'w') as f:
        f.write(latex_content)

def extract_variable_ranges_from_cuts(region_cuts):
    """Extract the x and y variable ranges from ABCD cuts"""
    if not region_cuts:
        return "Unknown", "Unknown", [], [], []
    
    import re
    
    # Extract cuts for each region
    regions = ['A', 'B', 'C', 'D']
    
    # Find common cuts (appear in all regions with same values)
    common_cuts = []
    
    # Find ABCD grid variables by looking at what differs between regions
    # Check for categorical variables (like nLeptonic vs nHadronic)
    x_categories = set()
    y_ranges = set()
    
    # Look for patterns in each region
    for region in regions:
        cuts = region_cuts.get(region, [])
        
        # Check for leptonic vs hadronic
        has_leptonic = any('nLeptonic == 1' in cut for cut in cuts)
        has_hadronic = any('nHadronic == 1' in cut for cut in cuts)
        
        if has_leptonic:
            x_categories.add('lep')
        elif has_hadronic:
            x_categories.add('had')
        
        # Extract Rs ranges for this region
        rs_values = []
        for cut in cuts:
            rs_match = re.search(r'rjr_Rs\[1\]\s*([><])\s*(\d+\.?\d*)', cut)
            if rs_match:
                rs_values.append(float(rs_match.group(2)))
        
        if len(rs_values) >= 2:
            rs_range = f"[{min(rs_values)},{max(rs_values)}]"
            y_ranges.add(rs_range)
    
    # Extract truly common cuts (cuts that appear identically in ALL regions)
    common_parts = []
    
    # Get all cuts from all regions to find truly common ones
    all_region_cuts = list(region_cuts.values())
    if len(all_region_cuts) >= 4:  # Should have A, B, C, D
        # Normalize cuts to treat LeptonicSV_dxySig and HadronicSV_dxySig as equivalent
        def normalize_cut(cut):
            # Replace both Leptonic and Hadronic SV cuts with generic SV_dxySig
            cut_normalized = re.sub(r'(Leptonic|Hadronic)SV_dxySig', 'SV_dxySig', cut)
            return cut_normalized
        
        # Create normalized sets for each region
        normalized_region_cuts = []
        for cuts_list in all_region_cuts:
            normalized_set = {normalize_cut(cut) for cut in cuts_list}
            normalized_region_cuts.append(normalized_set)
        
        # Find cuts that appear in ALL regions after normalization
        common_cuts_normalized = normalized_region_cuts[0]  # Start with region A cuts
        for normalized_set in normalized_region_cuts[1:]:
            common_cuts_normalized = common_cuts_normalized.intersection(normalized_set)
        
        # Convert back to actual cuts for processing
        common_cuts_set = set()
        for cut in all_region_cuts[0]:  # Use original cuts from first region
            if normalize_cut(cut) in common_cuts_normalized:
                common_cuts_set.add(normalize_cut(cut))  # Use normalized version
        
        # Group cuts by variable to combine upper/lower bounds into ranges
        variable_bounds = {}
        equality_cuts = []
        
        for cut in sorted(common_cuts_set):
            # Handle equality cuts (these are common across all regions)
            if '==' in cut:
                if 'nHadronic' in cut:
                    equality_cuts.append("$N_{\\mathrm{had}}^{\\mathrm{SV}} = 1$")
                elif 'nLeptonic' in cut:
                    equality_cuts.append("$N_{\\mathrm{lep}}^{\\mathrm{SV}} = 1$")
                continue
                
            # Extract variable bounds for range cuts
            if 'Ms' in cut and ('>' in cut or '<' in cut):
                ms_match = re.search(r'rjr_Ms\[1\]\s*([><])\s*(\d+\.?\d*)', cut)
                if ms_match:
                    op = ms_match.group(1)
                    val = float(ms_match.group(2))
                    if 'Ms' not in variable_bounds:
                        variable_bounds['Ms'] = {'min': None, 'max': None}
                    if op == '>':
                        variable_bounds['Ms']['min'] = val
                    elif op == '<':
                        variable_bounds['Ms']['max'] = val
                        
            elif ('SV_dxySig' in cut) and ('>' in cut or '<' in cut):
                sxy_match = re.search(r'SV_dxySig\[0\]\s*([><])\s*(\d+\.?\d*)', cut)
                if sxy_match:
                    op = sxy_match.group(1)
                    val = float(sxy_match.group(2))
                    if 'Sxy' not in variable_bounds:
                        variable_bounds['Sxy'] = {'min': None, 'max': None}
                    if op == '>':
                        variable_bounds['Sxy']['min'] = val
                    elif op == '<':
                        variable_bounds['Sxy']['max'] = val
                        
            elif 'Rs' in cut and ('>' in cut or '<' in cut):
                rs_match = re.search(r'rjr_Rs\[1\]\s*([><])\s*(\d+\.?\d*)', cut)
                if rs_match:
                    op = rs_match.group(1)
                    val = float(rs_match.group(2))
                    if 'Rs' not in variable_bounds:
                        variable_bounds['Rs'] = {'min': None, 'max': None}
                    if op == '>':
                        variable_bounds['Rs']['min'] = val
                    elif op == '<':
                        variable_bounds['Rs']['max'] = val
        
        # Add equality cuts first
        common_parts.extend(equality_cuts)
        
        # Format the grouped bounds
        for var, bounds in variable_bounds.items():
            if bounds['min'] is not None and bounds['max'] is not None:
                # Both upper and lower bounds - create range
                if var == 'Ms':
                    common_parts.append(f"$M_S \\in [{bounds['min']:.0f},{bounds['max']:.0f}]$")
                elif var == 'Sxy':
                    common_parts.append(f"$S_{{xy}} \\in [{bounds['min']:.0f},{bounds['max']:.0f}]$")
                elif var == 'Rs':
                    common_parts.append(f"$R_S \\in [{bounds['min']:.1f},{bounds['max']:.1f}]$")
            elif bounds['min'] is not None:
                # Only lower bound
                if var == 'Ms':
                    common_parts.append(f"$M_S > {bounds['min']:.0f}$")
                elif var == 'Sxy':
                    common_parts.append(f"$S_{{xy}} > {bounds['min']:.0f}$")
                elif var == 'Rs':
                    common_parts.append(f"$R_S > {bounds['min']:.1f}$")
            elif bounds['max'] is not None:
                # Only upper bound
                if var == 'Ms':
                    common_parts.append(f"$M_S < {bounds['max']:.0f}$")
                elif var == 'Sxy':
                    common_parts.append(f"$S_{{xy}} < {bounds['max']:.0f}$")
                elif var == 'Rs':
                    common_parts.append(f"$R_S < {bounds['max']:.1f}$")
    
    # Use \parbox to create multi-line content within the cell
    if len(common_parts) > 1:
        common_cuts_str = "\\parbox{4cm}{\\centering " + " \\\\ ".join(common_parts) + "}"
    else:
        common_cuts_str = common_parts[0] if common_parts else ""
    
    # Determine x and y variable descriptions
    if len(x_categories) > 1:
        x_var = "$N_{type}^{SV}$"
        x_vals = ["$N_{lep}^{SV}=1$", "$N_{had}^{SV}=1$"]
    else:
        # Check if it's a Sxy vs Rs grid (configs 2-4)
        # Extract Sxy ranges from regions
        sxy_ranges = set()
        for region in regions:
            cuts = region_cuts.get(region, [])
            sxy_values = []
            for cut in cuts:
                sxy_match = re.search(r'(Leptonic|Hadronic)SV_dxySig\[0\]\s*([><])\s*(\d+\.?\d*)', cut)
                if sxy_match:
                    sxy_values.append(float(sxy_match.group(3)))
            
            if len(sxy_values) >= 2:
                sxy_range = f"[{min(sxy_values):.0f},{max(sxy_values):.0f}]"
                sxy_ranges.add(sxy_range)
        
        if len(sxy_ranges) >= 2:
            x_var = "$S_{xy}$"
            x_vals = sorted(list(sxy_ranges))
        else:
            # Check if it's a Ms vs Rs grid (config 4)
            # Extract Ms ranges from regions
            ms_ranges = set()
            for region in regions:
                cuts = region_cuts.get(region, [])
                ms_values = []
                for cut in cuts:
                    ms_match = re.search(r'rjr_Ms\[1\]\s*([><])\s*(\d+\.?\d*)', cut)
                    if ms_match:
                        ms_values.append(float(ms_match.group(2)))
                
                if len(ms_values) >= 2:
                    ms_range = f"[{min(ms_values):.0f},{max(ms_values):.0f}]"
                    ms_ranges.add(ms_range)
            
            if len(ms_ranges) >= 2:
                x_var = "$M_S$"
                x_vals = sorted(list(ms_ranges))
            else:
                x_var = "Unknown"
                x_vals = []
    
    y_var = "$R_S$" 
    y_vals = sorted(list(y_ranges))
    
    return common_cuts_str, f"{y_var} vs {x_var}", x_vals, y_vals, common_parts

def generate_cuts_table(grouped_results, output_file):
    """Generate clean ABCD cuts table showing the grid structure"""
    
    latex_content = r"""
\documentclass[10pt]{article}
\usepackage[margin=0.5in, landscape]{geometry}
\usepackage{booktabs}
\usepackage{array}
\usepackage{longtable}
\usepackage{amsmath}

\begin{document}
\begin{center}
{\Large \bf ABCD Region Cut Definitions}
\end{center}

\begin{longtable}{c>{\centering\arraybackslash}p{4cm}c>{\centering\arraybackslash}p{2.2cm}>{\centering\arraybackslash}p{2.2cm}>{\centering\arraybackslash}p{2.2cm}>{\centering\arraybackslash}p{2.2cm}}
\toprule
Config & Common Cuts & Variables & $x_{\mathrm{low}}$ & $x_{\mathrm{high}}$ & $y_{\mathrm{low}}$ & $y_{\mathrm{high}}$ \\
\midrule
\endfirsthead

\toprule
Config & Common Cuts & Variables & $x_{\mathrm{low}}$ & $x_{\mathrm{high}}$ & $y_{\mathrm{low}}$ & $y_{\mathrm{high}}$ \\
\midrule
\endhead

\bottomrule
\endfoot

"""
    
    # Process each numbered group
    sorted_groups = sorted(grouped_results.keys())
    for i, group_num in enumerate(sorted_groups):
        group_data = grouped_results[group_num]
        
        # Get the first result to extract cuts
        first_result = group_data['results'][0]['result']
        region_cuts = first_result.get('region_cuts', {})
        
        if region_cuts:
            common_cuts, variables, x_vals, y_vals, common_parts = extract_variable_ranges_from_cuts(region_cuts)
            
            # Format x values (categorical or numeric)
            x_low = x_vals[0] if len(x_vals) >= 1 else "?"
            x_high = x_vals[1] if len(x_vals) >= 2 else "?"
            
            # Format y values (ranges)
            y_low = y_vals[0] if len(y_vals) >= 1 else "?"
            y_high = y_vals[1] if len(y_vals) >= 2 else "?"
            
            # Add extra spacing after rows with multi-line common cuts
            if i < len(sorted_groups) - 1:
                # More spacing after multi-line rows (parbox), normal spacing after single-line rows
                if len(common_parts) > 1:
                    latex_content += f"{group_num} & {common_cuts} & {variables} & {x_low} & {x_high} & {y_low} & {y_high} \\\\[1.5em]\n"
                else:
                    latex_content += f"{group_num} & {common_cuts} & {variables} & {x_low} & {x_high} & {y_low} & {y_high} \\\\[0.8em]\n"
            else:
                latex_content += f"{group_num} & {common_cuts} & {variables} & {x_low} & {x_high} & {y_low} & {y_high} \\\\\n"
    
    latex_content += r"""

\end{longtable}

\vspace{1cm}

\begin{center}
{\bf ABCD Grid Structure:} \\
\begin{tabular}{c|cc}
& $x_{\mathrm{low}}$ & $x_{\mathrm{high}}$ \\
\hline
$y_{\mathrm{high}}$ & A & B \\
$y_{\mathrm{low}}$ & C & D \\
\end{tabular}
\end{center}

\vspace{0.5cm}

\begin{center}
{\bf Notes:} \\
Each configuration defines a $2 \times 2$ grid using two variables. \\
The ABCD method predicts one region using the other three. \\
Common selection cuts ($N_{\mathrm{had}} = 1$, etc.) apply to all regions.
\end{center}

\end{document}
"""
    
    with open(output_file, 'w') as f:
        f.write(latex_content)

def compile_latex(tex_file, output_dir):
    """Compile LaTeX file to PDF"""
    try:
        # Run pdflatex twice for references
        for _ in range(2):
            result = subprocess.run([
                'pdflatex', 
                '-output-directory', output_dir,
                '-interaction=nonstopmode',
                tex_file
            ], capture_output=True, text=True)
            
            if result.returncode != 0:
                print(f"LaTeX compilation failed for {tex_file}")
                print(f"Error: {result.stderr}")
                return False
        
        # Clean up auxiliary files
        base_name = os.path.splitext(os.path.basename(tex_file))[0]
        for ext in ['.aux', '.log', '.out']:
            aux_file = os.path.join(output_dir, base_name + ext)
            if os.path.exists(aux_file):
                os.remove(aux_file)
        
        pdf_file = os.path.join(output_dir, base_name + '.pdf')
        if os.path.exists(pdf_file):
            print(f"✓ Generated: {pdf_file}")
            return True
        else:
            return False
            
    except FileNotFoundError:
        print("Error: pdflatex not found. Please install a LaTeX distribution (e.g., TeX Live)")
        return False
    except Exception as e:
        print(f"Error compiling {tex_file}: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description="Generate LaTeX tables from ABCD closure results")
    parser.add_argument("results_dir", help="Directory containing JSON closure results")
    parser.add_argument("--output-dir", help="Output directory for tables (default: <results_dir>/tables)")
    parser.add_argument("--no-compile", action="store_true", help="Generate .tex files only, don't compile to PDF")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.results_dir):
        print(f"Error: Results directory not found: {args.results_dir}")
        return 1
    
    # Set default output directory inside results directory
    if not args.output_dir:
        args.output_dir = os.path.join(args.results_dir, "tables")
    
    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Load results
    print(f"Loading closure results from: {args.results_dir}")
    results = load_closure_results(args.results_dir)
    
    if not results:
        print("Error: No closure result JSON files found")
        return 1
    
    print(f"Found {len(results)} closure results")
    
    # Organize results into numbered groups
    print("Organizing results into ABCD groups...")
    grouped_results = organize_results_by_groups(results)
    print(f"Created {len(grouped_results)} configuration groups")
    
    # Generate tables
    print("Generating comprehensive table...")
    comprehensive_tex = os.path.join(args.output_dir, "abcd_closure_comprehensive.tex")
    generate_comprehensive_table(grouped_results, comprehensive_tex)
    
    print("Generating group-specific tables...")
    generate_group_tables(grouped_results, args.output_dir)
    
    print("Generating configuration definitions table...")
    config_def_tex = os.path.join(args.output_dir, "abcd_config_definitions.tex")
    generate_config_definitions_table(grouped_results, config_def_tex)
    
    print("Generating cuts definition table...")
    cuts_tex = os.path.join(args.output_dir, "abcd_region_cuts.tex")
    generate_cuts_table(grouped_results, cuts_tex)
    
    # Compile to PDF if requested
    if not args.no_compile:
        print("\nCompiling LaTeX files to PDF...")
        tex_files = glob.glob(os.path.join(args.output_dir, "*.tex"))
        
        # Use parallel compilation for speed
        def compile_single_file(tex_file):
            return compile_latex(tex_file, args.output_dir), tex_file
        
        max_workers = min(len(tex_files), multiprocessing.cpu_count())
        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            results = list(executor.map(compile_single_file, tex_files))
        
        success_count = 0
        for success, tex_file in results:
            if success:
                success_count += 1
        
        print(f"\nSuccessfully compiled {success_count}/{len(tex_files)} tables")
    
    print(f"\nOutput files saved to: {args.output_dir}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
