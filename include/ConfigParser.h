#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <string>
#include <vector>
#include <map>
#include <memory>

// Forward declarations
class SimpleYAMLParser;

struct BinConfig {
    std::string name;
    std::string description;
    std::vector<std::string> cuts;
};

// Forward declaration
struct ABCDConfig;

struct SystematicConfig {
    std::string name;
    std::string type;
    double value;
    std::vector<std::string> bins;
    std::vector<std::string> processes;
    std::string formula;
    std::vector<std::string> parameters;
    
    // Method to resolve auto mappings based on ABCD config
    std::vector<std::string> ResolveBins(const ABCDConfig& abcd) const;
};

// Explicit ABCD axis definitions (new format)
struct AxisConfig {
    std::string name;        // axis name (e.g., "Ntype", "Sxy")
    std::string low_desc;    // description for low region
    std::string high_desc;   // description for high region
    std::vector<std::string> low_cuts;   // cuts for low region
    std::vector<std::string> high_cuts;  // cuts for high region
};

struct ABCDConfig {
    // Old format support
    std::map<std::string, std::string> regions;  // region_A -> bin_name mapping
    
    // New explicit format support  
    AxisConfig x_axis;                           // X-axis definition
    AxisConfig y_axis;                           // Y-axis definition
    std::vector<std::string> common_cuts;       // common cuts for all regions
    bool use_explicit_format;                   // flag to indicate which format is used
    
    std::string predicted_region;                 // which region is predicted (A, B, C, or D)
    std::string formula;                          // ABCD formula (default: "(@0*@1/@2)")
    bool generate_datacards;                      // whether to generate CombineHarvester datacards
    
    // Helper methods
    std::string GetPredictedBin() const {
        if (regions.find(predicted_region) != regions.end()) {
            return regions.at(predicted_region);
        }
        return "";
    }
    
    std::vector<std::string> GetControlBins() const {
        std::vector<std::string> controls;
        for (const auto& region_pair : regions) {
            if (region_pair.first != predicted_region) {
                controls.push_back(region_pair.second);
            }
        }
        return controls;
    }
    
    std::vector<std::string> GetFormulaParameters() const {
        std::vector<std::string> params;
        for (const auto& region_pair : regions) {
            if (region_pair.first != predicted_region) {
                params.push_back("scale_" + region_pair.second);
            }
        }
        return params;
    }
    
    bool IsValid() const {
        return regions.size() == 4;  // Just require 4 regions, predicted region handled at BF step
    }
};

struct AnalysisConfig {
    std::string name;
    std::string method;  // "standard" or "ABCD"
    bool data_as_background;  // Whether to treat data samples as background processes
    double luminosity;
    std::string output_json;
    std::string output_dir;
    
    std::vector<std::string> backgrounds;
    std::vector<std::string> signals;
    std::vector<std::string> data;  // Data samples to use
    std::vector<std::string> signal_points;  // Optional: specific signal points to use
    
    std::vector<BinConfig> bins;
    std::vector<SystematicConfig> systematics;
    
    // ABCD-specific configuration
    ABCDConfig abcd;
    std::vector<SystematicConfig> experimental_systematics;
    std::vector<SystematicConfig> abcd_systematics;
    std::vector<SystematicConfig> precision_systematics;
  
    // Runtime options
    int verbosity;
    bool parallel;
    bool dry_run;
};

class ConfigParser {
public:
    ConfigParser();
    ~ConfigParser();
    
    bool LoadConfig(const std::string& config_file);
    const AnalysisConfig& GetConfig() const { return config_; }
    
    // Utility methods
    std::string GetCombinedCuts(const std::string& bin_name) const;
    void PrintConfig() const;
    bool ValidateConfig() const;
    bool ValidateABCDConfig() const;
    void ParseSystematics(const SimpleYAMLParser& parser);
    void ParseSystematicBlock(const SimpleYAMLParser& parser,
                              const std::string& prefix,
                              std::vector<SystematicConfig>& target);

    void ParseSystematicCategory(const SimpleYAMLParser& parser, const std::string& category_prefix, std::vector<SystematicConfig>& systematics);
    void ParseSystematicCategoryNested(const SimpleYAMLParser& parser, const std::string& category_prefix, std::vector<SystematicConfig>& systematics);
    void ParseSystematicsFromCombinedList(const SimpleYAMLParser& parser);
    void CreateSystematicsFromNames(const SimpleYAMLParser& parser, const std::vector<std::string>& names, const std::string& category, std::vector<SystematicConfig>& systematics);
    
private:
    AnalysisConfig config_;
    bool LoadYAML(const std::string& config_file);
    void SetDefaults();
    void GenerateABCDBinsFromAxes();  // Generate ABCD bins from explicit axis definitions
};

#endif
