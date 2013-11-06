/*
 * The MIT License
 *
 * Copyright (c) 2013 The University of Utah
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


#include <sci_defs/cuda_defs.h>

class Int3 : public int3 {
  public:
    HOST_DEVICE Int3() {}
    HOST_DEVICE int& operator[](const int &i) {
              return (&x)[i];
    }
    HOST_DEVICE const int& operator[](const int &i) const {
              return (&x)[i];
    }
    HOST_DEVICE Int3(const int3& copy):int3(copy) {}
};

class uInt3 : public uint3 {
  public:
    HOST_DEVICE uInt3() {}
    HOST_DEVICE unsigned int& operator[](const int &i) {
              return (&x)[i];
    }
    HOST_DEVICE const unsigned int& operator[](const int &i) const {
              return (&x)[i];
    }
    HOST_DEVICE uInt3(const uint3& copy):uint3(copy) {}
};

class Float3 : public float3 {
  public:
    HOST_DEVICE Float3() {}
    HOST_DEVICE float& operator[](const int &i) {
              return (&x)[i];
    }
    HOST_DEVICE const float& operator[](const int &i) const {
              return (&x)[i];
    }
    HOST_DEVICE Float3(const float3& copy):float3(copy) {}
};

class Double3 : public double3 {
  public:
    HOST_DEVICE Double3() {}
    HOST_DEVICE double& operator[](const int &i) {
              return (&x)[i];
    }
    HOST_DEVICE const double& operator[](const int &i) const {
              return (&x)[i];
    }
    HOST_DEVICE Double3(const double3& copy):double3(copy) {}
};
