
/*
 *  HexMeshToGeom.cc:  Convert a HexMesh into geoemtry
 *
 *  Written by:
 *   Peter Jensen
 *   Sourced from MeshToGeom.cc by David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   April 1997
 *
 *  Copyright (C) 1997 SCI Group
 */

#include <SCICore/Datatypes/HexMesh.h>
#include <SCICore/Datatypes/ScalarFieldUG.h>
#include <SCICore/Geom/Color.h>
#include <SCICore/Geom/GeomGroup.h>
#include <SCICore/Geom/GeomLine.h>
#include <SCICore/Geom/Material.h>
#include <SCICore/Malloc/Allocator.h>
#include <PSECore/Datatypes/MeshPort.h>
#include <PSECore/Datatypes/GeometryPort.h>
#include <PSECore/Datatypes/HexMeshPort.h>

namespace SCIRun {
namespace Modules {

using namespace PSECore::Dataflow;
using namespace PSECore::Datatypes;
using namespace SCICore::Geometry;
using namespace SCICore::GeomSpace;

class HexMeshToGeom : public Module {
    HexMeshIPort* imesh;
    GeometryOPort* ogeom;

    void mesh_to_geom(const HexMeshHandle&, GeomGroup*);
public:
    HexMeshToGeom(const clString& id);
    virtual ~HexMeshToGeom();
    virtual void execute();
};

Module* make_HexMeshToGeom(const clString& id)
{
    return scinew HexMeshToGeom(id);
}

HexMeshToGeom::HexMeshToGeom(const clString& id)
: Module("HexMeshToGeom", id, Filter)
{
    // Create the input port
    imesh=scinew HexMeshIPort(this, "HexMesh", HexMeshIPort::Atomic);
    add_iport(imesh);
    ogeom=scinew GeometryOPort(this, "Geometry", GeometryIPort::Atomic);
    add_oport(ogeom);
}

HexMeshToGeom::~HexMeshToGeom()
{
}

void HexMeshToGeom::execute()
{
    HexMeshHandle mesh;    
    if (!imesh->get(mesh))
	return;

    int index, t, p1, p2;
    unsigned long l;    
    HexFace * f;
    HashTable<unsigned long, int> line_set;
    FourHexNodes c;
    GeomLines* lines = scinew GeomLines;
    
    for (index = mesh->high_face (); index >= 0; index--)
    {
      // Get a face.
    
      if ((f = mesh->find_face (index)) == NULL)
        continue;
    
      // Get the corners.
    
      c = f->corner_set ();
      
      // Loop through the pairs of corners.
      
      for (p1 = 0; p1 <= 3; p1++)
      {
        p2 = (p1 + 1) % 4 ;
        
        if (c.index[p1] == c.index[p2])
          continue;
        
        // Make a key for this pair of points.
          
        l = (c.index[p1] < c.index[p2]) ? (((unsigned long) c.index[p1]) << 16) + c.index[p2]
                                        : (((unsigned long) c.index[p2]) << 16) + c.index[p1];
        
        // If it's in the hash table, it's been added.
        
        if (line_set.lookup (l, t) != 0)
          continue;
          
        // Add the line segment.  
          
        line_set.insert (l, 1);
        
        lines->add (*(c.node[p1]), *(c.node[p2]));
      }
    }    
    
    GeomMaterial* matl=scinew GeomMaterial(lines,
					scinew Material(Color(0,0,0),
						     Color(0,.6,0), 
						     Color(.5,.5,.5), 20));
    ogeom->delAll();
    ogeom->addObj(matl, "Mesh1");
}
} // End namespace Modules
} // End namespace SCIRun


//
// $Log$
// Revision 1.1  1999/09/05 01:15:27  dmw
// added all of the old SCIRun mesh modules
//
