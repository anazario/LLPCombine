#include "BuildFit.h"

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


      
	cb.PrintAll();
	cb.WriteDatacard(datacard_dir+"/"+signalPoint+"/"+signalPoint+".txt");

}

void BuildFit::BuildABCDFit(JSONFactory* j, std::string signalPoint, std::string datacard_dir, const AnalysisConfig& config) {
    // Validate ABCD configuration
    if (!config.abcd.IsValid()) {
        std::cerr << "ABCD configuration validation failed:" << std::endl;
        std::cerr << "  Regions count: " << config.abcd.regions.size() << " (expected 4)" << std::endl;
        std::cerr << "  Predicted region: '" << config.abcd.predicted_region << "'" << std::endl;
        throw std::runtime_error("ABCD configuration is invalid");
    }
    
    // Get predicted region and control regions
    std::string predicted_bin = config.abcd.GetPredictedBin();
    std::vector<std::string> control_bins = config.abcd.GetControlBins();
    
    if (control_bins.size() != 3) {
        throw std::runtime_error("ABCD method requires exactly 3 control regions");
    }
    
    std::cout << "=== ABCD Configuration ===" << std::endl;
    std::cout << "Predicted region: " << predicted_bin << std::endl;
    std::cout << "Control regions: " << control_bins[0] << ", " << control_bins[1] << ", " << control_bins[2] << std::endl;
    
    // Standard CombineHarvester setup
    ch::Categories cats = BuildCats(j);
    std::map<std::string, float> obs_rates = BuildAsimovData(j);
    std::vector<std::string> bkgprocs = GetBkgProcs(j);
    std::vector<std::string> signalDetails = ExtractSignalDetails(signalPoint);
    
    // Calculate ABCD prediction: predicted = control1 * control2 / control3
    double predicted_rate = obs_rates[control_bins[0]] * obs_rates[control_bins[1]] / obs_rates[control_bins[2]];
    
    std::cout << "ABCD Calculation:" << std::endl;
    std::cout << "  " << predicted_bin << " = " << control_bins[0] << " * " << control_bins[1] << " / " << control_bins[2] << std::endl;
    std::cout << "  " << predicted_bin << " = " << obs_rates[control_bins[0]] << " * " << obs_rates[control_bins[1]] << " / " << obs_rates[control_bins[2]] << " = " << predicted_rate << std::endl;
    
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
    std::cout << "DEBUG: About to apply systematics..." << std::endl;
    std::cout << "  Experimental: " << config.experimental_systematics.size() << std::endl;
    std::cout << "  ABCD: " << config.abcd_systematics.size() << std::endl;
    std::cout << "  Precision: " << config.precision_systematics.size() << std::endl;
    
    ApplySystematics(config.experimental_systematics, config.abcd);
    ApplySystematics(config.abcd_systematics, config.abcd);  
    ApplySystematics(config.precision_systematics, config.abcd);
    
    cb.PrintAll();
    cb.WriteDatacard(datacard_dir + "/" + signalPoint + "/" + signalPoint + ".txt");
}

void BuildFit::ApplySystematics(const std::vector<SystematicConfig>& systematics, const ABCDConfig& abcd) {
    std::cout << "DEBUG ApplySystematics: Processing " << systematics.size() << " systematics" << std::endl;
    
    std::string predicted_bin = abcd.GetPredictedBin();
    std::vector<std::string> control_bins = abcd.GetControlBins();
    
    std::cout << "DEBUG: predicted_bin = " << predicted_bin << std::endl;
    std::cout << "DEBUG: control_bins = ";
    for (const auto& bin : control_bins) std::cout << bin << " ";
    std::cout << std::endl;
    
    for (const auto& syst : systematics) {
        std::cout << "DEBUG: Processing systematic: " << syst.name << " (type=" << syst.type << ")" << std::endl;
        
        // Resolve auto mappings
        std::vector<std::string> resolved_bins = syst.ResolveBins(abcd);
        
        std::cout << "DEBUG: Original bins: ";
        for (const auto& bin : syst.bins) std::cout << bin << " ";
        std::cout << std::endl;
        
        std::cout << "DEBUG: Resolved bins: ";
        for (const auto& bin : resolved_bins) std::cout << bin << " ";
        std::cout << std::endl;
        
        if (resolved_bins.empty()) {
            std::cout << "DEBUG: No resolved bins, skipping systematic" << std::endl;
            continue;
        }
        
        if (syst.type == "lnN") {
            // Create lnN systematics with format: lnN_[BIN_NAME]
            for (const auto& bin : resolved_bins) {
                std::cout << "DEBUG: Adding lnN systematic lnN_" << bin << " with value " << syst.value << std::endl;
                cb.cp().bin({bin}).AddSyst(cb, "lnN_" + bin, "lnN", SystMap<>::init(syst.value));
            }
            
        } else if (syst.type == "rateParam") {
            if (syst.name.find("closure_constraint") != std::string::npos) {
                // This is the ABCD formula constraint for predicted region
                std::cout << "DEBUG: Adding ABCD formula constraint for " << predicted_bin << std::endl;
                cb.cp().bin({predicted_bin}).AddSyst(cb, "scale_" + predicted_bin, "rateParam", SystMapFunc<>::init
                    ("(@0*@1/@2)", "scale_" + control_bins[0] + ",scale_" + control_bins[1] + ",scale_" + control_bins[2])
                );
            } else {
                // These are individual scale parameters for control regions
                for (const auto& bin : resolved_bins) {
                    std::cout << "DEBUG: Adding rateParam scale_" << bin << " with value " << syst.value << std::endl;
                    cb.cp().bin({bin}).AddSyst(cb, "scale_" + bin, "rateParam", SystMap<>::init(syst.value));
                }
            }
        }
    }
    
    std::cout << "DEBUG: ApplySystematics complete" << std::endl;
}

std::string BuildFit::JoinStrings(const std::vector<std::string>& strings, const std::string& delimiter) {
    if (strings.empty()) return "";
    
    std::string result = strings[0];
    for (size_t i = 1; i < strings.size(); ++i) {
        result += delimiter + strings[i];
    }
    return result;
}



