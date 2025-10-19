

#include "SampleTool.h"
#include "BuildFitInput.h"
#include "JSONFactory.h"
#include "ConfigParser.h"
#include "ArgumentParser.h"
#include <iostream>
#include <sys/stat.h>
#include <errno.h>
#include <algorithm>  // for std::find
#include <cstdlib>    // for system() calls
#include <memory>     // for smart pointers

using std::string;

// Function to copy text to clipboard (macOS/Linux compatible)
void copyToClipboard(const std::string& text) {
#ifdef __APPLE__
  std::string cmd = "echo '" + text + "' | pbcopy";
#else // Linux
  std::string cmd = "echo '" + text + "' | xclip -selection clipboard";
#endif
  int result = std::system(cmd.c_str());
  (void)result; // Suppress unused variable warning
}

// Function to process a single configuration file
int ProcessSingleConfig(const std::string& config_file, const ProgramOptions& options) {
  // Load configuration
  ConfigParser configParser;
  if (!configParser.LoadConfig(config_file)) {
    std::cerr << "Error: Failed to load configuration from: " << config_file << std::endl;
    return 1;
  }
	
  const AnalysisConfig& config = configParser.GetConfig();
	
  // Override config with command-line options if provided
  double luminosity = (options.luminosity > 0) ? options.luminosity : config.luminosity;
  int verbosity = (options.verbosity >= 0) ? options.verbosity : config.verbosity;
  std::string output_dir = options.output_dir.empty() ? config.output_dir : options.output_dir;
	
  // Print configuration if verbose
  if (verbosity > 0 && !options.batch_mode) {
    std::cout << "=== LLPCombine Analysis ===" << std::endl;
    configParser.PrintConfig();
    std::cout << "\nCommand-line overrides:" << std::endl;
    if (options.luminosity > 0) std::cout << "  Luminosity: " << luminosity << " fb^-1" << std::endl;
    if (options.verbosity >= 0) std::cout << "  Verbosity: " << verbosity << std::endl;
    if (!options.output_dir.empty()) std::cout << "  Output dir: " << output_dir << std::endl;
    std::cout << std::endl;
  } else if (options.batch_mode && verbosity > 0) {
    std::cout << "Processing: " << config.name << " -> " << config.output_json << std::endl;
  }
	
  // Dry run - just validate and exit
  if (options.dry_run || config.dry_run) {
    if (verbosity > 0) {
      std::cout << "Dry run completed - configuration " << config_file << " is valid." << std::endl;
    }
    return 0;
  }
	
  // Ensure output directory exists
  struct stat st = {0};
  if (stat(output_dir.c_str(), &st) == -1) {
    if (mkdir(output_dir.c_str(), 0755) != 0) {
      std::cerr << "Error creating output directory " << output_dir << ": " << strerror(errno) << std::endl;
      return 1;
    }
  }
	
  // Initialize SampleTool with configuration
  auto ST = std::make_unique<SampleTool>();
	
  // Convert vectors to the expected stringlist format
  stringlist bkglist(config.backgrounds.begin(), config.backgrounds.end());
  stringlist siglist(config.signals.begin(), config.signals.end());
  stringlist datalist(config.data.begin(), config.data.end());
  
  // Load samples based on data_as_background mode
  if (config.data_as_background) {
    if (verbosity > 0) {
      std::cout << "Data-as-background mode: treating data samples as backgrounds" << std::endl;
    }
    // In data-as-background mode, add data samples to background list
    for (const auto& data_sample : config.data) {
      bkglist.push_back(data_sample);
    }
    ST->LoadBkgs(bkglist);
    ST->LoadSigs(siglist);
    // Don't load data as observations - will use Asimov or no observations
  } else {
    if (verbosity > 0) {
      std::cout << "Standard mode: using data as observations" << std::endl;
    }
    // Standard mode: load data as observations
    ST->LoadBkgs(bkglist);
    ST->LoadSigs(siglist);
    ST->LoadData(datalist);
  }
	
  // Filter signal points if specified in configuration
  if (!config.signal_points.empty()) {
    if (verbosity > 0) {
      std::cout << "Filtering to " << config.signal_points.size() << " specific signal points..." << std::endl;
    }
		
    // Create a new filtered signal dictionary with only the specified signal points
    std::map<std::string, stringlist> filtered_sig_dict;
    stringlist filtered_signal_keys;
		
    for (const auto& point : config.signal_points) {
      bool found = false;
      // Look through all signal types and their files
      for (const auto& sig_pair : ST->SigDict) {
	for (const auto& file_path : sig_pair.second) {
	  // Check if this file contains the desired signal point
	  // Signal points are encoded in filenames like: mGl-2000_mN2-400_mN1-200_ct0p1
	  // and we want to match: gogoZ_2000_400_200_10
					
	  // Extract the parts of the signal point name (e.g. gogoZ_2000_400_200_10)
	  size_t first_underscore = point.find("_");
	  if (first_underscore == std::string::npos) continue;
					
	  size_t second_underscore = point.find("_", first_underscore + 1);
	  if (second_underscore == std::string::npos) continue;
					
	  size_t third_underscore = point.find("_", second_underscore + 1);
	  if (third_underscore == std::string::npos) continue;
					
	  size_t fourth_underscore = point.find("_", third_underscore + 1);
	  if (fourth_underscore == std::string::npos) continue;
					
	  std::string mgl = point.substr(first_underscore + 1, second_underscore - first_underscore - 1);
	  std::string mn2 = point.substr(second_underscore + 1, third_underscore - second_underscore - 1);
	  std::string mn1 = point.substr(third_underscore + 1, fourth_underscore - third_underscore - 1);
	  std::string ct = point.substr(fourth_underscore + 1);
					
	  // Convert ct lifetime: 10 -> 0p1, 001 -> 0p001, 30 -> 0p3, etc.
	  std::string ct_formatted;
	  if (ct == "10") ct_formatted = "0p1";
	  else if (ct == "001") ct_formatted = "0p001";
	  else if (ct == "30") ct_formatted = "0p3";
	  else ct_formatted = "0p" + ct;
					
	  // Check if the filename matches all components
	  if (file_path.find("mGl-" + mgl) != std::string::npos &&
	      file_path.find("mN2-" + mn2) != std::string::npos &&
	      file_path.find("mN1-" + mn1) != std::string::npos &&
	      file_path.find("ct" + ct_formatted) != std::string::npos) {
						
	    filtered_sig_dict[point].push_back(file_path);
	    filtered_signal_keys.push_back(point);
	    found = true;
	    if (verbosity > 1) {
	      std::cout << "  Including signal point: " << point << " -> " << file_path << std::endl;
	    }
	    break;
	  }
	}
	if (found) break;
      }
			
      if (!found) {
	std::cerr << "Warning: Signal point '" << point << "' not found in loaded signal files" << std::endl;
      }
    }
		
    if (filtered_sig_dict.empty()) {
      std::cerr << "Error: None of the specified signal points were found in the loaded signal files" << std::endl;
      return 1;
    }
		
    // Replace the signal dictionary and keys with the filtered ones
    ST->SigDict = filtered_sig_dict;
    ST->SignalKeys = filtered_signal_keys;
		
    if (verbosity > 0) {
      std::cout << "Using " << filtered_signal_keys.size() << " filtered signal points" << std::endl;
    }
  }
	
  if (verbosity > 1 && !options.batch_mode) {
    ST->PrintDict(ST->BkgDict);
    ST->PrintDict(ST->DataDict);
    ST->PrintDict(ST->SigDict);
    ST->PrintKeys(ST->SignalKeys);
  }
	
  // Initialize BuildFitInput
  auto BFI = std::make_unique<BuildFitInput>();
  
  // Load data conditionally based on mode
  if (!config.data_as_background) {
    BFI->LoadData_byMap(ST->DataDict, luminosity);
  }
  
  BFI->LoadBkg_byMap(ST->BkgDict, luminosity);
  BFI->LoadSig_byMap(ST->SigDict, luminosity);
	
  // Create analysis bins from configuration
  for (const auto& bin : config.bins) {
    std::string combined_cuts = configParser.GetCombinedCuts(bin.name);
		
    if (verbosity > 1 && !options.batch_mode) {
      std::cout << "Creating bin: " << bin.name << std::endl;
      std::cout << "  Description: " << bin.description << std::endl;
      std::cout << "  Cuts: " << combined_cuts << std::endl;
    }
		
    BFI->FilterRegions(bin.name, combined_cuts);
    BFI->CreateBin(bin.name);
  }
	
  // Book operations
  countmap countResults = BFI->CountRegions(BFI->bkg_filtered_dataframes);
  countmap countResults_S = BFI->CountRegions(BFI->sig_filtered_dataframes);
  
  // Handle data observations conditionally
  countmap countResults_obs;
  summap sumResults_obs;
  
  if (!config.data_as_background) {
    // Standard mode: use data observations
    countResults_obs = BFI->CountRegions(BFI->data_filtered_dataframes);
    sumResults_obs = BFI->SumRegions("evtwt", BFI->data_filtered_dataframes);
  } else {
    // Data-as-background mode: use Asimov data (sum of backgrounds)
    countResults_obs = countResults;  // Use background counts as observations
    sumResults_obs = BFI->SumRegions("evtwt", BFI->bkg_filtered_dataframes);
  }

  summap sumResults = BFI->SumRegions("evtwt", BFI->bkg_filtered_dataframes);
  summap sumResults_S = BFI->SumRegions("evtwt", BFI->sig_filtered_dataframes);
	
  // Initiate action
  BFI->ReportRegions(verbosity > 2 ? 1 : 0);
	
  // Compute errors and report bins
  errormap errorResults = BFI->ComputeStatError(countResults, BFI->bkg_evtwt);
  errormap errorResults_S = BFI->ComputeStatError(countResults_S, BFI->sig_evtwt);
  // TODO - won't use weighted events in fit for data
  errormap errorResults_obs = BFI->ComputeStatError(countResults_obs, BFI->data_evtwt);
	
  // Aggregate maps into more easily useable classes
  BFI->ConstructBkgBinObjects(countResults, sumResults, errorResults);
  BFI->AddSigToBinObjects(countResults_S, sumResults_S, errorResults_S, BFI->analysisbins);
  // Only write data to json if we have observations
  if(datalist.size() > 0 || config.data_as_background) BFI->AddDataToBinObjects(countResults_obs, sumResults_obs, errorResults_obs, BFI->analysisbins);
	
  if (verbosity > 0 && !options.batch_mode) {
    BFI->PrintBins(verbosity > 1 ? 1 : 0);
  }
	
  // Write output JSON
  std::string output_path = output_dir;
  if (output_path.back() != '/') {
    output_path += "/";
  }
  output_path += config.output_json;
	
  auto json = std::make_unique<JSONFactory>(BFI->analysisbins);
  json->WriteJSON(output_path);
	
  if (verbosity > 0) {
    if (options.batch_mode) {
      std::cout << "  -> " << output_path << std::endl;
    } else {
      std::cout << "Results written to: " << output_path << std::endl;
    }
  }
	
  // Generate BF.x command and copy to clipboard
  std::string bf_command = "./BF.x " + output_path;
  if (config.method == "ABCD") {
    bf_command += " " + config_file;
    if (verbosity > 1) {
      std::cout << "\nABCD method detected - JSON created with ABCD configuration." << std::endl;
      std::cout << "To generate ABCD datacards, use: " << bf_command << std::endl;
    }
  } else if (config.method == "standard") {
    if (verbosity > 1) {
      std::cout << "\nUsing standard analysis method" << std::endl;
      std::cout << "To generate datacards, use: " << bf_command << std::endl;
    }
  }
	
  // Copy BF.x command to clipboard for easy access
  copyToClipboard(bf_command);
  if (verbosity > 0) {
    std::cout << "📋 BF.x command copied to clipboard: " << bf_command << std::endl;
  }
	
  // Smart pointers automatically clean up
  return 0;
}

int main(int argc, char* argv[]) {
  // Parse command-line arguments
  ArgumentParser argParser;
  ProgramOptions options;
	
  try {
    options = argParser.Parse(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << "Error parsing arguments: " << e.what() << std::endl;
    argParser.PrintHelp(argv[0]);
    return 1;
  }
	
  // Handle help and version requests
  if (options.help) {
    argParser.PrintHelp(argv[0]);
    return 0;
  }
	
  if (options.version) {
    argParser.PrintVersion();
    return 0;
  }
	
  // Require configuration file(s)
  if (options.config_files.empty()) {
    std::cerr << "Error: At least one configuration file required. Use -c/--config or provide as positional argument." << std::endl;
    argParser.PrintHelp(argv[0]);
    return 1;
  }
	
  // Show batch mode status
  if (options.batch_mode && options.verbosity >= 0) {
    std::cout << "=== LLPCombine Batch Mode ===" << std::endl;
    std::cout << "Processing " << options.config_files.size() << " configuration files:" << std::endl;
    for (size_t i = 0; i < options.config_files.size(); ++i) {
      std::cout << "  " << (i+1) << ". " << options.config_files[i] << std::endl;
    }
    std::cout << std::endl;
  }
	
  // Process all configuration files
  int total_processed = 0;
  int total_failed = 0;
	
  for (const auto& config_file : options.config_files) {
    int result = ProcessSingleConfig(config_file, options);
    if (result == 0) {
      total_processed++;
    } else {
      total_failed++;
      if (!options.batch_mode) {
	// In single-file mode, exit immediately on failure
	return result;
      }
    }
  }
	
  // Print batch summary
  if (options.batch_mode) {
    std::cout << "\n=== Batch Processing Summary ===" << std::endl;
    std::cout << "Successfully processed: " << total_processed << "/" << options.config_files.size() << " files" << std::endl;
    if (total_failed > 0) {
      std::cout << "Failed: " << total_failed << " files" << std::endl;
    }
  }
	
  return (total_failed > 0) ? 1 : 0;
}
