/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/

//    File   : BuildInterpolant.h
//    Author : Michael Callahan
//    Date   : June 2001

#if !defined(BuildInterpolant_h)
#define BuildInterpolant_h

#include <Core/Disclosure/TypeDescription.h>
#include <Core/Disclosure/DynamicLoader.h>

namespace SCIRun {

class BuildInterpAlgo : public DynamicAlgoBase
{
public:
  virtual FieldHandle execute(MeshBaseHandle src, MeshBaseHandle dst,
			      Field::data_location loc) = 0;

  //! support the dynamically compiled algorithm concept
  static CompileInfo *get_compile_info(const TypeDescription *msrc,
				       const TypeDescription *lsrc,
				       const TypeDescription *mdst,
				       const TypeDescription *ldst,
				       const TypeDescription *fdst);
};


template <class MSRC, class LSRC, class MDST, class LDST, class FOUT>
class BuildInterpAlgoT : public BuildInterpAlgo
{
public:
  //! virtual interface. 
  virtual FieldHandle execute(MeshBaseHandle src, MeshBaseHandle dst,
			      Field::data_location loc);
};



template <class MSRC, class LSRC, class MDST, class LDST, class FOUT>
FieldHandle
BuildInterpAlgoT<MSRC, LSRC, MDST, LDST, FOUT>::execute(MeshBaseHandle src_meshH, MeshBaseHandle dst_meshH, Field::data_location loc)
{
  MSRC *src_mesh = dynamic_cast<MSRC *>(src_meshH.get_rep());
  MDST *dst_mesh = dynamic_cast<MDST *>(dst_meshH.get_rep());
  FOUT *ofield = new FOUT(dst_mesh, loc);

  typedef typename LDST::iterator DSTITR; 
  DSTITR itr = dst_mesh->tbegin((DSTITR *)0);
  DSTITR end_itr = dst_mesh->tend((DSTITR *)0);

  while (itr != end_itr)
  {
    typename LSRC::array_type locs;
    vector<double> weights;
    Point p;

    dst_mesh->get_center(p, *itr);

    src_mesh->get_weights(p, locs, weights);

    vector<pair<typename LSRC::index_type, double> > v;
    if (weights.size() > 0)
    {
      for (unsigned int i = 0; i < locs.size(); i++)
      {
	v.push_back(pair<typename LSRC::index_type, double>
		    (locs[i], weights[i]));
      }
    }
    else
    {
      //typename LSRC::index_type index;
      //find_closest(src_mesh, (LSRC *)0, index, p);
      //v.push_back(pair<typename LSRC::index_type, double>(index, 1.0));
    }

    ofield->set_value(v, *itr);
    ++itr;
  }

  FieldHandle fh(ofield);
  return fh;
}


} // end namespace SCIRun

#endif // BuildInterpolant_h
