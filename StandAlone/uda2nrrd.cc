/*
 *  puda.cc: Print out a uintah data archive
 *
 *  Written by:
 *   James L. Bigler
 *   Department of Computer Science
 *   University of Utah
 *   April 2003
 *
 *  Copyright (C) 2003 U of U
 */

#include <Packages/Uintah/Core/DataArchive/DataArchive.h>
#include <Packages/Uintah/Core/Grid/Grid.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/NodeIterator.h>
#include <Packages/Uintah/Core/Grid/CellIterator.h>
#include <Packages/Uintah/Core/Grid/SFCXVariable.h>
#include <Packages/Uintah/Core/Grid/SFCYVariable.h>
#include <Packages/Uintah/Core/Grid/SFCZVariable.h>
#include <Packages/Uintah/Core/Math/Matrix3.h>
#include <Packages/Uintah/Core/Grid/ShareAssignParticleVariable.h>
#include <Packages/Uintah/Core/Grid/Box.h>
#include <Packages/Uintah/Core/Disclosure/TypeDescription.h>
#include <Packages/Uintah/Dataflow/Modules/Selectors/PatchToField.h>
#include <Packages/Teem/Dataflow/Modules/DataIO/ConvertToNrrd.h>
#include <Core/Math/MinMax.h>
#include <Core/Geometry/Point.h>
#include <Core/Geometry/Vector.h>
#include <Core/Geometry/BBox.h>
#include <Core/Datatypes/LatVolField.h>
#include <Core/Datatypes/LatVolMesh.h>
#include <Core/OS/Dir.h>
#include <Core/Thread/Thread.h>
#include <Core/Thread/Semaphore.h>
#include <Core/Thread/Mutex.h>
#include <Core/Util/DynamicLoader.h>
#include <Core/Persistent/Pstreams.h>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <algorithm>

using namespace SCIRun;
using namespace std;
using namespace Uintah;

bool verbose = false;
bool quiet = false;

class QueryInfo {
public:
  QueryInfo() {}
  QueryInfo(DataArchive* archive,
	    LevelP level,
	    string varname,
	    int mat,
	    double time,
	    const Uintah::TypeDescription *type):
    archive(archive), level(level), varname(varname), mat(mat), time(time),
    type(type)
  {}
  
  DataArchive* archive;
  LevelP level;
  string varname;
  int mat;
  double time;
  const Uintah::TypeDescription *type;
};

void usage(const std::string& badarg, const std::string& progname)
{
    if(badarg != "")
	cerr << "Error parsing argument: " << badarg << endl;
    cerr << "Usage: " << progname << " [options] "
	 << "-uda <archive file>\n\n";
    cerr << "Valid options are:\n";
    cerr << "  -h,--help\n";
    cerr << "  -v,--variable <variable name>\n";
    cerr << "  -m,--material <material number> [defaults to 0]\n";
    cerr << "  -l,--level <level index> [defaults to 0]\n";
    cerr << "  -o,--out <outputfilename> [defaults to data]\n";
    cerr << "  -dh,--detatched-header - writes the data with detached headers.  The default is to not do this.";
    //    cerr << "  -binary (prints out the data in binary)\n";
    cerr << "  -tlow,--timesteplow [int] (only outputs timestep from int) [defaults to 0]\n";
    cerr << "  -thigh,--timestephigh [int] (only outputs timesteps up to int) [defaults to last timestep]\n";
    cerr << "  -vv,--verbose (prints status of output)\n";
    cerr << "  -q,--quiet (very little output)\n";
    exit(1);
}

#if 0
template<class T>
void printData(DataArchive* archive, string& variable_name,
	       int material, IntVector& var_id,
               unsigned long time_step_lower, unsigned long time_step_upper,
	       ostream& out) 

{
  // set defaults for output stream
  out.setf(ios::scientific,ios::floatfield);
  out.precision(8);
  
  // for each type available, we need to query the values for the time range, 
  // variable name, and material
  vector<T> values;
  try {
    archive->query(values, variable_name, material, var_id, times[time_step_lower], times[time_step_upper]);
  } catch (const VariableNotFoundInGrid& exception) {
    cerr << "Caught VariableNotFoundInGrid Exception: " << exception.message() << endl;
    exit(1);
  }
  // Print out data
  for(unsigned int i = 0; i < values.size(); i++) {
    out << times[i] << ", " << values[i] << endl;
  }
}
#endif

template <class T, class Var>
void build_field(QueryInfo &qinfo,
		 IntVector& lo,
		 Var& /*var*/,
		 LatVolField<T>*& sfd)
{
  int max_workers = Max(Thread::numProcessors()/2, 2);
  Semaphore* thread_sema = scinew Semaphore("extractor semaphore",
					    max_workers);
  Mutex lock("build_field lock");
  
  for( Level::const_patchIterator r = qinfo.level->patchesBegin();
      r != qinfo.level->patchesEnd(); ++r){
    IntVector low, hi;
    Var v;
    qinfo.archive->query( v, qinfo.varname, qinfo.mat, *r, qinfo.time);
    if( sfd->data_at() == Field::CELL){
      low = (*r)->getCellLowIndex();
      hi = (*r)->getCellHighIndex();
    } else {
      low = (*r)->getNodeLowIndex();
      switch (qinfo.type->getSubType()->getType()) {
      case Uintah::TypeDescription::SFCXVariable:
	hi = (*r)->getSFCXHighIndex();
	break;
      case Uintah::TypeDescription::SFCYVariable:
	hi = (*r)->getSFCYHighIndex();
	break;
      case Uintah::TypeDescription::SFCZVariable:
	hi = (*r)->getSFCZHighIndex();
	break;
      case Uintah::TypeDescription::NCVariable:
	hi = (*r)->getNodeHighIndex();
	break;
      default:
	cerr << "build_field::unknown variable.\n";
	exit(1);
      } 
    } 

    IntVector range = hi - low;

    int z_min = low.z();
    int z_max = low.z() + hi.z() - low.z();
    int z_step, z, N = 0;
    if ((z_max - z_min) >= max_workers){
      // in case we have large patches we'll divide up the work 
      // for each patch, if the patches are small we'll divide the
      // work up by patch.
      int cs = 25000000;  
      int S = range.x() * range.y() * range.z() * sizeof(T);
      N = Min(Max(S/cs, 1), (max_workers-1));
    }
    N = Max(N,2);
    z_step = (z_max - z_min)/(N - 1);
    for(z = z_min ; z < z_max; z += z_step) {
      
      IntVector min_i(low.x(), low.y(), z);
      IntVector max_i(hi.x(), hi.y(), Min(z+z_step, z_max));
      
      thread_sema->down();
/*       PatchToFieldThread<Var, T> *ptft = */
/*        scinew PatchToFieldThread<Var, T>(sfd, v, lo, min_i, max_i,//low, hi, */
/*  					  thread_sema, lock); */
/*       ptft->run(); */

/*       cerr<<"low = "<<low<<", hi = "<<hi<<", min_i = "<<min_i */
/* 	  <<", max_i = "<<max_i<<endl; */
  
#if 1
      Thread *thrd = scinew Thread( 
        (scinew PatchToFieldThread<Var, T>(sfd, v, lo, min_i, max_i,// low, hi,
				      thread_sema, lock)),
	"patch_to_field_worker");
      thrd->detach();
#endif
    }
  }
  thread_sema->down(max_workers);
  if( thread_sema ) delete thread_sema;
}

// getData<CCVariable<T>, T >();
template<class Var, class T>
void getData(QueryInfo &qinfo, IntVector &low,
	     LatVolMeshHandle mesh_handle_,
	     SCIRun::Field::data_location data_at,
	     string &filename) {
  Var gridVar;
  LatVolField<T> *vfd =
    scinew LatVolField<T>( mesh_handle_, data_at );
  // set the generation and timestep in the field
  if (!quiet) cout << "Building Field from uda data\n";
  build_field(qinfo, low, gridVar, vfd );

  // Convert the field to a nrrd
  if (!quiet) cout << "Converting field to nrrd.\n";
  const SCIRun::TypeDescription *td = vfd->get_type_description();
  CompileInfoHandle ci = ConvertToNrrdBase::get_compile_info(td);
  DynamicAlgoHandle algo;
  if (!DynamicLoader::scirun_loader().get(*(ci.get_rep()), algo)) {
    cerr << "Could not compile field to nrrd code.  Exiting.\n";
    exit(1);
  }
  
  string lab("uda2nrrd");
  ConvertToNrrdBase *nrrd_algo = (ConvertToNrrdBase*)(algo.get_rep());
  NrrdDataHandle onrrd_handle = nrrd_algo->convert_to_nrrd(vfd, lab);
  if (!quiet) cout << "Done converting field to nrrd.\n";
  // Write out the nrrd
  
  Piostream* stream;
  string ft("Binary");
  if (ft == "Binary")
  {
    stream = scinew BinaryPiostream(filename, Piostream::Write);
  }
  else
  {
    stream = scinew TextPiostream(filename, Piostream::Write);
  }
    
  if (stream->error()) {
    cerr << "Could not open file ("<<filename<<" for writing\n";
    exit(1);
  } else {
    // Write the file
    Pio(*stream, onrrd_handle); // wlll also write out a separate nrrd.
    delete stream; 
  } 
  
  
  return;
}

// getVariable<double>();
template<class T>
void getVariable(QueryInfo &qinfo, IntVector &low,
		 IntVector &range, BBox &box, string &filename) {
		 
  LatVolMeshHandle mesh_handle_;
  switch( qinfo.type->getType() ) {
  case Uintah::TypeDescription::CCVariable:
    mesh_handle_ = scinew LatVolMesh(range.x(), range.y(),
				     range.z(), box.min(),
				     box.max());
    getData<CCVariable<T>, T>(qinfo, low, mesh_handle_, Field::CELL,
			      filename);
    break;
  case Uintah::TypeDescription::NCVariable:
    mesh_handle_ = scinew LatVolMesh(range.x(), range.y(),
				     range.z(), box.min(),
				     box.max());
    getData<NCVariable<T>, T>(qinfo, low, mesh_handle_, Field::NODE,
			      filename);
    break;
#if 0
  case Uintah::TypeDescription::SFCXVariable:
    mesh_handle_ = scinew LatVolMesh(range.x(), range.y()-1,
				     range.z()-1, box.min(),
				     box.max());
    getData<SFCXVariable<T>, T>(qinfo, low, mesh_handle_, Field::NODE);
    break;
  case Uintah::TypeDescription::SFCYVariable:
    mesh_handle_ = scinew LatVolMesh(range.x()-1, range.y(),
				     range.z()-1, box.min(),
				     box.max());
    getData<SFCYVariable<T>, T>(qinfo, low, mesh_handle_, Field::NODE);
    break;
  case Uintah::TypeDescription::SFCZVariable:
    mesh_handle_ = scinew LatVolMesh(range.x()-1, range.y()-1,
				     range.z(), box.min(),
				     box.max());
    getData<SFCZVariable<T>, T>(qinfo, low, mesh_handle_, Field::NODE);
    break;
#endif
  default:
    cerr << "Type is unknown.\n";
    return;
    break;
  
  }
}


int main(int argc, char** argv)
{
  /*
   * Default values
   */
  bool do_binary=false;

  unsigned long time_step_lower = 0;
  // default to be last timestep, but can be set to 0
  unsigned long time_step_upper = (unsigned long)-1;

  string input_uda_name;
  string output_file_name("data");
  IntVector var_id(0,0,0);
  string variable_name;
  // It will use the first material found unless other indicated.
  int material = -1;
  int level_index = 0;
  
  /*
   * Parse arguments
   */
  for(int i=1;i<argc;i++){
    string s=argv[i];
    if(s == "-v" || s == "--variable") {
      variable_name = string(argv[++i]);
    } else if (s == "-m" || s == "--material") {
      material = atoi(argv[++i]);
    } else if (s == "-l" || s == "--level") {
      level_index = atoi(argv[++i]);
    } else if (s == "-vv" || s == "--verbose") {
      verbose = true;
    } else if (s == "-q" || s == "--quiet") {
      quiet = true;
    } else if (s == "-tlow" || s == "--timesteplow") {
      time_step_lower = strtoul(argv[++i],(char**)NULL,10);
    } else if (s == "-thigh" || s == "--timestephigh") {
      time_step_upper = strtoul(argv[++i],(char**)NULL,10);
    } else if (s == "-i" || s == "--index") {
      int x = atoi(argv[++i]);
      int y = atoi(argv[++i]);
      int z = atoi(argv[++i]);
      var_id = IntVector(x,y,z);
    } else if( (s == "-h") || (s == "--help") ) {
      usage( "", argv[0] );
    } else if (s == "-uda") {
      input_uda_name = string(argv[++i]);
    } else if (s == "-o" || s == "--out") {
      output_file_name = string(argv[++i]);
    } else if(s == "-binary") {
      do_binary=true;
    } else {
      usage(s, argv[0]);
    }
  }
  
  if(input_uda_name == ""){
    cerr << "No archive file specified\n";
    usage("", argv[0]);
  }

  try {
    DataArchive* archive = scinew DataArchive(input_uda_name);

    //////////////////////////////////////////////////////////
    // Get the variables and types
    vector<string> vars;
    vector<const Uintah::TypeDescription*> types;

    archive->queryVariables(vars, types);
    ASSERTEQ(vars.size(), types.size());
    if (verbose) cout << "There are " << vars.size() << " variables:\n";
    bool var_found = false;
    unsigned int var_index = 0;
    for (;var_index < vars.size(); var_index++) {
      if (variable_name == vars[var_index]) {
	var_found = true;
	break;
      }
    }
    
    if (!var_found) {
      cerr << "Variable \"" << variable_name << "\" was not found.\n";
      cerr << "If a variable name was not specified try -var [name].\n";
      cerr << "Possible variable names are:\n";
      var_index = 0;
      for (;var_index < vars.size(); var_index++) {
	cout << "vars[" << var_index << "] = " << vars[var_index] << endl;
      }
      cerr << "Aborting!!\n";
      exit(-1);
      //      var = vars[0];
    }

    if (!quiet) cout << "Extracing data for "<<vars[var_index] << ": " << types[var_index]->getName() <<endl;

    ////////////////////////////////////////////////////////
    // Get the times and indices.

    vector<int> index;
    vector<double> times;
    
    // query time info from dataarchive
    archive->queryTimesteps(index, times);
    ASSERTEQ(index.size(), times.size());
    if (!quiet) cout << "There are " << index.size() << " timesteps:\n";
    
    //------------------------------
    // figure out the lower and upper bounds on the timesteps
    if (time_step_lower >= times.size()) {
      cerr << "timesteplow must be between 0 and " << times.size()-1 << endl;
      exit(1);
    }
    
    // set default max time value
    if (time_step_upper == (unsigned long)-1) {
      if (verbose)
	cout <<"Initializing time_step_upper to "<<times.size()-1<<"\n";
      time_step_upper = times.size() - 1;
    }
    
    if (time_step_upper >= times.size() || time_step_upper < time_step_lower) {
      cerr << "timestephigh("<<time_step_lower<<") must be greater than " << time_step_lower 
	   << " and less than " << times.size()-1 << endl;
      exit(1);
    }
    
    if (!quiet) cout << "outputting for times["<<time_step_lower<<"] = " << times[time_step_lower]<<" to times["<<time_step_upper<<"] = "<<times[time_step_upper] << endl;

    ////////////////////////////////////////////////////////
    // Loop over each timestep
    for (unsigned long time = time_step_lower; time <= time_step_upper; time++){

      // Check the level index
      double current_time = times[time];
      GridP grid = archive->queryGrid(current_time);
      if (level_index >= grid->numLevels() || level_index < 0) {
	cerr << "level index is bad ("<<level_index<<").  Should be between 0 and "<<grid->numLevels()<<".\n";
	cerr << "Trying next timestep.\n";
	continue;
      }
    
      ///////////////////////////////////////////////////
      // Check the material number.

      LevelP level = grid->getLevel(level_index);
      const Patch* patch = *(level->patchesBegin());
      ConsecutiveRangeSet matls =
	archive->queryMaterials(variable_name, patch, current_time);
      
      int mat_num;
      if (material == -1) {
	mat_num = *(matls.begin());
      } else {
	unsigned int mat_index = 0;
	for (ConsecutiveRangeSet::iterator matlIter = matls.begin();
	     matlIter != matls.end(); matlIter++){
	  int matl = *matlIter;
	  if (matl == material) {
	    mat_num = matl;
	    break;
	  }
	  mat_index++;
	}
	if (mat_index == matls.size()) {
	  // then we didn't find the right material
	  cerr << "Didn't find material " << material << " in the data.\n";
	  cerr << "Trying next timestep.\n";
	  continue;
	}
      }
      if (!quiet) cout << "Extracting data for material "<<mat_num<<".\n";
      
      IntVector hi, low, range;
      level->findIndexRange(low, hi);
      range = hi - low;
      BBox box;
      level->getSpatialRange(box);
      
      // get type and subtype of data
      const Uintah::TypeDescription* td = types[var_index];
      const Uintah::TypeDescription* subtype = td->getSubType();
    
      QueryInfo qinfo(archive, level, variable_name, mat_num, current_time,
		      td);

      // Figure out the filename
      char filename_num[200];
      sprintf(filename_num, "%04lu", time);
      string filename(output_file_name + filename_num);
    
      switch (subtype->getType()) {
      case Uintah::TypeDescription::double_type:
	getVariable<double>(qinfo, low, range, box, filename);
	break;
      case Uintah::TypeDescription::int_type:
	getVariable<int>(qinfo, low, range, box, filename);
	break;
      case Uintah::TypeDescription::Vector:
	getVariable<Vector>(qinfo, low, range, box, filename);
	break;
      case Uintah::TypeDescription::Matrix3:
      case Uintah::TypeDescription::bool_type:
      case Uintah::TypeDescription::short_int_type:
      case Uintah::TypeDescription::long_type:
      case Uintah::TypeDescription::long64_type:
	cerr << "Subtype "<<subtype->getName()<<" is not implemented\n";
	exit(1);
	break;
      default:
	cerr << "Unknown subtype\n";
	exit(1);
      }
    } // end time step iteration
    
  } catch (Exception& e) {
    cerr << "Caught exception: " << e.message() << endl;
    exit(1);
  } catch(...){
    cerr << "Caught unknown exception\n";
    exit(1);
  }
}
