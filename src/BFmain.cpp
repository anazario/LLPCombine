
#include "JSONFactory.h"
#include "BuildFit.h"
#include "ConfigParser.h"
#include <vector>
#include <string>
#include <iostream>
#include <filesystem> // Required for std::filesystem
#include <cstdlib>    // Required for std::system
namespace fs = std::filesystem;

void PrintHelp(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS] JSON_FILE [CONFIG_FILE]\n\n";
    std::cout << "LLPCombine BuildFit (BF) - Generate CMS Combine datacards from JSON\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  JSON_FILE               Input JSON file from BFI step\n";
    std::cout << "  CONFIG_FILE             Original YAML config file (required for ABCD method)\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help              Show this help message and exit\n";
    std::cout << "  -o, --output-dir DIR    Output directory for datacards (default: datacards)\n";
    std::cout << "  -v, --verbose           Enable verbose output\n";
    std::cout << "  -r, --run-combine       Run macro/launchCombine.sh after generating datacards\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " analysis_results.json\n";
    std::cout << "  " << program_name << " --output-dir my_datacards results.json\n";
    std::cout << "  " << program_name << " -o datacards_run2 -v ./json/comprehensive_v36.json\n";
    std::cout << "  " << program_name << " -r -v results.json config/abcd_example.yaml  # ABCD analysis\n\n";
}

// Function to detect ABCD method from JSON metadata
bool IsABCDMethod(const std::string& json_file) {
    try {
        JSONFactory j(json_file);
        // Check if JSON contains ABCD-specific metadata or bin names
        // This is a simple heuristic - we could make this more robust
        BuildFit bf;
        std::vector<std::string> bins = bf.GetBinSet(&j);
        
        // Look for typical ABCD region patterns
        int region_count = 0;
        for (const auto& bin : bins) {
            if (bin.find("region_") != std::string::npos || 
                bin.find("Region") != std::string::npos ||
                bin.find("_A") != std::string::npos ||
                bin.find("_B") != std::string::npos ||
                bin.find("_C") != std::string::npos ||
                bin.find("_D") != std::string::npos) {
                region_count++;
            }
        }
        
        // If we have 4 regions that look like ABCD, assume it's ABCD method
        return (region_count >= 4);
    } catch (const std::exception& e) {
        return false;  // If JSON parsing fails, assume standard method
    }
}

int main(int argc, char* argv[]) {
    // Default values
    std::string input_json = "";
    std::string config_file = "";
    std::string datacard_dir = "datacards";
    bool verbose = false;
    bool help = false;
    bool run_combine = false;
    
    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            help = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-r" || arg == "--run-combine") {
            run_combine = true;
        } else if (arg == "-o" || arg == "--output-dir") {
            if (i + 1 < argc) {
                datacard_dir = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                return 1;
            }
        } else if (arg.front() == '-') {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            PrintHelp(argv[0]);
            return 1;
        } else {
            // Positional arguments - JSON file and optional config file
            if (input_json.empty()) {
                input_json = arg;
            } else if (config_file.empty()) {
                config_file = arg;
            } else {
                std::cerr << "Error: Too many positional arguments. Expected: JSON_FILE [CONFIG_FILE]" << std::endl;
                return 1;
            }
        }
    }
    
    // Handle help
    if (help) {
        PrintHelp(argv[0]);
        return 0;
    }
    
    // Validate required arguments
    if (input_json.empty()) {
        std::cerr << "Error: JSON input file required" << std::endl;
        PrintHelp(argv[0]);
        return 1;
    }
    
    // Check if input file exists
    if (!std::filesystem::exists(input_json)) {
        std::cerr << "Error: Input JSON file not found: " << input_json << std::endl;
        return 1;
    }
    
    // Detect analysis method
    bool is_abcd = IsABCDMethod(input_json);
    
    if (verbose) {
        std::cout << "=== LLPCombine BuildFit (BF) ===" << std::endl;
        std::cout << "Input JSON: " << input_json << std::endl;
        std::cout << "Config file: " << (config_file.empty() ? "not provided" : config_file) << std::endl;
        std::cout << "Analysis method: " << (is_abcd ? "ABCD" : "Standard") << std::endl;
        std::cout << "Output directory: " << datacard_dir << std::endl;
        std::cout << "Run Combine: " << (run_combine ? "yes" : "no") << std::endl;
        std::cout << std::endl;
    }
    
    // Validate ABCD requirements
    if (is_abcd && config_file.empty()) {
        std::cerr << "Error: ABCD method detected but no config file provided." << std::endl;
        std::cerr << "ABCD analysis requires the original YAML config file for systematic uncertainties." << std::endl;
        std::cerr << "Usage: " << argv[0] << " " << input_json << " CONFIG_FILE.yaml" << std::endl;
        return 1;
    }
    
    if (is_abcd && !std::filesystem::exists(config_file)) {
        std::cerr << "Error: Config file not found: " << config_file << std::endl;
        return 1;
    }
    
    // Load config if ABCD method
    ConfigParser* configParser = nullptr;
    AnalysisConfig analysis_config;
    if (is_abcd) {
        configParser = new ConfigParser();
        if (!configParser->LoadConfig(config_file)) {
            std::cerr << "Error: Failed to load config file: " << config_file << std::endl;
            return 1;
        }
        analysis_config = configParser->GetConfig();
        
        if (verbose) {
            std::cout << "Loaded ABCD configuration:" << std::endl;
            std::cout << "  Predicted region: " << analysis_config.abcd.predicted_region << std::endl;
            std::cout << "  Formula: " << analysis_config.abcd.formula << std::endl;
            std::cout << "  Generate datacards: " << (analysis_config.abcd.generate_datacards ? "yes" : "no") << std::endl;
            std::cout << std::endl;
        }
    }

	// Load JSON and get signal processes
	JSONFactory* j = new JSONFactory(input_json);
	std::vector<std::string> signals = j->GetSigProcs();
	
	if (verbose) {
		std::cout << "Found " << signals.size() << " signal processes:" << std::endl;
		for (const auto& signal : signals) {
			std::cout << "  " << signal << std::endl;
		}
		std::cout << std::endl;
	}
	
	// Remove existing datacard directory and recreate
	if (verbose) {
		std::cout << "Preparing output directory: " << datacard_dir << std::endl;
	}
	fs::path dir_path = datacard_dir;
	fs::remove_all(dir_path);
	
	// Generate datacards for each signal process
	for (size_t i = 0; i < signals.size(); i++) {
		if (verbose) {
			std::cout << "Processing signal " << (i+1) << "/" << signals.size() << ": " << signals[i] << std::endl;
		} else {
			std::cout << "building obs rates" << std::endl;
			std::cout << "Getting process list" << std::endl;
			std::cout << "Parse Signal point" << std::endl;
			std::cout << "Build cb objects" << std::endl;
		}
		
		BuildFit* BF = new BuildFit();
		std::filesystem::create_directories(datacard_dir + "/" + signals[i]);
		
		if (is_abcd) {
			// Use ABCD method
			BF->BuildABCDFit(j, signals[i], datacard_dir, analysis_config);
		} else {
			// Use standard method
			BF->BuildAsimovFit(j, signals[i], datacard_dir);
		}
		
		delete BF;
	}
	
	if (verbose) {
		std::cout << "\nDatacard generation completed!" << std::endl;
		std::cout << "Output directory: " << datacard_dir << std::endl;
	}
	
	// Run Combine analysis if requested
	if (run_combine) {
		if (verbose) {
			std::cout << "\nRunning Combine analysis..." << std::endl;
		}
		
		// Check if macro/launchCombine.sh exists
		std::string script_path = "macro/launchCombine.sh";
		if (!std::filesystem::exists(script_path)) {
			std::cerr << "Error: Combine script not found: " << script_path << std::endl;
			std::cerr << "Make sure you're running from the LLPCombine root directory" << std::endl;
			delete j;
			return 1;
		}
		
		// Construct the command to run the script with the datacard directory
		std::string command = "bash " + script_path + " " + datacard_dir;
		
		if (verbose) {
			std::cout << "Executing: " << command << std::endl;
		}
		
		// Execute the script
		int result = std::system(command.c_str());
		
		if (result == 0) {
			if (verbose) {
				std::cout << "Combine analysis completed successfully!" << std::endl;
			}
		} else {
			std::cerr << "Warning: Combine analysis script returned non-zero exit code: " << result << std::endl;
		}
	}
	
	// Cleanup
	delete j;
	if (configParser) {
		delete configParser;
	}
}
