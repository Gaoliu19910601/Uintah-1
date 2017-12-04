/*
 * DiscreteInterface.cc
 *
 *  Created on: Feb 18, 2017
 *      Author: jbhooper
 *
 *
 * The MIT License
 *
 * Copyright (c) 1997-2016 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <CCA/Components/MPM/Diffusion/DiffusionInterfaces/SimpleDiffusionContact.h>
using namespace Uintah;

SimpleSDInterface::SimpleSDInterface(
                                         ProblemSpecP     & probSpec  ,
                                         SimulationStateP & simState  ,
                                         MPMFlags         * mFlags    ,
                                         MPMLabel         * mpmLabel
                                        )
                                        : SDInterfaceModel(probSpec, simState,
                                                           mFlags, mpmLabel)

{
  sdInterfaceRate = VarLabel::create("g.dCdt_interface",
                                     NCVariable<double>::getTypeDescription());
}

SimpleSDInterface::~SimpleSDInterface()
{
  VarLabel::destroy(sdInterfaceRate);
}

void SimpleSDInterface::addComputesAndRequiresInterpolated(
                                                                   SchedulerP   & sched   ,
                                                             const PatchSet     * patches ,
                                                             const MaterialSet  * matls
                                                            )
{
  // Shouldn't need to directly modify the concentration.
}

void SimpleSDInterface::sdInterfaceInterpolated(
                                                  const ProcessorGroup  *         ,
                                                  const PatchSubset     * patches ,
                                                  const MaterialSubset  * matls   ,
                                                        DataWarehouse   * old_dw  ,
                                                        DataWarehouse   * new_dw
                                                 )
{

}

void SimpleSDInterface::addComputesAndRequiresDivergence(
                                                                 SchedulerP   & sched,
                                                           const PatchSet     * patches,
                                                           const MaterialSet  * matls
                                                          )
{
  Ghost::GhostType  gan   = Ghost::AroundNodes;
  Ghost::GhostType  gnone = Ghost::None;

  Task* task  = scinew Task("SimpleSDInterface::sdInterfaceDivergence", this,
                            &SimpleSDInterface::sdInterfaceDivergence);


  Ghost::GhostType gp;
  int              numGhost;
  d_shared_state->getParticleGhostLayer(gp, numGhost);

  // Everthing needs to register to calculate the basic interface flux rate
  setBaseComputesAndRequiresDivergence(task, matls->getUnion());

  task->requires(Task::OldDW, d_shared_state->get_delt_label());

  task->requires(Task::NewDW, d_mpm_lb->gMassLabel,           gan, 1);
  task->requires(Task::NewDW, d_mpm_lb->gConcentrationLabel,  gan, 1);

//  task->computes(sdInterfaceRate, mss);

  sched->addTask(task, patches, matls);
}

void SimpleSDInterface::sdInterfaceDivergence(
                                                const ProcessorGroup  *         ,
                                                const PatchSubset     * patches ,
                                                const MaterialSubset  * matls   ,
                                                      DataWarehouse   * oldDW   ,
                                                      DataWarehouse   * newDW
                                               )
{
  int numMatls = matls->size();

  Ghost::GhostType  typeGhost;
  int               numGhost;
  d_shared_state->getParticleGhostLayer(typeGhost, numGhost);

  StaticArray<constNCVariable<double> > gVolume(numMatls);
  StaticArray<constNCVariable<double> > gConc(numMatls);  // C_i,j = gConc[j][i]

  NCVariable<double> gTotalVolume;

  delt_vartype delT;
  oldDW->get(delT, d_shared_state->get_delt_label(), getLevel(patches) );

  for (int patchIdx = 0; patchIdx < patches->size(); ++patchIdx)
  {
    const Patch* patch = patches->get(patchIdx);

    std::vector<NCVariable<double> > gdCdt_interface(numMatls);
    std::vector<NCVariable<double> > gFluxAmount(numMatls);

    std::vector<double> massNormFactor(numMatls);
    // Loop over materials and preload our arrays of material based nodal data

    newDW->allocateTemporary(gTotalVolume, patch);
    gTotalVolume.initialize(0);
    for (int mIdx = 0; mIdx < numMatls; ++mIdx)
    {
      int dwi = matls->get(mIdx);
      newDW->get(gVolume[mIdx],
                 d_mpm_lb->gVolumeLabel,         dwi, patch, typeGhost,   numGhost);
      newDW->get(gConc[mIdx],
                 d_mpm_lb->gConcentrationLabel,  dwi, patch, typeGhost,   numGhost);

      newDW->allocateAndPut(gdCdt_interface[mIdx],
                            sdInterfaceRate,     dwi, patch, Ghost::None, 0);
      gdCdt_interface[mIdx].initialize(0);
      // Not in place yet, but should be for mass normalization.
      //massNormFactor[mIdx] = mpm_matl->getMassNormFactor();
      massNormFactor[mIdx] = 1.0;

      newDW->allocateTemporary(gFluxAmount[mIdx], patch);
      gFluxAmount[mIdx].initialize(0.0);


      for (NodeIterator nIt = patch->getNodeIterator(); !nIt.done(); ++nIt) {
        IntVector node = *nIt;
//        gConcMass[mIdx][node] = gMass[mIdx][node] * massNormFactor[mIdx];
        gFluxAmount[mIdx][node] = gVolume[mIdx][node] * massNormFactor[mIdx];
//        gTotalMass[node] += gMass[mIdx][node];
        gTotalVolume[node] += gVolume[mIdx][node];
      }
    }

    // Calculate which nodes share an interface.
    NCVariable<int> doInterface;
    newDW->allocateTemporary(doInterface, patch);
    doInterface.initialize(0);
    for (NodeIterator nIt = patch->getNodeIterator(); !nIt.done(); ++nIt) {
      IntVector node = *nIt;
      int mIdx = 0;
      while (mIdx < numMatls && doInterface[node] == 0) {
//        double checkMass = gMass[mIdx][node];
        double checkVolume = gVolume[mIdx][node];
        if ((checkVolume > 1e-15) && checkVolume != gTotalVolume[node]) {
          doInterface[node] = 1;
        }
        ++mIdx;
      }
    }

    for (NodeIterator nIt = patch->getNodeIterator(); !nIt.done(); ++nIt) {
      double numerator    = 0.0;
      double denominator  = 0.0;
      IntVector node = *nIt;

      if (doInterface[node] == 1) {
        for (int mIdx = 0; mIdx < numMatls; ++mIdx) {
          // Break out if we're not working with this material contact
          numerator   += (gConc[mIdx][node] * gFluxAmount[mIdx][node]);
          denominator += gFluxAmount[mIdx][node];
        }
        double contactConcentration = numerator/denominator;

      // FIXME TODO -- Ensure gConc has already had the mass factor divided out
      // by now. -- JBH
        for (int mIdx = 0; mIdx < numMatls; ++mIdx) {
          gdCdt_interface[mIdx][node] =
              (contactConcentration - gConc[mIdx][node]) / delT;
        } // Loop over materials
      } // Do interface
    } // Loop over nodes
  } // Loop over patches
}

void SimpleSDInterface::outputProblemSpec(
                                            ProblemSpecP  & ps
                                           )
{
  ProblemSpecP sdim_ps = ps;
  sdim_ps = ps->appendChild("diffusion_interface");
  sdim_ps->appendElement("type","simple");
  d_materials_list.outputProblemSpec(sdim_ps);
}
