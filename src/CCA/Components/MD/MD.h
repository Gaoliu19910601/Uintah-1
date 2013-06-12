/*

 The MIT License

 Copyright (c) 1997-2013 The University of Utah

 License for the specific language governing rights and limitations under
 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.

 */

#ifndef UINTAH_MD_H
#define UINTAH_MD_H

#include <Core/Parallel/UintahParallelComponent.h>
#include <CCA/Ports/SimulationInterface.h>
#include <CCA/Ports/Output.h>
#include <CCA/Components/MD/MDLabel.h>
#include <CCA/Components/MD/NonBonded.h>
#include <CCA/Components/MD/Electrostatics.h>

#include <vector>

namespace Uintah {

  typedef std::complex<double> dblcomplex;

  class Point;
  class Vector;
  class SimpleMaterial;
  class SPME;

  /**
   *  @class MD
   *  @ingroup MD
   *  @author Alan Humphrey and Justin Hooper
   *  @date   December, 2012
   *
   *  @brief
   *
   *  @param
   */
  class MD : public UintahParallelComponent, public SimulationInterface {

    public:

      /**
       * @brief
       * @param
       * @return
       */
      enum IntegratorType {
        Explicit, Implicit,
      };

      /**
       * @brief
       * @param
       */
      MD(const ProcessorGroup* myworld);

      /**
       * @brief
       * @param
       */
      virtual ~MD();

      /**
       * @brief
       * @param
       * @return
       */
      virtual void problemSetup(const ProblemSpecP& params,
                                const ProblemSpecP& restart_prob_spec,
                                GridP& grid,
                                SimulationStateP&);

      /**
       * @brief
       * @param
       * @return
       */
      virtual void scheduleInitialize(const LevelP& level,
                                      SchedulerP& sched);

      /**
       * @brief
       * @param
       * @return
       */
      virtual void scheduleComputeStableTimestep(const LevelP& level,
                                                 SchedulerP&);

      /**
       * @brief
       * @param
       * @return
       */
      virtual void scheduleTimeAdvance(const LevelP& level,
                                       SchedulerP&);

    protected:

      /**
       * @brief
       * @param
       * @return
       */
      void scheduleCalculateNonBondedForces(SchedulerP& sched,
                                            const PatchSet* patches,
                                            const MaterialSet* matls,
                                            const LevelP& level);

      /**
       * @brief
       * @param
       * @return
       */
      void scheduleInterpolateParticlesToGrid(SchedulerP&,
                                              const PatchSet*,
                                              const MaterialSet*);

      /**
       * @brief
       * @param
       * @return
       */
      void schedulePerformElectrostatics(SchedulerP& sched,
                                         const PatchSet* patched,
                                         const MaterialSet* matls,
                                         const LevelP& level);

      /**
       * @brief
       * @param
       * @return
       */
      void scheduleInterpolateToParticlesAndUpdate(SchedulerP& sched,
                                                   const PatchSet* patches,
                                                   const MaterialSet* matls);

      /**
       * @brief
       * @param
       * @return
       */
      void scheduleUpdatePosition(SchedulerP& sched,
                                  const PatchSet* patches,
                                  const MaterialSet* matls,
                                  const LevelP& level);

    private:

      /**
       * @brief
       * @param
       * @return
       */
      void initialize(const ProcessorGroup* pg,
                      const PatchSubset* patches,
                      const MaterialSubset* matls,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw);

      /**
       * @brief
       * @param
       * @return
       */
      void nonbondedInitialize(const ProcessorGroup* pg,
                               const PatchSubset* patches,
                               const MaterialSubset* matls,
                               DataWarehouse* old_dw,
                               DataWarehouse* new_dw);

      /**
       * @brief
       * @param
       * @return
       */
      void electrostaticsInitialize(const ProcessorGroup* pg,
                                    const PatchSubset* patches,
                                    const MaterialSubset* matls,
                                    DataWarehouse* old_dw,
                                    DataWarehouse* new_dw);

      /**
       * @brief
       * @param
       * @return
       */
      void registerPermanentParticleState(SimpleMaterial* matl);

      /**
       * @brief
       * @param
       * @return
       */
      void computeStableTimestep(const ProcessorGroup* pg,
                                 const PatchSubset* patches,
                                 const MaterialSubset* matls,
                                 DataWarehouse* old_dw,
                                 DataWarehouse* new_dw);

      /**
       * @brief
       * @param
       * @return
       */
      void performElectrostatics(const ProcessorGroup* pg,
                                 const PatchSubset* patches,
                                 const MaterialSubset* matls,
                                 DataWarehouse* old_dw,
                                 DataWarehouse* new_dw);

      /**
       * @brief
       * @param
       * @return
       */
      void calculateNonBondedForces(const ProcessorGroup* pg,
                                    const PatchSubset* patches,
                                    const MaterialSubset* matls,
                                    DataWarehouse* old_dw,
                                    DataWarehouse* new_dw);

      /**
       * @brief
       * @param
       * @return
       */
      void interpolateParticlesToGrid(const ProcessorGroup*,
                                      const PatchSubset* patches,
                                      const MaterialSubset* matls,
                                      DataWarehouse* old_dw,
                                      DataWarehouse* new_dw);

      /**
       * @brief
       * @param
       * @return
       */
      void interpolateToParticlesAndUpdate(const ProcessorGroup* pg,
                                           const PatchSubset* patches,
                                           const MaterialSubset* matls,
                                           DataWarehouse* old_dw,
                                           DataWarehouse* new_dw);
      /**
       * @brief
       * @param
       * @return
       */
      void updatePosition(const ProcessorGroup* pg,
                          const PatchSubset* patches,
                          const MaterialSubset* matls,
                          DataWarehouse* old_dw,
                          DataWarehouse* new_dw);

      /**
       * @brief
       * @param
       * @return
       */
      void extractCoordinates();

      /**
       * @brief
       * @param
       * @return
       */
      inline bool containsAtom(const IntVector& l,
                               const IntVector& h,
                               const Point& p) const
      {
        return ((p.x() >= l.x() && p.x() < h.x()) && (p.y() >= l.y() && p.y() < h.y()) && (p.z() >= l.z() && p.z() < h.z()));
      }

    private:

      struct Atom {
          Atom(Point pnt,
               double charge) :
              coords(pnt), charge(charge)
          {
          }
          Point coords;
          double charge;
      };

      MDLabel* d_lb;                       //!< Uintah VarLabels specific to Uintah::MD
      Output* d_dataArchiver;              //!< Handle to the Uintah data archiver
      SimulationStateP d_sharedState;      //!< Shared simulation state (global)
      SimpleMaterial* d_material;          //!< For now, this is a single material
      IntegratorType d_integrator;         //!< Integrator to use in position update of atoms
      double delt;                         //!< Simulation delta T

      string d_coordinateFile;             //!< Name of file with coordinates and charges of all atoms
      std::vector<Atom> d_atomList;        //!< Individual atom neighbor list

      Electrostatics* d_electrostatics;    //!< The simulation Electrostatics instance
      NonBonded* d_nonbonded;              //!< The simulation NonBonded instance
      MDSystem* d_system;                  //!< The global MD system

      vector<const VarLabel*> d_particleState;            //!< Atom (particle) state prior to relocation
      vector<const VarLabel*> d_particleState_preReloc;   //!< For atom (particle) relocation

      // copy constructor and assignment operator (privatized on purpose)
      MD(const MD&);
      MD& operator=(const MD&);
  };

}

#endif
