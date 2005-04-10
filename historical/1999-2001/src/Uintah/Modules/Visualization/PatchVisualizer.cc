/*
 *  PatchVisualizer.cc:  Displays Patch boundaries
 *
 *  This module is used display the patch boundaries.  There will be
 *  different methods of visualizing the boundaries (solid color, by x,
 *  y, and z, and random).  The coloring by x,y,z,random is based off of
 *  a color map passed in.  If no colormap is passed in then solid color
 *  coloring will be used.
 *
 *  One can change values for up to 6 different levels.  After that, levels
 *  6 and above will use the settings for level 5.
 *  
 *
 *  Written by:
 *   James Bigler
 *   Department of Computer Science
 *   University of Utah
 *   January 2001
 *
 *  Copyright (C) 2001 SCI Group
 */

#include <PSECore/Dataflow/Module.h>
#include <PSECore/Datatypes/GeometryPort.h>
#include <PSECore/Datatypes/ColorMapPort.h>
#include <SCICore/Geom/GeomGroup.h>
#include <SCICore/Geom/GeomLine.h>
#include <SCICore/Geom/Material.h>
#include <SCICore/Geometry/BBox.h>
#include <SCICore/Geometry/Point.h>
#include <SCICore/Malloc/Allocator.h>
#include <SCICore/TclInterface/TCLvar.h>
#include <Uintah/Components/MPM/Util/Matrix3.h>
#include <Uintah/Datatypes/ArchivePort.h>
#include <Uintah/Datatypes/Archive.h>
#include <Uintah/Grid/GridP.h>
#include <Uintah/Grid/Grid.h>
#include <Uintah/Grid/Level.h>
#include <Uintah/Grid/Patch.h>
#include <vector>
#include <sstream>
#include <iostream>
#include <float.h>
#include <time.h>
#include <stdlib.h>

namespace PSECommon {
namespace Modules {

using namespace PSECore::Dataflow;
using namespace PSECore::Datatypes;
using namespace SCICore::GeomSpace;
using namespace SCICore::Geometry;
using namespace SCICore::TclInterface;
using namespace Uintah;
using namespace Uintah::Datatypes;
using namespace std;

#define SOLID 0
#define X_DIM 1
#define Y_DIM 2
#define Z_DIM 3
#define RANDOM 4
  
class PatchVisualizer : public Module {
public:
  PatchVisualizer(const clString& id);
  virtual ~PatchVisualizer();
  virtual void execute();
  void tcl_command(TCLArgs& args, void* userdata);

private:
  void addBoxGeometry(GeomLines* edges, const Box& box,
		      const Vector & change);
  bool getGrid();
  void setupColors();
  int getScheme(clString scheme);
  MaterialHandle getColor(clString color, float value);
  
  ArchiveIPort* in;
  GeometryOPort* ogeom;
  ColorMapIPort *inColorMap;
  MaterialHandle level_color[6];
  int level_color_scheme[6];
  
  TCLstring level0_grid_color;
  TCLstring level1_grid_color;
  TCLstring level2_grid_color;
  TCLstring level3_grid_color;
  TCLstring level4_grid_color;
  TCLstring level5_grid_color;
  TCLstring level0_color_scheme;
  TCLstring level1_color_scheme;
  TCLstring level2_color_scheme;
  TCLstring level3_color_scheme;
  TCLstring level4_color_scheme;
  TCLstring level5_color_scheme;
  TCLint nl;
  TCLint patch_seperate;
  
  vector< double > times;
  DataArchive* archive;
  int old_generation;
  int old_timestep;
  int numLevels;
  GridP grid;
  
};

static clString widget_name("PatchVisualizer Widget");
 
extern "C" Module* make_PatchVisualizer(const clString& id) {
  return scinew PatchVisualizer(id);
}

PatchVisualizer::PatchVisualizer(const clString& id)
: Module("PatchVisualizer", id, Filter),
  level0_grid_color("level0_grid_color",id, this),
  level1_grid_color("level1_grid_color",id, this),
  level2_grid_color("level2_grid_color",id, this),
  level3_grid_color("level3_grid_color",id, this),
  level4_grid_color("level4_grid_color",id, this),
  level5_grid_color("level5_grid_color",id, this),
  level0_color_scheme("level0_color_scheme",id, this),
  level1_color_scheme("level1_color_scheme",id, this),
  level2_color_scheme("level2_color_scheme",id, this),
  level3_color_scheme("level3_color_scheme",id, this),
  level4_color_scheme("level4_color_scheme",id, this),
  level5_color_scheme("level5_color_scheme",id, this),
  nl("nl",id,this),
  patch_seperate("patch_seperate",id,this),
  grid(NULL),
  old_generation(-1), old_timestep(0), numLevels(0)
{

  // Create the input port
  in=scinew ArchiveIPort(this, "Data Archive", ArchiveIPort::Atomic);
  add_iport(in);

  // color map
  inColorMap = scinew ColorMapIPort( this, "ColorMap",
				     ColorMapIPort::Atomic);
  add_iport( inColorMap);

  // Create the output port
  ogeom=scinew GeometryOPort(this, "Geometry", GeometryIPort::Atomic);
  add_oport(ogeom);

  // seed the random number generator 
  time_t t;
  time(&t);
  srand(t);
  
}

PatchVisualizer::~PatchVisualizer()
{
}

// assigns a grid based on the archive and the timestep to grid
// return true if there was a new grid, false otherwise
bool PatchVisualizer::getGrid()
{
  ArchiveHandle handle;
  if(!in->get(handle)){
    std::cerr<<"Didn't get a handle\n";
    grid = NULL;
    return false;
  }

  // access the grid through the handle and dataArchive
  archive = (*(handle.get_rep()))();
  int new_generation = (*(handle.get_rep())).generation;
  bool archive_dirty =  new_generation != old_generation;
  int timestep = (*(handle.get_rep())).timestep();
  if (archive_dirty) {
    old_generation = new_generation;
    vector< int > indices;
    times.clear();
    archive->queryTimesteps( indices, times );
    // set old_timestep to something that will cause a new grid
    // to be queried.
    old_timestep = -1;
  }
  if (timestep != old_timestep) {
    double time = times[timestep];
    grid = archive->queryGrid(time);
    old_timestep = timestep;
    return true;
  }
  return false;
}

// adds the lines to edges that make up the box defined by box 
void PatchVisualizer::addBoxGeometry(GeomLines* edges, const Box& box,
				     const Vector & change)
{
  Point min = box.lower() + change;
  Point max = box.upper() - change;
  
  edges->add(Point(min.x(), min.y(), min.z()),
	     Point(min.x(), min.y(), max.z()));
  edges->add(Point(min.x(), min.y(), min.z()),
	     Point(min.x(), max.y(), min.z()));
  edges->add(Point(min.x(), min.y(), min.z()),
	     Point(max.x(), min.y(), min.z()));
  edges->add(Point(max.x(), min.y(), min.z()),
	     Point(max.x(), max.y(), min.z()));
  edges->add(Point(max.x(), min.y(), min.z()),
	     Point(max.x(), min.y(), max.z()));
  edges->add(Point(min.x(), max.y(), min.z()),
	     Point(max.x(), max.y(), min.z()));
  edges->add(Point(min.x(), max.y(), min.z()),
	     Point(min.x(), max.y(), max.z()));
  edges->add(Point(min.x(), min.y(), min.z()),
	     Point(min.x(), min.y(), max.z()));
  edges->add(Point(min.x(), min.y(), max.z()),
	     Point(max.x(), min.y(), max.z()));
  edges->add(Point(min.x(), min.y(), max.z()),
	     Point(min.x(), max.y(), max.z()));
  edges->add(Point(max.x(), max.y(), min.z()),
	     Point(max.x(), max.y(), max.z()));
  edges->add(Point(max.x(), min.y(), max.z()),
	     Point(max.x(), max.y(), max.z()));
  edges->add(Point(min.x(), max.y(), max.z()),
	     Point(max.x(), max.y(), max.z()));
}

// grabs the colors form the UI and assigns them to the local colors
void PatchVisualizer::setupColors() {
  ////////////////////////////////
  // Set up the colors used

  // assign some colors to the different levels
  level_color[0] = getColor(level0_grid_color.get(),1);
  level_color[1] = getColor(level1_grid_color.get(),1);
  level_color[2] = getColor(level2_grid_color.get(),1);
  level_color[3] = getColor(level3_grid_color.get(),1);
  level_color[4] = getColor(level4_grid_color.get(),1);
  level_color[5] = getColor(level5_grid_color.get(),1);

  // extract the coloring schemes
  level_color_scheme[0] = getScheme(level0_color_scheme.get());
  level_color_scheme[1] = getScheme(level1_color_scheme.get());
  level_color_scheme[2] = getScheme(level2_color_scheme.get());
  level_color_scheme[3] = getScheme(level3_color_scheme.get());
  level_color_scheme[4] = getScheme(level4_color_scheme.get());
  level_color_scheme[5] = getScheme(level5_color_scheme.get());
}

// returns an int corresponding to the string scheme
int PatchVisualizer::getScheme(clString scheme) {
  if (scheme == "solid")
    return SOLID;
  if (scheme == "x")
    return X_DIM;
  if (scheme == "y")
    return Y_DIM;
  if (scheme == "z")
    return Z_DIM;
  if (scheme == "random")
    return RANDOM;
  cerr << "PatchVisualizer: Warning: Unknown color scheme!\n";
  return SOLID;
}

// returns a MaterialHandle where the hue is based on the clString passed in
// and the value is based on value.
MaterialHandle PatchVisualizer::getColor(clString color, float value) {
  if (color == "red")
    return scinew Material(Color(0,0,0), Color(value,0,0),
			   Color(.5,.5,.5), 20);
  else if (color == "green")
    return scinew Material(Color(0,0,0), Color(0,value,0),
			   Color(.5,.5,.5), 20);
  else if (color == "yellow")
    return scinew Material(Color(0,0,0), Color(value,value,0),
			   Color(.5,.5,.5), 20);
  else if (color == "magenta")
    return scinew Material(Color(0,0,0), Color(value,0,value),
			   Color(.5,.5,.5), 20);
  else if (color == "cyan")
    return scinew Material(Color(0,0,0), Color(0,value,value),
			   Color(.5,.5,.5), 20);
  else if (color == "blue")
    return scinew Material(Color(0,0,0), Color(0,0,value),
			   Color(.5,.5,.5), 20);
  else
    return scinew Material(Color(0,0,0), Color(value,value,value),
			   Color(.5,.5,.5), 20);
}

void PatchVisualizer::execute()
{

  ogeom->delAll();

  // Get the handle on the grid and the number of levels
  bool new_grid = getGrid();
  if(!grid)
    return;
  int numLevels = grid->numLevels();

  
  ColorMapHandle cmap;
  int have_cmap=inColorMap->get( cmap );

  // setup the tickle stuff
  setupColors();
  if (new_grid) {
    nl.set(numLevels);
    clString visible;
    TCL::eval(id + " isVisible", visible);
    if ( visible == "1") {
      TCL::execute(id + " Rebuild");
      
      TCL::execute("update idletasks");
      reset_vars();
    }
  }

  //////////////////////////////////////////////////////////////////
  // Extract the geometry from the archive
  //////////////////////////////////////////////////////////////////
  
  // it's faster when we don't use push_back and know the exact size
  vector< vector<Box> > patches(numLevels);

  // initialize the min and max for the entire region.
  // Note: there are already function that compute this, but they currently
  //       iterate over the same data.  Rather than interate over all the data
  //       twice I will compute min/max here while I iterate over the data.
  //       This will make the code faster.
  Point min(DBL_MAX,DBL_MAX,DBL_MAX), max(DBL_MIN,DBL_MIN,DBL_MIN);

  for(int l = 0;l<numLevels;l++){
    LevelP level = grid->getLevel(l);

    vector<Box> patch_list(level->numPatches());
    Level::const_patchIterator iter;
    //---------------------------------------
    // for each patch in the level
    int i = 0;
    for(iter=level->patchesBegin();iter != level->patchesEnd(); iter++){
      const Patch* patch=*iter;
      Box box = patch->getBox();
      patch_list[i] = box;

      // determine boundaries
      if (box.upper().x() > max.x())
	max.x(box.upper().x());
      if (box.upper().y() > max.y())
	max.y(box.upper().y());
      if (box.upper().z() > max.z())
	max.z(box.upper().z());
      
      if (box.lower().x() < min.x())
	min.x(box.lower().x());
      if (box.lower().y() < min.y())
	min.y(box.lower().y());
      if (box.lower().z() < min.z())
	min.z(box.lower().z());
      i++;
    }
    patches[l] = patch_list;
  }
  // the change in size of the patches.
  // This is based on the actual size of the data, so it should be a pretty
  // good guess.
  Vector change_v; 
  if (patch_seperate.get() == 1) {
    double change_factor = 100;
    change_v = Vector((max.x() - min.x())/change_factor,
		      (max.y() - min.y())/change_factor,
		      (max.z() - min.z())/change_factor);
  } else {
    change_v = Vector(0,0,0);
  }

  //////////////////////////////////////////////////////////////////
  // Create the geometry for export
  //////////////////////////////////////////////////////////////////
  
  // loops over all the levels
  for(int l = 0;l<patches.size();l++){
    // there can be up to 6 levels only, after that the value of the last
    // level is used.
    int level_index = l;
    if (level_index >= 6)
      level_index = 5;

    // all the geometry for this level.  It will be added to the screen graph
    // seperately in order to be able to select and unselect them from
    // the renderer.
    GeomGroup *level_geom = scinew GeomGroup();

    int scheme;
    // if we don't have a colormap, then use solid color coloring
    if (have_cmap)
      scheme = level_color_scheme[level_index];
    else
      scheme = SOLID;
    
    // generate the geometry based the coloring scheme
    switch (scheme) {
    case SOLID: {
      
      // edges is all the edges made up all the patches in the level
      GeomLines* edges = scinew GeomLines();
      
      //---------------------------------------
      // for each patch in the level
      for(int i = 0; i < patches[l].size(); i++){
	addBoxGeometry(edges, patches[l][i], change_v);
      }
      
      level_geom->add(scinew GeomMaterial(edges, level_color[level_index]));
    }
    break;
    case X_DIM:
      cmap->Scale(min.x(),max.x());

      //---------------------------------------
      // for each patch in the level
      for(int i = 0; i < patches[l].size(); i++){
	GeomLines* edges = scinew GeomLines();
	addBoxGeometry(edges, patches[l][i], change_v);
	level_geom->add(scinew GeomMaterial(edges,
			       cmap->lookup(patches[l][i].lower().x())));
      }
      
      break;
    case Y_DIM:
      cmap->Scale(min.y(),max.y());

      //---------------------------------------
      // for each patch in the level
      for(int i = 0; i < patches[l].size(); i++){
	GeomLines* edges = scinew GeomLines();
	addBoxGeometry(edges, patches[l][i], change_v);
	level_geom->add(scinew GeomMaterial(edges,
			       cmap->lookup(patches[l][i].lower().y())));
      }
      
      break;
    case Z_DIM:
      cmap->Scale(min.z(),max.z());

      //---------------------------------------
      // for each patch in the level
      for(int i = 0; i < patches[l].size(); i++){
	GeomLines* edges = scinew GeomLines();
	addBoxGeometry(edges, patches[l][i], change_v);
	level_geom->add(scinew GeomMaterial(edges,
			       cmap->lookup(patches[l][i].lower().z())));
      }
      
      break;
    case RANDOM:
      // drand48 returns valued between 0 and 1
      cmap->Scale(0, 1);

      //---------------------------------------
      // for each patch in the level
      for(int i = 0; i < patches[l].size(); i++){
	GeomLines* edges = scinew GeomLines();
	addBoxGeometry(edges, patches[l][i], change_v);
	level_geom->add(scinew GeomMaterial(edges, cmap->lookup(drand48())));
      }
      
      break;
    } // end of switch
    
    // add all the edges for the level
    ostringstream name_edges;
    name_edges << "Patches - level " << l;
    ogeom->addObj(level_geom, name_edges.str().c_str());
  }
}

// This is called when the tcl code explicity calls a function besides
// needexecute.
void PatchVisualizer::tcl_command(TCLArgs& args, void* userdata)
{
  if(args.count() < 2) {
    args.error("Streamline needs a minor command");
    return;
  }
  else {
    Module::tcl_command(args, userdata);
  }
}


} // End namespace Modules
} // End namespace PSECommon


