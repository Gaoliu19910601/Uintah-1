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

#ifndef CCA_COMPONENTS_SCHEDULERS_UNSTRUCTURED_DWDATABASE_H
#define CCA_COMPONENTS_SCHEDULERS_UNSTRUCTURED_DWDATABASE_H

#include <CCA/Components/Schedulers/UnstructuredMemoryLog.h>

#include <Core/Grid/UnknownVariable.h>
#include <Core/Grid/Variables/UnstructuredReductionVariableBase.h>
#include <Core/Grid/Variables/UnstructuredPerPatchBase.h>
#include <Core/Grid/Variables/UnstructuredScrubItem.h>
#include <Core/Grid/Variables/UnstructuredVarLabel.h>
#include <Core/Grid/Variables/UnstructuredVarLabelMatl.h>
#include <Core/Grid/Variables/UnstructuredVariable.h>
#include <Core/Containers/FastHashTable.h>
#include <Core/Exceptions/InternalError.h>
#include <Core/Malloc/Allocator.h>
#include <Core/Parallel/MasterLock.h>
#include <Core/Parallel/Parallel.h>
#include <Core/Util/FancyAssert.h>

#include <sci_hash_map.h>

#include <ostream>
#include <sstream>
#include <string>
#include <vector>


/**************************************

  CLASS
    UnstructuredDWDatabase

  GENERAL INFORMATION

    UnstructuredDWDatabase.h

    Steven G. Parker
    Department of Computer Science
    University of Utah

    Center for the Simulation of Accidental Fires and Explosions (C-SAFE)


  KEYWORDS
    UnstructuredDWDatabase

  DESCRIPTION


****************************************/
namespace {

Uintah::MasterLock g_unstructured_keyDB_mutex{};
Uintah::MasterLock g_unstructured_mvars_mutex{};

}

namespace Uintah {


template<class DomainType>
class UnstructuredKeyDatabase {

  template<class T> friend class UnstructuredDWDatabase;

public:

  UnstructuredKeyDatabase() {};

  ~UnstructuredKeyDatabase() {};

  void clear();

  void insert( const UnstructuredVarLabel   * label
             ,       int          matlIndex
             , const DomainType * dom
             );

  int lookup( const UnstructuredVarLabel   * label
            ,       int          matlIndex
            , const DomainType * dom
            );

  void merge( const UnstructuredKeyDatabase<DomainType>& newDB );

  void print( std::ostream & out, int rank ) const;

private:

  using keyDBtype = hashmap<UnstructuredVarLabelMatl<DomainType>, int>;
  keyDBtype m_keys;

  int m_key_count { 0 };

};


template<class DomainType>
class UnstructuredDWDatabase {

  public:

    UnstructuredDWDatabase() {};

    ~UnstructuredDWDatabase();

    void clear();

    void doReserve( UnstructuredKeyDatabase<DomainType>* keydb );

    bool exists( const UnstructuredVarLabel   * label
               ,       int          matlIndex
               , const DomainType * dom
               ) const;


    void put( const UnstructuredVarLabel   * label
            ,       int          matlindex
            , const DomainType * dom
            ,       UnstructuredVariable   * var
            ,       bool         init
            ,       bool         replace
            );


    void putReduce( const UnstructuredVarLabel              * label
                  ,       int                     matlindex
                  , const DomainType            * dom
                  ,       UnstructuredReductionVariableBase * var
                  ,       bool init);


    void putForeign( const UnstructuredVarLabel   * label
                   ,       int          matlindex
                   , const DomainType * dom
                   ,       UnstructuredVariable   * var
                   ,       bool         init
                   );


    void get( const UnstructuredVarLabel   * label
            ,       int          matlindex
            , const DomainType * dom
            ,       UnstructuredVariable   & var
            ) const;


    void getlist( const UnstructuredVarLabel               * label
                ,       int                      matlIndex
                , const DomainType             * dom
                ,       std::vector<UnstructuredVariable*> & varlist
                ) const;


    inline UnstructuredVariable* get( const UnstructuredVarLabel   * label
                        ,       int          matlindex
                        , const DomainType * dom
                        ) const;


    void print( std::ostream & out, int rank ) const;


    void cleanForeign();

    // Scrub counter manipulator functions -- when the scrub count goes to
    // zero, the data is scrubbed.  Return remaining count

    // How Scrubbing works:  If a variable is in the OldDW, at the beginning of the timestep
    // initializeScrubs will be called for each of those variables.  For each variable computed
    // or copied via MPI, setScrubCount will be called on it, based on the scrubCountTable in
    // DetailedTasks.  Then, when the variable is used, decrementScrubCount is called on it
    // and if the count reaches zero, it is scrubbed.
    int decrementScrubCount( const UnstructuredVarLabel   * label
                           ,       int          matlindex
                           , const DomainType * dom
                           );


    void setScrubCount( const UnstructuredVarLabel   * label
                      ,       int          matlindex
                      , const DomainType * dom
                      ,       int          count
                      );


    void scrub( const UnstructuredVarLabel   * label
              ,       int          matlindex
              , const DomainType * dom
              );


    // add means increment the scrub count instead of setting it.  This is for when a DW
    // can act as a CoarseOldDW as well as an OldDW
    void initializeScrubs(       int                        dwid
                         , const FastHashTable<UnstructuredScrubItem> * scrubcounts
                         ,       bool                       add
                         );


    void logMemoryUse(       std::ostream  & out
                     ,       unsigned long & total
                     , const std::string   & tag
                     ,       int             dwid
                     );

    void getVarLabelMatlTriples(std::vector<UnstructuredVarLabelMatl<DomainType> >& vars) const;


  private:

    struct DataItem {

        DataItem() {}

        ~DataItem()
        {
          if (m_next) {
            delete m_next;
          }
          ASSERT(m_var);
          delete m_var;
        }
        UnstructuredVariable        * m_var  { nullptr };
        struct DataItem * m_next { nullptr };
    };

    DataItem* getDataItem( const UnstructuredVarLabel   * label
                         ,       int          matlindex
                         , const DomainType * dom
                         ) const;

    UnstructuredKeyDatabase<DomainType>* m_keyDB {};

    using varDBtype = std::vector<DataItem*>;
    varDBtype m_vars;

    using scrubDBtype = std::vector<int>;
    scrubDBtype m_scrubs;

    // eliminate copy, assignment and move
    UnstructuredDWDatabase( const UnstructuredDWDatabase & )            = delete;
    UnstructuredDWDatabase& operator=( const UnstructuredDWDatabase & ) = delete;
    UnstructuredDWDatabase( UnstructuredDWDatabase && )                 = delete;
    UnstructuredDWDatabase& operator=( UnstructuredDWDatabase && )      = delete;
};

//______________________________________________________________________
//
template<class DomainType>
UnstructuredDWDatabase<DomainType>::~UnstructuredDWDatabase()
{
  clear();
}

//______________________________________________________________________
//
template<class DomainType>
void UnstructuredDWDatabase<DomainType>::clear()
{
  for (auto iter = m_vars.begin(); iter != m_vars.end(); ++iter) {
    if (*iter) {
      delete *iter;
    }
    *iter = nullptr;
  }
  m_vars.clear();
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::cleanForeign()
{
  std::lock_guard<Uintah::MasterLock> exists_lock(g_unstructured_mvars_mutex);

  for (auto iter = m_vars.begin(); iter != m_vars.end(); ++iter) {
    if (*iter && (*iter)->m_var->isForeign()) {
      delete (*iter);
      (*iter) = nullptr;
    }
  }
}

//______________________________________________________________________
//
template<class DomainType>
int
UnstructuredDWDatabase<DomainType>::decrementScrubCount( const UnstructuredVarLabel   * label
                                           ,       int          matlIndex
                                           , const DomainType * dom
                                           )
{
  // Dav's conjectures on how this works:
  //   setScrubCount is called the first time with "count" set to some X.  
  //   This X represents the number of tasks that will use the var.  Later,
  //   after a task has used the var, it will call decrementScrubCount
  //   If scrubCount then is equal to 0, the var is scrubbed.

  ASSERT(matlIndex >= -1);
  int idx = m_keyDB->lookup(label, matlIndex, dom);
  if (idx == -1) {
    return 0;
  }
  if (!m_vars[idx]) {
    return 0;
  }
  int rt = __sync_sub_and_fetch(&(m_scrubs[idx]), 1);
  if (rt == 0) {
    delete m_vars[idx];
    m_vars[idx] = nullptr;
  }
  return rt;
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::setScrubCount( const UnstructuredVarLabel   * label
                                     ,       int          matlIndex
                                     , const DomainType * dom
                                     ,       int          count
                                     )
{
  int idx = m_keyDB->lookup(label, matlIndex, dom);
  if (idx == -1) {
    SCI_THROW(UnknownVariable(label->getName(), -99, dom, matlIndex, "UnstructuredDWDatabase::setScrubCount", __FILE__, __LINE__));
  }
  m_scrubs[idx] = count;

  // TODO do we need this - APH 03/01/17
//  if (!__sync_bool_compare_and_swap(&(m_scrubs[idx]), 0, count)) {
//      SCI_THROW(InternalError("overwriting non-zero scrub counter", __FILE__, __LINE__));
//  }
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::scrub( const UnstructuredVarLabel   * label
                             ,       int          matlIndex
                             , const DomainType * dom
                             )
{
  ASSERT(matlIndex >= -1);
  int idx = m_keyDB->lookup(label, matlIndex, dom);

#if 0
  if (m_vars[idx] == nullptr) {  // scrub not found
    std::ostringstream msgstr;
    msgstr << label->getName() << ", matl " << matlIndex << ", patch/level " << dom->getID() << " not found for scrubbing.";
    SCI_THROW(InternalError(msgstr.str(), __FILE__, __LINE__));
  }
#endif

  if (idx != -1 && m_vars[idx]) {
    delete m_vars[idx];
    m_vars[idx] = nullptr;
  }
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::initializeScrubs(       int                        dwid
                                        , const FastHashTable<UnstructuredScrubItem> * scrubcounts
                                        ,       bool                       add
                                        )
{
  // loop over each variable, probing the scrubcount map. Set the scrubcount appropriately.
  // If the variable has no entry in the scrubcount map, delete it
  for (auto keyiter = m_keyDB->m_keys.begin(); keyiter != m_keyDB->m_keys.end();) {
    if (m_vars[keyiter->second]) {
      UnstructuredVarLabelMatl<DomainType> vlm = keyiter->first;
      // See if it is in the scrubcounts map.
      UnstructuredScrubItem key(vlm.label_, vlm.matlIndex_, vlm.domain_, dwid);
      UnstructuredScrubItem* result = scrubcounts->lookup(&key);
      if (!result && !add) {
        delete m_vars[keyiter->second];
        m_vars[keyiter->second] = nullptr;
      }
      else {
        if (result) {
          if (add) {
            __sync_add_and_fetch(&(m_scrubs[keyiter->second]), result->m_count);
          }
          else {
            if (!__sync_bool_compare_and_swap(&(m_scrubs[keyiter->second]), 0, result->m_count)) {
              SCI_THROW(InternalError("initializing non-zero scrub counter", __FILE__, __LINE__));
            }
          }
        }
        keyiter++;
      }
    }
    else {
      keyiter++;
    }
  }
}

//______________________________________________________________________
//
template<class DomainType>
int
UnstructuredKeyDatabase<DomainType>::lookup( const UnstructuredVarLabel   * label
                               ,       int          matlIndex
                               , const DomainType * dom
                               )
{
  std::lock_guard<Uintah::MasterLock> lookup_lock(g_unstructured_keyDB_mutex);

  UnstructuredVarLabelMatl<DomainType> v(label, matlIndex, getRealDomain(dom));
  typename keyDBtype::const_iterator const_iter = m_keys.find(v);
  if (const_iter == m_keys.end()) {
    return -1;
  }
  else {
    return const_iter->second;
  }
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredKeyDatabase<DomainType>::merge( const UnstructuredKeyDatabase<DomainType> & newDB )
{
  for (typename keyDBtype::const_iterator const_keyiter = newDB.m_keys.begin(); const_keyiter != newDB.m_keys.end();
      const_keyiter++) {
    typename keyDBtype::const_iterator const_db_iter = m_keys.find(const_keyiter->first);
    if (const_db_iter == m_keys.end()) {
      m_keys.insert(std::pair<UnstructuredVarLabelMatl<DomainType>, int>(const_keyiter->first, m_key_count++));
    }
  }
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredKeyDatabase<DomainType>::insert( const UnstructuredVarLabel   * label
                               ,       int          matlIndex
                               , const DomainType * dom
                               )
{
  UnstructuredVarLabelMatl<DomainType> v(label, matlIndex, getRealDomain(dom));
  typename keyDBtype::const_iterator const_iter = m_keys.find(v);
  if (const_iter == m_keys.end()) {
    m_keys.insert(std::pair<UnstructuredVarLabelMatl<DomainType>, int>(v, m_key_count++));
  }
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredKeyDatabase<DomainType>::clear()
{
  m_keys.clear();
  m_key_count = 0;
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredKeyDatabase<DomainType>::print( std::ostream & out, int rank ) const
{
  for (auto keyiter = m_keys.begin(); keyiter != m_keys.end(); keyiter++) {
    const UnstructuredVarLabelMatl<DomainType>& vlm = keyiter->first;
    const DomainType* dom = vlm.domain_;
    if (dom) {
      out << rank << " Name: " << vlm.label_->getName() << "  domain: " << *dom << "  matl:" << vlm.matlIndex_ << '\n';
    }
    else {
      out << rank << " Name: " << vlm.label_->getName() << "  domain: N/A  matl: " << vlm.matlIndex_ << '\n';
    }
  }
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::doReserve( UnstructuredKeyDatabase<DomainType> * keydb )
{
  m_keyDB = keydb;
  m_vars.resize(m_keyDB->m_key_count + 1, (DataItem*)nullptr);
  m_scrubs.resize(m_keyDB->m_key_count + 1, 0);
}

//______________________________________________________________________
//
template<class DomainType>
bool
UnstructuredDWDatabase<DomainType>::exists( const UnstructuredVarLabel   * label
                              ,       int          matlIndex
                              , const DomainType * dom
                              ) const
{
  // lookup is lock_guard protected
  int idx = m_keyDB->lookup(label, matlIndex, dom);

  {
    std::lock_guard<Uintah::MasterLock> exists_lock(g_unstructured_mvars_mutex);
    if (idx == -1) {
      return false;
    }
    if (m_vars[idx] == nullptr) {
      return false;
    }
    return true;
  }

}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::put( const UnstructuredVarLabel   * label
                           ,       int          matlIndex
                           , const DomainType * dom
                           ,       UnstructuredVariable   * var
                           ,       bool         init
                           ,       bool         replace
                           )
{

  ASSERT(matlIndex >= -1);

  {
    std::lock_guard<Uintah::MasterLock> put_lock(g_unstructured_keyDB_mutex);
    if (init) {
      m_keyDB->insert(label, matlIndex, dom);
      this->doReserve(m_keyDB);
    }
  }

  // lookup is lock_guard protected
  int idx = m_keyDB->lookup(label, matlIndex, dom);
  if (idx == -1) {
    SCI_THROW(UnknownVariable(label->getName(), -1, dom, matlIndex, "check task computes", __FILE__, __LINE__));
  }

  if (m_vars[idx]) {
    if (m_vars[idx]->m_next) {
      SCI_THROW(InternalError("More than one vars on this label", __FILE__, __LINE__));
    }
    if (!replace) {
      SCI_THROW(InternalError("Put replacing old vars", __FILE__, __LINE__));
    }
    ASSERT(m_vars[idx]->m_var != var);
    delete m_vars[idx];
  }
  DataItem* newdi = new DataItem();
  newdi->m_var = var;
  m_vars[idx] = newdi;
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::putReduce( const UnstructuredVarLabel              * label
                                 ,       int                     matlIndex
                                 , const DomainType            * dom
                                 ,       UnstructuredReductionVariableBase * var
                                 ,       bool                    init
                                 )
{
  ASSERT(matlIndex >= -1);

  {
    std::lock_guard<Uintah::MasterLock> put_reduce_lock(g_unstructured_keyDB_mutex);
    if (init) {
      m_keyDB->insert(label, matlIndex, dom);
      this->doReserve(m_keyDB);
    }
  }

  // lookup is lock_guard protected
  int idx = m_keyDB->lookup(label, matlIndex, dom);
  if (idx == -1) {
    SCI_THROW(UnknownVariable(label->getName(), -1, dom, matlIndex, "check task computes", __FILE__, __LINE__));
  }

  DataItem* newdi = new DataItem();
  newdi->m_var = var;
  do {
    DataItem* olddi = __sync_lock_test_and_set(&m_vars[idx], 0);
    if (olddi == nullptr) {
      olddi = newdi;
    }
    else {
      UnstructuredReductionVariableBase* oldvar = dynamic_cast<UnstructuredReductionVariableBase*>(olddi->m_var);
      UnstructuredReductionVariableBase* newvar = dynamic_cast<UnstructuredReductionVariableBase*>(newdi->m_var);
      oldvar->reduce(*newvar);
      delete newdi;
    }
    newdi = __sync_lock_test_and_set(&m_vars[idx], olddi);
  }
  while (newdi != nullptr);
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::putForeign( const UnstructuredVarLabel   * label
                                  ,       int          matlIndex
                                  , const DomainType * dom
                                  ,       UnstructuredVariable   * var
                                  ,       bool         init
                                  )
{
  ASSERT(matlIndex >= -1);

  {
    std::lock_guard<Uintah::MasterLock> put_foreign_lock(g_unstructured_keyDB_mutex);
    if (init) {
      m_keyDB->insert(label, matlIndex, dom);
      this->doReserve(m_keyDB);
    }
  }

  // lookup is lock_guard protected
  int idx = m_keyDB->lookup(label, matlIndex, dom);
  if (idx == -1) {
    SCI_THROW(UnknownVariable(label->getName(), -1, dom, matlIndex, "check task computes", __FILE__, __LINE__));
  }

  DataItem* newdi = new DataItem();
  newdi->m_var = var;
  do {
    newdi->m_next = m_vars[idx];
  }
  while (!__sync_bool_compare_and_swap(&m_vars[idx], newdi->m_next, newdi));  // vars[iter->second] = newdi;
}

//______________________________________________________________________
//
template<class DomainType>
typename UnstructuredDWDatabase<DomainType>::DataItem*
UnstructuredDWDatabase<DomainType>::getDataItem( const UnstructuredVarLabel   * label
                                   ,       int          matlIndex
                                   , const DomainType * dom
                                   ) const
{
  ASSERT(matlIndex >= -1);
  int idx = m_keyDB->lookup(label, matlIndex, dom);
  if (idx == -1) {
    SCI_THROW(UnknownVariable(label->getName(), -99, dom, matlIndex, "UnstructuredDWDatabase::getDataItem", __FILE__, __LINE__));
  }
  return m_vars[idx];
}

//______________________________________________________________________
//
template<class DomainType>
inline
UnstructuredVariable*
UnstructuredDWDatabase<DomainType>::get( const UnstructuredVarLabel   * label
                           ,       int          matlIndex
                           , const DomainType * dom
                           ) const
{
  const DataItem* dataItem = getDataItem(label, matlIndex, dom);
  ASSERT(dataItem != nullptr);          // should have thrown an exception before
  ASSERT(dataItem->m_next == nullptr);  // should call getlist()
  return dataItem->m_var;
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::get( const UnstructuredVarLabel   * label
                           ,       int          matlIndex
                           , const DomainType * dom
                           ,       UnstructuredVariable   & var
                           ) const
{
  UnstructuredVariable* tmp = get(label, matlIndex, dom);
  var.copyPointer(*tmp);
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::getlist( const UnstructuredVarLabel               * label
                               ,       int                      matlIndex
                               , const DomainType             * dom
                               ,       std::vector<UnstructuredVariable*> & varlist
                               ) const
{
  // this function is allowed to return an empty list

  for (DataItem* dataItem = getDataItem(label, matlIndex, dom); dataItem != nullptr; dataItem = dataItem->m_next) {
    varlist.push_back(dataItem->m_var);
  }
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::print( std::ostream & out, int rank ) const
{
  for (auto keyiter = m_keyDB->m_keys.begin(); keyiter != m_keyDB->m_keys.end(); keyiter++) {
    if (m_vars[keyiter->second]) {
      const UnstructuredVarLabelMatl<DomainType>& vlm = keyiter->first;
      const DomainType* dom = vlm.domain_;
      if (dom) {
        out << rank << " Name: " << vlm.label_->getName() << "  domain: " << *dom << "  matl:" << vlm.matlIndex_ << '\n';
      }
      else {
        out << rank << " Name: " << vlm.label_->getName() << "  domain: N/A  matl: " << vlm.matlIndex_ << '\n';
      }
    }
  }
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::logMemoryUse(       std::ostream  & out
                                    ,       unsigned long & total
                                    , const std::string   & tag
                                    ,       int             dwid
                                    )
{
  for (auto keyiter = m_keyDB->m_keys.begin(); keyiter != m_keyDB->m_keys.end(); keyiter++) {
    if (m_vars[keyiter->second]) {
      UnstructuredVariable* var = m_vars[keyiter->second]->m_var;
      UnstructuredVarLabelMatl<DomainType> vlm = keyiter->first;
      const UnstructuredVarLabel* label = vlm.label_;
      std::string elems;
      unsigned long totsize;
      void* ptr;
      var->getSizeInfo(elems, totsize, ptr);
      const UnstructuredTypeDescription* td = label->typeDescription();

      logMemory(out, total, tag, label->getName(), (td ? td->getName() : "-"), vlm.domain_,
                vlm.matlIndex_, elems, totsize, ptr, dwid);
    }
  }
}

//______________________________________________________________________
//
template<class DomainType>
void
UnstructuredDWDatabase<DomainType>::getVarLabelMatlTriples( std::vector<UnstructuredVarLabelMatl<DomainType> > & v) const
{
  for (auto keyiter = m_keyDB->m_keys.begin(); keyiter != m_keyDB->m_keys.end(); keyiter++) {
    const UnstructuredVarLabelMatl<DomainType>& vlm = keyiter->first;
    if (m_vars[keyiter->second]) {
      v.push_back(vlm);
    }
  }
}

} // namespace Uintah


//
// Hash function for VarLabelMatl
//
#ifdef HAVE_GNU_HASHMAP

  namespace __gnu_cxx
  {
    using Uintah::UnstructuredDWDatabase;
    using Uintah::UnstructuredVarLabelMatl;
    template <class DomainType>
    struct hash<UnstructuredVarLabelMatl<DomainType> > : public std::unary_function<UnstructuredVarLabelMatl<DomainType>, size_t>
    {
      size_t operator()(const UnstructuredVarLabelMatl<DomainType>& v) const
      {
        size_t h=0;
        char *str =const_cast<char*> (v.label_->getName().data());
        while (int c = *str++) h = h*7+c;
        return ( ( ((size_t)v.label_) << (sizeof(size_t)/2) ^ ((size_t)v.label_) >> (sizeof(size_t)/2) )
                 ^ (size_t)v.domain_ ^ (size_t)v.matlIndex_ );
      }
    };
  }

#elif HAVE_TR1_HASHMAP || HAVE_C11_HASHMAP 

  namespace std {
#if HAVE_TR1_HASHMAP 
    namespace tr1 {
#endif 
      using Uintah::UnstructuredDWDatabase;
      using Uintah::UnstructuredVarLabelMatl;
      template <class DomainType>
      struct hash<UnstructuredVarLabelMatl<DomainType> > : public unary_function<UnstructuredVarLabelMatl<DomainType>, size_t>
      {
        size_t operator()(const UnstructuredVarLabelMatl<DomainType>& v) const
        {
          size_t h=0;
          char *str =const_cast<char*> (v.label_->getName().data());
          while (int c = *str++) h = h*7+c;
          return ( ( ((size_t)v.label_) << (sizeof(size_t)/2) ^ ((size_t)v.label_) >> (sizeof(size_t)/2) )
                   ^ (size_t)v.domain_ ^ (size_t)v.matlIndex_ );
        }
      };
#if HAVE_TR1_HASHMAP 
    } // end namespace tr1
#endif 
  } // end namespace std

#endif // HAVE_GNU_HASHMAP

#endif // CCA_COMPONENTS_SCHEDULERS_UnstructuredDWDATABASE_H
