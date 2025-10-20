// ConfigParser.cpp  (refactored)
// Keeps original behavior, reduces duplication, clearer helpers and logging.

#include "ConfigParser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <set>
#include <map>
#include <vector>
#include <functional>
#include <type_traits>

// ------------------------------------------------------------
// Small utility helpers (trim, removeQuotes, safe conversion)
// ------------------------------------------------------------
static inline std::string trimCopy(std::string s) {
    const char* ws = " \t\n\r\f\v";
    auto start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

static inline void trimInPlace(std::string &s) { s = trimCopy(s); }

static inline std::string removeQuotes(std::string s) {
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')))
        return s.substr(1, s.size() - 2);
    return s;
}

// Template to get typed value or default from parser map
template <typename T>
T GetValueOrDefault(const std::map<std::string, std::string>& values,
                    const std::string& key, const T& def) {
    auto it = values.find(key);
    if (it == values.end()) return def;
    const std::string& v = it->second;
    try {
        if constexpr (std::is_same_v<T, std::string>) return v;
        if constexpr (std::is_same_v<T, double>) return std::stod(v);
        if constexpr (std::is_same_v<T, int>) return std::stoi(v);
        if constexpr (std::is_same_v<T, bool>) return (v == "true" || v == "1");
    } catch (...) {
        return def;
    }
    return def;
}

static inline std::vector<std::string> GetListOrDefault(
    const std::map<std::string, std::vector<std::string>>& lists,
    const std::string& key) {
    std::cout << "DEBUG: GetListOrDefault called for key: " << key << std::endl;
    
    auto it = lists.find(key);
    if (it == lists.end()) {
        std::cout << "DEBUG: Key not found, returning empty vector" << std::endl;
        return std::vector<std::string>();
    }
    
    std::cout << "DEBUG: Key found, source vector address: " << reinterpret_cast<uintptr_t>(&it->second) << std::endl;
    std::cout << "DEBUG: Source vector size: " << it->second.size() << std::endl;
    
    // Safe copy to avoid memory corruption
    std::vector<std::string> result;
    std::cout << "DEBUG: Created result vector, about to reserve..." << std::endl;
    result.reserve(it->second.size());
    std::cout << "DEBUG: Reserved space, about to copy items..." << std::endl;
    
    size_t count = 0;
    for (const auto& item : it->second) {
        std::cout << "DEBUG: Copying item " << count << ": '" << item << "'" << std::endl;
        result.emplace_back(item);
        count++;
    }
    
    std::cout << "DEBUG: Copy completed, returning result with size: " << result.size() << std::endl;
    return result;
}

// ------------------------------------------------------------
// Simple verbosity-aware logger
// ------------------------------------------------------------
enum LogLevel { LOG_ERROR = 0, LOG_WARN = 1, LOG_INFO = 2, LOG_DEBUG = 3 };
static int CURRENT_LOG_LEVEL = 2; // default INFO

static inline void Log(LogLevel lvl, const std::string& msg) {
    if (lvl <= CURRENT_LOG_LEVEL) {
        if (lvl == LOG_ERROR) std::cerr << "ERROR: ";
        else if (lvl == LOG_WARN) std::cout << "WARN: ";
        else if (lvl == LOG_DEBUG) std::cout << "DEBUG: ";
        std::cout << msg << std::endl;
    }
}

// ------------------------------------------------------------
// SimpleYAMLParser 
// ------------------------------------------------------------
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
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) lines.push_back(line);
        file.close();

        parseAnchors(lines);

        std::string current_section = "";
        std::string current_subsection = "";
        std::string current_anchor_key = "";

        for (const std::string& line_raw : lines) {
            line = line_raw;

            // Count indentation in spaces before trimming
            size_t indent = 0;
            for (char c : line) {
                if (c == ' ') indent++; else break;
            }

            // Strip comments
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);

            // Trim whitespace
            trimInPlace(line);
            if (line.empty()) continue;

            if (indent == 0) {
                if (line.back() == ':') {
                    std::string section_key = line.substr(0, line.length() - 1);
                    // anchor & handling at top level
                    size_t anchor_pos = section_key.find('&');
                    if (anchor_pos != std::string::npos) {
                        current_anchor_key = trimCopy(section_key.substr(anchor_pos + 1));
                        current_section = trimCopy(section_key.substr(0, anchor_pos));
                    } else {
                        current_section = trimCopy(section_key);
                        current_anchor_key = "";
                    }
                    current_subsection = "";
                }
            } else if (indent <= 2) {
                if (line.back() == ':') {
                    current_subsection = trimCopy(line.substr(0, line.length() - 1));
                    size_t anchor_pos = current_subsection.find('&');
                    if (anchor_pos != std::string::npos) {
                        current_anchor_key = trimCopy(current_subsection.substr(anchor_pos + 1));
                        current_subsection = trimCopy(current_subsection.substr(0, anchor_pos));
                    } else {
                        current_anchor_key = "";
                    }
                } else {
                    parseKeyValue(line, current_section, "", current_anchor_key);
                }
            } else {
                // deeper nested: maintain state for nested keys
                static std::string current_key = "";
                static std::string last_section = "";
                static std::string last_subsection = "";

                if (current_section != last_section || current_subsection != last_subsection) {
                    current_key = "";
                    last_section = current_section;
                    last_subsection = current_subsection;
                }

                if (line.back() == ':') {
                    std::string extracted_key = trimCopy(line.substr(0, line.length() - 1));
                    current_key = extracted_key;
                } else {
                    std::string subsection_full = current_subsection;
                    if (!current_key.empty()) subsection_full += "." + current_key;
                    parseKeyValue(line, current_section, subsection_full, current_anchor_key);
                }
            }
        }
        return true;
    }

private:
    std::string line;

    void parseAnchors(const std::vector<std::string>& lines) {
        std::string current_anchor = "";
        for (const std::string& lraw : lines) {
            std::string l = lraw;
            size_t comment_pos = l.find('#');
            if (comment_pos != std::string::npos) l = l.substr(0, comment_pos);
            trimInPlace(l);
            if (l.empty()) continue;
            size_t anchor_pos = l.find('&');
            if (anchor_pos != std::string::npos && l.find(':') != std::string::npos) {
                current_anchor = trimCopy(l.substr(anchor_pos + 1));
            } else if (l.back() == ':') {
                current_anchor = "";
            }
            if (!current_anchor.empty() && !l.empty() && l.front() == '-') {
                std::string item = trimCopy(l.substr(1));
                item = removeQuotes(item);
                anchors[current_anchor].push_back(item);
            }
        }
    }

    void parseKeyValue(const std::string& raw_line, const std::string& section, const std::string& subsection, const std::string& current_anchor = "") {
        std::string line = raw_line;
        if (line.empty()) return;
        if (line.front() == '-') {
            std::string item = trimCopy(line.substr(1));
            if (!item.empty() && item.front() == '*') {
                std::string anchor_name = item.substr(1);
                if (anchors.count(anchor_name)) {
                    std::string key = section + (subsection.empty() ? "" : "." + subsection);
                    for (const auto& anchor_item : anchors[anchor_name]) lists[key].push_back(anchor_item);
                    return;
                }
            }
            item = removeQuotes(item);
            std::string key = section + (subsection.empty() ? "" : "." + subsection);
            lists[key].push_back(item);
            if (!current_anchor.empty()) anchors[current_anchor].push_back(item);
            return;
        }

        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) return;
        std::string key = trimCopy(line.substr(0, colon_pos));
        std::string value = trimCopy(line.substr(colon_pos + 1));

        // anchor in key
        size_t anchor_pos = key.find('&');
        std::string anchor_name = "";
        if (anchor_pos != std::string::npos) {
            anchor_name = trimCopy(key.substr(anchor_pos + 1));
            key = trimCopy(key.substr(0, anchor_pos));
        }

        // array notation [a, b, c]
        if (!value.empty() && value.front() == '[' && value.back() == ']') {
            std::string array_content = value.substr(1, value.length() - 2);
            std::stringstream ss(array_content);
            std::string item;
            std::string full_key = section + (subsection.empty() ? "" : "." + subsection) + "." + key;
            std::vector<std::string> items;
            while (std::getline(ss, item, ',')) {
                trimInPlace(item);
                item = removeQuotes(item);
                if (!item.empty()) {
                    lists[full_key].push_back(item);
                    items.push_back(item);
                }
            }
            if (!anchor_name.empty()) anchors[anchor_name] = items;
        } else {
            value = removeQuotes(value);
            std::string full_key = section + (subsection.empty() ? "" : "." + subsection) + "." + key;
            values[full_key] = value;
            if (!anchor_name.empty()) {
                // possible to store anchor - mimic original behavior
                anchors[anchor_name].push_back(value);
            }
        }
    }
};

// ------------------------------------------------------------
// ConfigParser implementation (refactored)
// ------------------------------------------------------------

ConfigParser::ConfigParser() {
    std::cout << "DEBUG: ConfigParser constructor started" << std::endl;
    std::cout << "DEBUG: About to call SetDefaults()" << std::endl;
    SetDefaults();
    std::cout << "DEBUG: SetDefaults() completed" << std::endl;
}
ConfigParser::~ConfigParser() {}

void ConfigParser::SetDefaults() {
    std::cout << "DEBUG: SetDefaults started" << std::endl;
    config_.name = "default_analysis";
    std::cout << "DEBUG: Set name" << std::endl;
    config_.data_as_background = false;
    config_.apply_trigger_cuts = true;  
    config_.luminosity = 400.0;
    config_.output_json = "output.json";
    config_.output_dir = "./json/";
    config_.verbosity = 1;
    config_.parallel = false;
    config_.dry_run = false;
    std::cout << "DEBUG: Set basic config" << std::endl;
    
    // Skip ABCD initialization for now to isolate the issue
    std::cout << "DEBUG: SetDefaults completed - skipping ABCD init" << std::endl;
}

bool ConfigParser::LoadConfig(const std::string& config_file) {
    return LoadYAML(config_file);
}

bool ConfigParser::LoadYAML(const std::string& config_file) {
    SimpleYAMLParser parser;
    if (!parser.parse(config_file)) return false;

    // set logging level from defaults or options later (verbosity already in config_)
    CURRENT_LOG_LEVEL = LOG_DEBUG; // Force debug level to help diagnose crash

    // --- analysis section
    config_.name = GetValueOrDefault<std::string>(parser.values, "analysis.name", config_.name);
    config_.data_as_background = GetValueOrDefault<bool>(parser.values, "analysis.data_as_background", config_.data_as_background);
    config_.apply_trigger_cuts = GetValueOrDefault<bool>(parser.values, "analysis.apply_trigger_cuts", config_.apply_trigger_cuts);
    config_.luminosity = GetValueOrDefault<double>(parser.values, "analysis.luminosity", config_.luminosity);
    config_.output_json = GetValueOrDefault<std::string>(parser.values, "analysis.output_json", config_.output_json);
    config_.output_dir = GetValueOrDefault<std::string>(parser.values, "analysis.output_dir", config_.output_dir);
    config_.method = GetValueOrDefault<std::string>(parser.values, "analysis.method", config_.method);

    // --- ABCD handling
    if (config_.method == "ABCD") {
        bool has_explicit_format = parser.values.count("x_axis.name") > 0;
        if (has_explicit_format) {
            config_.abcd.use_explicit_format = true;

            // x axis
            config_.abcd.x_axis.name = GetValueOrDefault<std::string>(parser.values, "x_axis.name", "");
            config_.abcd.x_axis.low_desc = GetValueOrDefault<std::string>(parser.values, "x_axis.x_low.description", "");
            config_.abcd.x_axis.high_desc = GetValueOrDefault<std::string>(parser.values, "x_axis.x_high.description", "");
            config_.abcd.x_axis.low_cuts = GetListOrDefault(parser.lists, "x_axis.x_low.cuts");
            config_.abcd.x_axis.high_cuts = GetListOrDefault(parser.lists, "x_axis.x_high.cuts");

            // y axis
            config_.abcd.y_axis.name = GetValueOrDefault<std::string>(parser.values, "y_axis.name", "");
            config_.abcd.y_axis.low_desc = GetValueOrDefault<std::string>(parser.values, "y_axis.y_low.description", "");
            config_.abcd.y_axis.high_desc = GetValueOrDefault<std::string>(parser.values, "y_axis.y_high.description", "");
            config_.abcd.y_axis.low_cuts = GetListOrDefault(parser.lists, "y_axis.y_low.cuts");
            config_.abcd.y_axis.high_cuts = GetListOrDefault(parser.lists, "y_axis.y_high.cuts");

            // common cuts - the original used "abcd_common_cuts" as top-level list
            Log(LOG_DEBUG, "Looking for abcd_common_cuts key in parser.lists");
            
            // Debug: Print all available keys first
            for (const auto& pair : parser.lists) {
                Log(LOG_DEBUG, "Available list key: '" + pair.first + "' with " + std::to_string(pair.second.size()) + " items");
            }
            
            // Debug: Check if the specific key exists and examine its contents
            auto it = parser.lists.find("abcd_common_cuts");
            if (it != parser.lists.end()) {
                Log(LOG_DEBUG, "Found abcd_common_cuts vector at address: " + std::to_string(reinterpret_cast<uintptr_t>(&it->second)));
                Log(LOG_DEBUG, "Vector size: " + std::to_string(it->second.size()));
                Log(LOG_DEBUG, "Vector capacity: " + std::to_string(it->second.capacity()));
                
                // Try to access each element individually
                for (size_t i = 0; i < it->second.size(); ++i) {
                    try {
                        const std::string& item = it->second[i];
                        Log(LOG_DEBUG, "Item " + std::to_string(i) + " at address " + std::to_string(reinterpret_cast<uintptr_t>(&item)) + ": '" + item + "'");
                    } catch (...) {
                        Log(LOG_DEBUG, "ERROR: Failed to access item " + std::to_string(i));
                        break;
                    }
                }
            } else {
                Log(LOG_DEBUG, "abcd_common_cuts key not found!");
            }
            
            Log(LOG_DEBUG, "About to call GetListOrDefault...");
            
            // Debug the destination vector before assignment
            Log(LOG_DEBUG, "Destination vector address: " + std::to_string(reinterpret_cast<uintptr_t>(&config_.abcd.common_cuts)));
            Log(LOG_DEBUG, "Destination vector size before: " + std::to_string(config_.abcd.common_cuts.size()));
            Log(LOG_DEBUG, "Destination vector capacity before: " + std::to_string(config_.abcd.common_cuts.capacity()));
            
            // Try assignment
            auto temp_result = GetListOrDefault(parser.lists, "abcd_common_cuts");
            Log(LOG_DEBUG, "GetListOrDefault returned vector with size: " + std::to_string(temp_result.size()));
            
            // Try explicit assignment instead of direct assignment
            Log(LOG_DEBUG, "About to perform assignment...");
            config_.abcd.common_cuts = std::move(temp_result);
            Log(LOG_DEBUG, "Assignment completed. Result size: " + std::to_string(config_.abcd.common_cuts.size()));

            GenerateABCDBinsFromAxes();
        } else {
            // old format: read abcd.regions.* values
            config_.abcd.use_explicit_format = false;
            for (const auto& pair : parser.values) {
                if (pair.first.find("abcd.regions.") == 0) {
                    std::string region_name = pair.first.substr(13);
                    config_.abcd.regions[region_name] = pair.second;
                }
            }
        }

        config_.abcd.formula = GetValueOrDefault<std::string>(parser.values, "abcd.formula", std::string("(@0*@1/@2)"));
        config_.abcd.generate_datacards = GetValueOrDefault<bool>(parser.values, "abcd.generate_datacards", true);
    }

    // --- samples
    config_.backgrounds = GetListOrDefault(parser.lists, "samples.backgrounds");
    config_.signals = GetListOrDefault(parser.lists, "samples.signals");
    config_.data = GetListOrDefault(parser.lists, "samples.data");
    config_.signal_points = GetListOrDefault(parser.lists, "samples.signal_points");

    // --- systematics (unified)
    ParseSystematics(parser);

    // --- bins from lists (legacy behavior preserved)
    for (const auto& pair : parser.lists) {
        if (pair.first.find("bins.") == 0) {
            std::string full_key = pair.first;
            std::string bin_name;
            size_t bins_pos = full_key.find("bins.");
            if (bins_pos != std::string::npos) {
                std::string remainder = full_key.substr(bins_pos + 5);
                size_t dot_pos = remainder.find('.');
                if (dot_pos != std::string::npos) {
                    if (full_key.size() >= 5 && full_key.substr(full_key.length() - 5) == ".cuts") {
                        bin_name = remainder.substr(0, dot_pos);
                    } else {
                        continue;
                    }
                } else {
                    bin_name = remainder;
                }
            }
            if (!bin_name.empty()) {
                BinConfig bin_config;
                bin_config.name = bin_name;
                bin_config.cuts = pair.second;
                std::string desc_key = "bins." + bin_name + ".description";
                if (parser.values.count(desc_key)) bin_config.description = parser.values.at(desc_key);
                config_.bins.push_back(bin_config);
            }
        }
    }

    // --- options
    config_.verbosity = GetValueOrDefault<int>(parser.values, "options.verbosity", config_.verbosity);
    config_.parallel = GetValueOrDefault<bool>(parser.values, "options.parallel", config_.parallel);
    config_.dry_run = GetValueOrDefault<bool>(parser.values, "options.dry_run", config_.dry_run);

    return ValidateConfig();
}

// ------------------------------------------------------------
// Get default trigger cuts
// ------------------------------------------------------------
std::string ConfigParser::GetTriggerCuts() const {
    // Comment out the next line to disable trigger cuts for all datasets
    return "Flag_BadChargedCandidateFilter && Flag_BadPFMuonDzFilter && Flag_BadPFMuonFilter && Flag_HBHENoiseFilter && Flag_HBHENoiseIsoFilter && Flag_ecalBadCalibFilter && Flag_eeBadScFilter && Flag_goodVertices && Flag_hfNoisyHitsFilter && Flag_globalSuperTightHalo2016Filter && (Trigger_PFMET120_PFMHT120_IDTight || Trigger_PFMETNoMu120_PFMHTNoMu120_IDTight || Trigger_PFMET120_PFMHT120_IDTight_PFHT60 || Trigger_PFMETNoMu120_PFMHTNoMu120_IDTight_PFHT60)";
    // return "";  // Uncomment this line to disable trigger cuts
}

// ------------------------------------------------------------
// Combine cuts for a bin
// ------------------------------------------------------------
std::string ConfigParser::GetCombinedCuts(const std::string& bin_name) const {
    for (const auto& bin : config_.bins) {
        if (bin.name == bin_name) {
            if (bin.cuts.empty()) return "";
            std::string combined = "(" + bin.cuts[0] + ")";
            for (size_t i = 1; i < bin.cuts.size(); ++i) combined += " && (" + bin.cuts[i] + ")";
            return combined;
        }
    }
    return "";
}

// Get combined cuts with trigger cuts (for data samples)
std::string ConfigParser::GetCombinedCuts(const std::string& bin_name, bool include_trigger_cuts) const {
    for (const auto& bin : config_.bins) {
        if (bin.name == bin_name) {
            std::vector<std::string> all_cuts;
            
            // Add trigger cuts first if enabled and requested
            if (config_.apply_trigger_cuts && include_trigger_cuts) {
                all_cuts.push_back(GetTriggerCuts());
            }
            
            // Add bin-specific cuts
            for (const auto& cut : bin.cuts) {
                all_cuts.push_back(cut);
            }
            
            if (all_cuts.empty()) return "";
            
            std::string combined = "(" + all_cuts[0] + ")";
            for (size_t i = 1; i < all_cuts.size(); ++i) combined += " && (" + all_cuts[i] + ")";
            return combined;
        }
    }
    return "";
}

// ------------------------------------------------------------
// PrintConfig
// ------------------------------------------------------------
void ConfigParser::PrintConfig() const {
    std::cout << "=== Analysis Configuration ===\n";
    std::cout << "Name: " << config_.name << "\n";
    std::cout << "Luminosity: " << config_.luminosity << " fb^-1\n";
    std::cout << "Output JSON: " << config_.output_json << "\n";
    std::cout << "Output Directory: " << config_.output_dir << "\n";

    std::cout << "\nBackgrounds: ";
    for (const auto& bg : config_.backgrounds) std::cout << bg << " ";
    std::cout << "\nSignals: ";
    for (const auto& s : config_.signals) std::cout << s << " ";
    std::cout << "\n";

    if (!config_.signal_points.empty()) {
        std::cout << "Signal Points: ";
        for (const auto& p : config_.signal_points) std::cout << p << " ";
        std::cout << "\n";
    }

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
        std::cout << "\nAnalysis Bins:\n";
        for (const auto& bin : config_.bins) {
            if (config_.method == "ABCD" && config_.abcd.use_explicit_format &&
                (bin.name.find("_A") != std::string::npos || bin.name.find("_B") != std::string::npos ||
                 bin.name.find("_C") != std::string::npos || bin.name.find("_D") != std::string::npos)) {
                continue;
            }
            std::cout << "  " << bin.name << ": " << bin.description << "\n";
            std::cout << "    Cuts: " << GetCombinedCuts(bin.name) << "\n";
        }
    }

    std::cout << "\nOptions:\n"
              << "  Verbosity: " << config_.verbosity << "\n"
              << "  Parallel: " << (config_.parallel ? "true" : "false") << "\n"
              << "  Dry run: " << (config_.dry_run ? "true" : "false") << "\n";
}

// ------------------------------------------------------------
// Validation 
// ------------------------------------------------------------
bool ConfigParser::ValidateConfig() const {
    if (config_.luminosity <= 0) {
        std::cerr << "Error: Luminosity must be positive\n";
        return false;
    }
    if (config_.backgrounds.empty()) Log(LOG_WARN, "Warning: No background samples specified");
    if (config_.bins.empty()) {
        std::cerr << "Error: No analysis bins defined\n";
        return false;
    }
    if (config_.method == "ABCD") return ValidateABCDConfig();
    return true;
}

bool ConfigParser::ValidateABCDConfig() const {
    if (!config_.abcd.IsValid()) {
        std::cerr << "Error: Invalid ABCD configuration\n";
        std::cerr << "  Regions count: " << config_.abcd.regions.size() << " (expected 4)\n";
        std::cerr << "  Predicted region: '" << config_.abcd.predicted_region << "'\n";
        return false;
    }
    for (const auto& region_pair : config_.abcd.regions) {
        const std::string& bin_name = region_pair.second;
        bool found = false;
        for (const auto& bin : config_.bins) { if (bin.name == bin_name) { found = true; break; } }
        if (!found) {
            std::cerr << "Error: ABCD region '" << region_pair.first << "' references undefined bin '" << bin_name << "'\n";
            return false;
        }
    }
    return true;
}

// ------------------------------------------------------------
// Systematics parsing 
// ------------------------------------------------------------
void ConfigParser::ParseSystematics(const SimpleYAMLParser& parser) {
    ParseSystematicBlock(parser, "systematics.abcd_systematics", config_.abcd_systematics);
    ParseSystematicBlock(parser, "systematics.precision_systematics", config_.precision_systematics);
    ParseSystematicBlock(parser, "systematics.experimental_systematics", config_.experimental_systematics);
}

void ConfigParser::ParseSystematicBlock(const SimpleYAMLParser& parser,
					const std::string& prefix,
					std::vector<SystematicConfig>& target) {
    if (parser.lists.count(prefix) == 0) return;
    const auto& names = parser.lists.at(prefix);

    std::string type_key = prefix + ".type";
    std::string value_key = prefix + ".value";
    std::string formula_key = prefix + ".formula";
    std::string bins_key = prefix + ".bins";
    std::string processes_key = prefix + ".processes";

    std::string common_type = GetValueOrDefault<std::string>(parser.values, type_key, "");
    double common_value = GetValueOrDefault<double>(parser.values, value_key, 1.0);
    std::string common_formula = GetValueOrDefault<std::string>(parser.values, formula_key, "");

    std::vector<std::string> bins_list = GetListOrDefault(parser.lists, bins_key);
    std::vector<std::string> processes_list = GetListOrDefault(parser.lists, processes_key);

    for (size_t i = 0; i < names.size(); ++i) {
        SystematicConfig s;
        s.name = names[i];
        s.type = common_type;
        s.value = common_value;
        s.formula = common_formula;
        if (i < bins_list.size()) s.bins.push_back(bins_list[i]);
        if (i < processes_list.size()) s.processes.push_back(processes_list[i]);
        target.push_back(s);
    }
}

// ------------------------------------------------------------
// Generate ABCD bins from axis 
// ------------------------------------------------------------
void ConfigParser::GenerateABCDBinsFromAxes() {
    if (!config_.abcd.use_explicit_format) return;

    auto filterCutsForSV = [](const std::vector<std::string>& cuts, const std::string& sv_type) {
        std::vector<std::string> filtered;
        filtered.reserve(cuts.size());
        for (const auto& cut : cuts) {
            if ((cut.find("LeptonicSV_") == 0 && sv_type == "nLeptonic") ||
                (cut.find("HadronicSV_") == 0 && sv_type == "nHadronic") ||
                (cut.find("SV_") == 0) || (cut.find("rjr_") == 0)) {
                filtered.push_back(cut);
            }
        }
        return filtered;
    };

    auto getSVType = [](const std::vector<std::string>& cuts) {
        for (const auto& c : cuts) {
            if (c.find("SV_nLeptonic") != std::string::npos) return std::string("nLeptonic");
            if (c.find("SV_nHadronic") != std::string::npos) return std::string("nHadronic");
        }
        return std::string("");
    };

    std::string sv_type_low = getSVType(config_.abcd.x_axis.low_cuts);
    std::string sv_type_high = getSVType(config_.abcd.x_axis.high_cuts);
    std::string prefix = config_.abcd.x_axis.name + "_" + config_.abcd.y_axis.name + "_";

    auto x_low_desc = config_.abcd.x_axis.low_desc.empty() ? config_.abcd.x_axis.name + "_low" : config_.abcd.x_axis.low_desc;
    auto x_high_desc = config_.abcd.x_axis.high_desc.empty() ? config_.abcd.x_axis.name + "_high" : config_.abcd.x_axis.high_desc;
    auto y_low_desc = config_.abcd.y_axis.low_desc.empty() ? config_.abcd.y_axis.name + "_low" : config_.abcd.y_axis.low_desc;
    auto y_high_desc = config_.abcd.y_axis.high_desc.empty() ? config_.abcd.y_axis.name + "_high" : config_.abcd.y_axis.high_desc;

    struct RegionSpec {
        const std::vector<std::string>& xcuts;
        const std::vector<std::string>& ycuts;
        std::string sv_type;
        std::string suffix;
        std::string xdesc;
        std::string ydesc;
    };

    std::map<std::string, RegionSpec> regions = {
        {"region_A", {config_.abcd.x_axis.low_cuts,  config_.abcd.y_axis.high_cuts, sv_type_low,  "A", x_low_desc,  y_high_desc}},
        {"region_B", {config_.abcd.x_axis.high_cuts, config_.abcd.y_axis.high_cuts, sv_type_high, "B", x_high_desc, y_high_desc}},
        {"region_C", {config_.abcd.x_axis.low_cuts,  config_.abcd.y_axis.low_cuts,  sv_type_low,  "C", x_low_desc,  y_low_desc}},
        {"region_D", {config_.abcd.x_axis.high_cuts, config_.abcd.y_axis.low_cuts,  sv_type_high, "D", x_high_desc, y_low_desc}}
    };

    for (const auto& [region_key, spec] : regions) {
        BinConfig bin;
        bin.name = prefix + spec.suffix;
        bin.description = spec.xdesc + ", " + spec.ydesc;

        // combine common + xcuts + filtered ycuts
        bin.cuts.insert(bin.cuts.end(), config_.abcd.common_cuts.begin(), config_.abcd.common_cuts.end());
        bin.cuts.insert(bin.cuts.end(), spec.xcuts.begin(), spec.xcuts.end());
        auto y_filtered = filterCutsForSV(spec.ycuts, spec.sv_type);
        bin.cuts.insert(bin.cuts.end(), y_filtered.begin(), y_filtered.end());

        config_.bins.push_back(bin);
        config_.abcd.regions[region_key] = bin.name;
    }

    // Display (kept same style)
    std::cout << "\n=== Generated ABCD Regions ===\n";
    std::cout << "X-axis: " << config_.abcd.x_axis.name << "\n";
    std::cout << "Y-axis: " << config_.abcd.y_axis.name << "\n";
    std::cout << "------------------------------------\n";

    for (const auto& region : config_.abcd.regions) {
        const std::string& region_key = region.first;
        const std::string& bin_name = region.second;

        auto it = std::find_if(config_.bins.begin(), config_.bins.end(),
                               [&](const BinConfig& b) { return b.name == bin_name; });

        if (it == config_.bins.end()) continue;
        const auto& bin = *it;

        std::cout << region_key << " (" << bin.name << "): " << bin.description << "\n";
        for (const auto& cut : bin.cuts) std::cout << "   - " << cut << "\n";
        std::cout << "   Combined: " << GetCombinedCuts(bin.name) << "\n";
        std::cout << "------------------------------------\n";
    }
    std::cout << "====================================\n";
}

// ------------------------------------------------------------
// Systematic resolution helper 
// ------------------------------------------------------------
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
            for (const auto& region_pair : abcd.regions) resolved_bins.push_back(region_pair.second);
        } else {
            resolved_bins.push_back(bin);
        }
    }
    return resolved_bins;
}
