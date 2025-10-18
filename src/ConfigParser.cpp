#include "ConfigParser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <set>

// Simple YAML parser implementation
// For production use, consider yaml-cpp library
class SimpleYAMLParser {
public:
    std::map<std::string, std::string> values;
    std::map<std::string, std::vector<std::string>> lists;
    std::map<std::string, std::map<std::string, std::string>> sections;
    std::map<std::string, std::vector<std::string>> anchors;
    
    bool parse(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open config file: " << filename << std::endl;
            return false;
        }
        
        // First pass: collect all anchors
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        file.close();
        
        // Parse anchors first
        parseAnchors(lines);
        
        // Second pass: normal parsing with anchor expansion
        std::string current_section = "";
        std::string current_subsection = "";
        std::string current_anchor_key = "";  // Track current anchor being defined
        
        for (const std::string& line_raw : lines) {
            line = line_raw;
            // Check indentation level BEFORE trimming
            size_t indent = 0;
            for (char c : line) {
                if (c == ' ') indent++;
                else break;
            }
            
            // Remove comments and trim whitespace
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }
            
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            
            if (line.empty()) continue;
            
            if (indent == 0) {
                // Top level section
                if (line.back() == ':') {
                    std::string section_key = line.substr(0, line.length() - 1);
                    
                    // Check if this is an anchor definition at top level
                    size_t anchor_pos = section_key.find('&');
                    if (anchor_pos != std::string::npos) {
                        current_anchor_key = section_key.substr(anchor_pos + 1);
                        current_anchor_key.erase(0, current_anchor_key.find_first_not_of(" \t"));
                        current_section = section_key.substr(0, anchor_pos);
                        current_section.erase(current_section.find_last_not_of(" \t") + 1);
                    } else {
                        current_section = section_key;
                        current_anchor_key = "";
                    }
                    current_subsection = "";
                }
            } else if (indent <= 2) {
                // Second level
                if (line.back() == ':') {
                    current_subsection = line.substr(0, line.length() - 1);
                    current_subsection.erase(0, current_subsection.find_first_not_of(" \t"));
                    
                    // Check if this is an anchor definition
                    size_t anchor_pos = current_subsection.find('&');
                    if (anchor_pos != std::string::npos) {
                        current_anchor_key = current_subsection.substr(anchor_pos + 1);
                        current_anchor_key.erase(0, current_anchor_key.find_first_not_of(" \t"));
                        current_subsection = current_subsection.substr(0, anchor_pos);
                        current_subsection.erase(current_subsection.find_last_not_of(" \t") + 1);
                    } else {
                        current_anchor_key = "";
                    }
                } else {
                    // This is a key-value pair at subsection level, so clear subsection
                    parseKeyValue(line, current_section, "", current_anchor_key);
                }
            } else {
	      // Third level or list items (like cuts: - "..." or systematics list items)
	      static std::string current_key = "";
	      static std::string last_section = "";
	      static std::string last_subsection = "";
	      
	      // Reset current_key when we change sections or subsections
	      if (current_section != last_section || current_subsection != last_subsection) {
	          current_key = "";
	          last_section = current_section;
	          last_subsection = current_subsection;
	      }
	      
	      if (line.back() == ':') {
		// We're entering a subkey, extract the actual key name
		std::string extracted_key = line.substr(0, line.length() - 1);
		extracted_key.erase(0, extracted_key.find_first_not_of(" \t"));
		extracted_key.erase(extracted_key.find_last_not_of(" \t") + 1);
		
		current_key = extracted_key;
	      } else {
		// If we're in a nested list context, append current_key only if it's meaningful
		std::string subsection_full = current_subsection;
		if (!current_key.empty()) {
		  subsection_full += "." + current_key;
		}
		
		parseKeyValue(line, current_section, subsection_full, current_anchor_key);
	      }
	    }
        }
        
        return true;
    }
    
private:
    void parseAnchors(const std::vector<std::string>& lines) {
        std::string current_anchor = "";
        
        for (const std::string& line_raw : lines) {
            std::string line = line_raw;
            
            // Remove comments and trim whitespace
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }
            
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            
            if (line.empty()) continue;
            
            
            // Look for anchor definitions
            size_t anchor_pos = line.find('&');
            if (anchor_pos != std::string::npos && line.find(':') != std::string::npos) {
                // This line contains both : and &, it's an anchor definition
                current_anchor = line.substr(anchor_pos + 1);
                current_anchor.erase(0, current_anchor.find_first_not_of(" \t"));
                current_anchor.erase(current_anchor.find_last_not_of(" \t") + 1);
            } else if (line.back() == ':') {
                current_anchor = "";
            }
            
            if (!current_anchor.empty() && line.front() == '-') {
                // This is a list item for the current anchor
                std::string item = line.substr(1);
                item.erase(0, item.find_first_not_of(" \t"));
                // Remove quotes if present
                if (!item.empty() && ((item.front() == '"' && item.back() == '"') || 
                    (item.front() == '\'' && item.back() == '\''))) {
                    item = item.substr(1, item.length() - 2);
                }
                anchors[current_anchor].push_back(item);
            }
        }
    }
    void parseKeyValue(const std::string& line, const std::string& section, const std::string& subsection, const std::string& current_anchor = "") {
        if (line.front() == '-') {
            // List item
            std::string item = line.substr(1);
            item.erase(0, item.find_first_not_of(" \t"));
            
            // Handle references (*anchor_name)
            if (!item.empty() && item.front() == '*') {
                std::string anchor_name = item.substr(1);
                if (anchors.count(anchor_name)) {
                    // Expand the anchor
                    std::string key = section + (subsection.empty() ? "" : "." + subsection);
                    for (const auto& anchor_item : anchors[anchor_name]) {
                        lists[key].push_back(anchor_item);
                    }
                    return;
                }
            }
            
            // Remove quotes if present
            if (!item.empty() && ((item.front() == '"' && item.back() == '"') || 
                (item.front() == '\'' && item.back() == '\''))) {
                item = item.substr(1, item.length() - 2);
            }
            
            std::string key = section + (subsection.empty() ? "" : "." + subsection);
            lists[key].push_back(item);
            
            // If we're in an anchor definition, also store in anchors
            if (!current_anchor.empty()) {
                anchors[current_anchor].push_back(item);
            }
        } else {
            // Key-value pair
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                // Check for anchor definition (key: &anchor_name)
                size_t anchor_pos = key.find('&');
                std::string anchor_name = "";
                if (anchor_pos != std::string::npos) {
                    // Extract anchor name
                    anchor_name = key.substr(anchor_pos + 1);
                    anchor_name.erase(0, anchor_name.find_first_not_of(" \t"));
                    // Remove the anchor part from the key
                    key = key.substr(0, anchor_pos);
                    key.erase(key.find_last_not_of(" \t") + 1);
                }
                
                // Handle array notation [item1, item2, ...]
                if (!value.empty() && value.front() == '[' && value.back() == ']') {
                    std::string array_content = value.substr(1, value.length() - 2);
                    std::string full_key = section + (subsection.empty() ? "" : "." + subsection) + "." + key;
                    
                    std::vector<std::string> items;
                    
                    // Split by comma
                    std::stringstream ss(array_content);
                    std::string item;
                    while (std::getline(ss, item, ',')) {
                        // Trim whitespace
                        item.erase(0, item.find_first_not_of(" \t"));
                        item.erase(item.find_last_not_of(" \t") + 1);
                        // Remove quotes
                        if (!item.empty() && ((item.front() == '"' && item.back() == '"') || 
                            (item.front() == '\'' && item.back() == '\''))) {
                            item = item.substr(1, item.length() - 2);
                        }
                        if (!item.empty()) {
                            items.push_back(item);
                            lists[full_key].push_back(item);
                        }
                    }
                    
                    // Store anchor if defined
                    if (!anchor_name.empty()) {
                        anchors[anchor_name] = items;
                    }
                } else {
                    // Regular key-value pair
                    // Remove quotes if present
                    if (!value.empty() && ((value.front() == '"' && value.back() == '"') || 
                        (value.front() == '\'' && value.back() == '\''))) {
                        value = value.substr(1, value.length() - 2);
                    }
                    
                    std::string full_key = section + (subsection.empty() ? "" : "." + subsection) + "." + key;
                    values[full_key] = value;
                }
            }
        }
    }
};

ConfigParser::ConfigParser() {
    SetDefaults();
}

ConfigParser::~ConfigParser() {}

void ConfigParser::SetDefaults() {
    config_.name = "default_analysis";
    config_.luminosity = 400.0;
    config_.output_json = "output.json";
    config_.output_dir = "./json/";
    config_.verbosity = 1;
    config_.parallel = false;
    config_.dry_run = false;
}

bool ConfigParser::LoadConfig(const std::string& config_file) {
    return LoadYAML(config_file);
}

bool ConfigParser::LoadYAML(const std::string& config_file) {
    std::cout << "DEBUG: Starting LoadYAML for " << config_file << std::endl;
    SimpleYAMLParser parser;
    
    if (!parser.parse(config_file)) {
        return false;
    }
    std::cout << "DEBUG: YAML parsing completed successfully" << std::endl;
    
    // Debug: Print all available keys in parser.lists
    std::cout << "DEBUG: Available parser.lists keys:" << std::endl;
    for (const auto& pair : parser.lists) {
        std::cout << "  Key: '" << pair.first << "', Size: " << pair.second.size() << std::endl;
    }
    
    // Parse analysis section
    if (parser.values.count("analysis.name")) {
        config_.name = parser.values["analysis.name"];
    }
    if (parser.values.count("analysis.luminosity")) {
        config_.luminosity = std::stod(parser.values["analysis.luminosity"]);
    }
    if (parser.values.count("analysis.output_json")) {
        config_.output_json = parser.values["analysis.output_json"];
    }
    if (parser.values.count("analysis.output_dir")) {
        config_.output_dir = parser.values["analysis.output_dir"];
    }
    if (parser.values.count("analysis.method")) {
        config_.method = parser.values["analysis.method"];
    }
    
    // Parse ABCD configuration if method is ABCD
    if (config_.method == "ABCD") {
        // Check if using explicit x_axis/y_axis format
        bool has_explicit_format = parser.values.count("x_axis.name") > 0;
        
        
        if (has_explicit_format) {
            // Parse explicit format
            config_.abcd.use_explicit_format = true;
            
            // Parse x_axis
            std::cout << "DEBUG: About to parse x_axis" << std::endl;
            config_.abcd.x_axis.name = parser.values["x_axis.name"];
            std::cout << "DEBUG: Parsed x_axis.name: " << config_.abcd.x_axis.name << std::endl;
            
            config_.abcd.x_axis.low_desc = parser.values["x_axis.x_low.description"];  
            std::cout << "DEBUG: Parsed x_axis.x_low.description" << std::endl;
            
            config_.abcd.x_axis.high_desc = parser.values["x_axis.x_high.description"];
            std::cout << "DEBUG: Parsed x_axis.x_high.description" << std::endl;
            
            std::cout << "DEBUG: About to assign x_axis.x_low.cuts" << std::endl;
            config_.abcd.x_axis.low_cuts = parser.lists["x_axis.x_low.cuts"];
            std::cout << "DEBUG: Successfully assigned x_axis.x_low.cuts" << std::endl;
            
            config_.abcd.x_axis.high_cuts = parser.lists.at("x_axis.x_high.cuts");
            
            // Parse y_axis  
            config_.abcd.y_axis.name = parser.values["y_axis.name"];
            config_.abcd.y_axis.low_desc = parser.values["y_axis.y_low.description"];
            config_.abcd.y_axis.high_desc = parser.values["y_axis.y_high.description"]; 
            config_.abcd.y_axis.low_cuts = parser.lists["y_axis.y_low.cuts"];
            config_.abcd.y_axis.high_cuts = parser.lists["y_axis.y_high.cuts"];
            
            // Parse common cuts
            if (parser.lists.count("abcd_common_cuts")) {
                config_.abcd.common_cuts = parser.lists["abcd_common_cuts"];
                std::cout << "DEBUG: Found " << config_.abcd.common_cuts.size() << " common cuts:" << std::endl;
                for (const auto& cut : config_.abcd.common_cuts) {
                    std::cout << "  - " << cut << std::endl;
                }
            } else {
                std::cout << "DEBUG: No abcd_common_cuts found in config" << std::endl;
                std::cout << "DEBUG: Available list keys:" << std::endl;
                for (const auto& pair : parser.lists) {
                    std::cout << "  - " << pair.first << " (size: " << pair.second.size() << ")" << std::endl;
                }
                std::cout << "DEBUG: Available value keys containing 'common':" << std::endl;
                for (const auto& pair : parser.values) {
                    if (pair.first.find("common") != std::string::npos) {
                        std::cout << "  - " << pair.first << " = " << pair.second << std::endl;
                    }
                }
            }
            
            // Generate ABCD bins dynamically from axis definitions
            GenerateABCDBinsFromAxes();
            
        } else {
            // Parse old format (regions)
            config_.abcd.use_explicit_format = false;
            for (const auto& pair : parser.values) {
                if (pair.first.find("abcd.regions.") == 0) {
                    std::string region_name = pair.first.substr(13); // Remove "abcd.regions."
                    config_.abcd.regions[region_name] = pair.second;
                }
            }
        }
        
        // Skip predicted_region parsing - will be handled at BF step
        // if (parser.values.count("abcd.predicted_region")) {
        //     config_.abcd.predicted_region = parser.values["abcd.predicted_region"];
        // }
        
        if (parser.values.count("abcd.formula")) {
            config_.abcd.formula = parser.values["abcd.formula"];
        } else {
            config_.abcd.formula = "(@0*@1/@2)";  // Default ABCD formula
        }
        
        if (parser.values.count("abcd.generate_datacards")) {
            config_.abcd.generate_datacards = (parser.values["abcd.generate_datacards"] == "true");
        } else {
            config_.abcd.generate_datacards = true;  // Default: generate datacards for ABCD
        }
    }
    
    // Parse samples
    if (parser.lists.count("samples.backgrounds")) {
        config_.backgrounds = parser.lists["samples.backgrounds"];
    }
    if (parser.lists.count("samples.signals")) {
        config_.signals = parser.lists["samples.signals"];
    }
    if (parser.lists.count("samples.data")) {
        config_.data = parser.lists.at("samples.data");
    }
    
    if (parser.lists.count("samples.signal_points")) {
        config_.signal_points = parser.lists["samples.signal_points"];
    }
    
    // Parse systematics
    ParseSystematics(parser);
    
    // Parse bins
    for (const auto& pair : parser.lists) {
        if (pair.first.find("bins.") == 0) {
            std::string full_key = pair.first;
            std::string bin_name;
            
            // Extract bin name from the key
            size_t bins_pos = full_key.find("bins.");
            if (bins_pos != std::string::npos) {
                std::string remainder = full_key.substr(bins_pos + 5); // Remove "bins."
                size_t dot_pos = remainder.find('.');
                
                if (dot_pos != std::string::npos) {
                    // Key like "bins.nLeptonic_Rs_A.cuts" - this IS our format!
                    if (full_key.substr(full_key.length() - 5) == ".cuts") {
                        bin_name = remainder.substr(0, dot_pos);  // Extract bin name before .cuts
                    } else {
                        continue;
                    }
                } else {
                    // Key like "bins.single_bin" - this would be direct cuts list (old format)
                    bin_name = remainder;
                }
            }
            
            if (!bin_name.empty()) {
                BinConfig bin_config;
                bin_config.name = bin_name;
                bin_config.cuts = pair.second;
                
                // Look for description
                std::string desc_key = "bins." + bin_name + ".description";
                if (parser.values.count(desc_key)) {
                    bin_config.description = parser.values[desc_key];
                }
                
                config_.bins.push_back(bin_config);
            }
        }
    }
    
    // Parse options
    if (parser.values.count("options.verbosity")) {
        config_.verbosity = std::stoi(parser.values["options.verbosity"]);
    }
    if (parser.values.count("options.parallel")) {
        config_.parallel = (parser.values["options.parallel"] == "true");
    }
    if (parser.values.count("options.dry_run")) {
        config_.dry_run = (parser.values["options.dry_run"] == "true");
    }
    
    return ValidateConfig();
}

std::string ConfigParser::GetCombinedCuts(const std::string& bin_name) const {
    for (const auto& bin : config_.bins) {
        if (bin.name == bin_name) {
            if (bin.cuts.empty()) return "";
            
            std::string combined = "(" + bin.cuts[0] + ")";
            for (size_t i = 1; i < bin.cuts.size(); ++i) {
                combined += " && (" + bin.cuts[i] + ")";
            }
            return combined;
        }
    }
    return "";
}

void ConfigParser::PrintConfig() const {
    std::cout << "=== Analysis Configuration ===" << std::endl;
    std::cout << "Name: " << config_.name << std::endl;
    std::cout << "Luminosity: " << config_.luminosity << " fb^-1" << std::endl;
    std::cout << "Output JSON: " << config_.output_json << std::endl;
    std::cout << "Output Directory: " << config_.output_dir << std::endl;
    
    std::cout << "\nBackgrounds: ";
    for (const auto& bg : config_.backgrounds) {
        std::cout << bg << " ";
    }
    std::cout << std::endl;
    
    std::cout << "Signals: ";
    for (const auto& sig : config_.signals) {
        std::cout << sig << " ";
    }
    std::cout << std::endl;
    
    if (!config_.signal_points.empty()) {
        std::cout << "Signal Points: ";
        for (const auto& point : config_.signal_points) {
            std::cout << point << " ";
        }
        std::cout << std::endl;
    }
    
    // Check if there are any non-ABCD bins to display
    bool has_non_abcd_bins = false;
    for (const auto& bin : config_.bins) {
        if (!(config_.method == "ABCD" && config_.abcd.use_explicit_format && 
            (bin.name.find("_A") != std::string::npos || bin.name.find("_B") != std::string::npos || 
             bin.name.find("_C") != std::string::npos || bin.name.find("_D") != std::string::npos))) {
            has_non_abcd_bins = true;
            break;
        }
    }
    
    if (has_non_abcd_bins) {
        std::cout << "\nAnalysis Bins:" << std::endl;
        for (const auto& bin : config_.bins) {
            // Skip ABCD bins in explicit format since they're already shown in "Generated ABCD Regions"
            if (config_.method == "ABCD" && config_.abcd.use_explicit_format && 
                (bin.name.find("_A") != std::string::npos || bin.name.find("_B") != std::string::npos || 
                 bin.name.find("_C") != std::string::npos || bin.name.find("_D") != std::string::npos)) {
                continue;
            }
            std::cout << "  " << bin.name << ": " << bin.description << std::endl;
            std::cout << "    Cuts: " << GetCombinedCuts(bin.name) << std::endl;
        }
    }
    
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  Verbosity: " << config_.verbosity << std::endl;
    std::cout << "  Parallel: " << (config_.parallel ? "true" : "false") << std::endl;
    std::cout << "  Dry run: " << (config_.dry_run ? "true" : "false") << std::endl;
}

bool ConfigParser::ValidateConfig() const {
    if (config_.luminosity <= 0) {
        std::cerr << "Error: Luminosity must be positive" << std::endl;
        return false;
    }
    
    if (config_.backgrounds.empty()) {
        std::cerr << "Warning: No background samples specified" << std::endl;
    }
    
    if (config_.bins.empty()) {
        std::cerr << "Error: No analysis bins defined" << std::endl;
        return false;
    }
    
    // ABCD-specific validation
    if (config_.method == "ABCD") {
        return ValidateABCDConfig();
    }
    
    return true;
}

bool ConfigParser::ValidateABCDConfig() const {
    if (!config_.abcd.IsValid()) {
        std::cerr << "Error: Invalid ABCD configuration" << std::endl;
        std::cerr << "  Regions count: " << config_.abcd.regions.size() << " (expected 4)" << std::endl;
        std::cerr << "  Predicted region: '" << config_.abcd.predicted_region << "'" << std::endl;
        return false;
    }
    
    // Check that all ABCD regions are defined as bins
    for (const auto& region_pair : config_.abcd.regions) {
        const std::string& region_name = region_pair.first;
        const std::string& bin_name = region_pair.second;
        bool found = false;
        for (const auto& bin : config_.bins) {
            if (bin.name == bin_name) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "Error: ABCD region '" << region_name << "' references undefined bin '" << bin_name << "'" << std::endl;
            return false;
        }
    }
    
    return true;
}

// Implementation of SystematicConfig::ResolveBins
std::vector<std::string> SystematicConfig::ResolveBins(const ABCDConfig& abcd) const {
    std::vector<std::string> resolved_bins;
    
    for (const std::string& bin : bins) {
        if (bin == "auto_predicted") {
            resolved_bins.push_back(abcd.GetPredictedBin());
        } else if (bin == "auto_control_1") {
            auto controls = abcd.GetControlBins();
            if (controls.size() >= 1) resolved_bins.push_back(controls[0]);
        } else if (bin == "auto_control_2") {
            auto controls = abcd.GetControlBins();
            if (controls.size() >= 2) resolved_bins.push_back(controls[1]);
        } else if (bin == "auto_control_3") {
            auto controls = abcd.GetControlBins();
            if (controls.size() >= 3) resolved_bins.push_back(controls[2]);
        } else if (bin == "all") {
            // Add all ABCD bins
            for (const auto& region_pair : abcd.regions) {
                resolved_bins.push_back(region_pair.second);
            }
        } else {
            resolved_bins.push_back(bin);  // Literal bin name
        }
    }
    
    return resolved_bins;
}

void ConfigParser::ParseSystematics(const SimpleYAMLParser& parser) {
    // Parse ABCD systematics
    ParseSystematicCategory(parser, "systematics.abcd_systematics", config_.abcd_systematics);
    
    // Parse precision systematics  
    ParseSystematicCategory(parser, "systematics.precision_systematics", config_.precision_systematics);
    
    // Parse experimental systematics
    ParseSystematicCategory(parser, "systematics.experimental_systematics", config_.experimental_systematics);
}

void ConfigParser::ParseSystematicCategory(const SimpleYAMLParser& parser, const std::string& category_prefix, std::vector<SystematicConfig>& systematics) {
    // Get the list of systematic names
    if (parser.lists.count(category_prefix) == 0) {
        return; // No systematics for this category
    }
    
    const auto& systematic_names = parser.lists.at(category_prefix);
    
    // Get common properties for this category
    std::string common_type;
    double common_value = 1.0;
    std::string common_formula;
    
    if (parser.values.count(category_prefix + ".type")) {
        common_type = parser.values.at(category_prefix + ".type");
    }
    if (parser.values.count(category_prefix + ".value")) {
        common_value = std::stod(parser.values.at(category_prefix + ".value"));
    }
    if (parser.values.count(category_prefix + ".formula")) {
        common_formula = parser.values.at(category_prefix + ".formula");
    }
    
    // Get arrays for bins and processes
    std::vector<std::string> bins_list;
    std::vector<std::string> processes_list;
    
    if (parser.lists.count(category_prefix + ".bins")) {
        bins_list = parser.lists.at(category_prefix + ".bins");
    }
    if (parser.lists.count(category_prefix + ".processes")) {
        processes_list = parser.lists.at(category_prefix + ".processes");
    }
    
    // Create systematics - one for each name
    for (size_t i = 0; i < systematic_names.size(); i++) {
        SystematicConfig syst;
        syst.name = systematic_names[i];
        syst.type = common_type;
        syst.value = common_value;
        syst.formula = common_formula;
        
        // Assign bins and processes (use index if arrays are long enough)
        if (i < bins_list.size()) {
            syst.bins.push_back(bins_list[i]);
        }
        if (i < processes_list.size()) {
            syst.processes.push_back(processes_list[i]);
        }
        
        systematics.push_back(syst);
    }
}

void ConfigParser::ParseSystematicCategoryNested(const SimpleYAMLParser& parser, const std::string& category_prefix, std::vector<SystematicConfig>& systematics) {
    
    // The issue is that the SimpleYAMLParser is parsing all systematics into "systematics.- bins"
    // instead of separating abcd_systematics vs precision_systematics
    // Let's look for the general "systematics.- bins" pattern and parse all systematics from there
    
    std::string general_prefix = "systematics.- bins";
    
    // Check if this category has any systematics
    if (parser.lists.count(general_prefix)) {
        const auto& list_items = parser.lists.at(general_prefix);
        
        // Print all list items to understand the structure
        for (size_t i = 0; i < list_items.size(); ++i) {
        }
        
        // The problem is the YAML parser is merging all systematics into one list
        // We need to parse the individual properties for each systematic from the values map
        
        // Look for systematic names in the list and try to find their properties
        std::vector<std::string> systematic_names;
        
        // Extract names from the values
        for (const auto& pair : parser.values) {
            if (pair.first.find(general_prefix + ".name") != std::string::npos) {
                systematic_names.push_back(pair.second);
            }
        }
        
        // For each systematic name, try to find its properties
        for (const std::string& name : systematic_names) {
            SystematicConfig syst;
            syst.name = name;
            
            // Look for properties - they should be in the same order as the names
            for (const auto& pair : parser.values) {
                if (pair.first.find(general_prefix + ".type") != std::string::npos) {
                    syst.type = pair.second;
                    break; // Take the first one for now
                }
            }
            
            for (const auto& pair : parser.values) {
                if (pair.first.find(general_prefix + ".value") != std::string::npos) {
                    try {
                        syst.value = std::stod(pair.second);
                    } catch (...) {
                        syst.value = 1.0;
                    }
                    break;
                }
            }
            
            // Look for formula
            for (const auto& pair : parser.values) {
                if (pair.first.find(general_prefix + ".formula") != std::string::npos) {
                    syst.formula = pair.second;
                    break;
                }
            }
            
            // Add bins and processes from the list items (they're mixed in the list)
            for (const std::string& item : list_items) {
                if (item.find("auto_") == 0 || item.find("nLeptonic") == 0) {
                    syst.bins.push_back(item);
                } else if (item == "backgrounds" || item == "signals") {
                    syst.processes.push_back(item);
                }
            }
            
            if (!syst.name.empty() && !syst.type.empty()) {
                systematics.push_back(syst);
            }
        }
    }
    
}

void ConfigParser::ParseSystematicsFromCombinedList(const SimpleYAMLParser& parser) {
    
    if (!parser.lists.count("systematics")) {
        return;
    }
    
    const auto& all_items = parser.lists.at("systematics");
    
    // Print all items to understand structure
    for (size_t i = 0; i < all_items.size(); ++i) {
    }
    
    // Separate systematic names by category based on naming patterns
    std::vector<std::string> abcd_names, precision_names, experimental_names;
    
    for (const std::string& item : all_items) {
        if (item.find("scale_") == 0 || item.find("abcd_") == 0) {
            abcd_names.push_back(item);
        } else if (item.find("precision") != std::string::npos) {
            precision_names.push_back(item);
        } else if (item.find("experimental") != std::string::npos) {
            experimental_names.push_back(item);
        }
    }
    
    // Create systematics for each category
    CreateSystematicsFromNames(parser, abcd_names, "abcd_systematics", config_.abcd_systematics);
    CreateSystematicsFromNames(parser, precision_names, "precision_systematics", config_.precision_systematics);
    CreateSystematicsFromNames(parser, experimental_names, "experimental_systematics", config_.experimental_systematics);
}

void ConfigParser::CreateSystematicsFromNames(const SimpleYAMLParser& parser, const std::vector<std::string>& names, const std::string& category, std::vector<SystematicConfig>& systematics) {
    if (names.empty()) return;
    
    
    // Get common properties for this category
    std::string common_type = "";
    double common_value = 1.0;
    
    std::string type_key = "systematics." + category + ".type";
    std::string value_key = "systematics." + category + ".value";
    
    if (parser.values.count(type_key)) {
        common_type = parser.values.at(type_key);
    }
    
    if (parser.values.count(value_key)) {
        try {
            common_value = std::stod(parser.values.at(value_key));
        } catch (...) {
            common_value = 1.0;
        }
    }
    
    // Get bins and processes from proper keys
    std::vector<std::string> bins_list, processes_list;
    
    std::string bins_key = "systematics." + category + ".bins";
    std::string processes_key = "systematics." + category + ".processes";
    
    if (parser.lists.count(bins_key)) {
        bins_list = parser.lists.at(bins_key);
    }
    
    if (parser.lists.count(processes_key)) {
        processes_list = parser.lists.at(processes_key);
    }
    
    
    // Create systematics - one for each name
    for (size_t i = 0; i < names.size(); i++) {
        SystematicConfig syst;
        syst.name = names[i];
        syst.type = common_type;
        syst.value = common_value;
        
        // Assign bins and processes (use index if arrays are long enough)
        if (i < bins_list.size()) {
            syst.bins.push_back(bins_list[i]);
        }
        if (i < processes_list.size()) {
            syst.processes.push_back(processes_list[i]);
        }
        
        systematics.push_back(syst);
    }
}

void ConfigParser::GenerateABCDBinsFromAxes() {
    // Generate ABCD bins dynamically from x_axis and y_axis definitions
    // A = [x_low, y_high], B = [x_high, y_high], C = [x_low, y_low], D = [x_high, y_low]
    
    if (!config_.abcd.use_explicit_format) {
        return;
    }
    
    // Helper function to filter cuts for specific SV type
    auto filterCutsForSV = [](const std::vector<std::string>& cuts, const std::string& sv_type) {
        std::vector<std::string> filtered_cuts;
        for (const std::string& cut : cuts) {
            // Check if this is an SV-specific cut
            if (cut.find("LeptonicSV_") == 0 && sv_type == "nLeptonic") {
                filtered_cuts.push_back(cut);
            } else if (cut.find("HadronicSV_") == 0 && sv_type == "nHadronic") {
                filtered_cuts.push_back(cut);
            } else if (cut.find("SV_") == 0 || cut.find("rjr_") == 0) {
                // Non-SV-specific cuts (like SV_nLeptonic, rjr_Ms, etc.) - include always
                filtered_cuts.push_back(cut);
            }
        }
        return filtered_cuts;
    };
    
    // Generate bin names based on axis names
    std::string bin_prefix = config_.abcd.x_axis.name + "_" + config_.abcd.y_axis.name + "_";
    
    // Create regions mapping
    config_.abcd.regions["region_A"] = bin_prefix + "A";
    config_.abcd.regions["region_B"] = bin_prefix + "B"; 
    config_.abcd.regions["region_C"] = bin_prefix + "C";
    config_.abcd.regions["region_D"] = bin_prefix + "D";
    
    // Determine SV types from x_axis cuts
    std::string sv_type_low = "", sv_type_high = "";
    for (const std::string& cut : config_.abcd.x_axis.low_cuts) {
        if (cut.find("SV_nLeptonic") != std::string::npos) sv_type_low = "nLeptonic";
        else if (cut.find("SV_nHadronic") != std::string::npos) sv_type_low = "nHadronic";
    }
    for (const std::string& cut : config_.abcd.x_axis.high_cuts) {
        if (cut.find("SV_nLeptonic") != std::string::npos) sv_type_high = "nLeptonic";
        else if (cut.find("SV_nHadronic") != std::string::npos) sv_type_high = "nHadronic";
    }
    
    // Generate Region A: [x_low, y_high]
    BinConfig bin_a;
    bin_a.name = bin_prefix + "A";
    std::string low_desc = config_.abcd.x_axis.low_desc.empty() ? config_.abcd.x_axis.name + "_low" : config_.abcd.x_axis.low_desc;
    std::string high_desc = config_.abcd.y_axis.high_desc.empty() ? config_.abcd.y_axis.name + "_high" : config_.abcd.y_axis.high_desc;
    bin_a.description = low_desc + ", " + high_desc;
    
    // Add common cuts
    bin_a.cuts.insert(bin_a.cuts.end(), config_.abcd.common_cuts.begin(), config_.abcd.common_cuts.end());
    
    // Add x_low cuts
    bin_a.cuts.insert(bin_a.cuts.end(), config_.abcd.x_axis.low_cuts.begin(), config_.abcd.x_axis.low_cuts.end());
    
    // Add y_high cuts, filtered for the appropriate SV type
    auto y_high_filtered = filterCutsForSV(config_.abcd.y_axis.high_cuts, sv_type_low);
    bin_a.cuts.insert(bin_a.cuts.end(), y_high_filtered.begin(), y_high_filtered.end());
    
    config_.bins.push_back(bin_a);
    
    // Generate Region B: [x_high, y_high]
    BinConfig bin_b;
    bin_b.name = bin_prefix + "B";
    std::string x_high_desc = config_.abcd.x_axis.high_desc.empty() ? config_.abcd.x_axis.name + "_high" : config_.abcd.x_axis.high_desc;
    std::string y_high_desc = config_.abcd.y_axis.high_desc.empty() ? config_.abcd.y_axis.name + "_high" : config_.abcd.y_axis.high_desc;
    bin_b.description = x_high_desc + ", " + y_high_desc;
    
    // Add common cuts
    bin_b.cuts.insert(bin_b.cuts.end(), config_.abcd.common_cuts.begin(), config_.abcd.common_cuts.end());
    
    // Add x_high cuts
    bin_b.cuts.insert(bin_b.cuts.end(), config_.abcd.x_axis.high_cuts.begin(), config_.abcd.x_axis.high_cuts.end());
    
    // Add y_high cuts, filtered for the appropriate SV type
    y_high_filtered = filterCutsForSV(config_.abcd.y_axis.high_cuts, sv_type_high);
    bin_b.cuts.insert(bin_b.cuts.end(), y_high_filtered.begin(), y_high_filtered.end());
    
    config_.bins.push_back(bin_b);
    
    // Generate Region C: [x_low, y_low]
    BinConfig bin_c;
    bin_c.name = bin_prefix + "C";
    std::string x_low_desc = config_.abcd.x_axis.low_desc.empty() ? config_.abcd.x_axis.name + "_low" : config_.abcd.x_axis.low_desc;
    std::string y_low_desc = config_.abcd.y_axis.low_desc.empty() ? config_.abcd.y_axis.name + "_low" : config_.abcd.y_axis.low_desc;
    bin_c.description = x_low_desc + ", " + y_low_desc;
    
    // Add common cuts
    bin_c.cuts.insert(bin_c.cuts.end(), config_.abcd.common_cuts.begin(), config_.abcd.common_cuts.end());
    
    // Add x_low cuts
    bin_c.cuts.insert(bin_c.cuts.end(), config_.abcd.x_axis.low_cuts.begin(), config_.abcd.x_axis.low_cuts.end());
    
    // Add y_low cuts, filtered for the appropriate SV type
    auto y_low_filtered = filterCutsForSV(config_.abcd.y_axis.low_cuts, sv_type_low);
    bin_c.cuts.insert(bin_c.cuts.end(), y_low_filtered.begin(), y_low_filtered.end());
    
    config_.bins.push_back(bin_c);
    
    // Generate Region D: [x_high, y_low]
    BinConfig bin_d;
    bin_d.name = bin_prefix + "D";
    std::string x_high_desc_d = config_.abcd.x_axis.high_desc.empty() ? config_.abcd.x_axis.name + "_high" : config_.abcd.x_axis.high_desc;
    std::string y_low_desc_d = config_.abcd.y_axis.low_desc.empty() ? config_.abcd.y_axis.name + "_low" : config_.abcd.y_axis.low_desc;
    bin_d.description = x_high_desc_d + ", " + y_low_desc_d;
    
    // Add common cuts
    bin_d.cuts.insert(bin_d.cuts.end(), config_.abcd.common_cuts.begin(), config_.abcd.common_cuts.end());
    
    // Add x_high cuts
    bin_d.cuts.insert(bin_d.cuts.end(), config_.abcd.x_axis.high_cuts.begin(), config_.abcd.x_axis.high_cuts.end());
    
    // Add y_low cuts, filtered for the appropriate SV type
    y_low_filtered = filterCutsForSV(config_.abcd.y_axis.low_cuts, sv_type_high);
    bin_d.cuts.insert(bin_d.cuts.end(), y_low_filtered.begin(), y_low_filtered.end());
    
    config_.bins.push_back(bin_d);
    
    // Show generated ABCD regions
    std::cout << "\n=== Generated ABCD Regions ===" << std::endl;
    std::cout << "X-axis: " << config_.abcd.x_axis.name << std::endl;
    std::cout << "Y-axis: " << config_.abcd.y_axis.name << std::endl;
    std::cout << "------------------------------------" << std::endl;

    for (const auto& region : config_.abcd.regions) {
      const std::string& region_key = region.first;
      const std::string& bin_name = region.second;

      auto it = std::find_if(config_.bins.begin(), config_.bins.end(),
			     [&](const BinConfig& b) { return b.name == bin_name; });

      if (it == config_.bins.end()) continue;

      const auto& bin = *it;

      std::cout << region_key << " (" << bin.name << "): "
		<< bin.description << std::endl;

      for (const std::string& cut : bin.cuts) {
        std::cout << "   - " << cut << std::endl;
      }

      std::cout << "   Combined: " << GetCombinedCuts(bin.name) << std::endl;
      std::cout << "------------------------------------" << std::endl;
    }

    std::cout << "====================================" << std::endl;
}
