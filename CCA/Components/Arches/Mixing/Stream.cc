//----- Stream.cc --------------------------------------------------

#include <Packages/Uintah/CCA/Components/Arches/Mixing/Stream.h>
#include <Packages/Uintah/CCA/Components/Arches/Mixing/ChemkinInterface.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpecP.h>
#include <Packages/Uintah/Core/Exceptions/InvalidValue.h>
#include <string>
#include <iostream>

using namespace Uintah;
using namespace std;
const int Stream::NUM_DEP_VARS;
Stream::Stream()
{
}

Stream::Stream(const int numSpecies,  int numElements)
{
  d_speciesConcn = vector<double>(numSpecies, 0.0); // initialize with 0
  d_pressure = 0.0;
  d_density = 0.0;
  d_temperature = 0.0;
  d_enthalpy = 0.0;
  d_sensibleEnthalpy = 0.0;
  d_moleWeight = 0.0;
  d_cp = 0.0;
  d_mole = false;
  d_numMixVars = 0;
  d_numRxnVars = 0;
  d_lsoot = false;
  //d_atomNumbers = vector<double>(numElements, 0.0); // initialize with 0
  // NUM_DEP_VARS corresponds to pressure, density, temp, enthalpy, sensh, 
  // cp, molwt; total number of dependent state space variables also 
  // includes mass fraction of each species
  d_depStateSpaceVars = NUM_DEP_VARS + numSpecies;
}


Stream::Stream(const int numSpecies, const int numElements, 
	       const int numMixVars, const int numRxnVars, bool lsoot): 
  d_numMixVars(numMixVars), d_numRxnVars(numRxnVars), d_lsoot(lsoot)
{
  d_speciesConcn = vector<double>(numSpecies, 0.0); // initialize with 0
  d_pressure = 0.0;
  d_density = 0.0;
  d_temperature = 0.0;
  d_enthalpy = 0.0;
  d_sensibleEnthalpy = 0.0;
  d_moleWeight = 0.0;
  d_cp = 0.0;
  d_mole = false;
  //d_atomNumbers = vector<double>(numElements, 0.0); // initialize with 0
  if (d_numRxnVars > 0) {
    d_rxnVarRates = vector<double>(d_numRxnVars, 0.0); // initialize with 0
    d_rxnVarNorm = vector<double>(2*d_numRxnVars, 0.0); // min & max values
                                         // for normalizing rxn parameter

  }
  int sootTrue = 0;
  if (d_lsoot) {
    d_sootData = vector<double>(2, 0.0);
    sootTrue = 1;
  }
  // NUM_DEP_VARS corresponds to pressure, density, temp, enthalpy, sensh, 
  // cp, molwt; total number of dependent state space variables also 
  // includes mass fraction of each species, soot volume fraction and 
  // diameter, source terms (rxn rates) for each rxn variable, and min/max 
  // values of each rxn variable for normalization
  d_depStateSpaceVars = NUM_DEP_VARS + numSpecies + 2*sootTrue + 
    3*d_numRxnVars;
}


Stream::Stream(const Stream& strm) // copy constructor
{
  d_speciesConcn = strm.d_speciesConcn;
  d_pressure = strm.d_pressure;
  d_density = strm.d_density;
  d_temperature = strm.d_temperature;
  d_enthalpy = strm.d_enthalpy;
  d_sensibleEnthalpy = strm.d_sensibleEnthalpy;
  d_moleWeight = strm.d_moleWeight;
  d_cp = strm.d_cp;
  d_depStateSpaceVars = strm.d_depStateSpaceVars;
  d_mole = strm.d_mole;
  d_numMixVars = strm.d_numMixVars;
  d_numRxnVars = strm.d_numRxnVars;
  d_lsoot = strm.d_lsoot;
  //d_atomNumbers = strm.d_atomNumbers;
  if (strm.d_numRxnVars > 0) {
    d_rxnVarRates = strm.d_rxnVarRates;
    d_rxnVarNorm = strm.d_rxnVarNorm;
  }
  if (strm.d_lsoot)
    d_sootData = strm.d_sootData;

}

Stream&
Stream::operator=(const Stream &rhs)
{
  // Guard against self-assignment
  if (this != &rhs)
    {	
      d_speciesConcn = rhs.d_speciesConcn;
      d_pressure = rhs.d_pressure;
      d_density = rhs.d_density;
      d_temperature = rhs.d_temperature;
      d_enthalpy = rhs.d_enthalpy;
      d_sensibleEnthalpy = rhs.d_sensibleEnthalpy;
      d_moleWeight = rhs.d_moleWeight;
      d_cp = rhs.d_cp;
      d_depStateSpaceVars = rhs.d_depStateSpaceVars;
      d_mole = rhs.d_mole;
      d_numMixVars = rhs.d_numMixVars;
      d_numRxnVars = rhs.d_numRxnVars;
      d_lsoot = rhs.d_lsoot;
      //d_atomNumbers = rhs.d_atomNumbers;
      if (rhs.d_numRxnVars > 0) {
	d_rxnVarRates = rhs.d_rxnVarRates;
	d_rxnVarNorm = rhs.d_rxnVarNorm;
      }
      if (rhs.d_lsoot)
	d_sootData = rhs.d_sootData;
    }
  return *this;
  }

Stream::~Stream()
{
}

void
Stream::addSpecies(const ChemkinInterface* chemInterf,
		   const char* speciesName, double mfrac)
{
  int indx = speciesIndex(chemInterf, speciesName);
  d_speciesConcn[indx] = mfrac;
}

void 
Stream::addStream(const Stream& strm, ChemkinInterface* chemInterf,
		  const double factor) 
{
  vector<double> spec_mfrac;
  if (strm.d_mole) // convert to mass fraction
    spec_mfrac = chemInterf->convertMolestoMass(strm.d_speciesConcn);
  else
    spec_mfrac = strm.d_speciesConcn;
  for (int i = 0; i < spec_mfrac.size(); i++)
    d_speciesConcn[i] += factor*spec_mfrac[i];
  d_pressure += factor*strm.d_pressure;
  d_density += factor*strm.d_density;
  d_temperature += factor*strm.d_temperature;
  d_enthalpy += factor*strm.d_enthalpy;
  d_sensibleEnthalpy += factor*strm.d_sensibleEnthalpy; //Does this even make sense??
  d_moleWeight += factor*strm.d_moleWeight;
  d_cp += factor*strm.d_cp;
  d_mole = false;
  //if (d_lsoot) {
  //for (int i = 0; i < strm.d_sootData.size(); i++)
  //  d_sootData[i] += factor*strm.d_sootData[i];
  //}
}


int
Stream::speciesIndex(const ChemkinInterface* chemInterf, const char* speciesName) 
{
  for (int i = 0; i < d_speciesConcn.size(); i++) {
    if (strlen(speciesName) == strlen(chemInterf->d_speciesNames[i])) {
      if (strncmp(speciesName, chemInterf->d_speciesNames[i],
		  (size_t) strlen(speciesName)) == 0) 
	return i;
    }
  }
  throw InvalidValue("Species not found");
}

double
Stream::getValue(int count, bool lfavre) 
{
  int sumVars = NUM_DEP_VARS + d_speciesConcn.size();
  //Need to work on how to handle soot here ****
  if ((d_numRxnVars > 0)&&(count >= sumVars))
    {
      if (count < (sumVars + d_numRxnVars))
	return d_rxnVarRates[count-NUM_DEP_VARS-d_speciesConcn.size()];
      else if (count < (sumVars + 3*d_numRxnVars))
	return d_rxnVarNorm[count-NUM_DEP_VARS-d_speciesConcn.size()-
			   d_numRxnVars];
      else if (count < d_depStateSpaceVars)
	return d_sootData[count-NUM_DEP_VARS-d_speciesConcn.size()-
			 3*d_numRxnVars];
      else {
	cerr << "Invalid count value" << '/n';
	return 0;
      }
    }	
  if ((count >= NUM_DEP_VARS)&&(count < sumVars))
    return d_speciesConcn[count-NUM_DEP_VARS];
  else
    {
      switch (count) {
      case 0:
	return d_pressure;
      case 1:
	return d_density;
      case 2:
	if (lfavre){
	  cout<<"Stream::lfavre is true"<<endl;
	  return d_temperature/d_density;
	}
	else
	  //cout<<"Stream::temp = "<<d_temperature<<endl;
	  return d_temperature;
      case 3:
	return d_enthalpy;
      case 4:
	return d_sensibleEnthalpy;
      case 5:
	return d_moleWeight;
      case 6:
	return d_cp;
      default:
	cerr << "Invalid count value" << '/n';
	return 0;
      }
    }
}

void
Stream::normalizeStream() {
  double sum = 0.0;
  for (vector<double>::iterator iter = d_speciesConcn.begin();
       iter != d_speciesConcn.end(); ++iter) 
    sum += *iter;
  for (vector<double>::iterator iter = d_speciesConcn.begin();
       iter != d_speciesConcn.end(); ++iter) 
    *iter /= sum;
}  
	
void
Stream::convertVecToStream(const vector<double>& vec_stateSpace, const bool lfavre,
                           const int numMixVars, const int numRxnVars,
			   const bool lsoot) {
  //cout<<"Stream::convertVecToStream"<<endl;
  d_depStateSpaceVars = vec_stateSpace.size();
  //cout<<"Stream::depStateSpace= "<<d_depStateSpaceVars<<endl;
  d_pressure = vec_stateSpace[0];
  d_density = vec_stateSpace[1];
  if (lfavre) 
    d_temperature = d_density*vec_stateSpace[2];
  else
    d_temperature = vec_stateSpace[2];
  d_enthalpy = vec_stateSpace[3];
  d_sensibleEnthalpy = vec_stateSpace[4];
  d_moleWeight = vec_stateSpace[5];
  d_cp = vec_stateSpace[6];
  d_numMixVars = numMixVars;
  d_numRxnVars = numRxnVars;
  d_lsoot = lsoot;
  d_mole = false; //???Is this true???
  int incSoot = 0;
  if (lsoot)
    incSoot = 2;
  //cout << "Stream::incsoot = " << incSoot << " " << lsoot << endl;
  d_speciesConcn = vector<double> (vec_stateSpace.begin()+NUM_DEP_VARS,
				   vec_stateSpace.end()-3*d_numRxnVars-incSoot);
  if (d_numRxnVars > 0) {
    d_rxnVarRates = vector<double> (vec_stateSpace.end()-3*d_numRxnVars-incSoot, 
				    vec_stateSpace.end()-2*d_numRxnVars-incSoot);
    d_rxnVarNorm = vector<double> (vec_stateSpace.end()-2*d_numRxnVars-incSoot, 
				   vec_stateSpace.end()-incSoot);
    //cerr << "Stream::rate = " << d_rxnVarRates[0] << endl;
    //cerr << "Stream::norm = " << d_rxnVarNorm[0] << " " << d_rxnVarNorm[1] << endl;
  } 
  //cout << "Stream::lsoot = " << lsoot << endl;
  if (lsoot) {
    d_sootData = vector<double> (vec_stateSpace.end()-2, 
				 vec_stateSpace.end());
    //cerr << "Stream::soot = " << d_sootData[0] << " " << d_sootData[1] << endl;
  }
#if 0
  cout << "Density: "<< d_density << endl;
  cout << "Pressure: "<< d_pressure << endl;
  cout << "Temperature: "<< d_temperature << endl;
  cout << "Enthalpy: "<< d_enthalpy << endl;
  cout << "Sensible Enthalpy: "<< d_sensibleEnthalpy << endl;
  cout << "Molecular Weight: "<< d_moleWeight << endl;
  cout << "CP: "<< d_cp << endl;
  cout << "Species concentration in mass fraction: " << endl;
  for (int ii = 0; ii < d_speciesConcn.size(); ii++) {
    cout.width(10);
    cout << d_speciesConcn[ii] << " " ; 
    if (!(ii % 10)) cout << endl; 
    cout << endl;
  }
   cout << d_numMixVars << " " << d_numRxnVars << " " << d_lsoot << " " << d_mole << " " << d_depStateSpaceVars << endl; 
#endif
}     

vector<double>
//Stream::convertStreamToVec(const bool lsoot)
Stream::convertStreamToVec()
{
  //cout << "Stream::streamToVec" << endl;
  vector<double> vec_stateSpace;
  vec_stateSpace.push_back(d_pressure);
  vec_stateSpace.push_back(d_density);
  vec_stateSpace.push_back(d_temperature);
  vec_stateSpace.push_back(d_enthalpy);
  vec_stateSpace.push_back(d_sensibleEnthalpy);
  vec_stateSpace.push_back(d_moleWeight);
  vec_stateSpace.push_back(d_cp);
  // copy d_speciesConcn to rest of the vector
  //int jj = 0;
  for (vector<double>::iterator iter = d_speciesConcn.begin(); 
       iter != d_speciesConcn.end(); ++iter) {
    vec_stateSpace.push_back(*iter);
    //cout << d_speciesConcn[jj++] <<  endl;
  }
  if (d_numRxnVars > 0) {
    for (vector<double>::iterator iter = d_rxnVarRates.begin(); 
	 iter != d_rxnVarRates.end(); ++iter)
      vec_stateSpace.push_back(*iter);
    for (vector<double>::iterator iter = d_rxnVarNorm.begin(); 
	 iter != d_rxnVarNorm.end(); ++iter)
      vec_stateSpace.push_back(*iter);
  }
  //cout << "Stream::lsoot = " << lsoot << endl;
  //cout << "Stream::streamToVec::sootData = " << d_sootData[0] << " " 
  //     << d_sootData[1] << endl;
  if (d_lsoot) {
    for (vector<double>::iterator iter = d_sootData.begin(); 
       iter != d_sootData.end(); ++iter)
    vec_stateSpace.push_back(*iter);
    //cout << "Stream::streamToVec::sootData = " << d_sootData[0] << " " 
    // << d_sootData[1] << endl;
  }
  return vec_stateSpace;
}


Stream& Stream::linInterpolate(double upfactor, double lowfactor,
			       Stream& rightvalue) {
  d_pressure = upfactor*d_pressure+lowfactor*rightvalue.d_pressure;
  d_density = upfactor*d_density+lowfactor*rightvalue.d_density;
  d_temperature = upfactor*d_temperature+lowfactor*rightvalue.d_temperature;
  d_enthalpy = upfactor*d_enthalpy+lowfactor*rightvalue.d_enthalpy;
  d_sensibleEnthalpy = upfactor*d_sensibleEnthalpy+lowfactor*
                                           rightvalue.d_sensibleEnthalpy;
  d_moleWeight = upfactor*d_moleWeight+lowfactor*rightvalue.d_moleWeight;
  d_cp = upfactor*d_cp+lowfactor*rightvalue.d_cp;
  for (int i = 0; i < d_speciesConcn.size(); i++)
    d_speciesConcn[i] = upfactor*d_speciesConcn[i] +
                   lowfactor*rightvalue.d_speciesConcn[i];
  if (d_numRxnVars > 0) {
    for (int i = 0; i < d_rxnVarRates.size(); i++)
      d_rxnVarRates[i] = upfactor*d_rxnVarRates[i] +
	lowfactor*rightvalue.d_rxnVarRates[i];
    for (int i = 0; i < d_rxnVarNorm.size(); i++)
      d_rxnVarNorm[i] = upfactor*d_rxnVarNorm[i] +
	lowfactor*rightvalue.d_rxnVarNorm[i];
  }
  if (d_lsoot) {
    for (int i = 0; i < d_sootData.size(); i++)
      d_sootData[i] = upfactor*d_sootData[i] +
	lowfactor*rightvalue.d_sootData[i];
  }
  //cout << "Stream::linInterpolate made it out " << endl;
  //cout << "Stream:: " <<d_numMixVars << " " << d_numRxnVars << " " << d_lsoot << " " << d_mole << " " << d_depStateSpaceVars << endl;
  //cout << "atomNumbers" << d_atomNumbers[0] << " " << d_atomNumbers[1] << " " <<  d_atomNumbers[2] << " " <<  d_atomNumbers[3] << endl;
  //cout << "Stream::linInterpolate rxnData = " << d_rxnVarRates[0] << " " <<  
  //d_rxnVarNorm[0] << " " <<  d_rxnVarNorm[1] << endl;
  //cout << "Density: "<< d_density << endl;
  //cout << "Pressure: "<< d_pressure << endl;
  //cout << "Temperature: "<< d_temperature << endl;
  //cout << "Enthalpy: "<< d_enthalpy << endl;
  //cout << "Sensible Enthalpy: "<< d_sensibleEnthalpy << endl;
  //cout << "Molecular Weight: "<< d_moleWeight << endl;
  //cout << "CP: "<< d_cp << endl;
  //cout << "Species concentration in mass fraction: " << endl;
  //for (int ii = 0; ii < d_speciesConcn.size(); ii++) {
  //  cout.width(10);
  //  cout << d_speciesConcn[ii] << " " ; 
  //  if (!(ii % 10)) cout << endl; 
  //  cout << endl;
  //}
  //if (d_lsoot) {
  //  cout << "Soot Data: " << endl;
  //  for (int ii = 0; ii < d_sootData.size(); ii++) {
  //    cout.width(10);
  //    cout << d_sootData[ii] << " " ; 
  //    if (!(ii % 10)) cout << endl; 
  // }
  //}

  return *this;
}

void
Stream::print(std::ostream& out) const {
  out << "Integrated values"<< '\n';
  out << "Density: "<< d_density << endl;
  out << "Pressure: "<< d_pressure << endl;
  out << "Temperature: "<< d_temperature << endl;
  out << "Enthalpy: "<< d_enthalpy << endl;
  out << "Sensible Enthalpy: "<< d_sensibleEnthalpy << endl;
  out << "Molecular Weight: "<< d_moleWeight << endl;
  out << "CP: "<< d_cp << endl;
  out << "Species concentration in mass fraction: " << endl;
  for (int ii = 0; ii < d_speciesConcn.size(); ii++) {
    out.width(10);
    out << d_speciesConcn[ii] << " " ; 
    if (!(ii % 10)) out << endl; 
    out << endl;
  }
  if (d_lsoot) {
    out << "Soot Data: " << endl;
    for (int ii = 0; ii < d_sootData.size(); ii++) {
      out.width(10);
      out << d_sootData[ii] << " " ; 
      if (!(ii % 10)) out << endl; 
    }
  }
  out << endl;
}

void
Stream::print(std::ostream& out, ChemkinInterface* chemInterf) {
  out << "Integrated values"<< '\n';
  out << "Density: "<< d_density << endl;
  out << "Pressure: "<< d_pressure << endl;
  out << "Temperature: "<< d_temperature << endl;
  out << "Enthalpy: "<< d_enthalpy << endl;
  out << "Sensible Enthalpy: "<< d_sensibleEnthalpy << endl;
  out << "Molecular Weight: "<< d_moleWeight << endl;
  out << "CP: "<< d_cp << endl;
  int numSpecies = chemInterf->getNumSpecies();
  double* specMW = new double[numSpecies];
 chemInterf->getMoleWeight(specMW);
  out << "Species concentration in mole fraction: " << endl;
  for (int ii = 0; ii < d_speciesConcn.size(); ii++) {
    out.width(10);
    out << d_speciesConcn[ii]/specMW[ii] << " " ; 
    if (!(ii % 10)) out << endl; 
  }
  if (d_lsoot) {
    for (int ii = 0; ii < d_sootData.size(); ii++) {
      out.width(10);
      out << d_sootData[ii] << " " ; 
      if (!(ii % 10)) out << endl; 
    }
  }
  out << endl;
}


//
// $Log$
// Revision 1.7  2001/09/04 23:44:27  rawat
// Added ReactingScalar transport equation to run ILDM.
// Also, merged Jennifer's changes to run ILDM in the mixing directory.
//

// Revision 1.6  2001/08/25 07:32:45  skumar
// Incorporated Jennifer's beta-PDF mixing model code with some
// corrections to the equilibrium code.
// Added computation of scalar variance for use in PDF model.
// Properties::computeInletProperties now uses speciesStateSpace
// instead of computeProps from d_mixingModel.
//
// Revision 1.5  2001/07/27 20:51:40  sparker
// Include file cleanup
// Fix uninitialized array element
//
// Revision 1.4  2001/07/16 21:15:38  rawat
// added enthalpy solver and Jennifer's changes in Mixing and Reaction model required for ILDM and non-adiabatic cases
//
// Revision 1.1  2001/01/31 16:35:30  rawat
// Implemented mixing and reaction models for fire.
//
// Revision 1.1  2001/01/15 23:38:21  rawat
// added some more classes for implementing mixing model
//
// Revision 1.1 2001/07/16 spinti
// added classes for implement reaction model
//
//
