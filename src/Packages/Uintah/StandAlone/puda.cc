
/*
 *  puda.cc: Print out a uintah data archive
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   February 2000
 *
 *  Copyright (C) 2000 U of U
 */

#include <Uintah/Interface/DataArchive.h>
#include <Uintah/Grid/Grid.h>
#include <Uintah/Grid/Level.h>
#include <Uintah/Grid/NodeIterator.h>
#include <SCICore/Math/MinMax.h>
#include <SCICore/Geometry/Point.h>
#include <SCICore/Geometry/Vector.h>
#include <SCICore/OS/Dir.h>
#include <PSECore/XMLUtil/XMLUtil.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>


using SCICore::Exceptions::Exception;
using namespace SCICore::Geometry;
using namespace SCICore::Math;
using namespace SCICore::OS;
using namespace std;
using namespace Uintah;
using namespace PSECore::XMLUtil;

typedef struct{
  vector<ParticleVariable<double> > pv_double_list;
  vector<ParticleVariable<Point> > pv_point_list;
  vector<ParticleVariable<Vector> > pv_vector_list;
  ParticleVariable<Point> p_x;
} MaterialData;

// takes a string and replaces all occurances of old with newch
string replaceChar(string s, char old, char newch) {
  string result;
  for (int i = 0; i<s.size(); i++)
    if (s[i] == old)
      result += newch;
    else
      result += s[i];
  return result;
}

// use this function to open a pair of files for outputing
// data to the reat-time raytracer.
//
// in: pointers to the pointer to the file
//     the file name
// out: inialized files for writing
//      boolean reporting the success of the file creation
bool setupOutFiles(FILE** data, FILE** header, string name, string head) {
  FILE* datafile;
  FILE* headerfile;
  string headername = name + string(".") + head;

  datafile = fopen(name.c_str(),"w");
  if (!datafile) {
    cerr << "Can't open output file " << name << endl;
    return false;
  }
  
  headerfile = fopen(headername.c_str(),"w");
  if (!headerfile) {
    cerr << "Can't open output file " << headername << endl;
    return false;
  }
  
  *data = datafile;
  *header = headerfile;
  return true;
}

// given the various parts of the name we piece together the full name
string makeFileName(string raydatadir, string variable_file, string time_file, 
		    string patchID_file, string materialType_file) {

  string raydatafile;
  if (raydatadir != "")
    raydatafile+= raydatadir + string("/");
  raydatafile+= string("TS_") + time_file + string("/");
  if (variable_file != "")
    raydatafile+= string("VAR_") + variable_file + string(".");
  if (materialType_file != "")
    raydatafile+= string("MT_") + materialType_file + string(".");
  raydatafile+= string("PI_") + patchID_file;
  return raydatafile;
}

void usage(const std::string& badarg, const std::string& progname)
{
    if(badarg != "")
	cerr << "Error parsing argument: " << badarg << '\n';
    cerr << "Usage: " << progname << " [options] <archive file>\n\n";
    cerr << "Valid options are:\n";
    cerr << "  -h[elp]\n";
    cerr << "  -timesteps\n";
    cerr << "  -gridstats\n";
    cerr << "  -listvariables\n";
    cerr << "  -varsummary\n";
    cerr << "  -rtdata [output directory]\n";
    cerr << "  -PTvar\n";
    cerr << "  -ptonly (prints out only the point location\n";
    cerr << "  -patch (outputs patch id with data)\n";
    cerr << "  -material (outputs material number with data)\n";
    cerr << "  -NCvar [double or point or vector]\n";
    cerr << "  -verbose (prints status of output)\n";
    cerr << "  -timesteplow [int] (only outputs timestep from int)\n";
    cerr << "  -timestephigh [int] (only outputs timesteps upto int)\n";
    cerr << "*NOTE* to use -PTvar or -NVvar -rtdata must be used\n";
    cerr << "*NOTE* ptonly, patch, material, timesteplow, timestephigh \
are used in conjuntion with -PTvar.\n\n";
    
    cerr << "USAGE IS NOT FINISHED\n\n";
    exit(1);
}

int main(int argc, char** argv)
{
  /*
   * Default values
   */
  bool do_timesteps=false;
  bool do_gridstats=false;
  bool do_listvars=false;
  bool do_varsummary=false;
  bool do_rtdata = false;
  bool do_NCvar_double = false;
  bool do_NCvar_point = false;
  bool do_NCvar_vector = false;
  bool do_PTvar = false;
  bool do_PTvar_all = true;
  bool do_patch = false;
  bool do_material = false;
  bool do_verbose = false;
  int time_step_lower = -1;
  int time_step_upper = -1;
  string filebase;
  string raydatadir;
  /*
   * Parse arguments
   */
  for(int i=1;i<argc;i++){
    string s=argv[i];
    if(s == "-timesteps"){
      do_timesteps=true;
    } else if(s == "-gridstats"){
      do_gridstats=true;
    } else if(s == "-listvariables"){
      do_listvars=true;
    } else if(s == "-varsummary"){
      do_varsummary=true;
    } else if(s == "-rtdata") {
      do_rtdata = true;
      if (++i < argc) {
	s = argv[i];
	if (s[0] == '-')
	  usage("-rtdata", argv[0]);
	raydatadir = s;
      }
    } else if(s == "-NCvar") {
      if (++i < argc) {
	s = argv[i];
	if (s == "double")
	  do_NCvar_double = true;
	else if (s == "point")
	  do_NCvar_point = true;
	else if (s == "vector")
	  do_NCvar_vector = true;
	else
	  usage("-NCvar", argv[0]);
      }
      else
	usage("-NCvar", argv[0]);
    } else if(s == "-PTvar") {
      do_PTvar = true;
    } else if (s == "-ptonly") {
      do_PTvar_all = false;
    } else if (s == "-patch") {
      do_patch = true;
    } else if (s == "-material") {
      do_material = true;
    } else if (s == "-verbose") {
      do_verbose = true;
    } else if (s == "-timesteplow") {
      time_step_lower = atoi(argv[++i]);
    } else if (s == "-timestephigh") {
      time_step_upper = atoi(argv[++i]);
    } else if( (s == "-help") || (s == "-h") ) {
      usage( "", argv[0] );
    } else {
      if(filebase!="")
	usage(s, argv[0]);
      else
	filebase = argv[i];
    }
  }
  
  if(filebase == ""){
    cerr << "No archive file specified\n";
    usage("", argv[0]);
  }

  try {
    XMLPlatformUtils::Initialize();
  } catch(const XMLException& toCatch) {
    cerr << "Caught XML exception: " << toString(toCatch.getMessage()) 
	 << '\n';
    exit( 1 );
  }
  
  try {
    DataArchive* da = new DataArchive(filebase);
    
    if(do_timesteps){
      vector<int> index;
      vector<double> times;
      da->queryTimesteps(index, times);
      ASSERTEQ(index.size(), times.size());
      cout << "There are " << index.size() << " timesteps:\n";
      for(int i=0;i<index.size();i++)
	cout << index[i] << ": " << times[i] << '\n';
    }
    if(do_gridstats){
      vector<int> index;
      vector<double> times;
      da->queryTimesteps(index, times);
      ASSERTEQ(index.size(), times.size());
      cout << "There are " << index.size() << " timesteps:\n";
      for(int i=0;i<index.size();i++){
	cout << index[i] << ": " << times[i] << '\n';
	GridP grid = da->queryGrid(times[i]);
	grid->performConsistencyCheck();
	grid->printStatistics();
      }
    }
    if(do_listvars){
      vector<string> vars;
      vector<const TypeDescription*> types;
      da->queryVariables(vars, types);
      cout << "There are " << vars.size() << " variables:\n";
      for(int i=0;i<vars.size();i++){
	cout << vars[i] << ": " << types[i]->getName() << '\n';
      }
    }
    if(do_varsummary){
      vector<string> vars;
      vector<const TypeDescription*> types;
      da->queryVariables(vars, types);
      ASSERTEQ(vars.size(), types.size());
      cout << "There are " << vars.size() << " variables:\n";
      
      vector<int> index;
      vector<double> times;
      da->queryTimesteps(index, times);
      ASSERTEQ(index.size(), times.size());
      cout << "There are " << index.size() << " timesteps:\n";
      
      if (time_step_lower <= -1)
	time_step_lower =0;
      else if (time_step_lower >= times.size()) {
	cerr << "timesteplow must be between 0 and " << times.size()-1 << endl;
	abort();
      }
      if (time_step_upper <= -1)
	time_step_upper = times.size()-1;
      else if (time_step_upper >= times.size()) {
	cerr << "timesteplow must be between 0 and " << times.size()-1 << endl;
	abort();
      }
      
      for(int t=time_step_lower;t<=time_step_upper;t++){
	double time = times[t];
	cout << "time = " << time << "\n";
	GridP grid = da->queryGrid(time);
	for(int v=0;v<vars.size();v++){
	  std::string var = vars[v];
	  const TypeDescription* td = types[v];
	  const TypeDescription* subtype = td->getSubType();
	  cout << "\tVariable: " << var << ", type " << td->getName() << "\n";
	  for(int l=0;l<grid->numLevels();l++){
	    LevelP level = grid->getLevel(l);
	    for(Level::const_patchIterator iter = level->patchesBegin();
		iter != level->patchesEnd(); iter++){
	      const Patch* patch = *iter;
	      cout << "\t\tPatch: " << patch->getID() << "\n";
	      int numMatls = da->queryNumMaterials(var, patch, time);
	      for(int matl=0;matl<numMatls;matl++){
		cout << "\t\t\tMaterial: " << matl << "\n";
		switch(td->getType()){
		case TypeDescription::ParticleVariable:
		  switch(subtype->getType()){
		  case TypeDescription::double_type:
		    {
		      ParticleVariable<double> value;
		      da->query(value, var, matl, patch, time);
		      ParticleSubset* pset = value.getParticleSubset();
		      cout << "\t\t\t\t" << td->getName() << " over " << pset->numParticles() << " particles\n";
		      if(pset->numParticles() > 0){
			double min, max;
			ParticleSubset::iterator iter = pset->begin();
			min=max=value[*iter++];
			for(;iter != pset->end(); iter++){
			  min=Min(min, value[*iter]);
			  max=Max(max, value[*iter]);
			}
			cout << "\t\t\t\tmin value: " << min << '\n';
			cout << "\t\t\t\tmax value: " << max << '\n';
		      }
		    }
		  break;
		  case TypeDescription::Point:
		    {
		      ParticleVariable<Point> value;
		      da->query(value, var, matl, patch, time);
		      ParticleSubset* pset = value.getParticleSubset();
		      cout << "\t\t\t\t" << td->getName() << " over " << pset->numParticles() << " particles\n";
		      if(pset->numParticles() > 0){
			Point min, max;
			ParticleSubset::iterator iter = pset->begin();
			min=max=value[*iter];
			for(;iter != pset->end(); iter++){
			  min=Min(min, value[*iter]);
			  max=Max(max, value[*iter]);
			}
			cout << "\t\t\t\tmin value: " << min << '\n';
			cout << "\t\t\t\tmax value: " << max << '\n';
		      }
		    }
		  break;
		  case TypeDescription::Vector:
		    {
		      ParticleVariable<Vector> value;
		      da->query(value, var, matl, patch, time);
		      ParticleSubset* pset = value.getParticleSubset();
		      cout << "\t\t\t\t" << td->getName() << " over " << pset->numParticles() << " particles\n";
		      if(pset->numParticles() > 0){
			double min, max;
			ParticleSubset::iterator iter = pset->begin();
			min=max=value[*iter++].length2();
			for(;iter != pset->end(); iter++){
			  min=Min(min, value[*iter].length2());
			  max=Max(max, value[*iter].length2());
			}
			cout << "\t\t\t\tmin magnitude: " << sqrt(min) << '\n';
			cout << "\t\t\t\tmax magnitude: " << sqrt(max) << '\n';
		      }
		    }
		  break;
		  default:
		    cerr << "Particle Variable of unknown type: " << subtype->getType() << '\n';
		    break;
		  }
		  break;
		case TypeDescription::NCVariable:
		  switch(subtype->getType()){
		  case TypeDescription::double_type:
		    {
		      NCVariable<double> value;
		      da->query(value, var, matl, patch, time);
		      cout << "\t\t\t\t" << td->getName() << " over " << value.getLowIndex() << " to " << value.getHighIndex() << "\n";
		      IntVector dx(value.getHighIndex()-value.getLowIndex());
		      if(dx.x() && dx.y() && dx.z()){
			double min, max;
			NodeIterator iter = patch->getNodeIterator();
			min=max=value[*iter];
			for(;!iter.done(); iter++){
			  min=Min(min, value[*iter]);
			  max=Max(max, value[*iter]);
			}
			cout << "\t\t\t\tmin value: " << min << '\n';
			cout << "\t\t\t\tmax value: " << max << '\n';
		      }
		    }
		  break;
		  case TypeDescription::Point:
		    {
		      NCVariable<Point> value;
		      da->query(value, var, matl, patch, time);
		      cout << "\t\t\t\t" << td->getName() << " over " << value.getLowIndex() << " to " << value.getHighIndex() << "\n";
		      IntVector dx(value.getHighIndex()-value.getLowIndex());
		      if(dx.x() && dx.y() && dx.z()){
			Point min, max;
			NodeIterator iter = patch->getNodeIterator();
			min=max=value[*iter];
			for(;!iter.done(); iter++){
			  min=Min(min, value[*iter]);
			  max=Max(max, value[*iter]);
			}
			cout << "\t\t\t\tmin value: " << min << '\n';
			cout << "\t\t\t\tmax value: " << max << '\n';
		      }
		    }
		  break;
		  case TypeDescription::Vector:
		    {
		      NCVariable<Vector> value;
		      da->query(value, var, matl, patch, time);
		      cout << "\t\t\t\t" << td->getName() << " over " << value.getLowIndex() << " to " << value.getHighIndex() << "\n";
		      IntVector dx(value.getHighIndex()-value.getLowIndex());
		      if(dx.x() && dx.y() && dx.z()){
			double min, max;
			NodeIterator iter = patch->getNodeIterator();
			min=max=value[*iter].length2();
			for(;!iter.done(); iter++){
			  min=Min(min, value[*iter].length2());
			  max=Max(max, value[*iter].length2());
			}
			cout << "\t\t\t\tmin magnitude: " << sqrt(min) << '\n';
			cout << "\t\t\t\tmax magnitude: " << sqrt(max) << '\n';
		      }
		    }
		  break;
		  default:
		    cerr << "NC Variable of unknown type: " << subtype->getType() << '\n';
		    break;
		  }
		  break;
		default:
		  cerr << "Variable of unknown type: " << td->getType() << '\n';
		  break;
		}
	      }
	    }
	  }
	}
      }
    }
    if (do_rtdata) {
      // Create a directory if it's not already there.
      // The exception occurs when the directory is already there
      // and the Dir.create fails.  This exception is ignored. 
      if(raydatadir != "") {
	Dir rayDir;
	try {
	  rayDir.create(raydatadir);
	}
	catch (Exception& e) {
	  cerr << "Caught exception: " << e.message() << '\n';
	}
      }

      // set up the file that contains a list of all the files
      FILE* filelist;
      string filelistname = raydatadir + string("/") + string("timelist");
      filelist = fopen(filelistname.c_str(),"w");
      if (!filelist) {
	cerr << "Can't open output file " << filelistname << endl;
	abort();
      }

      vector<string> vars;
      vector<const TypeDescription*> types;
      da->queryVariables(vars, types);
      ASSERTEQ(vars.size(), types.size());
      cout << "There are " << vars.size() << " variables:\n";
      
      vector<int> index;
      vector<double> times;
      da->queryTimesteps(index, times);
      ASSERTEQ(index.size(), times.size());
      cout << "There are " << index.size() << " timesteps:\n";

      std::string time_file;
      std::string variable_file;
      std::string patchID_file;
      std::string materialType_file;
      
      if (time_step_lower <= -1)
	time_step_lower =0;
      else if (time_step_lower >= times.size()) {
	cerr << "timesteplow must be between 0 and " << times.size()-1 << endl;
	abort();
      }
      if (time_step_upper <= -1)
	time_step_upper = times.size()-1;
      else if (time_step_upper >= times.size()) {
	cerr << "timesteplow must be between 0 and " << times.size()-1 << endl;
	abort();
      }
      
      // for all timesteps
      for(int t=time_step_lower;t<=time_step_upper;t++){
	double time = times[t];
	ostringstream tempstr_time;
	tempstr_time << setprecision(17) << time;
	time_file = replaceChar(string(tempstr_time.str()),'.','_');
	GridP grid = da->queryGrid(time);
	fprintf(filelist,"<TIMESTEP>\n");
	if(do_verbose)
	  cout << "time = " << time << "\n";
	// Create a directory if it's not already there.
	// The exception occurs when the directory is already there
	// and the Dir.create fails.  This exception is ignored. 
	Dir rayDir;
	try {
	  rayDir.create(raydatadir + string("/TS_") + time_file);
	}
	catch (Exception& e) {
	  cerr << "Caught directory making exception: " << e.message() << '\n';
	}
	if (t==4) break;
	// for each level in the grid
	for(int l=0;l<grid->numLevels();l++){
	  LevelP level = grid->getLevel(l);
	  
	  // for each patch in the level
	  for(Level::const_patchIterator iter = level->patchesBegin();
	      iter != level->patchesEnd(); iter++){
	    const Patch* patch = *iter;
	    ostringstream tempstr_patch;
	    tempstr_patch << patch->getID();
	    patchID_file = tempstr_patch.str();
	    fprintf(filelist,"<PATCH>\n");

	    vector<MaterialData> material_data_list; 
	    	    
	    // for all vars in one timestep in one patch
	    for(int v=0;v<vars.size();v++){
	      std::string var = vars[v];
	      //cerr << "***********Found variable " << var << "*********\n";
	      variable_file = replaceChar(var,'.','_');
	      const TypeDescription* td = types[v];
	      const TypeDescription* subtype = td->getSubType();
	      int numMatls = da->queryNumMaterials(var, patch, time);
	      
	      // for all the materials in the patch
	      for(int matl=0;matl<numMatls;matl++){
		ostringstream tempstr_matl;
		tempstr_matl << matl;
		materialType_file = tempstr_matl.str();

		MaterialData material_data;

		if (matl < material_data_list.size())
		  material_data = material_data_list[matl];
		
	        switch(td->getType()){
	        case TypeDescription::ParticleVariable:
		  if (do_PTvar) {
		    switch(subtype->getType()){
		    case TypeDescription::double_type:
		      {
			ParticleVariable<double> value;
			da->query(value, var, matl, patch, time);
			material_data.pv_double_list.push_back(value);
		      }
		    break;
		    case TypeDescription::Point:
		      {
			ParticleVariable<Point> value;
			da->query(value, var, matl, patch, time);
			
			if (var == "p.x") {
			  material_data.p_x = value;
			} else {
			  material_data.pv_point_list.push_back(value);
			}
		      }
		    break;
		    case TypeDescription::Vector:
		      {
			ParticleVariable<Vector> value;
			da->query(value, var, matl, patch, time);
			material_data.pv_vector_list.push_back(value);
		      }
		    break;
		    default:
		      cerr << "Particle Variable of unknown type: " << subtype->getType() << '\n';
		      break;
		    }
		    break;
		  }
		case TypeDescription::NCVariable:
		  switch(subtype->getType()){
		  case TypeDescription::double_type:
		    {
		      if (do_NCvar_double) {
			// setup output files
			string raydatafile = makeFileName(raydatadir,variable_file,time_file,patchID_file,materialType_file);			
			FILE* datafile;
			FILE* headerfile;
			if (!setupOutFiles(&datafile,&headerfile,raydatafile,string("hdr")))
			  abort();

			// addfile to filelist
			fprintf(filelist,"%s\n",raydatafile.c_str());
			// get the data and write it out
			double min, max;
			NCVariable<double> value;
			da->query(value, var, matl, patch, time);
			IntVector dim(value.getHighIndex()-value.getLowIndex());
			if(dim.x() && dim.y() && dim.z()){
			  NodeIterator iter = patch->getNodeIterator();
			  min=max=value[*iter];
			  for(;!iter.done(); iter++){
			    min=Min(min, value[*iter]);
			    max=Max(max, value[*iter]);
			    float temp_value = (float)value[*iter];
			    fwrite(&temp_value, sizeof(float), 1, datafile);
			  }	  
			}
			
			Point b_min = patch->getBox().lower();
			Point b_max = patch->getBox().upper();
			
			// write the header file
			fprintf(headerfile, "%d %d %d\n",dim.x(), dim.y(), dim.z());
			fprintf(headerfile, "%f %f %f\n",(float)b_min.x(),(float)b_min.y(),(float)b_min.z());
			fprintf(headerfile, "%f %f %f\n",(float)b_max.x(),(float)b_max.y(),(float)b_max.z());
			fprintf(headerfile, "%f %f\n",(float)min,(float)max);

			fclose(datafile);
			fclose(headerfile);
		      }
		    }
		  break;
		  case TypeDescription::Point:
		    {
		      if (do_NCvar_point) {
			// not implemented at this time
		      }
		    }
		  break;
		  case TypeDescription::Vector:
		    {
		      if (do_NCvar_vector) {
			// not implemented at this time
		      }
		    }
		  break;
		  default:
		    cerr << "NC variable of unknown type: " << subtype->getType() << '\n';
		    break;
		  }
		  break;
		default:
		  cerr << "Variable of unknown type: " << td->getType() << '\n';
		  break;
		} // end switch(td->getType())
		if (matl < material_data_list.size())
		  material_data_list[matl] = material_data;
		else
		  material_data_list.push_back(material_data);
	      } // end matl
	      
	    } // end vars
	    // after all the variable data has been collected write it out
	    if (do_PTvar) {
	      FILE* datafile;
	      FILE* headerfile;
	      //--------------------------------------------------
	      // set up the first min/max
	      Point min, max;
	      vector<double> d_min,d_max,v_min,v_max;
	      bool data_found = false;
	      int total_particles = 0;
	      
	      // loops until a non empty material_data set has been
	      // found and inialized the mins and maxes
	      for(int m = 0; m < material_data_list.size(); m++) {
		// determine the min and max
		MaterialData md = material_data_list[m];
		//cerr << "First md = " << m << endl;
		ParticleSubset* pset = md.p_x.getParticleSubset();
		if (!pset) {
		  cerr << "No particle location variable found\n";
		  abort();
		}
		int numParticles = pset->numParticles();
		if(numParticles > 0){
		  ParticleSubset::iterator iter = pset->begin();

		  // setup min/max for p.x
		  min=max=md.p_x[*iter];
		  // setup min/max for all others
		  if (do_PTvar_all) {
		    for(int i = 0; i < md.pv_double_list.size(); i++) {
		      d_min.push_back(md.pv_double_list[i][*iter]);
		      d_max.push_back(md.pv_double_list[i][*iter]);
		    }
		    for(int i = 0; i < md.pv_vector_list.size(); i++) {
		      v_min.push_back(md.pv_vector_list[i][*iter].length());
		      v_max.push_back(md.pv_vector_list[i][*iter].length());
		    }
		  }
		  // initialized mins/maxes
		  data_found = true;
		  // setup output files
		  string raydatafile = makeFileName(raydatadir,string(""),time_file,patchID_file,string(""));
		  if (!setupOutFiles(&datafile,&headerfile,raydatafile,string("meta")))
		    abort();
		  // addfile to filelist
		  fprintf(filelist,"%s\n",raydatafile.c_str());
		  
		  break;
		}
		
	      }

	      //--------------------------------------------------
	      // extract data and write it to a file MaterialData at a time

	      if (do_verbose)
		cerr << "---Extracting data and writing it out  ";
	      for(int m = 0; m < material_data_list.size(); m++) {
		MaterialData md = material_data_list[m];
		ParticleSubset* pset = md.p_x.getParticleSubset();
		// a little redundant, but may not have been cought
		// by the previous section
		if (!pset) {
		  cerr << "No particle location variable found\n";
		  abort();
		}
		
		int numParticles = pset->numParticles();
		total_particles+= numParticles;
		if(numParticles > 0){
		  ParticleSubset::iterator iter = pset->begin();
		  for(;iter != pset->end(); iter++){
		    // p_x
		    min=Min(min, md.p_x[*iter]);
		    max=Max(max, md.p_x[*iter]);
		    float temp_value = (float)(md.p_x[*iter]).x();
		    fwrite(&temp_value, sizeof(float), 1, datafile);
		    temp_value = (float)(md.p_x[*iter]).y();
		    fwrite(&temp_value, sizeof(float), 1, datafile);
		    temp_value = (float)(md.p_x[*iter]).z();
		    fwrite(&temp_value, sizeof(float), 1, datafile);
		    if (do_PTvar_all) {
		      // double data
		      for(int i = 0; i < md.pv_double_list.size(); i++) {
			double value = md.pv_double_list[i][*iter];
			d_min[i]=Min(d_min[i],value);
			d_max[i]=Max(d_max[i],value);
			temp_value = (float)value;
			fwrite(&temp_value, sizeof(float), 1, datafile);
		      }
		      // vector data
		      for(int i = 0; i < md.pv_vector_list.size(); i++) {
			double value = md.pv_vector_list[i][*iter].length();
			v_min[i]=Min(v_min[i],value);
			v_max[i]=Max(v_max[i],value);
			temp_value = (float)value;
			fwrite(&temp_value, sizeof(float), 1, datafile);
		      }
		      if (do_patch) {
			temp_value = (float)patch->getID();
			fwrite(&temp_value, sizeof(float), 1, datafile);
		      }
		      if (do_material) {
			temp_value = (float)m;
			fwrite(&temp_value, sizeof(float), 1, datafile);
		      }
		    }
		  }
		}
	      }
	      
	      //--------------------------------------------------
	      // write the header file

	      if (do_verbose)
		cerr << "---Writing header file\n";
	      if (data_found) {
		fprintf(headerfile,"%d\n",total_particles);
		fprintf(headerfile,"%g\n",(max.x()-min.x())/total_particles);
		fprintf(headerfile,"%g %g\n",min.x(),max.x());
		fprintf(headerfile,"%g %g\n",min.y(),max.y());
		fprintf(headerfile,"%g %g\n",min.z(),max.z());
		if (do_PTvar_all) {
		  for(int i = 0; i < d_min.size(); i++) {
		    fprintf(headerfile,"%g %g\n",d_min[i],d_max[i]);
		  }
		  for(int i = 0; i < v_min.size(); i++) {
		    fprintf(headerfile,"%g %g\n",v_min[i],v_max[i]);
		  }
		  if (do_patch) {
		    fprintf(headerfile,"%g %g\n",(float)patch->getID(),(float)patch->getID());
		  }
		  if (do_material) {
		    fprintf(headerfile,"%g %g\n",0.0,(float)material_data_list.size());
		  }
		}
	      }
	      fclose(datafile);
	      fclose(headerfile);
	    }
	    fprintf(filelist,"</PATCH>\n");
	  } // end patch
	} // end level
	fprintf(filelist,"</TIMESTEP>\n");
      } // end timestep
      fclose(filelist);
    } // end do_rtdata
  } catch (Exception& e) {
    cerr << "Caught exception: " << e.message() << '\n';
    abort();
  } catch(...){
    cerr << "Caught unknown exception\n";
    abort();
  }
}

//
// $Log$
// Revision 1.7  2000/06/21 21:05:55  bigler
// Made it so that when writing particle data it puts all the files for one timestep in a single subdirectory.
// Also added command line options to output data on a range of timesteps.  This is good for all output formats.
//
// Revision 1.6  2000/06/15 12:58:51  bigler
// Added functionality to output particle variable data for the real-time raytracer
//
// Revision 1.5  2000/06/08 20:58:42  bigler
// Added support to ouput data for the reat-time raytracer.
//
// Revision 1.4  2000/05/30 20:18:39  sparker
// Changed new to scinew to help track down memory leaks
// Changed region to patch
//
// Revision 1.3  2000/05/21 08:19:04  sparker
// Implement NCVariable read
// Do not fail if variable type is not known
// Added misc stuff to makefiles to remove warnings
//
// Revision 1.2  2000/05/20 19:54:52  dav
// browsing puda, added a couple of things to usage, etc.
//
// Revision 1.1  2000/05/20 08:09:01  sparker
// Improved TypeDescription
// Finished I/O
// Use new XML utility libraries
//
//
