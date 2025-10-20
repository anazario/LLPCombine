#include "BuildFit.h"
#include <set>

ch::Categories BuildFit::BuildCats(JSONFactory* j){
	ch::Categories cats{};
	int binNum=0;
	for (json::iterator it = j->j.begin(); it != j->j.end(); ++it) {
  		//std::cout << it.key() <<"\n";
		cats.push_back( {binNum, it.key()} );
		binNum++;
	}
	return cats;
}
std::map<std::string, float> BuildFit::BuildAsimovData(JSONFactory* j){

	std::map<std::string, float> obs_rates{};
	
	//outer loop bin iterator
	for (json::iterator it = j->j.begin(); it != j->j.end(); ++it){
		//inner loop process iterator
		std::string binname = it.key();
		float totalBkg = 0;
		for (json::iterator it2 = it.value().begin(); it2 != it.value().end(); ++it2){
			//std::cout<< it2.key()<<"\n";
			
			if( BFTool::ContainsAnySubstring( it2.key(), sigkeys)){
				continue;
			}
			else{
				//get the wnevents, index 1 of array
				json json_array = it2.value();
				//std::cout<< it2.key()<<" "<<json_array[1].get<float>()<<" "<<"\n";
				totalBkg+= json_array[1].get<float>();
			}
		}
		obs_rates[binname] = float(int(totalBkg));
		//std::cout<<"adding totalbkg: "<<binname<<" "<< float(int(totalBkg))<<"\n";
	}
	return obs_rates;	
}
std::vector<std::string> BuildFit::GetBkgProcs(JSONFactory* j){
	std::vector<std::string> bkgprocs{};

	for (json::iterator it = j->j.begin(); it != j->j.end(); ++it){
                //inner loop process iterator
                std::string binname = it.key();
                for (json::iterator it2 = it.value().begin(); it2 != it.value().end(); ++it2){
                //      std::cout<< it2.key()<<"\n";
                        if( BFTool::ContainsAnySubstring( it2.key(), sigkeys)){
                                continue;
                        }
                        else{
				bkgprocs.push_back(it2.key());
			}
		}
	}
	return bkgprocs;
}

std::vector<std::string> BuildFit::GetDataProcs(JSONFactory* j){
	std::vector<std::string> dataprocs{};
	
	for (json::iterator it = j->j.begin(); it != j->j.end(); ++it){
		//inner loop process iterator
		std::string binname = it.key();
		for (json::iterator it2 = it.value().begin(); it2 != it.value().end(); ++it2){
			if(BFTool::ContainsAnySubstring(it2.key(), datakeys)){
				dataprocs.push_back(it2.key());
			}
		}
	}
	//make this set unique 
	std::set<std::string> my_data_set(dataprocs.begin(), dataprocs.end());
	std::vector<std::string> dataprocsunique(my_data_set.begin(), my_data_set.end());
	
	return dataprocsunique;
}

std::map<std::string, float> BuildFit::LoadDataProcesses(JSONFactory* j, std::vector<std::string> dataKeys){
	std::map<std::string, float> obs_rates = {};
	float obs_rate = 0.;
	for (json::iterator it = j->j.begin(); it != j->j.end(); ++it){
		//inner loop process iterator
		std::string binname = it.key();
		//assign yield to obs bin map
		obs_rate = 0.;  // Reset for each bin
		for(int i = 0; i < dataKeys.size(); i++){
			if(j->j[binname].contains(dataKeys[i])){
				json json_array = j->j[binname][dataKeys[i]];
				obs_rate += json_array[1].get<float>();
			}
		}
		obs_rates[binname] = obs_rate;
	}
	return obs_rates;
}
std::vector<std::string> BuildFit::ExtractSignalDetails( std::string signalPoint){

	std::vector<std::string> splitPoint = BFTool::SplitString( signalPoint, "_");
	std::string analysis = splitPoint[0];
	std::string channel = "gamma";	
	//pad for mass?
	std::string mass = "";
	for( long unsigned int i=1; i< splitPoint.size(); i++){
		mass += splitPoint[i];
	}

	std::vector<std::string> signalDetails = {analysis, channel, mass};
	return signalDetails;

}
std::vector<std::string> BuildFit::GetBinSet( JSONFactory* j){
	std::vector<std::string> bins{};
        for (json::iterator it = j->j.begin(); it != j->j.end(); ++it) {
                //std::cout << it.key() <<"\n";
                bins.push_back(  it.key() );
        }
        return bins;

}

void BuildFit::BuildAsimovFit(JSONFactory* j, std::string signalPoint, std::string datacard_dir){
	ch::Categories cats = BuildCats(j);
	std::cout<<"building obs rates \n";
	std::map<std::string, float> obs_rates = BuildAsimovData(j);
	std::cout<<"Getting process list\n";
	std::vector<std::string> bkgprocs = GetBkgProcs(j);
	std::cout<<"Parse Signal point\n";
	std::vector<std::string> signalDetails = ExtractSignalDetails( signalPoint);
	std::cout<<"Build cb objects\n";
	//cb.SetVerbosity(3);
	cb.AddObservations({"*"}, {signalDetails[0]}, {"13.6TeV"}, {signalDetails[1]}, cats);
	cb.AddProcesses(   {"*"}, {signalDetails[0]}, {"13.6TeV"}, {signalDetails[1]}, bkgprocs, cats, false);
	cb.AddProcesses(   {signalDetails[2]}, {signalDetails[0]}, {"13.6Tev"}, {signalDetails[1]}, {signalPoint}, cats, true);
	cb.ForEachObs([&](ch::Observation *x){
		x->set_rate(obs_rates[x->bin()]);
	});
	cb.ForEachProc([&j](ch::Process *x) {
	    //std::cout<<x->bin()<<" "<<x->process()<<"\n";
	    json json_array = j->j[x->bin()][x->process()];
	    x->set_rate(json_array[1].get<float>());
	});

	std::vector<std::string> binset = GetBinSet(j);
	cb.cp().bin(binset).AddSyst(cb, "DummySys", "lnN", SystMap<>::init(1.10));


      
	//cb.PrintAll();  // Commented out to reduce verbose output
	cb.WriteDatacard(datacard_dir+"/"+signalPoint+"/"+signalPoint+".txt");

}

void BuildFit::BuildABCDFit(JSONFactory* j, std::string signalPoint, std::string datacard_dir, const AnalysisConfig& config) {
    // Validate ABCD configuration
    if (!config.abcd.IsValid()) {
        std::cerr << "ABCD configuration validation failed:" << std::endl;
        std::cerr << "  Regions count: " << config.abcd.regions.size() << " (expected 4)" << std::endl;
        throw std::runtime_error("ABCD configuration is invalid");
    }
    
    // Validate predicted region is set (either from config or command line)
    if (config.abcd.predicted_region.empty()) {
        std::cerr << "Error: No predicted region specified. This should be set by BF.x" << std::endl;
        throw std::runtime_error("No predicted region specified");
    }
    
    // Get ABCD regions in proper order from config
    std::string region_A = config.abcd.regions.at("region_A");
    std::string region_B = config.abcd.regions.at("region_B"); 
    std::string region_C = config.abcd.regions.at("region_C");
    std::string region_D = config.abcd.regions.at("region_D");
    
    std::string predicted_bin = config.abcd.GetPredictedBin();
    
    std::cout << "=== ABCD Configuration ===" << std::endl;
    std::cout << "Region A: " << region_A << std::endl;
    std::cout << "Region B: " << region_B << std::endl;
    std::cout << "Region C: " << region_C << std::endl;
    std::cout << "Region D: " << region_D << std::endl;
    std::cout << "Predicted region: " << predicted_bin << std::endl;
    
    // Standard CombineHarvester setup
    ch::Categories cats = BuildCats(j);
    std::vector<std::string> signalDetails = ExtractSignalDetails(signalPoint);
    
    // Set up datakeys from config for data-as-background mode
    datakeys = config.data;
    
    std::map<std::string, float> obs_rates;
    std::vector<std::string> bkgprocs;
    
    if (config.data_as_background) {
        std::cout << "Using data-as-background mode for ABCD" << std::endl;
        // In data-as-background mode, use data samples as background processes
        obs_rates = LoadDataProcesses(j, config.data);
        bkgprocs = GetDataProcs(j);
    } else {
        std::cout << "Using standard MC-based ABCD mode" << std::endl;
        // Standard mode: use MC backgrounds and Asimov data
        obs_rates = BuildAsimovData(j);
        bkgprocs = GetBkgProcs(j);
    }
    
    // Get the control regions (the 3 regions that are NOT being predicted)
    std::vector<std::string> all_regions = {region_A, region_B, region_C, region_D};
    std::vector<std::string> control_regions;
    
    for (const auto& region : all_regions) {
        if (region != predicted_bin) {
            control_regions.push_back(region);
        }
    }
    
    // Calculate ABCD prediction using correct formula based on which region is predicted
    double predicted_rate;
    std::string formula_str;
    
    if (predicted_bin == region_A) {
        // A = B * C / D
        predicted_rate = obs_rates[region_B] * obs_rates[region_C] / obs_rates[region_D];
        formula_str = region_B + " * " + region_C + " / " + region_D;
    } else if (predicted_bin == region_B) {
        // B = A * D / C  
        predicted_rate = obs_rates[region_A] * obs_rates[region_D] / obs_rates[region_C];
        formula_str = region_A + " * " + region_D + " / " + region_C;
    } else if (predicted_bin == region_C) {
        // C = A * D / B
        predicted_rate = obs_rates[region_A] * obs_rates[region_D] / obs_rates[region_B];
        formula_str = region_A + " * " + region_D + " / " + region_B;
    } else if (predicted_bin == region_D) {
        // D = B * C / A
        predicted_rate = obs_rates[region_B] * obs_rates[region_C] / obs_rates[region_A];
        formula_str = region_B + " * " + region_C + " / " + region_A;
    } else {
        throw std::runtime_error("Invalid predicted region: " + predicted_bin);
    }
    
    std::cout << "ABCD Calculation:" << std::endl;
    std::cout << "  " << predicted_bin << " = " << formula_str << std::endl;
    std::cout << "  " << predicted_bin << " = " << predicted_rate << std::endl;
    
    // Add observations and processes
    cb.AddObservations({"*"}, {signalDetails[0]}, {"13.6TeV"}, {signalDetails[1]}, cats);
    cb.AddProcesses({"*"}, {signalDetails[0]}, {"13.6TeV"}, {signalDetails[1]}, bkgprocs, cats, false);
    cb.AddProcesses({signalDetails[2]}, {signalDetails[0]}, {"13.6TeV"}, {signalDetails[1]}, {signalPoint}, cats, true);
    
    // Set rates - use ABCD prediction for predicted region
    cb.ForEachObs([&](ch::Observation *x){ 
        x->set_rate(obs_rates[x->bin()]); 
    });
    
    cb.ForEachProc([&](ch::Process *x) {
        //std::cout<<x->bin()<<" "<<x->process()<<"\n";
        if(x->bin() == predicted_bin && x->process() != signalPoint) {
            x->set_rate(predicted_rate);  // Use ABCD prediction
        } else {
            json json_array = j->j[x->bin()][x->process()];
            x->set_rate(json_array[1].get<float>());
        }
    });
    
    // Apply systematics from configuration with auto-resolution
    std::cout << "\n=== Applying Systematics ===" << std::endl;
    std::cout << "Experimental systematics: " << config.experimental_systematics.size() << std::endl;
    std::cout << "ABCD systematics: " << config.abcd_systematics.size() << std::endl;
    std::cout << "Precision systematics: " << config.precision_systematics.size() << std::endl;
    
    ApplySystematics(config.experimental_systematics, config.abcd);
    ApplySystematics(config.abcd_systematics, config.abcd);  
    ApplySystematics(config.precision_systematics, config.abcd);
    
    std::cout << "=== Systematics Applied ===" << std::endl;
    
    //cb.PrintAll();  // Commented out to reduce verbose output
    cb.WriteDatacard(datacard_dir + "/" + signalPoint + "/" + signalPoint + ".txt");
}

void BuildFit::ApplySystematics(const std::vector<SystematicConfig>& systematics, const ABCDConfig& abcd) {
    std::string predicted_bin = abcd.GetPredictedBin();
    std::vector<std::string> control_bins = abcd.GetControlBins();
    
    std::cout << "Processing " << systematics.size() << " systematics..." << std::endl;
    std::cout << "Predicted bin: " << predicted_bin << std::endl;
    
    for (const auto& syst : systematics) {
        std::cout << "  Systematic: " << syst.name << " (type: " << syst.type << ", value: " << syst.value << ")" << std::endl;
        
        // Resolve auto mappings
        std::vector<std::string> resolved_bins = syst.ResolveBins(abcd);
        
        std::cout << "    Resolved to " << resolved_bins.size() << " bins: ";
        for (const auto& bin : resolved_bins) {
            std::cout << bin << " ";
        }
        std::cout << std::endl;
        
        if (resolved_bins.empty()) {
            std::cout << "    Skipping - no resolved bins" << std::endl;
            continue;
        }
        
        if (syst.type == "lnN") {
            // Create lnN systematics with format: lnN_[BIN_NAME]
            for (const auto& bin : resolved_bins) {
                cb.cp().bin({bin}).AddSyst(cb, "lnN_" + bin, "lnN", SystMap<>::init(syst.value));
            }
            
        } else if (syst.type == "rateParam") {
            if (syst.name.find("closure_constraint") != std::string::npos) {
                // This is the ABCD formula constraint for predicted region
                // Generate correct formula based on which region is predicted
                std::string formula;
                std::string parameters;
                
                // Get ABCD regions in standard order
                std::string region_A = abcd.regions.at("region_A");
                std::string region_B = abcd.regions.at("region_B");
                std::string region_C = abcd.regions.at("region_C");
                std::string region_D = abcd.regions.at("region_D");
                
                if (predicted_bin == region_A) {
                    // A = B * C / D
                    formula = "(@0*@1/@2)";
                    parameters = "scale_" + region_B + ",scale_" + region_C + ",scale_" + region_D;
                } else if (predicted_bin == region_B) {
                    // B = A * D / C
                    formula = "(@0*@1/@2)";
                    parameters = "scale_" + region_A + ",scale_" + region_D + ",scale_" + region_C;
                } else if (predicted_bin == region_C) {
                    // C = A * D / B
                    formula = "(@0*@1/@2)";
                    parameters = "scale_" + region_A + ",scale_" + region_D + ",scale_" + region_B;
                } else if (predicted_bin == region_D) {
                    // D = B * C / A
                    formula = "(@0*@1/@2)";
                    parameters = "scale_" + region_B + ",scale_" + region_C + ",scale_" + region_A;
                }
                
                cb.cp().bin({predicted_bin}).AddSyst(cb, "scale_$BIN", "rateParam", SystMapFunc<>::init
                    (formula, parameters)
                );
            } else {
                // These are individual scale parameters for control regions
                for (const auto& bin : resolved_bins) {
                    cb.cp().bin({bin}).AddSyst(cb, "scale_" + bin, "rateParam", SystMap<>::init(syst.value));
                }
            }
        }
    }
}

std::string BuildFit::JoinStrings(const std::vector<std::string>& strings, const std::string& delimiter) {
    if (strings.empty()) return "";
    
    std::string result = strings[0];
    for (size_t i = 1; i < strings.size(); ++i) {
        result += delimiter + strings[i];
    }
    return result;
}



