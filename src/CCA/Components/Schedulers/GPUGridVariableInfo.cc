/*
 * The MIT License
 *
 * Copyright (c) 1997-2014 The University of Utah
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
#include <CCA/Components/Schedulers/GPUGridVariableInfo.h>

deviceGridVariableInfo::deviceGridVariableInfo(GridVariableBase* gridVar,
            IntVector sizeVector,
            size_t sizeOfDataType,
            int varSize,
            IntVector offset,
            int materialIndex,
            const Patch* patchPointer,
            const Task::Dependency* dep,
            bool validOnDevice,
            Ghost::GhostType gtype,
            int numGhostCells,
            int whichGPU) {
  this->gridVar = gridVar;
  this->sizeVector = sizeVector;
  this->sizeOfDataType = sizeOfDataType;
  this->varSize = varSize;
  this->offset = offset;
  this->materialIndex = materialIndex;
  this->patchPointer = patchPointer;
  this->dep = dep;
  this->validOnDevice = validOnDevice;
  this->gtype = gtype;
  this->numGhostCells = numGhostCells;
  this->whichGPU = whichGPU;
}


deviceGridVariables::deviceGridVariables() {
  totalSize = 0;
  for (int i = 0; i < Task::TotalDWs; i++) {
    totalSizeForDataWarehouse[i] = 0;
  }
}
void deviceGridVariables::add(const Patch* patchPointer,
          int materialIndex,
          IntVector sizeVector,
          size_t sizeOfDataType,
          IntVector offset,
          GridVariableBase* gridVar,
          const Task::Dependency* dep,
          bool validOnDevice,
          Ghost::GhostType gtype,
          int numGhostCells,
          int whichGPU) {
  int varSize = sizeVector.x() * sizeVector.y() * sizeVector.z() * sizeOfDataType;
  totalSize += varSize;
  totalSizeForDataWarehouse[dep->mapDataWarehouse()] += varSize;
  deviceGridVariableInfo tmp(gridVar, sizeVector, sizeOfDataType, varSize, offset, materialIndex, patchPointer, dep, validOnDevice, gtype, numGhostCells, whichGPU);
  vars.push_back(tmp);
}


size_t deviceGridVariables::getTotalSize() {
  return totalSize;
}

size_t deviceGridVariables::getSizeForDataWarehouse(int dwIndex) {
  return totalSizeForDataWarehouse[dwIndex];
}

unsigned int deviceGridVariables::numItems() {
  return vars.size();
}


int deviceGridVariables::getMaterialIndex(int index) {
  return vars.at(index).materialIndex;
}
const Patch* deviceGridVariables::getPatchPointer(int index) {
  return vars.at(index).patchPointer;
}
IntVector deviceGridVariables::getSizeVector(int index) {
  return vars.at(index).sizeVector;
}
IntVector deviceGridVariables::getOffset(int index) {
    return vars.at(index).offset;
}

GridVariableBase* deviceGridVariables::getGridVar(int index) {
  return vars.at(index).gridVar;
}
const Task::Dependency* deviceGridVariables::getDependency(int index) {
  return vars.at(index).dep;
}

size_t deviceGridVariables::getSizeOfDataType(int index) {
  return vars.at(index).sizeOfDataType;
}

int deviceGridVariables::getVarSize(int index) {
  return vars.at(index).varSize;
}

Ghost::GhostType deviceGridVariables::getGhostType(int index) {
  return vars.at(index).gtype;
}

int deviceGridVariables::getNumGhostCells(int index) {
  return vars.at(index).numGhostCells;
}

int deviceGridVariables::getWhichGPU(int index) {
  return vars.at(index).whichGPU;
}