//  ElasticConstitutiveModel.h 
//  
//  class ConstitutiveModel ConstitutiveModel data type -- 3D - 
//  holds ConstitutiveModel
//  information for the FLIP technique:
//    This is for elastic materials
//     
//    
//    
//
//    Features:
//     
//      
//      Usage:



#ifndef __ELASTIC_CONSTITUTIVE_MODEL_H__
#define __ELASTIC_CONSTITUTIVE_MODEL_H__

#include "ConstitutiveModel.h"	
#include <Uintah/Components/MPM/Util/Matrix3.h>
#include <vector>


namespace Uintah {
   namespace MPM {
      
      class ElasticConstitutiveModel : public ConstitutiveModel {
      private:
	 // data areas
	 // Symmetric stress tensor (3 x 3 matrix)  
	 Matrix3 stressTensor;
	 // Symmetric strain tensor (3 x 3 matrix)
	 Matrix3 strainTensor;
	 // Symmetric strain increment tensor (3 x 3 matrix)
	 Matrix3 strainIncrement;
	 // Symmetric stress increment tensor (3 x 3 matrix)
	 Matrix3 stressIncrement;
	 // rotation increment (3 x 3 matrix)
	 Matrix3 rotationIncrement;

	 struct CMData {
	    // ConstitutiveModel's properties
	    // Young's Modulus
	    double YngMod;
	    // Poisson's Ratio
	    double PoiRat;
	 };
	 
      public:
	 // constructors
	 ElasticConstitutiveModel(ProblemSpecP& ps);
	 ElasticConstitutiveModel(double YM,double PR);
	 
	 // copy constructor
	 ElasticConstitutiveModel(const ElasticConstitutiveModel &cm);
	 
	 // destructor 
	 virtual ~ElasticConstitutiveModel();
	 
	 // assign the ElasticConstitutiveModel components 
	 
	 // assign the Symmetric stress tensor
	 virtual void setStressTensor(Matrix3 st);
	 // assign the strain increment tensor
	 void  setDeformationMeasure(Matrix3 strain);
	 // assign the strain increment tensor
	 void  setStrainIncrement(Matrix3 si);
	 // assign the stress increment tensor
	 void  setStressIncrement(Matrix3 si);
	 // assign the rotation increment tensor
	 void  setRotationIncrement(Matrix3 ri);
	 
	 // access the Symmetric stress tensor
	 virtual Matrix3 getStressTensor() const;
	 // access the Symmetric strain tensor
	 virtual Matrix3 getDeformationMeasure() const;
	 // access the mechanical properties
	 std::vector<double> getMechProps() const;
	 
	 // access the strain increment tensor
	 Matrix3 getStrainIncrement() const;
	 // access the stress increment tensor
	 Matrix3 getStressIncrement() const;
	 // access the rotation increment tensor
	 Matrix3 getRotationIncrement() const;
	 
	 // Return the Lame constants - used for computing sound speeds
	 double getLambda() const;
	 double getMu() const;
	 
	 
	 // Compute the various quantities of interest
	 
	 Matrix3 deformationIncrement(double time_step);
	 void computeStressIncrement();
	 void computeRotationIncrement(Matrix3 defInc);
	 //////////
	 // Basic constitutive model calculations
	 virtual void computeStressTensor(const Region* region,
					  const MPMMaterial* matl,
					  const DataWarehouseP& new_dw,
					  DataWarehouseP& old_dw);
	 
	 //////////
	 // Computation of strain energy.  Useful for tracking energy balance.
	 virtual double computeStrainEnergy(const Region* region,
					    const MPMMaterial* matl,
					    const DataWarehouseP& new_dw);
	 
	 // initialize  each particle's constitutive model data
	 virtual void initializeCMData(const Region* region,
				       const MPMMaterial* matl,
				       DataWarehouseP& new_dw);   
	 
	 virtual void addComputesAndRequires(Task* task,
					     const MPMMaterial* matl,
					     const Region* region,
					     const DataWarehouseP& old_dw,
					     DataWarehouseP& new_dw) const;

	 // class function to read correct number of parameters
	 // from the input file
	 static void readParameters(ProblemSpecP ps, double *p_array);
	 
	 // class function to write correct number of parameters
	 // to the output file
	 static void writeParameters(std::ofstream& out, double *p_array);
	 
	 // class function to read correct number of parameters
	 // from the input file, and create a new object
	 static ConstitutiveModel* readParametersAndCreate(ProblemSpecP ps);
	 
	 // member function to write correct number of parameters
	 // to output file, and to write any other particle information
	 // needed to restart the model for this particle
	 virtual void writeRestartParameters(std::ofstream& out) const;
	 
	 // member function to read correct number of parameters
	 // from the input file, and any other particle information
	 // need to restart the model for this particle 
	 // and create a new object
	 static ConstitutiveModel* readRestartParametersAndCreate(ProblemSpecP ps);
	 
	 // class function to create a new object from parameters
	 static ConstitutiveModel* create(double *p_array);
	 
	 // member function to determine the model type.
	 virtual int getType() const;
	 // member function to get model's name
	 virtual std::string getName() const;
	 // member function to get number of parameters for model
	 virtual int getNumParameters() const;
	 // member function to print parameter names for model
	 virtual void printParameterNames(std::ofstream& out) const;
	 
	 // member function to make a duplicate
	 virtual ConstitutiveModel* copy() const;
	 
	 ConstitutiveModel & operator=(const ElasticConstitutiveModel &cm);
	 
	 virtual int getSize() const;

	 CMData d_initialData;
	 const VarLabel* p_cmdata_label;
      };
      
      
   } // end namespace Components
} // end namespace Uintah


#endif  // __ELASTIC_CONSTITUTIVE_MODEL_H__ 

// $Log$
// Revision 1.9  2000/05/07 06:02:04  sparker
// Added beginnings of multiple patch support and real dependencies
//  for the scheduler
//
// Revision 1.8  2000/05/01 16:18:12  sparker
// Completed more of datawarehouse
// Initial more of MPM data
// Changed constitutive model for bar
//
// Revision 1.7  2000/04/26 06:48:16  sparker
// Streamlined namespaces
//
// Revision 1.6  2000/04/25 18:42:34  jas
// Revised the factory method and constructor to take a ProblemSpec argument
// to create a new constitutive model.
//
// Revision 1.5  2000/04/19 21:15:55  jas
// Changed BoundedArray to vector<double>.  More stuff to compile.  Critical
// functions that need access to data warehouse still have WONT_COMPILE_YET
// around the methods.
//
// Revision 1.4  2000/04/19 05:26:04  sparker
// Implemented new problemSetup/initialization phases
// Simplified DataWarehouse interface (not finished yet)
// Made MPM get through problemSetup, but still not finished
//
// Revision 1.3  2000/04/14 02:19:42  jas
// Now using the ProblemSpec for input.
//
// Revision 1.2  2000/03/20 17:17:08  sparker
// Made it compile.  There are now several #idef WONT_COMPILE_YET statements.
//
// Revision 1.1  2000/03/14 22:11:49  jas
// Initial creation of the constitutive model directory with the legacy
// constitutive model classes.
//
// Revision 1.1  2000/02/24 06:11:55  sparker
// Imported homebrew code
//
// Revision 1.1  2000/01/24 22:48:50  sparker
// Stuff may actually work someday...
//
// Revision 1.5  1999/12/17 22:05:23  guilkey
// Changed all constitutive models to take in velocityGradient and dt as
// arguments.  This allowed getting rid of velocityGradient as stored data
// in the constitutive model.  Also, in all hyperelastic models,
// deformationGradientInc was also removed from the private data.
//
// Revision 1.4  1999/11/17 22:26:36  guilkey
// Added guts to computeStrainEnergy functions for CompNeoHook CompNeoHookPlas
// and CompMooneyRivlin.  Also, made the computeStrainEnergy function non consted
// for all models.
//
// Revision 1.3  1999/11/17 20:08:47  guilkey
// Added a computeStrainEnergy function to each constitutive model
// so that we can have a valid strain energy calculation for functions
// other than the Elastic Model.  This is called from printParticleData.
// Currently, only the ElasticConstitutiveModel version gives the right
// answer, but that was true before as well.  The others will be filled in.
//
// Revision 1.2  1999/09/04 22:55:52  jas
// Added assingnment operator.
//
// Revision 1.1  1999/06/14 06:23:39  cgl
// - src/mpm/Makefile modified to work for IRIX64 or Linux
// - src/grid/Grid.cc added length to character array, since it
// 	was only 4 long, but was being sprintf'd with a 4 character
// 	number, leaving no room for the terminating 0.
// - added smpm directory. to house the samrai version of mpm.
//
// Revision 1.10  1999/05/31 19:36:13  cgl
// Work in stand-alone version of MPM:
//
// - Added materials_dat.cc in src/constitutive_model to generate the
//   materials.dat file for preMPM.
// - Eliminated references to ConstitutiveModel in Grid.cc and GeometryObject.cc
//   Now only Particle and Material know about ConstitutiveModel.
// - Added reads/writes of Particle start and restart information as member
//   functions of Particle
// - "part.pos" now has identicle format to the restart files.
//   mpm.cc modified to take advantage of this.
//
// Revision 1.9  1999/05/30 02:10:48  cgl
// The stand-alone version of ConstitutiveModel and derived classes
// are now more isolated from the rest of the code.  A new class
// ConstitutiveModelFactory has been added to handle all of the
// switching on model type.  Between the ConstitutiveModelFactory
// class functions and a couple of new virtual functions in the
// ConstitutiveModel class, new models can be added without any
// source modifications to any classes outside of the constitutive_model
// directory.  See csafe/Uintah/src/CD/src/constitutive_model/HOWTOADDANEWMODEL
// for updated details on how to add a new model.
//
// --cgl
//
// Revision 1.8  1999/04/10 00:11:01  guilkey
// Added set and access operators for constitutive model data
//
// Revision 1.7  1999/02/26 19:27:06  guilkey
// Removed unused functions.
//
// Revision 1.6  1999/02/19 20:39:52  guilkey
// Changed constitutive models to take advantage of the Matrix3 class
// for efficiency.
//
// Revision 1.5  1999/01/26 21:30:52  campbell
// Added logging capabilities
//
