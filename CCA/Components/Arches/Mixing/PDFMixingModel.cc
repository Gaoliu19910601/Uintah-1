//----- PDFMixingModel.cc --------------------------------------------------

#include <Packages/Uintah/CCA/Components/Arches/Mixing/PDFMixingModel.h>
#include <Packages/Uintah/CCA/Components/Arches/Mixing/Integrator.h>
#include <Packages/Uintah/CCA/Components/Arches/Mixing/KDTree.h>
#include <Packages/Uintah/CCA/Components/Arches/Mixing/ReactionModel.h>
#include <Packages/Uintah/CCA/Components/Arches/Mixing/StanjanEquilibriumReactionModel.h>
#include <Packages/Uintah/CCA/Components/Arches/Mixing/ChemkinInterface.h>
#include <Packages/Uintah/CCA/Components/Arches/Mixing/MixRxnTableInfo.h>
#include <Packages/Uintah/CCA/Components/Arches/Mixing/DynamicTable.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpecP.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Packages/Uintah/Core/Exceptions/InvalidValue.h>
#include <string>
#include <iostream>
#include <math.h>
#include <Core/Math/MiscMath.h>
using namespace std;
using namespace Uintah;
using namespace SCIRun;
//****************************************************************************
// Default constructor for PDFMixingModel
//****************************************************************************
PDFMixingModel::PDFMixingModel():MixingModel(), DynamicTable()
{
}

//****************************************************************************
// Destructor
//****************************************************************************
PDFMixingModel::~PDFMixingModel()
{
}

//****************************************************************************
// Problem Setup for PDFMixingModel
//****************************************************************************
void 
PDFMixingModel::problemSetup(const ProblemSpecP& params)
{
  ProblemSpecP db = params->findBlock("PDFMixingModel");
  db->require("adiabatic",d_adiabatic);
  if (!d_adiabatic)
    cout<<"PDF::problem is nonadiabatic"<<endl;
  db->require("mixstatvars",d_numMixStatVars);
  db->require("rxnvars",d_numRxnVars);
  // read and initialize reaction model with chemkin interface
  string rxnModel;
  db->require("reaction_model",rxnModel);
  // ***Is there a better way to do this???
  if (rxnModel == "EquilibriumReactionModel")
    d_rxnModel = new StanjanEquilibriumReactionModel(d_adiabatic);
  else if (rxnModel == "ILDMReactionModel")
    cout<<"yeah"<<endl<<endl;
    //d_rxnModel = new ILDMReactionModel(d_adiabatic);
  else
    throw InvalidValue("Reaction Model not supported" + rxnModel);
  d_rxnModel->problemSetup(db);
  //d_rxnModel->problemSetup(db, this);
  // number of species
  ChemkinInterface* chemInterf = d_rxnModel->getChemkinInterface();
  int nofSpecies = chemInterf->getNumSpecies();
  // Read the mixing variable streams, total is noofStreams 0 
  int nofstrm = 0;
  string speciesName;
  double mfrac; //mole or mass fraction
  for (ProblemSpecP stream_db = db->findBlock("Stream");
       stream_db != 0; stream_db = stream_db->findNextBlock("Stream")) {
    // Create the stream and add it to the vector
    d_streams.push_back(Stream(nofSpecies));
    stream_db->require("pressure", d_streams[nofstrm].d_pressure);
    stream_db->require("temperature", d_streams[nofstrm].d_temperature);
    // mole fraction or mass fraction
    stream_db->require("mole",d_streams[nofstrm].d_mole);
    for (ProblemSpecP species_db = stream_db->findBlock("Species");
	 species_db !=0; species_db = species_db->findNextBlock("Species")) {
      species_db->require("symbol",speciesName);
      species_db->require("mfraction",mfrac);
      // convert c++ string to c string
      d_streams[nofstrm].addSpecies(d_rxnModel->getChemkinInterface(),
					    speciesName.c_str(), mfrac);
    }
    d_streams[nofstrm].normalizeStream(); // normalize sum to be 1
    vector<double> ymassVec;
    double* ymassFrac = new double[nofSpecies];
    if (d_streams[nofstrm].d_mole) {
      ymassVec = chemInterf->convertMolestoMass(
					  d_streams[nofstrm].d_speciesConcn);
      d_streams[nofstrm].d_mole = false;
      d_streams[nofstrm].d_speciesConcn = ymassVec;
    }
    else
      ymassVec = d_streams[nofstrm].d_speciesConcn;
    // convert vec to array
    for (int ii = 0; ii < nofSpecies; ii++)
      ymassFrac[ii] = ymassVec[ii];
    double strmTemp = d_streams[nofstrm].d_temperature;
    double strmPress = d_streams[nofstrm].d_pressure;
    d_streams[nofstrm].d_density=chemInterf->getMassDensity(strmPress,
						     strmTemp, ymassFrac);
    d_streams[nofstrm].d_enthalpy=chemInterf->getMixEnthalpy(strmTemp, ymassVec);
    d_streams[nofstrm].d_moleWeight=chemInterf->getMixMoleWeight(ymassFrac);
    d_streams[nofstrm].d_cp=chemInterf->getMixSpecificHeat(strmTemp, ymassFrac);
    // store as mass fraction
    d_streams[nofstrm].print(cerr );
    delete[] ymassFrac;
    ++nofstrm;
  }
  // num_mix_scalars = num_streams -1
  d_numMixingVars = nofstrm - 1;
  d_tableDimension = d_numMixingVars + d_numMixStatVars + d_numRxnVars + !(d_adiabatic);
  d_tableInfo = new MixRxnTableInfo(d_tableDimension);
  bool mixTableFlag = true; //This is a mixing table, not a rxn table
  d_tableInfo->problemSetup(db, mixTableFlag, this);
  //d_tableInfo->problemSetup(db, this);
  d_depStateSpaceVars = d_streams[0].getDepStateSpaceVars();
  d_mixTable = new KD_Tree(d_tableDimension, d_depStateSpaceVars);
  // tableSetup is a function in DynamicTable; it allocates memory for table
  tableSetup(d_tableDimension, d_tableInfo);
  d_integrator = new Integrator(d_tableDimension, this, d_rxnModel, d_tableInfo);
  d_integrator->problemSetup(db);  

}

Stream
PDFMixingModel::speciesStateSpace(const vector<double>& mixVar) 
{
  ChemkinInterface* chemInterf = d_rxnModel->getChemkinInterface();
  int nofSpecies = chemInterf->getNumSpecies();
  Stream mixedStream(nofSpecies,d_numMixingVars, d_numRxnVars);
  // if adiabatic
  int count = 0;
  // store species as massfraction
  mixedStream.d_mole = false;
  double* sumMixVarFrac = new double[d_numMixingVars+1];
  double sum = 0.0;
  int ii;
  for (int ii = 0; ii < d_numMixingVars; ii ++) {
    sum += mixVar[ii];
    sumMixVarFrac[ii] = mixVar[ii];
  }
  sumMixVarFrac[d_numMixingVars] = 1.0 - sum;
  for (vector<Stream>::iterator iter = d_streams.begin();
       iter != d_streams.end(); ++iter) {
    Stream& inlstrm = *iter;
    mixedStream.addStream(inlstrm, chemInterf, sumMixVarFrac[count]);
    ++count;
  }
  --count; // nummixvars = numstreams -1
  assert(count==d_numMixingVars);
  delete[] sumMixVarFrac;
  
  return mixedStream;

}
      

//****************************************************************************
// Actually compute the properties here
//****************************************************************************
void 
PDFMixingModel::computeProps(const InletStream& inStream,
			     Stream& outStream)
{
  // convert inStream to array
  std::vector<double> mixRxnVar(d_tableDimension);  
  std::vector<double> normVar(d_tableDimension);
  int count = 0;
  double absEnthalpy = 0.0;
  if (!(d_adiabatic)) {
    absEnthalpy = inStream.d_enthalpy; 
    mixRxnVar[0] = absEnthalpy;
    normVar[0] = 0.0;
    count ++;
  }
  for (int i = 0; i < d_numMixingVars; i++) {
    mixRxnVar[count] = inStream.d_mixVars[i];
    normVar[count] = inStream.d_mixVars[i];
    count++;
  }
  for (int i = 0; i < d_numMixStatVars; i++) {
    mixRxnVar[count] = inStream.d_mixVarVariance[i];
    normVar[count] = inStream.d_mixVarVariance[i];
    count++;
  }
  for (int i = 0; i < d_numRxnVars;i++) {
    mixRxnVar[count] = inStream.d_rxnVars[i];
    normVar[count] = inStream.d_rxnVars[i];
    count++;
  }
  // count and d_tableDimension should be equal
  assert(count==d_tableDimension);
  // Normalize enthalpy
  if (!(d_adiabatic)) {
    Stream normStream = getProps(normVar);
    double adiabaticEnthalpy = normStream.d_enthalpy; //Use Get functions???
    double sensEnthalpy = normStream.d_sensibleEnthalpy;
    double normEnthalpy;
    if (Abs(absEnthalpy) < 1e-20)
      normEnthalpy = 0.0;
    else 
      normEnthalpy = (absEnthalpy - adiabaticEnthalpy)/sensEnthalpy;
    
    //cout << "Absolute enthalpy: " << normEnthalpy*sensEnthalpy+adiabaticEnthalpy 
    //<< endl;
    mixRxnVar[0] = normEnthalpy;
  }
    outStream = getProps(mixRxnVar);
#if 0
  //Normalize reaction variables
  if (d_numRxnVars > 0) {
    for (int i = 0; i < d_numRxnVars;i++) {
      vector<double> paramValues = d_rxnModel->normalizeParameter(mixRxnVar);//Better to use array??
      double minParamValue = paramValues[0];
      double maxParamValue = paramValues[1];
      double normParam = (mixRxnVar[rxncount+i] - minParamValue)/(maxParamValue - 
								  minParamValue);
      mixRxnVar[rxncount+i] = normParam;
    }
  }
#endif
}


Stream
PDFMixingModel::tableLookUp(int* tableKeyIndex) {
  Stream stateSpaceVars;
  vector<double> vec_stateSpaceVars;
  if (!(d_mixTable->Lookup(tableKeyIndex, vec_stateSpaceVars))) 
    {
      // call to integrator
      if (d_numMixStatVars) {
	stateSpaceVars = d_integrator->integrate(tableKeyIndex);
      }
      else
	{
	  stateSpaceVars = d_integrator->computeMeanValues(tableKeyIndex);
	}
      bool flag = false;
      vec_stateSpaceVars = stateSpaceVars.convertStreamToVec(flag);
      // defined in K-D tree implementation
      d_mixTable->Insert(tableKeyIndex, vec_stateSpaceVars);
      //cerr << "state space vars in PDFMixModel: " << tableKeyIndex[0] << endl;
      //stateSpaceVars.print(cerr);
    }
  else {
    bool flag = false;
    stateSpaceVars.convertVecToStream(vec_stateSpaceVars, flag, d_numMixingVars,
                                      d_numRxnVars);
  }
  return stateSpaceVars;
  
}




