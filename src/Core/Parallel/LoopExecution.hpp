/*
 * The MIT License
 *
 * Copyright (c) 1997-2018 The University of Utah
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

//The purpose of this file is to provide portability between Kokkos and non-Kokkos builds.
//For example, if a user calls a parallel_for loop but Kokkos is NOT provided, this will run the
//functor in a loop and also not use Kokkos views.  If Kokkos is provided, this creates a
//lambda expression and inside that it contains loops over the functor.  Kokkos Views are also used.
//At the moment we seek to only support regular CPU code, Kokkos OpenMP, and CUDA execution spaces,
//though it shouldn't be too difficult to expand it to others.  This doesn't extend it
//to CUDA kernels (without Kokkos), and that can get trickier (block/dim parameters) especially with
//regard to a parallel_reduce (many ways to "reduce" a value and return it back to host memory)

#ifndef UINTAH_HOMEBREW_LOOP_EXECUTION_HPP
#define UINTAH_HOMEBREW_LOOP_EXECUTION_HPP

#include <Core/Parallel/Parallel.h>
#include <Core/Exceptions/InternalError.h>
#include <cstddef> //What is this doing here?

#include <sci_defs/cuda_defs.h>
#include <sci_defs/kokkos_defs.h>

#if defined(UINTAH_ENABLE_KOKKOS)
#include <Kokkos_Core.hpp>
  const bool HAVE_KOKKOS = true;
#if !defined(HAVE_CUDA)
  //Kokkos GPU included in this build.  Create some stub types so these types at least exist.
  namespace Kokkos {
    class Cuda {};
    class CudaSpace {};
  }
#endif
#else
  //Kokkos not included in this build.  Create some stub types so these types at least exist.
  namespace Kokkos {
    class OpenMP {};
    class HostSpace {};
    class Cuda {};
    class CudaSpace {};
  }
  const bool HAVE_KOKKOS = false;
#endif //UINTAH_ENABLE_KOKKOS

// Create some data types for non-Kokkos CPU runs.
namespace UintahSpaces{
  class CPU {};
  class HostSpace {};
}

// What the user specifies into our macro.
using UINTAH_CPU_TAG = UintahSpaces::CPU;
using KOKKOS_OPENMP_TAG = Kokkos::OpenMP;
using KOKKOS_CUDA_TAG = Kokkos::Cuda;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define COMMA ,  //Macros don't like passing in data types that contain commas in them, such as two template arguments. This helps fix that.
//#if defined(UINTAH_ENABLE_KOKKOS)

//#if defined(HAVE_CUDA)
//#define NUM_EXECUTION_SPACES 2
//#define EXECUTION_SPACE_0 Kokkos::Cuda
//#define EXECUTION_SPACE_1 Kokkos::OpenMP
//#define MEMORY_SPACE_0    Kokkos::CudaSpace
//#define MEMORY_SPACE_1    Kokkos::HostSpace
//#else
//#define NUM_EXECUTION_SPACES 1
//#define EXECUTION_SPACE_0 Kokkos::OpenMP
//#define MEMORY_SPACE_0    Kokkos::HostSpace
//#define EXECUTION_SPACE_1 Kokkos::OpenMP
//#define MEMORY_SPACE_1    Kokkos::HostSpace
//#endif //#if defined(HAVE_CUDA)
//#else //if not UINTAH_ENABLE_KOKKOS
//#define NUM_EXECUTION_SPACES 1
//#define EXECUTION_SPACE_0 UintahSpaces::CPU
//#define MEMORY_SPACE_0    UintahSpaces::HostSpace
//#define EXECUTION_SPACE_1 UintahSpaces::CPU
//#define MEMORY_SPACE_1    UintahSpaces::HostSpace

//#endif //#if defined(UINTAH_ENABLE_KOKKOS)


//This means that this tasks supports any of these execution spaces
//Depending on the configure/compiler flags determines which of these are actually
//compiled.

enum TaskAssignedExecutionSpace {
  NONE = 0,
  UINTAH_CPU = 1,                          //binary 001
  KOKKOS_OPENMP = 2,                       //binary 010
  KOKKOS_CUDA = 4,                          //binary 100
};
//
////An object of this class is used for CALL_ASSIGN_PORTABLE_TASK.
//class TaskExecutionSpaces {
//public:
//  TaskExecutionSpaces() {}
//  TaskExecutionSpaces(unsigned int firstSpace) {
//    executionSpaces = firstSpace;
//  }
//  TaskExecutionSpaces(unsigned int firstSpace, unsigned int secondSpace) {
//    executionSpaces = firstSpace | secondSpace;
//  }
//  TaskExecutionSpaces(unsigned int firstSpace, unsigned int secondSpace, unsigned int thirdSpace) {
//    executionSpaces = firstSpace | secondSpace | thirdSpace;
//  }
//  void loadSpaces(unsigned int firstSpace) {
//    executionSpaces = executionSpaces | firstSpace;
//  }
//  void loadSpaces(unsigned int firstSpace, unsigned int secondSpace) {
//    executionSpaces = executionSpaces | firstSpace | secondSpace;
//  }
//  void loadSpaces(unsigned int firstSpace, unsigned int secondSpace, unsigned int thirdSpace) {
//    executionSpaces = executionSpaces | firstSpace | secondSpace | thirdSpace;
//  }
//  unsigned int getTaskExecutionSpaces() {
//    return executionSpaces;
//  }
//private:
//  unsigned int executionSpaces{0};
//};


#define PREPARE_UINTAH_CPU_TASK(EXECUTION_SPACE,                                                   \
                                TASK, TASK_DEPENDENCIES,                                           \
                                FUNCTION_NAME, FUNCTION_CODE_NAME,                                 \
                                PATCHES, MATERIALS, ...) {                                         \
  TASK = scinew Task(FUNCTION_NAME,                                                                \
                           this,                                                                   \
                           &FUNCTION_CODE_NAME<EXECUTION_SPACE, UintahSpaces::HostSpace>,          \
                           ## __VA_ARGS__);                                                        \
}

#define PREPARE_KOKKOS_OPENMP_TASK(EXECUTION_SPACE,                                                \
                                TASK, TASK_DEPENDENCIES,                                           \
                                FUNCTION_NAME, FUNCTION_CODE_NAME,                                 \
                                PATCHES, MATERIALS, ...) {                                         \
  TASK = scinew Task(FUNCTION_NAME,                                                                \
                           this,                                                                   \
                           &FUNCTION_CODE_NAME<EXECUTION_SPACE, Kokkos::HostSpace>,                \
                           ## __VA_ARGS__);                                                        \
                                                                                                   \
  TASK->usesKokkosOpenMP(true);                                                                    \
}

#define PREPARE_KOKKOS_CUDA_TASK(EXECUTION_SPACE,                                                  \
                                TASK, TASK_DEPENDENCIES,                                           \
                                FUNCTION_NAME, FUNCTION_CODE_NAME,                                 \
                                PATCHES, MATERIALS, ...) {                                         \
  TASK = scinew Task(FUNCTION_NAME,                                                                \
                           this,                                                                   \
                           &FUNCTION_CODE_NAME<EXECUTION_SPACE, Kokkos::CudaSpace>,                \
                           ## __VA_ARGS__);                                                        \
                                                                                                   \
  TASK->usesDevice(true);                                                                          \
  TASK->usesKokkosCuda(true);                                                                      \
  TASK->usesSimVarPreloading(true);                                                                \
}
// This macro gives a mechanism to allow the user to do two things.
// 1) Specify all execution spaces allowed by this task
// 2) Generate task objects and task object options.
//    At compile time, the compiler will compile the task for all specified execution spaces.
//    At run time, the appropriate if statement logic will determine which task to use.
// TAG1, TAG2, and TAG3 are possible execution spaces this task supports.
// TASK_DEPENDENCIES are a functor which performs all additional task specific options the user desires
// FUNCTION_NAME is the string name of the task.
// FUNCTION_CODE_NAME is the function pointer without the template arguments, and this macro
// tacks on the appropriate template arguments.  (This is the major reason why a macro is used.)
// PATCHES and MATERIALS are normal Task object arguments.
// ... are additional variatic Task arguments

// Logic note, we don't allow both a Uintah CPU task and a Kokkos CPU task to exist in the same
// compiled build.  But we do allow a Kokkos CPU and Kokkos GPU task to exist in the same build
#define CALL_ASSIGN_PORTABLE_TASK_3TAGS(TAG1, TAG2, TAG3,                                          \
                                  TASK_DEPENDENCIES,                                               \
                                  FUNCTION_NAME, FUNCTION_CODE_NAME,                               \
                                  PATCHES, MATERIALS, ...) {                                       \
  Task* task{nullptr};                                                                             \
                                                                                                   \
  if (Uintah::Parallel::usingDevice()) {                                                           \
    if        (std::is_same< Kokkos::Cuda,      TAG1 >::value) {                                   \
      PREPARE_KOKKOS_CUDA_TASK(TAG1, task, TASK_DEPENDENCIES, FUNCTION_NAME, FUNCTION_CODE_NAME,   \
                                 PATCHES, MATERIALS, ## __VA_ARGS__);                              \
    } else if (std::is_same< Kokkos::Cuda,      TAG2 >::value) {                                   \
      PREPARE_KOKKOS_CUDA_TASK(TAG2, task, TASK_DEPENDENCIES, FUNCTION_NAME, FUNCTION_CODE_NAME,   \
                                 PATCHES, MATERIALS, ## __VA_ARGS__);                              \
    } else if (std::is_same< Kokkos::Cuda,      TAG3 >::value) {                                   \
      PREPARE_KOKKOS_CUDA_TASK(TAG3, task, TASK_DEPENDENCIES, FUNCTION_NAME, FUNCTION_CODE_NAME,   \
                                 PATCHES, MATERIALS, ## __VA_ARGS__);                              \
    }                                                                                              \
  }                                                                                                \
                                                                                                   \
  if (!task) {                                                                                     \
    if      (std::is_same< Kokkos::OpenMP,      TAG1 >::value) {                                   \
      PREPARE_KOKKOS_OPENMP_TASK(TAG1, task, TASK_DEPENDENCIES, FUNCTION_NAME, FUNCTION_CODE_NAME, \
                                 PATCHES, MATERIALS, ## __VA_ARGS__);                              \
    } else if (std::is_same< Kokkos::OpenMP,    TAG2 >::value) {                                   \
      PREPARE_KOKKOS_OPENMP_TASK(TAG2, task, TASK_DEPENDENCIES, FUNCTION_NAME, FUNCTION_CODE_NAME, \
                                 PATCHES, MATERIALS, ## __VA_ARGS__);                              \
    } else if (std::is_same< Kokkos::OpenMP,    TAG3 >::value) {                                   \
      PREPARE_KOKKOS_OPENMP_TASK(TAG3, task, TASK_DEPENDENCIES, FUNCTION_NAME, FUNCTION_CODE_NAME, \
                                 PATCHES, MATERIALS, ## __VA_ARGS__);                              \
    } else if (std::is_same< UintahSpaces::CPU, TAG1 >::value) {                                   \
      PREPARE_UINTAH_CPU_TASK(   TAG1, task, TASK_DEPENDENCIES, FUNCTION_NAME, FUNCTION_CODE_NAME, \
                                 PATCHES, MATERIALS, ## __VA_ARGS__);                              \
    } else if (std::is_same< UintahSpaces::CPU, TAG2 >::value) {                                   \
      PREPARE_UINTAH_CPU_TASK(   TAG2, task, TASK_DEPENDENCIES, FUNCTION_NAME, FUNCTION_CODE_NAME, \
                                 PATCHES, MATERIALS, ## __VA_ARGS__);                              \
    } else if (std::is_same< UintahSpaces::CPU, TAG3 >::value) {                                   \
      PREPARE_UINTAH_CPU_TASK(   TAG3, task, TASK_DEPENDENCIES, FUNCTION_NAME, FUNCTION_CODE_NAME, \
                                 PATCHES, MATERIALS, ## __VA_ARGS__);                              \
    }                                                                                              \
  }                                                                                                \
                                                                                                   \
  if (task) {                                                                                      \
    TASK_DEPENDENCIES(task);                                                                       \
  }                                                                                                \
                                                                                                   \
  if (task) {                                                                                      \
    sched->addTask(task, PATCHES, MATERIALS);                                                      \
  }                                                                                                \
}

//If only 1 execution space tag is specified
#define CALL_ASSIGN_PORTABLE_TASK_1TAGS(TAG1, TASK_DEPENDENCIES,                                   \
                                  FUNCTION_NAME, FUNCTION_CODE_NAME,                               \
                                  PATCHES, MATERIALS, ...) {                                       \
  CALL_ASSIGN_PORTABLE_TASK_3TAGS(TAG1, void, void, TASK_DEPENDENCIES,                             \
                            FUNCTION_NAME, FUNCTION_CODE_NAME,                                     \
                            PATCHES, MATERIALS, ## __VA_ARGS__);                                   \
}

//If only 2 execution space tags are specified
#define CALL_ASSIGN_PORTABLE_TASK_2TAGS(TAG1, TAG2, TASK_DEPENDENCIES,                             \
                                  FUNCTION_NAME, FUNCTION_CODE_NAME,                               \
                                  PATCHES, MATERIALS, ...) {                                       \
  CALL_ASSIGN_PORTABLE_TASK_3TAGS(TAG1, TAG2, void, TASK_DEPENDENCIES,                             \
                            FUNCTION_NAME, FUNCTION_CODE_NAME,                                     \
                            PATCHES, MATERIALS, ## __VA_ARGS__);                                   \
}

namespace Uintah {

class BlockRange
{
public:

  enum { rank = 3 };

  BlockRange(){}

  template <typename ArrayType>
  void setValues( ArrayType const & c0, ArrayType const & c1 )
  {
    for (int i=0; i<rank; ++i) {
      m_offset[i] = c0[i] < c1[i] ? c0[i] : c1[i];
      m_dim[i] =   (c0[i] < c1[i] ? c1[i] : c0[i]) - m_offset[i];
    }
  }

  template <typename ArrayType>
  BlockRange( ArrayType const & c0, ArrayType const & c1 )
  {
    setValues( c0, c1 );
  }

  BlockRange( const BlockRange& obj ) {
    for (int i=0; i<rank; ++i) {
      this->m_offset[i] = obj.m_offset[i];
      this->m_dim[i] = obj.m_dim[i];
    }
  }

#ifdef HAVE_CUDA
  template <typename ArrayType>
  void setValues(cudaStream_t* stream, ArrayType const & c0, ArrayType const & c1 )
  {
    for (int i=0; i<rank; ++i) {
      m_offset[i] = c0[i] < c1[i] ? c0[i] : c1[i];
      m_dim[i] =   (c0[i] < c1[i] ? c1[i] : c0[i]) - m_offset[i];
    }
    m_stream = stream;
  }

  template <typename ArrayType>
  BlockRange(cudaStream_t* stream, ArrayType const & c0, ArrayType const & c1 )
  {
    setValues( stream, c0, c1 );
  }



#endif

  int begin( int r ) const { return m_offset[r]; }
  int   end( int r ) const { return m_offset[r] + m_dim[r]; }

  size_t size() const
  {
    size_t result = 1u;
    for (int i=0; i<rank; ++i) {
      result *= m_dim[i];
    }
    return result;
  }

private:
  int m_offset[rank];
  int m_dim[rank];
#ifdef HAVE_CUDA
public:
  cudaStream_t* getStream() const { return m_stream; }
private:
  cudaStream_t* m_stream {nullptr};
#endif
};

//BlockRange range() const
//{
//  return BlockRange{getLowIndex(), getHighIndex()};
//}


template <typename Functor>
void serial_for( BlockRange const & r, const Functor & f )
{
  const int ib = r.begin(0); const int ie = r.end(0);
  const int jb = r.begin(1); const int je = r.end(1);
  const int kb = r.begin(2); const int ke = r.end(2);

  for (int k=kb; k<ke; ++k) {
  for (int j=jb; j<je; ++j) {
  for (int i=ib; i<ie; ++i) {
    f(i,j,k);
  }}}
}



template <typename ExecutionSpace, typename Functor, typename ReductionType>
void parallel_reduce_sum( BlockRange const & r, const Functor & functor, ReductionType & red  )
{
  ReductionType tmp = red;
  unsigned int i_size = r.end(0) - r.begin(0);
  unsigned int j_size = r.end(1) - r.begin(1);
  unsigned int k_size = r.end(2) - r.begin(2);
  unsigned int rbegin0 = r.begin(0);
  unsigned int rbegin1 = r.begin(1);
  unsigned int rbegin2 = r.begin(2);

  const unsigned int teamThreadRangeSize = (i_size > 0 ? i_size : 1) * (j_size > 0 ? j_size : 1) * (k_size > 0 ? k_size : 1);

#if defined(UINTAH_ENABLE_KOKKOS)
#if defined(HAVE_CUDA)
  if ( std::is_same< Kokkos::Cuda , ExecutionSpace >::value ) {

    //If 256 threads aren't needed, use less.
    //But cap at 256 threads total, as this will correspond to 256 threads in a block.
    //Later the TeamThreadRange will reuse those 256 threads.  For example, if teamThreadRangeSize is 800, then
    //Cuda thread 0 will be assigned to n = 0, n = 256, n = 512, and n = 768,
    //Cuda thread 1 will be assigned to n = 1, n = 257, n = 513, and n = 769...
    const unsigned int actualThreads = teamThreadRangeSize > 256 ? 256 : teamThreadRangeSize;

    typedef Kokkos::TeamPolicy< Kokkos::Cuda > policy_type;

    Kokkos::parallel_reduce (Kokkos::TeamPolicy<Kokkos::Cuda>( 1, actualThreads ),
                             KOKKOS_LAMBDA ( typename policy_type::member_type thread, ReductionType& inner_sum ) {
      Kokkos::parallel_for (Kokkos::TeamThreadRange(thread, teamThreadRangeSize), [&] (const int& n) {

        const int i = n / (j_size * k_size) + rbegin0;
        const int j = (n / k_size) % j_size + rbegin1;
        const int k = n % k_size + rbegin2;
        functor( i, j, k, inner_sum );
      });
    }, tmp);
  } else
#endif  // #if defined(HAVE_CUDA)
  if ( std::is_same< Kokkos::OpenMP , ExecutionSpace >::value ) {
//    typedef typename Kokkos::MDRangePolicy<ExecutionSpace, Kokkos::Rank<3,
//                                                         Kokkos::Iterate::Left,
//                                                         Kokkos::Iterate::Left>
//                                                         > MDPolicyType_3D;
//
//    MDPolicyType_3D mdpolicy_3d( {{r.begin(0),r.begin(1),r.begin(2)}}, {{r.end(0),r.end(1),r.end(2)}} );
//
//    Kokkos::parallel_reduce( mdpolicy_3d, f, tmp );


//    const int ib = r.begin(0); const int ie = r.end(0);
//    const int jb = r.begin(1); const int je = r.end(1);
//    const int kb = r.begin(2); const int ke = r.end(2);
//
//    Kokkos::parallel_reduce( Kokkos::RangePolicy<ExecutionSpace, int>(kb, ke).set_chunk_size(2), KOKKOS_LAMBDA(int k, ReductionType & tmp) {
//      for (int j=jb; j<je; ++j) {
//      for (int i=ib; i<ie; ++i) {
//        f(i,j,k,tmp);
//      }}
//    });

    const unsigned int actualThreads = teamThreadRangeSize > 16 ? 16 : teamThreadRangeSize;

    typedef Kokkos::TeamPolicy< Kokkos::OpenMP > policy_type;

    Kokkos::parallel_reduce (Kokkos::TeamPolicy<Kokkos::OpenMP>( 1, actualThreads ),
                             KOKKOS_LAMBDA ( typename policy_type::member_type thread, ReductionType& inner_sum ) {
      //printf("i is %d\n", thread.team_rank());
      Kokkos::parallel_for (Kokkos::TeamThreadRange(thread, teamThreadRangeSize), [&] (const int& n) {
        const int i = n / (j_size * k_size) + rbegin0;
        const int j = (n / k_size) % j_size + rbegin1;
        const int k = n % k_size + rbegin2;
        functor(i, j, k, inner_sum);
      });
    }, tmp);

  } //else
#else //#if defined(UINTAH_ENABLE_KOKKOS)
  if(std::is_same< UintahSpaces::CPU, ExecutionSpace >::value) {
      const int ib = r.begin(0); const int ie = r.end(0);
      const int jb = r.begin(1); const int je = r.end(1);
      const int kb = r.begin(2); const int ke = r.end(2);

      ReductionType tmp = red;
      for (int k=kb; k<ke; ++k) {
      for (int j=jb; j<je; ++j) {
      for (int i=ib; i<ie; ++i) {
        functor(i,j,k,tmp);
      }}}
      red = tmp;
  }
#endif //#if defined(UINTAH_ENABLE_KOKKOS)
  else {
    printf("Unknown execution space supplied!\n");
    SCI_THROW(InternalError("Unknown execution space supplied!", __FILE__, __LINE__));
  }
  red = tmp;
}

#if defined( UINTAH_ENABLE_KOKKOS )

template <typename Functor>
void parallel_for( BlockRange const & r, const Functor & f )
{
  const int ib = r.begin(0); const int ie = r.end(0);
  const int jb = r.begin(1); const int je = r.end(1);
  const int kb = r.begin(2); const int ke = r.end(2);

#if defined( HAVE_CUDA )

  // Note: I can only seem to invoke functors with nvcc_wrapper.  I can't invoke lambdas.
  // I get obnoxious compiler errors.   At the moment this limits us to 1D loops.  Brad P Nov 23 2017.
  //Kokkos::parallel_for( Kokkos::RangePolicy<Kokkos::Cuda, int>(kb, ke).set_chunk_size(2), KOKKOS_LAMBDA(int k) {
  //Kokkos::parallel_for( Kokkos::RangePolicy<Kokkos::Cuda, int>(kb, ke).set_chunk_size(2), [=](int k) {
  //  for (int j=jb; j<je; ++j) {
  //  for (int i=ib; i<ie; ++i) {
  //    f(i,j,k);
  //  }}
  //});
  Kokkos::parallel_for( Kokkos::RangePolicy<Kokkos::Cuda, int>(kb, ke).set_chunk_size(2), f );
#else
  Kokkos::parallel_for( Kokkos::RangePolicy<Kokkos::OpenMP, int>(kb, ke).set_chunk_size(2), KOKKOS_LAMBDA(int k) {
    for (int j=jb; j<je; ++j) {
    for (int i=ib; i<ie; ++i) {
      f(i,j,k);
    }}
  });
#endif
}


//Runtime code has already started using parallel_for constructs.  These should NOT be executed on
//a GPU.  This function allows a developer to ensure the task only runs on CPU code.  Further, we
//will just run this without the use of Kokkos (this is so GPU builds don't require OpenMP as well).
template <typename Functor>
void parallel_for_cpu_only( BlockRange const & r, const Functor & f )
{
  const int ib = r.begin(0); const int ie = r.end(0);
  const int jb = r.begin(1); const int je = r.end(1);
  const int kb = r.begin(2); const int ke = r.end(2);

  for (int k=kb; k<ke; ++k) {
  for (int j=jb; j<je; ++j) {
  for (int i=ib; i<ie; ++i) {
    f(i,j,k);
  }}}
}

template <typename Functor, typename Option>
void parallel_for( BlockRange const & r, const Functor & f, const Option & op )
{
  const int ib = r.begin(0); const int ie = r.end(0);
  const int jb = r.begin(1); const int je = r.end(1);
  const int kb = r.begin(2); const int ke = r.end(2);

#if defined( HAVE_CUDA )
  Kokkos::parallel_for( Kokkos::RangePolicy<Kokkos::Cuda, int>(kb, ke).set_chunk_size(2), f );
  //Kokkos::parallel_for( Kokkos::RangePolicy<Kokkos::Cuda, int>(kb, ke).set_chunk_size(2), KOKKOS_LAMBDA(int k) {
  //    for (int j=jb; j<je; ++j) {
  //    for (int i=ib; i<ie; ++i) {
  //      f(op,i,j,k);
  //    }}
  //  });
#else
  Kokkos::parallel_for( Kokkos::RangePolicy<Kokkos::OpenMP, int>(kb, ke).set_chunk_size(2), KOKKOS_LAMBDA(int k) {
      for (int j=jb; j<je; ++j) {
      for (int i=ib; i<ie; ++i) {
        f(op,i,j,k);
      }}
    });
#endif
}


//This FunctorBuilder exists because I couldn't go the lambda approach.
//I was running into some conflict with Uintah/nvcc_wrapper/Kokkos/CUDA somewhere.
//So I went the alternative route and built a functor instead of building a lambda.
#if defined( HAVE_CUDA )
template < typename Functor, typename ReductionType >
struct FunctorBuilderReduce {
  //Functor& f = nullptr;

  //std::function is probably a wrong idea, CUDA doesn't support these.
  //std::function<void(int i, int j, int k, ReductionType & red)> f;
  int ib{0};
  int ie{0};
  int jb{0};
  int je{0};

  FunctorBuilderReduce(const BlockRange & r, const Functor & f) {
    ib = r.begin(0);
    ie = r.end(0);
    jb = r.begin(1);
    je = r.end(1);
  }
  void operator()(int k,  ReductionType & red) const {
    //const int ib = r.begin(0); const int ie = r.end(0);
    //const int jb = r.begin(1); const int je = r.end(1);

    for (int j=jb; j<je; ++j) {
      for (int i=ib; i<ie; ++i) {
        f(i,j,k,red);
      }
    }
  }
};
#endif

template <typename Functor, typename ReductionType>
void parallel_reduce_1D( BlockRange const & r, const Functor & f, ReductionType & red  ) {
#if !defined( HAVE_CUDA )
  if ( ! Uintah::Parallel::usingDevice() ) {
    ReductionType tmp = red;
    Kokkos::RangePolicy<Kokkos::OpenMP> rangepolicy(r.begin(0), r.end(0));
    Kokkos::parallel_reduce( rangepolicy, f, tmp );
    red = tmp;
  }
#elif defined( HAVE_CUDA )
  //else {
    //This must be a single dimensional range policy, so use Kokkos::RangePolicy
    ReductionType *tmp;
    cudaMallocHost( (void**)&tmp, sizeof(ReductionType) );

    //No streaming, no launch bounds
    //Kokkos::RangePolicy<Kokkos::Cuda> rangepolicy(r.begin(0), r.end(0));
    //No streaming, launch bounds (512 gave 128 registers, 640 gave 96 registers)
    //Kokkos::RangePolicy<Kokkos::Cuda, Kokkos::LaunchBounds<512,1>> rangepolicy(r.begin(0), r.end(0));

    //Streaming
    Kokkos::Cuda instanceObject = Kokkos::Cuda( *(r.getStream()) );
 
    //Streaming, no launch bounds
    //Kokkos::RangePolicy<Kokkos::Cuda> rangepolicy(instanceObject, r.begin(0), r.end(0));
    //Streaming, launch bounds   
    Kokkos::RangePolicy<Kokkos::Cuda, Kokkos::LaunchBounds<512,1>> rangepolicy(instanceObject,  r.begin(0), r.end(0));

    Kokkos::parallel_reduce( rangepolicy, f, *tmp );  //TODO: Don't forget about these reduction values.
  //}
#endif
}

//template <typename Functor, typename ReductionType>
//void parallel_reduce_sum( BlockRange const & r, const Functor & functor, ReductionType & red  )
//{
//
//  ReductionType tmp = red;
//
//
//#if defined( HAVE_CUDA )
//
//      typedef typename Kokkos::MDRangePolicy<Kokkos::Cuda, Kokkos::Rank<3,
//                                                         Kokkos::Iterate::Left,
//                                                         Kokkos::Iterate::Left>
//                                                         > MDPolicyType_3D;
//
//    MDPolicyType_3D mdpolicy_3d( {{r.begin(0),r.begin(1),r.begin(2)}}, {{r.end(0),r.end(1),r.end(2)}} );
//
//    Kokkos::parallel_reduce( mdpolicy_3d, functor, tmp );
//#else
//
//    const int ib = r.begin(0); const int ie = r.end(0);
//    const int jb = r.begin(1); const int je = r.end(1);
//    const int kb = r.begin(2); const int ke = r.end(2);
//
//    Kokkos::parallel_reduce( Kokkos::RangePolicy<Kokkos::OpenMP, int>(kb, ke).set_chunk_size(2), KOKKOS_LAMBDA(int k, ReductionType & tmp) {
//      for (int j=jb; j<je; ++j) {
//      for (int i=ib; i<ie; ++i) {
//        functor(i,j,k,tmp);
//      }}
//    });
//
//#endif
//  red = tmp;
//}

//template <typename ExecutionSpace, typename Functor, typename ReductionType>
//struct parallel_functions {
//  static void parallel_reduce_sum( BlockRange const & r, const Functor & functor, ReductionType & red  ) {
//    printf("The supplied Execution Space is unknown or not supported.");
//    exit(-1);
//  }
//};
//
//#if HAVE_CUDA
//template <Kokkos::Cuda, typename Functor, typename ReductionType>
//struct parallel_functions {
//  static void parallel_reduce_sum( BlockRange const & r, const Functor & functor, ReductionType & red ) {
//    ReductionType tmp = red;
//    const unsigned int i_size = r.end(0) - r.begin(0);
//    const unsigned int j_size = r.end(1) - r.begin(1);
//    const unsigned int k_size = r.end(2) - r.begin(2);
//    const unsigned int rbegin0 = r.begin(0);
//    const unsigned int rbegin1 = r.begin(1);
//    const unsigned int rbegin2 = r.begin(2);
//    //TODO: Verify teamThreadRangeSize is greater than the teamPolicy range.
//    const unsigned int teamThreadRangeSize = (i_size > 0 ? i_size : 1) * (j_size > 0 ? j_size : 1) * (k_size > 0 ? k_size : 1);
//    //if ( std::is_same< Kokkos::Cuda , ExecutionSpace >::value ) {
//      typedef Kokkos::TeamPolicy< Kokkos::Cuda > policy_type;
//
//      Kokkos::parallel_reduce (Kokkos::TeamPolicy<Kokkos::Cuda>(1,256),
//                               KOKKOS_LAMBDA ( typename policy_type::member_type thread, ReductionType& inner_sum ) {
//        printf("i is %d\n", thread.team_rank());
//        Kokkos::parallel_for (Kokkos::TeamThreadRange(thread, teamThreadRangeSize), [&] (const int& n) {
//          printf("n is %d\n", n);
//          int i = n / (j_size * k_size) + rbegin0;
//          int j = (n / k_size) % j_size + rbegin1;
//          int k = n % k_size + rbegin2;
//          functor(i,j,k, inner_sum);
//        });
//      }, tmp);
//  //}
//  }
//};
//#endif
//
//template <Kokkos::OpenMP, typename Functor, typename ReductionType>
//struct parallel_functions {
//  static void parallel_reduce_sum( BlockRange const & r, const Functor & functor, ReductionType & red ) {
//
//    ReductionType tmp = red;
//    const unsigned int i_size = r.end(0) - r.begin(0);
//    const unsigned int j_size = r.end(1) - r.begin(1);
//    const unsigned int k_size = r.end(2) - r.begin(2);
//    const unsigned int rbegin0 = r.begin(0);
//    const unsigned int rbegin1 = r.begin(1);
//    const unsigned int rbegin2 = r.begin(2);
//    //TODO: Verify teamThreadRangeSize is greater than the teamPolicy range.
//    const unsigned int teamThreadRangeSize = (i_size > 0 ? i_size : 1) * (j_size > 0 ? j_size : 1) * (k_size > 0 ? k_size : 1);
//    //if ( std::is_same< Kokkos::Cuda , ExecutionSpace >::value ) {
//      typedef Kokkos::TeamPolicy< Kokkos::OpenMP > policy_type;
//
//      Kokkos::parallel_reduce (Kokkos::TeamPolicy<Kokkos::OpenMP>(1,16),
//                               KOKKOS_LAMBDA ( typename policy_type::member_type thread, ReductionType& inner_sum ) {
//        printf("i is %d\n", thread.team_rank());
//        Kokkos::parallel_for (Kokkos::TeamThreadRange(thread, teamThreadRangeSize), [&] (const int& n) {
//          printf("n is %d\n", n);
//          int i = n / (j_size * k_size) + rbegin0;
//          int j = (n / k_size) % j_size + rbegin1;
//          int k = n % k_size + rbegin2;
//          functor(i,j,k, inner_sum);
//        });
//      }, tmp);
//  //}
//  }
//};


template <typename Functor, typename ReductionType>
void parallel_reduce_min( BlockRange const & r, const Functor & f, ReductionType & red  )
{
  ReductionType tmp = red;
#if defined( HAVE_CUDA )
  typedef typename Kokkos::MDRangePolicy<Kokkos::OpenMP, Kokkos::Rank<3,
                                                       Kokkos::Iterate::Left,
                                                       Kokkos::Iterate::Left>
                                                       > MDPolicyType_3D;

  MDPolicyType_3D mdpolicy_3d( {{r.begin(0),r.begin(1),r.begin(2)}}, {{r.end(0),r.end(1),r.end(2)}} );

  Kokkos::parallel_reduce( mdpolicy_3d, f, tmp );
#else
  const int ib = r.begin(0); const int ie = r.end(0);
  const int jb = r.begin(1); const int je = r.end(1);
  const int kb = r.begin(2); const int ke = r.end(2);

  Kokkos::parallel_reduce( Kokkos::RangePolicy<Kokkos::OpenMP, int>(kb, ke).set_chunk_size(2), KOKKOS_LAMBDA(int k, ReductionType & tmp) {
    for (int j=jb; j<je; ++j) {
    for (int i=ib; i<ie; ++i) {
      f(i,j,k,tmp);
    }}
  }, Kokkos::Experimental::Min<ReductionType>(tmp));

#endif
  red = tmp;
}


#else //if !defined( UINTAH_ENABLE_KOKKOS )
//This section is for running a functor/lambda expression if Kokkos isn't used at all.
//Instead of Kokkos being responsible for the execution, we execute it.

template <typename Functor>
void parallel_for( BlockRange const & r, const Functor & f )
{
  const int ib = r.begin(0); const int ie = r.end(0);
  const int jb = r.begin(1); const int je = r.end(1);
  const int kb = r.begin(2); const int ke = r.end(2);

  for (int k=kb; k<ke; ++k) {
  for (int j=jb; j<je; ++j) {
  for (int i=ib; i<ie; ++i) {
    f(i,j,k);
  }}}
};

//Runtime code has already started using parallel_for constructs.  These should NOT be executed on
//a GPU.  This function allows a developer to ensure the task only runs on CPU code.
template <typename Functor>
void parallel_for_cpu_only( BlockRange const & r, const Functor & f )
{
  parallel_for( r, f );
}


template <typename Functor, typename Option>
void parallel_for( BlockRange const & r, const Functor & f, const Option& op )
{
  const int ib = r.begin(0); const int ie = r.end(0);
  const int jb = r.begin(1); const int je = r.end(1);
  const int kb = r.begin(2); const int ke = r.end(2);

  for (int k=kb; k<ke; ++k) {
  for (int j=jb; j<je; ++j) {
  for (int i=ib; i<ie; ++i) {
    f(op,i,j,k);
  }}}
};

//template <typename Functor, typename ReductionType>
//void parallel_reduce_sum( BlockRange const & r, const Functor & f, ReductionType & red  )
//{
//  const int ib = r.begin(0); const int ie = r.end(0);
//  const int jb = r.begin(1); const int je = r.end(1);
//  const int kb = r.begin(2); const int ke = r.end(2);
//
//  ReductionType tmp = red;
//  for (int k=kb; k<ke; ++k) {
//  for (int j=jb; j<je; ++j) {
//  for (int i=ib; i<ie; ++i) {
//    f(i,j,k,tmp);
//  }}}
//  red = tmp;
//};

template <typename Functor, typename ReductionType>
void parallel_reduce_min( BlockRange const & r, const Functor & f, ReductionType & red  )
{
  const int ib = r.begin(0); const int ie = r.end(0);
  const int jb = r.begin(1); const int je = r.end(1);
  const int kb = r.begin(2); const int ke = r.end(2);

  ReductionType tmp = red;
  for (int k=kb; k<ke; ++k) {
  for (int j=jb; j<je; ++j) {
  for (int i=ib; i<ie; ++i) {
    f(i,j,k,tmp);
  }}}
  red = tmp;
};

#endif


} // namespace Uintah

#endif // UINTAH_HOMEBREW_LOOP_EXECUTION_HPP
