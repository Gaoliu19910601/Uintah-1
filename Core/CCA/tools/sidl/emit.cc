/*
  For more information, please see: http://software.sci.utah.edu

  The MIT License

  Copyright (c) 2004 Scientific Computing and Imaging Institute,
  University of Utah.

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

#include <sci_defs/config_defs.h>
#include <sci_defs/mpi_defs.h>
#include <Core/CCA/tools/sidl/Spec.h>
#include <Core/CCA/tools/sidl/SymbolTable.h>
#include <Core/Util/FileUtils.h>

#include <map>
#include <algorithm>
#include <iostream>
#include <sstream>

#include <ctype.h>
#include <stdio.h>

extern bool doing_cia;

//Uncomment line below should debugging files of data transfer be created
//#define MxNDEBUG

#ifdef OLD_STUFF
static std::string handle_class = "\
class @ {\n\
  @_interface* ptr;\n\
public:\n\
  static const ::SCIRun::TypeInfo* _getTypeInfo();\n\
  typedef @_interface interfacetype;\n\
  inline @()\n\
  {\n\
    ptr=0;\n\
  }\n\
  inline @(@_interface* ptr)\n\
    : ptr(ptr)\n\
  {\n\
    if(ptr)\n\
      ptr->_addReference();\n\
  }\n\
  inline ~@()\n\
  {\n\
    if(ptr)\n\
      ptr->_deleteReference();\n\
  }\n\
  inline @(const @& copy)\n\
    : ptr(copy.ptr)\n\
  {\n\
    if(ptr)\n\
      ptr->_addReference();\n\
  }\n\
  inline @& operator=(const @& copy)\n\
  {\n\
    if(&copy != this){\n\
      if(ptr)\n\
        ptr->_deleteReference();\n\
        if(copy.ptr)\n\
          copy.ptr->_addReference();\n\
    }\n\
    ptr=copy.ptr;\n\
    return *this;\n\
  }\n\
  inline @_interface* getPointer() const\n\
  {\n\
    return ptr;\n\
  }\n\
  inline @_interface* operator->() const\n\
  {\n\
    return ptr;\n\
  }\n\
  inline operator bool() const\n\
  {\n\
    return ptr != 0;\n\
  }\n\
  inline operator ::SCIRun::Object() const\n\
  {\n\
    return ptr;\n\
  }\n\
";
#endif

/*
  Local method which takes possibly illegal variable name
  and converts it to a legal, but (hopefully) still unique name
*/
std::string produceLegalVar(std::string illegalVar)
{
  std::string legal = "_";
  for (unsigned int i = 0; i < illegalVar.size(); i++) {
    if (isalnum(illegalVar[i])) {
      legal += illegalVar[i];
    }
  }
  return legal;
}

struct Leader {
};

static Leader leader2;

struct SState : public std::ostringstream {
  std::string leader;
  SymbolTable* currentPackage;
  EmitState*e;
  SState(EmitState* e) : e(e) {
    currentPackage = 0;
    leader = "";
  }
  void begin_namespace(SymbolTable*);
  void close_namespace();
  void recursive_begin_namespace(SymbolTable*);

  std::string push_leader() {
    std::string oldleader=leader;
    leader += "  ";
    return oldleader;
  }
  void pop_leader(const std::string& oldleader) {
    leader = oldleader;
  }

  friend std::ostream& operator<<( std::ostream& out, const Leader& );
};

std::ostream& operator<<( SState& out, const Leader& ) {
  out << out.leader;
  return out;
}


struct EmitState {
  int instanceNum;
  int handlerNum;
  EmitState();

  SState fwd;
  SState decl;
  SState out;
  SState proxy;

  //Used for the exception casting method
  SState xcept;
};

EmitState::EmitState()
  : fwd(this), decl(this), out(this), proxy(this), xcept(this)
{
  instanceNum = 0;
  handlerNum = 0;
}

void emit_cast_exception(EmitState& e)
{
  e.out << "\n//cast exceptions\n";
  e.out << "static void _castException(int _xid, SCIRun::Message** _xMsg)\n";
  e.out << "{\n";
  e.out << leader2 << "  switch (_xid) {\n";
  e.out << e.xcept.str();
  e.out << leader2 << "  default:\n";
  e.out << leader2 << "    throw ::SCIRun::InternalError(\"Trying to cast an unknown exception\", __FILE__, __LINE__);\n";
  e.out << leader2 << "  }\n";
  e.out << "}\n\n";
}

void SpecificationList::emit(std::ostream& out, std::ostream& hdr,
			     const std::string& hname) const
{
  EmitState e;
  // Emit code for each definition
  globals->emit(e);
  e.fwd.close_namespace();
  e.decl.close_namespace();
  e.out.close_namespace();
  e.proxy.close_namespace();
  emit_cast_exception(e);

  hdr << "/*\n";
  hdr << " * This code was automatically generated by sidl,\n";
  hdr << " * do not edit directly!\n";
  hdr << " */\n";
  hdr << "\n";
  std::string ifname(hname);
  replace(ifname.begin(), ifname.end(), '/', '_');
  replace(ifname.begin(), ifname.end(), '.', '_');
  hdr << "#ifndef _sidl_generated_" << ifname << '\n';
  hdr << "#define _sidl_generated_" << ifname << '\n';
  hdr << "\n";
  hdr << "#include <Core/CCA/PIDL/Object.h>\n";
  hdr << "#include <Core/CCA/PIDL/pidl_cast.h>\n";
  hdr << "#include <Core/CCA/PIDL/MxNArrayRep.h>\n";
  hdr << "#include <Core/CCA/SSIDL/sidl_sidl.h>\n";
  hdr << "#include <Core/CCA/SSIDL/array.h>\n";
  hdr << "#include <Core/CCA/SSIDL/string.h>\n";
  hdr << "#include <Core/CCA/SmartPointer.h>\n";
  hdr << "#include <complex>\n";
  hdr << std::endl;

  //Include imported file headers *HACK*
  std::vector<Specification*>::const_iterator iter;
  for (iter = specs.begin(); iter != specs.end(); iter++) {
    if ((*iter)->isImport) {
      Definition* def = (*iter)->packages->list.back();
      std::string fname = def->curfile;

      std::string::size_type i = 0;
      // Not terribly efficient, but works until another solution for
      // searching build directory for c++ headers generated from sidl
      // is found.
      while (i < fname.length()) {
	std::string::size_type start_pos;
	if (i == 0) { // start search from beginning of curfile
	  start_pos = 0;
	  i += 1;
	} else {
	  // WARNING: MS Windows uses dir separator "\"
	  start_pos = fname.find("/", i);
	  start_pos += 1;
	  i = start_pos;
	}
	std::string::size_type end_pos = fname.find(".sidl");
	std::string inclfile = fname.substr(start_pos, end_pos - start_pos) + "_sidl.h";

	if (SCIRun::validFile(inclfile)) {
	  hdr << "#include <" << inclfile << ">\n";
	  break;
	}
      }
    }
  }

  hdr << std::endl;
  hdr << e.fwd.str();
  hdr << e.decl.str();
  hdr << "\n#endif\n" << std::endl;

  // Emit #includes
  out << "/*\n";
  out << " * This code was automatically generated by sidl,\n";
  out << " * do not edit directly!\n";
  out << " */\n";
  out << "\n";
  out << "#include \"" << hname << "\"\n";
  out << "#include <Core/Exceptions/InternalError.h>\n";
  out << "#include <Core/CCA/PIDL/Object_proxy.h>\n";
  out << "#include <Core/CCA/PIDL/ProxyBase.h>\n";
  out << "#include <Core/CCA/PIDL/Reference.h>\n";
  out << "#include <Core/CCA/PIDL/ReferenceMgr.h>\n";
  out << "#include <Core/CCA/PIDL/ServerContext.h>\n";
  out << "#include <Core/CCA/PIDL/TypeInfo.h>\n";
  out << "#include <Core/CCA/PIDL/TypeInfo_internal.h>\n";
  out << "#include <Core/CCA/PIDL/PIDL.h>\n";
  out << "#include <Core/CCA/PIDL/Message.h>\n";
  out << "#include <Core/CCA/PIDL/PRMI.h>\n";
  out << "#include <Core/CCA/PIDL/MxNScheduler.h>\n";
  out << "#include <Core/CCA/PIDL/MxNArrSynch.h>\n";
  out << "#include <Core/CCA/PIDL/MxNMetaSynch.h>\n";
  out << "#include <Core/CCA/PIDL/xcept.h>\n";
  out << "#include <Core/Util/NotFinished.h>\n";
  out << "#include <Core/Thread/Thread.h>\n";
  out << "#include <iostream>\n";
  out << "#include <vector>\n";
  out << "#include <sci_defs/config_defs.h>\n";
#ifdef MxNDEBUG
  out << "#include <sci_defs/mpi_defs.h>\n";
  out << "#include <sci_mpi.h> //Debugging purposes\n";
#endif
  out << "\n";
  out << "\n//cast exceptions\n";
  out << "static void _castException(int _xid, SCIRun::Message** _xMsg);\n" << std::endl;

  out << e.proxy.str();
  out << e.out.str() << std::endl;
}

void SymbolTable::emit(EmitState& e) const
{
  for (std::map<std::string, Symbol*>::const_iterator iter = symbols.begin();
       iter != symbols.end();iter++) {
    iter->second->emit(e);
  }
}

void Symbol::emit(EmitState& e)
{
  switch(type) {
  case PackageType:
    definition->getSymbolTable()->emit(e);
    break;
  case InterfaceType:
  case ClassType:
  case EnumType:
    if (definition->isImport) {
      CI* importCI = dynamic_cast<CI* >(definition);
      if (importCI) importCI->emit_proxyclass(e);
      return;
    }
    definition->emit(e);
    break;
  case MethodType:
    std::cerr << "Symbol::emit called for a method!\n";
    exit(1);
  case EnumeratorType:
  case DistArrayType:
    // Nothing gets emitted
    break;
  }
  emitted_forward=true;
}

void Symbol::emit_forward(EmitState& e)
{
  if (definition->isImport) {
    return;
  }
  if (emitted_forward) {
    return;
  }
  switch(type) {
  case PackageType:
    std::cerr << "Why is emit forward being called for a package?\n";
    exit(1);
  case InterfaceType:
  case ClassType:
    if (definition->emitted_declaration) {
      emitted_forward=true;
      return;
    }
    e.fwd.begin_namespace(symtab);
    e.fwd << leader2 << "class " << name << ";\n";
    break;
  case EnumType:
    if (definition->emitted_declaration) {
      emitted_forward=true;
      return;
    }
    definition->emit(e);
    break;
  case MethodType:
    std::cerr << "Symbol::emit_forward called for a method!\n";
    exit(1);
  case EnumeratorType:
    std::cerr << "Symbol::emit_forward called for an enumerator!\n";
    exit(1);
  case DistArrayType:
    std::cerr << "Symbol::emit_forward called for an distributed array type!\n";
    exit(1);
  }
  emitted_forward = true;
}

void Package::emit(EmitState&)
{
  std::cerr << "Package::emit should not be called...\n";
  exit(1);
}

void SState::begin_namespace(SymbolTable* stab)
{
  if (currentPackage == stab) {
    return;
  }

  // Close off previous namespace...
  close_namespace();

  // Open new namespace...
  recursive_begin_namespace(stab);
  currentPackage=stab;
}

void SState::close_namespace()
{
  if (currentPackage) {
    while (currentPackage->getParent()) {
      for (SymbolTable* p = currentPackage->getParent(); p->getParent() != 0; p = p->getParent()) {
        *this << "  ";
      }
      *this << "} // End namespace " << currentPackage->getName() <<'\n';
      currentPackage=currentPackage->getParent();
    }
    *this << "\n";
  }
  leader = "";
}

void SState::recursive_begin_namespace(SymbolTable* stab)
{
  SymbolTable* parent=stab->getParent();
  if (parent) {
    recursive_begin_namespace(parent);
    *this << leader << "namespace " << stab->getName() << " {\n";
    push_leader();
  }
}

bool CI::iam_class()
{
  bool iam = false;
  if (dynamic_cast<Class*>(this)) {
    iam = true;
  }
  return iam;
}

void CI::emit(EmitState& e)
{
  if (emitted_declaration) {
    return;
  }

  for (std::vector<BaseInterface*>::iterator iter=parent_ifaces.begin();
       iter != parent_ifaces.end(); iter++) {
    (*iter)->emit(e);
  }

  // Emit parent classes...
  if (parentclass) {
    parentclass->emit(e);
  }

  emitted_declaration=true;
  if (!isImport) {
    emit_proxyclass(e);
  }

  if (!do_emit) {
    return;
  }

  emit_header(e);

  e.instanceNum++;

  // Emit handler functions
  emit_handlers(e);

  // Emit handler table
  emit_handler_table(e);

  emit_interface(e);
  emit_typeinfo(e);
  emit_proxy(e);
}

void CI::emit_typeinfo(EmitState& e)
{
  std::string fn=cppfullname(0);

  e.out << "const ::SCIRun::TypeInfo* " << fn << "::_static_getTypeInfo()\n";
  e.out << "{\n";
  e.out << "  static ::SCIRun::TypeInfo* ti=0;\n";
  e.out << "  if(!ti){\n";
  e.out << "    ::SCIRun::TypeInfo_internal* tii=\n";

  e.out << "       new ::SCIRun::TypeInfo_internal(\""
	<< cppfullname(0) << "\", \n";
  e.out << "                         NULL,\n";
  e.out << "                         0,\n";
  e.out << "                         &::" << fn << "_proxy::create_proxy);\n\n";
  SymbolTable* localScope=symbols->getParent();
  if (parentclass) {
    e.out << "    tii->add_parentclass(" << parentclass->cppfullname(localScope) << "::_static_getTypeInfo(), " << parentclass->vtable_base << ");\n";
  }
  for (std::vector<BaseInterface*>::iterator iter=parent_ifaces.begin();
       iter != parent_ifaces.end(); iter++) {
    e.out << "    tii->add_parentiface(" << (*iter)->cppfullname(localScope) << "::_static_getTypeInfo(), " << (*iter)->vtable_base << ");\n";
  }
  e.out << "    ti=new ::SCIRun::TypeInfo(tii);\n";
  e.out << "  }\n";
  e.out << "  return ti;\n";
  e.out << "}\n\n";
  e.out << "const ::SCIRun::TypeInfo* " << fn << "::_virtual_getTypeInfo() const\n";
  e.out << "{\n";
  e.out << "  return _static_getTypeInfo();\n";
  e.out << "}\n\n";
}

void CI::emit_handlers(EmitState& e)
{
  // Emit isa handler...
  e.out << "// methods from " << name << " " << curfile << ":" << lineno << "\n\n";
  e.out << "// isa handler\n";
  isaHandler=++e.handlerNum;
  e.out << "static void _handler" << isaHandler << "(::SCIRun::Message* message)\n";
  e.out << "{\n";
  e.out << "  int classname_size;\n";
  e.out << "  message->unmarshalInt(&classname_size);\n";
  e.out << "  char* classname=new char[classname_size+1];\n";
  e.out << "  message->unmarshalChar(classname, classname_size);\n";
  e.out << "  classname[classname_size]=0;\n";
  e.out << "  int _addRef;\n";
  e.out << "  message->unmarshalInt(&_addRef);\n";
  e.out << "  message->unmarshalReply();\n";
  e.out << "  const ::SCIRun::TypeInfo* ti=" << cppfullname(0) << "::_static_getTypeInfo();\n";
  e.out << "  int result=ti->isa(classname);\n";
  e.out << "  delete[] classname;\n";
  e.out << "  int flag;\n";
  e.out << "  if(result == ::SCIRun::TypeInfo::VTABLE_INVALID) {\n";
  e.out << "    flag=0;\n";
  e.out << "  } else {\n";
  e.out << "    flag=1;\n";
  e.out << "    if(_addRef){\n";
  e.out << "      void* _v=message->getLocalObj();\n";
  e.out << "      ::SCIRun::ServerContext* _sc=static_cast< ::SCIRun::ServerContext*>(_v);\n";
  e.out << "      _sc->d_objptr->addReference();\n";
  e.out << "    }\n";
  e.out << "  }\n";
  e.out << "  message->createMessage();\n";
  e.out << "  message->marshalInt(&flag);\n";
  e.out << "  message->marshalInt(&result);\n";
  e.out << "  message->sendMessage(0);\n";
  e.out << "  message->destroyMessage();\n";
  e.out << "}\n\n";

  // Emit delete reference handler...
  e.out << "// methods from " << name << " " << curfile << ":" << lineno << "\n\n";
  e.out << "// delete reference handler\n";
  deleteReferenceHandler=++e.handlerNum;
  e.out << "static void _handler" << deleteReferenceHandler << "(::SCIRun::Message* message)\n";
  e.out << "{\n";
  e.out << "  void* _v=message->getLocalObj();\n";
  e.out << "  ::SCIRun::ServerContext* _sc=static_cast< ::SCIRun::ServerContext*>(_v);\n";
  e.out << "  _sc->d_objptr->deleteReference();\n";
  e.out << "  message->destroyMessage();\n";
  e.out << "}\n\n";

  // Emit method handlers...
  std::vector<Method*> vtab;
  gatherVtable(vtab, false);
  int handlerOff = 0;
  for (std::vector<Method*>::const_iterator iter=vtab.begin();
       iter != vtab.end();iter++) {
    Method* m=*iter;
    e.handlerNum++;
    m->handlerNum=e.handlerNum;
    m->handlerOff=handlerOff++;
    m->emit_handler(e, this);
  }


  // Emit setCallerDistribution Handler
  if (doRedistribution) {
    callerDistHandler=++e.handlerNum;
    e.out << "// setCallerDistribution handler\n";
    e.out << "static void _handler" << callerDistHandler << "(::SCIRun::Message* message)\n";
    e.out << "{\n";
    e.out << "  void* _v=message->getLocalObj();\n";
    e.out << "  ::SCIRun::ServerContext* _sc=static_cast< ::SCIRun::ServerContext*>(_v);\n";
    e.out << "  //Unmarshal received distribution name\n";
    e.out << "  int distname_s;\n";
    e.out << "  message->unmarshalInt(&distname_s, 1);\n";
    e.out << "  char * name = new char[distname_s+1];\n";
    e.out << "  message->unmarshalChar(name,distname_s);\n";
    e.out << "  name[distname_s] = 0;\n";
    e.out << "  std::string dname(name);\n";
    e.out << "  delete[] name;\n";
    e.out << "  //Unmarshal sessionID\n";
    //    e.out << "  std::string _sessionID(36, ' ');\n";
    //    e.out << "  message->unmarshalChar(const_cast<char*>(_sessionID.c_str()), 36);\n";
    e.out << "  int iid, pid;\n";
    e.out << "  message->unmarshalInt(&iid);\n";
    e.out << "  message->unmarshalInt(&pid);\n";
    e.out << "  SCIRun::ProxyID _sessionID(iid, pid);\n";

    e.out << "  //Unmarshal rank\n";
    e.out << "  int rank;\n";
    e.out << "  message->unmarshalInt(&rank, 1);\n";
    e.out << "  //Unmarshal size\n";
    e.out << "  int size;\n";
    e.out << "  message->unmarshalInt(&size, 1);\n";
    e.out << "  //Get/Initialize the synchronization object for meta data distribution.\n";
    e.out << "  SCIRun::MxNMetaSynch* _synch = _sc->d_sched->getMetaSynch(dname,_sessionID,size);\n";
    e.out << "  //Unmarshal recieved distribution\n";
    e.out << "  int _arg1_dim[2];\n";
    e.out << "  message->unmarshalInt(&_arg1_dim[0], 2);\n";
    e.out << "  ::SSIDL::array2< int> _arg1(_arg1_dim[0], _arg1_dim[1]);\n";
    e.out << "  int _arg1_totalsize=_arg1_dim[0]*_arg1_dim[1];\n";
    e.out << "  ::SSIDL::array2< int>::pointer _arg1_uptr=const_cast< ::SSIDL::array2< int>::pointer>(&_arg1[0][0]);\n";
    e.out << "  message->unmarshalInt(_arg1_uptr, _arg1_totalsize);\n";
    e.out << "  message->unmarshalReply();\n\n";
    e.out << "  //Reply with my own distribution\n";
    e.out << "  ::SCIRun::MxNArrayRep* arrrep = _sc->d_sched->calleeGetCalleeRep(dname);\n";
    e.out << "  ::SSIDL::array2< int> _ret;\n";
    e.out << "  if (arrrep != NULL) {\n";
    e.out << "    _ret = arrrep->getArray();\n";
    e.out << "  }\n";
    e.out << "  message->createMessage();\n";
    e.out << "  int _ret_mdim[2];\n";
    e.out << "  _ret_mdim[0]=_ret.size1();\n";
    e.out << "  _ret_mdim[1]=_ret.size2();\n";
    e.out << "  int _ret_mtotalsize=_ret_mdim[0]*_ret_mdim[1];\n";
    e.out << "  ::SSIDL::array2< int>::pointer _ret_mptr=const_cast< ::SSIDL::array2< int>::pointer>(&_ret[0][0]);\n";
    e.out << "  message->marshalInt(&_ret_mdim[0], 2);\n";
    e.out << "  message->marshalInt(_ret_mptr, _ret_mtotalsize);\n";
    e.out << "  message->sendMessage(0);\n";
    e.out << "  message->destroyMessage();\n";
    e.out << "  //Report the recieved distribution to this object's scheduler\n";
    e.out << "  ::SCIRun::MxNArrayRep* arep = new ::SCIRun::MxNArrayRep(_arg1);\n";
    e.out << "  arep->setRank(rank);\n";
    e.out << "  _synch->enter();\n";
    e.out << "  _sc->d_sched->setCallerRepresentation(dname,_sessionID,arep);\n";
    e.out << "  _synch->leave(rank);\n";
    e.out << "}\n\n";

  }

}

void CI::emit_recursive_vtable_comment(EmitState& e, bool top)
{
  e.out << "  // " << (top?"":"and ") << (iam_class()?"class ":"interface ")
	<< name << "\n";
  if (parentclass) {
    parentclass->emit_recursive_vtable_comment(e, false);
  }

  for (std::vector<BaseInterface*>::const_iterator iter=parent_ifaces.begin();
       iter != parent_ifaces.end(); iter++) {
    (*iter)->emit_recursive_vtable_comment(e, false);
  }
}

bool CI::singly_inherited() const
{
  // A class is singly inherited if it has one parent class,
  // or one parent interface, and it's parent is singly_inherited
  if ((parentclass && parent_ifaces.size() > 0)
      || parent_ifaces.size()>1) {
    return false;
  }

  if (parent_ifaces.size()>0) {
    // First element...
    if (!(*parent_ifaces.begin())->singly_inherited()) {
      return false;
    }
  } else if (parentclass) {
    if (!parentclass->singly_inherited()) {
      return false;
    }
  }
  return true;
}

void CI::emit_handler_table_body(EmitState& e, int& vtable_base, bool top)
{
  bool single = singly_inherited();
  if (single)
    emit_recursive_vtable_comment(e, true);
  else
    e.out << "  // " << (iam_class()?"class ":"interface ") << name << "\n";
  e.out << "  // vtable_base = " << vtable_base << '\n';
  std::vector<Method*> vtab;
  gatherVtable(vtab, false);
  int i = vtable_base;
  for (std::vector<Method*>::const_iterator iter=vtab.begin();
       iter != vtab.end();iter++) {
    if (iter != vtab.begin()) {
      e.out << "\n";
    }
    Method* m = *iter;
    m->emit_comment(e, "  ", false);
    i++;
    e.out << "  epc->registerHandler(" << i <<",(void*)_handler" << m->handlerNum << ");";
  }
  if (doRedistribution) {
    e.out << "\n  //setCallerDistribution handler";
    e.out << "\n  epc->registerHandler(" << (++i)
          << ",(void*)_handler" << callerDistHandler << ");";
    callerDistHandler = i-1;
  }
  i++;
  e.out << "\n    // Red zone\n";
  e.out << "  epc->registerHandler(" << i <<",NULL);";

  if (single) {
    if (top) {
      if (parentclass) {
        parentclass->vtable_base=vtable_base;
      }
      for (std::vector<BaseInterface*>::iterator iter=parent_ifaces.begin();
	   iter != parent_ifaces.end(); iter++) {
        (*iter)->vtable_base=vtable_base;
      }
    }
    if (doRedistribution) {
      vtable_base+=vtab.size()+2;
    }
    else {
      vtable_base+=vtab.size()+1;
    }
    return;
  }

  if (doRedistribution) {
    vtable_base+=vtab.size()+2;
  }
  else {
    vtable_base+=vtab.size()+1;
  }
  // For each parent, emit the handler table..
  if (parentclass) {
    if (top) {
      parentclass->vtable_base=vtable_base;
    }
    parentclass->emit_handler_table_body(e, vtable_base, false);
  }
  for (std::vector<BaseInterface*>::iterator iter=parent_ifaces.begin();
       iter != parent_ifaces.end(); iter++) {
    if (top) {
      (*iter)->vtable_base=vtable_base;
    }
    (*iter)->emit_handler_table_body(e, vtable_base, false);
  }
}

void CI::emit_handler_table(EmitState& e)
{
  e.out << "// handler table for " << (iam_class()?"class ":"interface ") << name << "\n";
  e.out << "//" << curfile << ":" << lineno << "\n\n";
  e.out << "void "<< cppfullname(0)
        << "::registerhandlers(SCIRun::EpChannel* epc)\n";
  e.out << "{\n";

  EmitState* tempe = new EmitState();
  int vtable_base=3;
  emit_handler_table_body(*tempe, vtable_base, true);

  e.out << "  epc->allocateHandlerTable(" << (vtable_base) << ");\n";
  e.out << "  epc->registerHandler(1,(void*)_handler" << isaHandler << ");\n";
  e.out << "  epc->registerHandler(2,(void*)_handler" << deleteReferenceHandler << ");\n";
  e.out << "  epc->registerHandler(3,NULL);\n";
  e.out << tempe->out.str();
  delete tempe;

  e.out << "\n} // vtable_size=" << vtable_base << "\n\n";
}

bool Method::reply_required() const
{
  if (modifier2 == Oneway) {
    return false;
  } else {
    return true;
  }
}

std::string Method::get_classname() const
{
  std::string n;
  if (myclass) {
    n=myclass->cppfullname(0);
  } else if (myinterface) {
    n=myinterface->cppfullname(0);
  } else {
    std::cerr << "ERROR: Method does not have an associated class or interface\n";
    exit(1);
  }
  return n;
}

void Method::emit_comment(EmitState& e, const std::string& leader,
			  bool print_filename) const
{
  if (print_filename) {
    e.out << leader << "// from " << curfile << ":" << lineno << '\n';
  }
  e.out << leader << "// " << fullsignature() << '\n';
}

void Method::emit_prototype(SState& out, Context ctx,
			    SymbolTable* localScope) const
{
  out << leader2 << "// " << fullsignature() << '\n';
  out << leader2 << "virtual ";
  return_type->emit_prototype(out, ReturnType, localScope);
  out << " " << name << "(";
  std::vector<Argument*>& list=args->getList();
  int c = 0;
  for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
    if (c++>0) {
      out << ", ";
    }
    Argument* arg=*iter;
    arg->emit_prototype(out, localScope);
  }
  out << ")";
  if (ctx == PureVirtual && !doing_cia) {
    out << "=0";
  }
  out << ";\n";
}

void Method::emit_prototype_defin(EmitState& e, const std::string& prefix,
				  SymbolTable* localScope) const
{
  e.out << "// " << fullsignature() << '\n';
  return_type->emit_prototype(e.out, ReturnType, 0);
  e.out << " " << prefix << name << "(";
  std::vector<Argument*>& list=args->getList();
  int c = 0;
  for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
    if (c++>0) {
      e.out << ", ";
    }
    Argument* arg=*iter;
    std::ostringstream argname;
    argname << "_arg" << c;
    arg->emit_prototype_defin(e.out, argname.str(), localScope);
  }
  e.out << ")";
}

void Method::emit_handler(EmitState& e, CI* emit_class) const
{
  // Server-side handlers
  emit_comment(e, "", true);
  e.out << "static void _handler" << e.handlerNum << "(::SCIRun::Message* message)\n";
  e.out << "{\n";
  std::string oldleader=e.out.push_leader();
  e.out << leader2 << "void* _v=message->getLocalObj();\n";
  std::string myclass = emit_class->cppfullname(0);
  e.out << leader2 << "::SCIRun::ServerContext* _sc=static_cast< ::SCIRun::ServerContext*>(_v);\n";
  e.out << leader2 << myclass << "* _obj=static_cast< " << myclass << "*>(_sc->d_ptr);\n";
  e.out << "\n";

  if (return_type) {
    if (!return_type->isvoid()) {
      e.out << leader2;
      return_type->emit_rettype(e, "_ret");
      e.out << ";\n";
    }
  }

  if (isCollective) {
    e.out << leader2 << "//Unmarshal distribution flag\n";
    e.out << leader2 << "::SCIRun::callType _flag;\n";
    e.out << leader2 << "message->unmarshalInt((int*)&_flag);\n";
    //#ifdef HAVE_MPI
    e.out << leader2 << "//Unmarshal sessionID and number of calls\n";
    //    e.out << leader2 << "std::string _sessionID(36, ' ');\n";
    //    e.out << leader2 << "message->unmarshalChar(const_cast<char*>(_sessionID.c_str()), 36);\n";
    // unmarshal UUIDs
    e.out << leader2 << "int iid, pid;\n";
    e.out << leader2 << "message->unmarshalInt(&iid);\n";
    e.out << leader2 << "message->unmarshalInt(&pid);\n";
    e.out << leader2 << "SCIRun::ProxyID _sessionID(iid,pid);\n";
    e.out << leader2 << "message->unmarshalInt(&iid);\n";
    e.out << leader2 << "message->unmarshalInt(&pid);\n";
    e.out << leader2 << "SCIRun::ProxyID _currentPrxoyID(iid,pid);\n";
    e.out << leader2 << "SCIRun::PRMI::setProxyID(_currentPrxoyID);\n";

    e.out << leader2 << "int _numCalls;\n";
    e.out << leader2 << "message->unmarshalInt(&_numCalls);\n\n";
    e.out << leader2 << "//Unmarshal callID\n";
    e.out << leader2 << "int _callID;\n";
    e.out << leader2 << "message->unmarshalInt(&_callID);\n";
    //#endif
  }

  /***********************************/
  /*CALLONLY||CALLNORET              */
  /***********************************/
  std::string f_leader;
  std::string a_leader;
  if (isCollective) {
    e.out << leader2 << "if ((_flag == ::SCIRun::CALLONLY)||(_flag == ::SCIRun::CALLNORET)) {  /*CALLONLY || CALLNORET*/ \n";
    e.out << leader2 << "  SCIRun::PRMI::setInvID(SCIRun::PRMI::getProxyID());\n";
    f_leader=e.out.push_leader();
  }

  std::vector<Argument*>& list=args->getList();
  int argNum = 0;
  for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
    argNum++;
    Argument* arg=*iter;
    if (arg->getMode() != Argument::Out) {
      std::ostringstream argname;
      argname << "_arg" << argNum;
      arg->emit_unmarshal(e, argname.str(), "1", 0, ArgIn, false, true);
    }
  }

  // Declare out arguments...
  argNum = 0;
  for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
    argNum++;
    Argument* arg=*iter;
    if (arg->getMode() == Argument::Out) {
      std::ostringstream argname;
      argname << "_arg" << argNum;
      arg->emit_declaration(e, argname.str());
    }
  }

  // Unmarshal the reply
  e.out << leader2 << "message->unmarshalReply();\n";

  std::string x_leader;
  if (throws_clause) {
    e.out << leader2 << "try {\n";
    x_leader=e.out.push_leader();
  }
  e.out << leader2 << "// Call the method\n";
  // Call the method...
  e.out << leader2;
  if (return_type) {
    if (!return_type->isvoid()) {
      e.out << "_ret =";
    }
  }
  e.out << "_obj->" << name << "(";
  argNum = 0;
  for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
    if (argNum > 0) {
      e.out << ", ";
    }
    argNum++;
    e.out << "_arg" << argNum;
  }
  e.out << ");\n";
  if (throws_clause) {
    e.out.pop_leader(x_leader);
    int cnt = -1;
    const std::vector<ScopedName*>& thlist=throws_clause->getList();
    for (std::vector<ScopedName*>::const_iterator iter=thlist.begin();iter != thlist.end();iter++) {
      std::string name = (*iter)->cppfullname();
      Definition* def = (*iter)->getSymbol()->getDefinition();
      CI* xci = dynamic_cast< CI*>(def);
      if (xci) cnt = xci->exceptionID;
      e.out << leader2 << "} catch(" << name << "* _e_ptr) {\n";
      e.out << leader2 << "  std::cerr << \"Throwing " << name << "\\n\";\n";
      e.out << leader2 << "  _marshal_exception< " << name << ">(message,_e_ptr," << cnt << ");\n";
      e.out << leader2 << "  return;\n";
    }
    e.out << leader2 << "}\n";
  }


  if (reply_required()) {
    // Set up startpoints for any objects...
    e.out << leader2 << "// Size the reply...\n";
    e.out << leader2 << "message->createMessage();\n";

    e.out << leader2 << "int _x_flag=0;\n";
    e.out << leader2 << "message->marshalInt(&_x_flag);\n";

    if (return_type) {
      if (!return_type->isvoid()) {
        e.out << leader2 << "// Marshal return value\n";
        if (isCollective) {
          return_type->emit_marshal(e, "_ret", "1", 0 , true, ReturnType, false, doStore);
        } else {
          return_type->emit_marshal(e, "_ret", "1", 0 , true, ReturnType, false, noneStorage);
        }
      }
    }

    if (list.size() != 0) {
      e.out << leader2 << "// Marshal arguments\n";
    }
    argNum = 0;
    for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
      argNum++;
      Argument* arg=*iter;
      if (arg->getMode() != Argument::In) {
        std::ostringstream argname;
        argname << "_arg" << argNum;
        if (isCollective) {
          arg->emit_marshal(e, argname.str(), "1", 0 , true, ArgOut, false, doStore);
        } else {
          arg->emit_marshal(e, argname.str(), "1", 0 , true, ArgOut, false, noneStorage);
        }
      }
    }

    /***********************************/
    /*CALLONLY                         */
    /***********************************/
    if (isCollective) {
      //Do this here because CALLNORET needs data redistribution
      //calls found in emit_marshal (OUT args)
      e.out << leader2 << "if (_flag == ::SCIRun::CALLNORET) {\n";
      if (throws_clause) {
        int reply_handler_id = 0; // Always 0
        //TODO: it seems that the corresponding proxy does not handle the throws_clause, K.Z.
        e.out << leader2 << "  message->sendMessage(" << reply_handler_id << ");\n";
        e.out << leader2 << "  message->destroyMessage();\n";
        e.out << leader2 << "  return;\n";
      } else {
        //Even CALLNORET replies in order to maintain the PRMI's integrity
        int reply_handler_id = 0; // Always 0
        e.out << leader2 << "  message->sendMessage(" << reply_handler_id << ");\n";
        e.out << leader2 << "  message->destroyMessage();\n";
        e.out << leader2 << "  return;\n";
      }
      e.out << leader2 << "}\n";
      e.out << leader2 << "else { /*CALLONLY*/ \n";
      a_leader = e.out.push_leader();
    }

    if (isCollective) {
      e.out << leader2 << "// Send the reply...\n";
      e.out << leader2 << "  SCIRun::ProxyID nextProxyID=SCIRun::PRMI::peekProxyID();\n";
      e.out << leader2 << "// marshal the current proxy id\n";
      e.out << leader2 << "  message->marshalInt(&nextProxyID.iid);\n";
      e.out << leader2 << "  message->marshalInt(&nextProxyID.pid);\n";
    }
    int reply_handler_id = 0; // Always 0
    e.out << leader2 << "message->sendMessage(" << reply_handler_id << ");\n";
  }
  e.out << leader2 << "message->destroyMessage();\n";
  //e.out << leader2 << "return;\n";

  if (isCollective) {
    e.out.pop_leader(a_leader);
    e.out << leader2 << "}\n";
    e.out.pop_leader(f_leader);
    e.out << leader2 << "}\n";
  }

  /***********************************/
  /*NOCALLRET                        */
  /***********************************/
  if (isCollective) {
    e.out << leader2 << "if (_flag == ::SCIRun::NOCALLRET) {  /*NOCALLRET*/ \n";
    f_leader=e.out.push_leader();

    if (reply_required()) {
      e.out << leader2 << "message->unmarshalReply();\n";
      e.out << leader2 << "message->createMessage();\n";

      e.out << leader2 << "int _x_flag=0;\n";
      e.out << leader2 << "message->marshalInt(&_x_flag);\n";

      if (return_type) {
        if (!return_type->isvoid()) {
          e.out << leader2 << "// Marshal return value\n";
          if (isCollective) {
            return_type->emit_marshal(e, "_ret", "1", 0 , true, ReturnType, false, doRetrieve);
          } else {
            return_type->emit_marshal(e, "_ret", "1", 0 , true, ArgOut, false, noneStorage);
          }
        }
      }

      if (list.size() != 0) {
        e.out << leader2 << "// Marshal arguments\n";
      }
      argNum = 0;
      for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
        argNum++;
        Argument* arg=*iter;
        if (arg->getMode() != Argument::In) {
          std::ostringstream argname;
          argname << "_arg" << argNum;
          if (isCollective) {
            arg->emit_marshal(e, argname.str(), "1", 0 , true, ArgOut, false, doRetrieve);
          } else {
            arg->emit_marshal(e, argname.str(), "1", 0 , true, ArgOut, false, noneStorage);
          }
        }
      }


      e.out << leader2 << "// Send the reply...\n";
      e.out << leader2 << "  SCIRun::ProxyID nextProxyID=SCIRun::PRMI::peekProxyID();\n";
      e.out << leader2 << "// marshal the current proxy id\n";
      e.out << leader2 << "  message->marshalInt(&nextProxyID.iid);\n";
      e.out << leader2 << "  message->marshalInt(&nextProxyID.pid);\n";

      int reply_handler_id = 0; // Always 0
      e.out << leader2 << "message->sendMessage(" << reply_handler_id << ");\n";
    }

    e.out << leader2 << "message->destroyMessage();\n";
    e.out.pop_leader(f_leader);
    e.out << leader2 << "}\n";
  } /*endif isCollective*/

  /***********************************/
  /*REDISONLY                        */
  /***********************************/
  if (doRedistribution) {
    //undefs for the arg_ptrs we defined so that no copies are made
    argNum = 0;
    for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
      argNum++;
      e.out << leader2 << "#undef _arg" << argNum << "\n";
    }

    e.out << leader2 << "if (_flag == ::SCIRun::REDIS) {  /*REDISONLY*/ \n";

    f_leader=e.out.push_leader();
    e.out << leader2 << "//Unmarshal received distribution name\n";
    e.out << leader2 << "int distname_s;\n";
    e.out << leader2 << "message->unmarshalInt(&distname_s, 1);\n";
    e.out << leader2 << "char * name = new char[distname_s+1];\n";
    e.out << leader2 << "message->unmarshalChar(name,distname_s);\n";
    e.out << leader2 << "name[distname_s] = 0;\n";
    e.out << leader2 << "std::string dname(name);\n";
    e.out << leader2 << "delete[] name;\n";

    argNum = 0;
    for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
      argNum++;
      Argument* arg=*iter;
      std::ostringstream argname;
      argname << "_arg" << argNum;
      if (arg->getMode() != Argument::Out) {
        //Call with special Redis option turned on
        arg->emit_unmarshal(e, argname.str(), "1", 0, ArgIn, true, true);
      }
      if (arg->getMode() != Argument::In) {
        //Call with special Redis option turned on
        arg->emit_marshal(e, argname.str(), "1", 0, true, ArgOut, true);
      }
    }
    e.out << "  } /*end else*/\n";
    e.out.pop_leader(f_leader);
  }

  //#ifdef HAVE_MPI
  if (isCollective) {
    //e.out << "releaseTicket:\n";
    //e.out << leader2 << "if (_flag != ::SCIRun::REDIS) {\n";
    //e.out << leader2 << "  //Release a ticket from the gatekeeper object\n";
    //e.out << leader2 << "  _sc->gatekeeper->releaseOneTicket(" << e.handlerNum << ");\n";
    //e.out << leader2 << "}\n";
  }
  //#endif

  e.out.pop_leader(oldleader);
  e.out << "}\n\n";
}

class output_sub {
  SState& out;
  std::string classname;
public:
  output_sub(SState& out, const std::string& classname)
    : out(out), classname(classname) {}
  void operator()(char x) {
    if (x == '@') {
      out << classname;
    } else if (x == '\n') {
      out << '\n' << out.leader;
    } else {
      out << x;
    }
  }
};

void CI::emit_proxyclass(EmitState& e)
{
  e.proxy.begin_namespace(symbols->getParent());

  // Proxy
  std::string pname=name+"_proxy";

  e.proxy << leader2 << "class " << pname << " : public ::SCIRun::ProxyBase, public " << name << " {\n";
  e.proxy << leader2 << "public:\n";
  e.proxy << leader2 << "  " << pname << "(const ::SCIRun::ReferenceMgr&);\n";
  e.proxy << leader2 << "  " << pname << "(const ::SCIRun::Reference&);\n";
  std::string oldleader=e.proxy.push_leader();
  std::vector<Method*> vtab;
  gatherVtable(vtab, false);

  for (std::vector<Method*>::const_iterator iter=vtab.begin();
       iter != vtab.end();iter++) {
    e.proxy << '\n';
    Method* m = *iter;
    m->emit_prototype(e.proxy, Method::Normal, symbols->getParent());
  }

  e.proxy << "\n";
  e.proxy << leader2 << "virtual void createSubset(int localsize, int remotesize);\n";
  e.proxy << "\n";
  e.proxy << leader2 << "virtual void setRankAndSize(int rank, int size);\n";
  e.proxy << "\n";
  e.proxy << leader2 << "virtual void resetRankAndSize();\n";
  e.proxy << "\n";
  e.proxy << leader2 << "virtual void getException();\n";

  if (doRedistribution) {
    e.proxy << '\n';
    e.proxy << leader2 << "virtual void setCallerDistribution(std::string distname,\n";
    e.proxy << leader2 << "\t\t\t\tSCIRun::MxNArrayRep* arrrep);\n";
  }
  e.proxy.pop_leader(oldleader);
  e.proxy << leader2 << "protected:\n";
  e.proxy << leader2 << "  virtual ~" << pname << "();\n";
  e.proxy << leader2 << "private:\n";
  e.proxy << leader2 << "  virtual void _getReferenceCopy(::SCIRun::ReferenceMgr**) const;\n";
  e.proxy << leader2 << "  friend const ::SCIRun::TypeInfo* " << name << "::_static_getTypeInfo();\n";
  e.proxy << leader2 << "  static ::SCIRun::Object* create_proxy(const ::SCIRun::ReferenceMgr&);\n";
  e.proxy << leader2 << "  " << pname << "(const " << pname << "&);\n";
  e.proxy << leader2 << "  " << pname << "& operator=(const " << pname << "&);\n";
  e.proxy << leader2 << "};\n\n";
  e.proxy.close_namespace();
}

void CI::emit_header(EmitState& e)
{
  e.decl.begin_namespace(symbols->getParent());

  std::vector<Method*>& mymethods=myMethods();

  // interface
  e.decl << leader2 << "class " << name << " : ";

  // Parents
  bool haveone = false;
  if (parentclass) {
    e.decl << "public " << parentclass->cppfullname(e.decl.currentPackage);
    haveone = true;
  }

  for (std::vector<BaseInterface*>::iterator iter=parent_ifaces.begin();
       iter != parent_ifaces.end(); iter++) {
    if (!haveone) {
      haveone = true;
    } else {
      e.decl << ", ";
    }
    e.decl << "virtual public " << (*iter)->cppfullname(e.decl.currentPackage);
  }
  if (!haveone) {
    e.decl << "virtual public ::SCIRun::Object";
  }
  e.decl << " {\n";
  e.decl << leader2 << "public:\n";

  // The smart pointer class
  e.decl << leader2 << "  typedef CCALib::SmartPointer<" << name << "> pointer;\n";

  // The interace class body
  e.decl << leader2 << "  virtual ~" << name << "();\n";
  std::string oldleader = e.decl.push_leader();

  for (std::vector<Method*>::const_iterator iter = mymethods.begin();
       iter != mymethods.end();iter++) {
    e.decl << '\n';
    Method* m = *iter;
    m->emit_prototype(e.decl, Method::PureVirtual, symbols->getParent());
  }

  e.decl << "\n";
  e.decl << leader2 << "virtual void createSubset(int localsize, int remotesize);\n";
  e.decl << "\n";
  e.decl << leader2 << "virtual void setRankAndSize(int rank, int size);\n";
  e.decl << "\n";
  e.decl << leader2 << "virtual void resetRankAndSize();\n";
  e.decl << "\n";
  e.decl << leader2 << "virtual void getException();\n";

  if (doRedistribution) {
    e.decl << '\n';
    e.decl << leader2 << "virtual void setCallerDistribution(std::string distname,\n";
    e.decl << leader2 << "\t\t\t\tSCIRun::MxNArrayRep* arrrep);\n";
  }

  e.decl.pop_leader(oldleader);
  // The type signature method...
  e.decl << leader2 << "  virtual const ::SCIRun::TypeInfo* _virtual_getTypeInfo() const;\n";
  e.decl << leader2 << "  static const ::SCIRun::TypeInfo* _static_getTypeInfo();\n";
  if (!iam_class()) {
    //Interface constructor is protected
    e.decl << leader2 << "protected:\n";
  }
  e.decl << leader2 << "  " << name << "(bool initServer = true);\n";
  e.decl << leader2 << "private:\n";
  e.decl << leader2 << "  void registerhandlers(SCIRun::EpChannel* epc);\n";
  e.decl << leader2 << "  " << name << "(const " << name << "&);\n";
  e.decl << leader2 << "  " << name << "& operator=(const " << name << "&);\n";
  e.decl << leader2 << "};\n\n";

#ifdef OLDSTUFF
  for_each(handle_class.begin(), handle_class.end(), output_sub(e.decl, name));
  e.decl << "  // Conversion operations\n";
  // Emit an operator() for each parent class and interface...
  std::vector<CI*> parents;
  gatherParents(parents);
  for (std::vector<CI*>::iterator iter=parents.begin();
       iter != parents.end(); iter++) {
    if (*iter != this) {
      e.decl << leader2 << "  inline operator " << (*iter)->cppfullname(e.decl.currentPackage) << "() const\n";
      e.decl << leader2 << "  {\n";
      e.decl << leader2 << "    return ptr;\n";
      e.decl << leader2 << "  }\n";
      e.decl << "\n";
    }
  }
  e.decl << leader2 << "};\n\n";
#endif
}

void CI::emit_interface(EmitState& e)
{
  std::string fn = cppfullname(0);
  std::string cn = cppclassname();

  //Emit dummy setCallerDistribution() method
  if (doRedistribution) {
    e.out << "void " << fn << "::setCallerDistribution(std::string distname,\n"
          << "\t\t\t\tSCIRun::MxNArrayRep* arrrep)\n{  }\n\n";
  }

  e.out << "// subsetting method\n";
  e.out << "void " << fn << "::createSubset(int localsize, int remotesize)\n";
  e.out << "{\n";
  e.out << "}\n\n";

  e.out << "void " << fn << "::setRankAndSize(int rank, int size)\n";
  e.out << "{\n}\n\n";
  e.out << "void " << fn << "::resetRankAndSize()\n";
  e.out << "{\n}\n\n";

  e.out << "// retreive all exceptions method\n";
  e.out << "void " << fn << "::getException()\n";
  e.out << "{\n";
  e.out << "  std::cout << \"This method should only be called on proxies\\n\";\n";
  e.out << "}\n\n";

  e.out << fn << "::" << cn << "(bool initServer)\n";
  if (parent_ifaces.size() != 0 || parentclass) {
    e.out << " : ";
  }
  SymbolTable* localScope=symbols->getParent();

  if (parent_ifaces.size() > 0) {
    std::vector<BaseInterface*> parents;
    gatherParentInterfaces(parents);
    for (std::vector<BaseInterface*>::iterator iter=parents.begin();
	 iter != parents.end(); iter++) {
      if (*iter != this) {
        if (iter != parents.begin()) {
	  e.out << ",\n   ";
        }
        e.out << (*iter)->cppfullname(localScope) << "(false)";
      }
    }
    if (parentclass) {
      e.out << ",\n   ";
    }
  }

  if (parentclass) {
    e.out << parentclass->cppfullname(localScope) << "(false)";
  }

  e.out << "\n{\n";
  e.out << "  if (initServer) {\n";
  if (doRedistribution) {
    e.out << "    createScheduler();\n";
  }
  e.out << "    SCIRun::EpChannel* epc = ::SCIRun::PIDL::getEpChannel();\n";
  e.out << "    registerhandlers(epc);\n";
  e.out << "    initializeServer(" << cppfullname(0) << "::_static_getTypeInfo(), this, epc);\n";
  e.out << "  }\n";
  e.out << "}\n\n";

  e.out << fn << "::~" << cn << "()\n";
  e.out << "{\n";
  e.out << "}\n\n";
}

void CI::emit_proxy(EmitState& e)
{
  //Emit proxy constructor:
  std::string fn=cppfullname(0)+"_proxy";
  if (fn[0] == ':' && fn[1] == ':') {
    fn=fn.substr(2);
  }
  std::string cn=cppclassname()+"_proxy";
  e.out << fn << "::" << cn << "(const ::SCIRun::ReferenceMgr& _ref) :\n";
  SymbolTable* localScope=symbols->getParent();

  std::vector<BaseInterface*> parents;
  gatherParentInterfaces(parents);

  for (std::vector<BaseInterface*>::iterator iter=parents.begin();
       iter != parents.end(); iter++) {
    e.out  << "   "<< (*iter)->cppfullname(localScope) << "(false),";
  }
  e.out << "::SCIRun::ProxyBase(_ref),";
  e.out << cppfullname(localScope) << "(false)";

  e.out << "\n";
  e.out << "{\n";
  if (doRedistribution) {
    e.out << "  d_sched = new SCIRun::MxNScheduler();\n";
  }
  e.out << "}\n\n";

  //Another constructor that takes just a reference:
  e.out << fn << "::" << cn << "(const ::SCIRun::Reference& _ref) :\n";
  localScope=symbols->getParent();
  gatherParentInterfaces(parents);

  for (std::vector<BaseInterface*>::iterator iter=parents.begin();
       iter != parents.end(); iter++) {
    e.out  << "   "<< (*iter)->cppfullname(localScope) << "(false),";
  }
  e.out << "::SCIRun::ProxyBase(_ref),";
  e.out << cppfullname(localScope) << "(false)";

  e.out << "\n";
  e.out << "{\n";

  if (doRedistribution) {
    e.out << "  d_sched = new SCIRun::MxNScheduler();\n";
  }
  e.out << "}\n\n";

  e.out << fn << "::~" << cn << "()\n";
  e.out << "{\n";

  if (doRedistribution) {
    e.out << "  delete d_sched;\n";
  }
  e.out << "}\n\n";
  e.out << "void " << fn << "::_getReferenceCopy(::SCIRun::ReferenceMgr** refM) const\n";
  e.out << "{\n";
  e.out << "  (*refM) = new ::SCIRun::ReferenceMgr(*(_proxyGetReferenceMgr()));\n";
  e.out << "}\n\n";
  e.out << "::SCIRun::Object* " << fn << "::create_proxy(const ::SCIRun::ReferenceMgr& ref)\n";
  e.out << "{\n";
  e.out << "  return new " << cn << "(ref);\n";
  e.out << "}\n\n";

  std::vector<Method*> vtab;
  gatherVtable(vtab, false);

  for (std::vector<Method*>::const_iterator iter=vtab.begin();
       iter != vtab.end();iter++) {
    e.out << '\n';
    Method* m=*iter;
    m->emit_proxy(e, fn, localScope);
  }

  e.out << "\n// subsetting proxy method\n";
  e.out << "void " << fn << "::createSubset(int localsize, int remotesize)\n";
  e.out << "{\n";
  //#ifdef HAVE_MPI
  e.out << "  _proxycreateSubset(localsize, remotesize);\n";
  //#endif
  e.out << "}\n";

  e.out << "\n// set rank and size methods\n";
  e.out << "void " << fn << "::setRankAndSize(int rank, int size)\n";
  e.out << "{\n";
  //#ifdef HAVE_MPI
  e.out << "  _proxysetRankAndSize(rank, size);\n";
  //#endif
  e.out << "}\n";
  e.out << "void " << fn << "::resetRankAndSize()\n";
  e.out << "{\n";
  //#ifdef HAVE_MPI
  e.out << "  _proxyresetRankAndSize();\n";
  //#endif
  e.out << "}\n";


  e.out << "\n// \"smoke\" out all exceptions method\n";
  e.out << "void " << fn << "::getException()\n";
  e.out << "{\n";
  //#ifdef HAVE_MPI
  e.out << "  ::SCIRun::Message* _xMsg;\n";
  e.out << "  int _xid = xr->checkException(&_xMsg);\n";
  e.out << "  if (_xid != 0) {\n";
  e.out << "    _castException(_xid,&_xMsg);\n";
  e.out << "  } else {\n";
  e.out << "    _xid = _proxygetException();\n";
  e.out << "    if (_xid != 0) {\n";
  e.out << "      //There is some exception that I have not yet received\n";
  e.out << "      int _xlineID;\n";
  e.out << "      _xid = xr->readException(&_xMsg,&_xlineID);\n";
  e.out << "      if (_xid) {\n";
  e.out << "        xr->resetlineID();\n";
  e.out << "        _castException(_xid,&_xMsg);\n";
  e.out << "      }\n";
  e.out << "    }\n";
  e.out << "  }\n";
  //#endif
  e.out << "}\n";

  // Emit setCallerDistribution proxy
  if (doRedistribution) {
    e.out << "\n// redestribution proxy\n";
    e.out << "void " << fn << "::setCallerDistribution(std::string distname,\n"
          << "\t\t\t\tSCIRun::MxNArrayRep* in_arrrep)\n";
    e.out << "{\n";
    e.out << "  //copy the MxNArrayRep\n";
    e.out << "  SCIRun::MxNArrayRep* arrrep = new SCIRun::MxNArrayRep(*in_arrrep);\n";
    e.out << "  //First clear this distribution in case it already exists\n";
    e.out << "  d_sched->clear(distname, getProxyUUID(), ::SCIRun::caller);\n";
    e.out << "  d_sched->clear(distname, getProxyUUID(), ::SCIRun::callee);\n";
    e.out << "  d_sched->setCallerRepresentation(distname,getProxyUUID(),arrrep);\n";
    e.out << "  //Scatter to all callee objects\n";
    e.out << "  ::SSIDL::array2< int> _rep = arrrep->getArray();\n";
    e.out << "  ::SCIRun::refList* _refL;\n";
    e.out << "  ::SCIRun::refList::iterator iter;\n";
    e.out << "  ::SCIRun::ReferenceMgr* _rm = _proxyGetReferenceMgr();\n";
    e.out << "  _refL = _rm->getAllReferences();\n";
    e.out << "  ::SCIRun::Message** msgs = new ::SCIRun::Message*[_refL->size()];\n";
#ifdef MxNDEBUG
    e.out << "  //Turn on debug to a file\n";
    e.out << "  std::ostringstream fname;\n";
    e.out << "  fname << distname << \"_\" << rm.getRank()  << \".caller.out\";\n";
    e.out << "  d_sched->dbg.open(fname.str().c_str(), std::ios_base::app);\n";
#endif
    e.out << "  \n";
    e.out << "  iter = _refL->begin();\n";
    e.out << "  for (unsigned int i=0; i < _refL->size(); i++, iter++) {\n";
    e.out << "    msgs[i] = (*iter)->chan->getMessage();\n";
    e.out << "    ::SCIRun::Message* message = msgs[i];\n";
    e.out << "    message->createMessage();\n";
    e.out << "    //Marshal distribution name\n";
    e.out << "    int distname_s = distname.size();\n";
    e.out << "    message->marshalInt(&distname_s, 1);\n";
    e.out << "    message->marshalChar((char*)distname.c_str(),distname_s);\n";

    e.out << "    //Marshal uuid\n";
    //    e.out << "    std::string _sessionID = getProxyUUID();\n";
    e.out << "    SCIRun::ProxyID _sessionID = getProxyUUID();\n";
    //    e.out << "    message->marshalChar(const_cast<char*>(_sessionID.c_str()), 36);\n";
    e.out << "    message->marshalInt(&_sessionID.iid);\n";
    e.out << "    message->marshalInt(&_sessionID.pid);\n";
    e.out << "    //Marshal rank\n";
    e.out << "    int rank = rm.getRank();\n";
    e.out << "    message->marshalInt(&rank, 1);\n";
    e.out << "    //Marshal size\n";
    e.out << "    int size = rm.getSize();\n";
    e.out << "    message->marshalInt(&size, 1);\n";
    e.out << "    //Marshal distribution representation array\n";
    e.out << "    int _rep_mdim[2];\n";
    e.out << "    _rep_mdim[0]=_rep.size1();\n";
    e.out << "    _rep_mdim[1]=_rep.size2();\n";
    e.out << "    int _rep_mtotalsize=_rep_mdim[0]*_rep_mdim[1];\n";
    e.out << "    ::SSIDL::array2< int>::pointer _rep_mptr=const_cast< ::SSIDL::array2< int>::pointer>(&_rep[0][0]);\n";
    e.out << "    message->marshalInt(&_rep_mdim[0], 2);\n";
    e.out << "    message->marshalInt(_rep_mptr, _rep_mtotalsize);\n";
    e.out << "    int _handler=" << callerDistHandler << ";\n";
    e.out << "    message->sendMessage(_handler);\n";
    e.out << "  }\n";
    e.out << "  //Gather from all callee objects\n";
    e.out << "  iter = _refL->begin();\n";
    e.out << "  for (unsigned int i=0; i < _refL->size(); i++, iter++) {\n";
    e.out << "    ::SCIRun::Message* message = msgs[i];\n";
    e.out << "    message->waitReply();\n";
    e.out << "    //Unmarshal distribution representation array\n";
    e.out << "    int _ret_dim[2];\n";
    e.out << "    message->unmarshalInt(&_ret_dim[0], 2);\n";
    e.out << "    ::SSIDL::array2< int> _ret(_ret_dim[0], _ret_dim[1]);\n";
    e.out << "    int _ret_totalsize=_ret_dim[0]*_ret_dim[1];\n";
    e.out << "    ::SSIDL::array2< int>::pointer _ret_uptr=const_cast< ::SSIDL::array2< int>::pointer>(&_ret[0][0]);\n";
    e.out << "    message->unmarshalInt(_ret_uptr, _ret_totalsize);\n";
    e.out << "    message->destroyMessage();\n";
    e.out << "    ::SCIRun::MxNArrayRep* arep = new ::SCIRun::MxNArrayRep(_ret,(*iter));\n";
    e.out << "    //Assuming that the servers are ordered by their rank, future work will follow this assumption.\n";
    e.out << "    arep->setRank(i);\n";
    e.out << "    d_sched->setCalleeRepresentation(distname,arep);\n";
    e.out << "  }\n";
    e.out << "  d_sched->print();\n";
#ifdef MxNDEBUG
    e.out << "  d_sched->dbg.close();\n";
#endif
    e.out << "}\n\n";
  }
}

void CI::emit_exceptionCast(EmitState &e, Symbol *sym)
{
  std::string xceptName = sym->cppfullname();

  e.xcept << leader2 << "  case " << exceptionID << ": {\n";
  e.xcept << leader2 << "    " << xceptName << "* _e_ptr = _unmarshal_exception<" << xceptName << ", " << xceptName << "_proxy>(*_xMsg);\n";
  e.xcept << leader2 << "    throw _e_ptr;\n";
  e.xcept << leader2 << "    break;\n";
  e.xcept << leader2 << "  }\n";
  castException_emitted = true;
}

void Method::emit_proxy(EmitState& e, const std::string& fn,
			SymbolTable* localScope) const
{
  std::vector<Argument*>& list=args->getList();
  emit_prototype_defin(e, fn+"::", localScope);
  e.out << "\n{\n";
  std::string oldleader = e.out.push_leader();
  e.out << leader2 << "::SCIRun::ReferenceMgr* _rm = _proxyGetReferenceMgr();\n";
  if (isCollective) {
    e.out << leader2 << "std::vector< ::SCIRun::Reference*> _ref;\n";
    e.out << leader2 << "::SCIRun::callType _flag;\n";
    e.out << leader2 << "::SCIRun::Message* save_callonly_msg = NULL;\n";
    e.out << leader2 << "std::vector < ::SCIRun::Message*> save_callnoret_msg;\n";
    e.out << leader2 << "//Imprecise exception check if someone caught an exception\n";
    e.out << leader2 << "::SCIRun::Message* _xMsg;\n";
    //#ifdef HAVE_MPI
    e.out << leader2 << "int _xid = xr->checkException(&_xMsg);\n";
    e.out << leader2 << "if (_xid != 0) {\n";
    e.out << leader2 << "  _castException(_xid,&_xMsg);\n";
    e.out << leader2 << "}\n";
    //#endif

    if (throws_clause) {
      //Write things for the castException method
      const std::vector<ScopedName*>& thlist = throws_clause->getList();
      for (std::vector<ScopedName*>::const_iterator iter = thlist.begin();
	   iter != thlist.end(); iter++) {
        //STEST
        Definition* def = (*iter)->getSymbol()->getDefinition();
        CI* xci = dynamic_cast< CI*>(def);
        if (xci && ! xci->castException_isEmitted()) {
	  xci->emit_exceptionCast(e, (*iter)->getSymbol());
        }
      }
      //EOF write things for the castException method
    }

    e.out << leader2 << "int remoteSize = _rm->getRemoteSize();\n";
    //#ifdef HAVE_MPI
    //    e.out << leader2 << "std::string _sessionID = getProxyUUID();\n";
    e.out << leader2 << "SCIRun::ProxyID _sessionID = getProxyUUID();\n";
    //    e.out << leader2 << "//std::cout << \" sending _sessionID = '\" << _sessionID << \"'\\n\";\n";
    e.out << leader2 << "//std::cout << \" sending _sessionID = '\" << _sessionID.iid <<'|'<<_sessionID.pid << \"'\\n\";\n";
    e.out << leader2 << "int _numCalls = (_rm->getSize() / remoteSize);\n";
    e.out << leader2 << "((_rm->getSize() % remoteSize) > _rm->getRank()) ?_numCalls++ :0;\n\n";
    //#endif
  }

  if (reply_required()) {
    if (return_type) {
      if (!return_type->isvoid()) {
        return_type->emit_declaration(e,"_ret");
      }
    }
  }

  //emit special redistribution code here for IN args:
  if (doRedistribution) {
    int argNum = 0;
    for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
      argNum++;
      Argument* arg=*iter;
      if (arg->getMode() != Argument::Out) {
        std::ostringstream argname;
        argname << "_arg" << argNum;
        //emit special redistribution code here:
        arg->emit_marshal(e, argname.str(), "1", handlerOff , true, ArgIn, true);
      }
    }
  }
  //ZZZZZZZZZ
  //eof special redistribution code for IN args


  e.out << "\n";


  /***********************************/
  /*NOCALLRET                        */
  /***********************************/
  if (isCollective) {
    e.out << leader2 << "if (_rm->getRank() >= remoteSize) {\n";
    e.out << leader2 << "  /*NOCALLRET*/\n";
    std::string nocallret_ldr=e.out.push_leader();

    e.out << leader2 << "_ref = _rm->getCollectiveReference(SCIRun::NOCALLRET);\n";
    e.out << leader2 << "if (_ref.size() <= 0) goto exitmethod;\n";
    e.out << leader2 << "::SCIRun::Message* message = (_ref[0])->chan->getMessage();\n";
    e.out << leader2 << "message->createMessage();\n";
    e.out << leader2 << "//Marshal flag which informs handler that\n";
    e.out << leader2 << "// this message is NOCALLRET:\n";
    e.out << leader2 << "_flag = ::SCIRun::NOCALLRET;\n";
    e.out << leader2 << "message->marshalInt((int*)&_flag);\n";
    //#ifdef HAVE_MPI
    e.out << leader2 << "//Marshal the sessionID and number of actual calls from this proxy\n";
    //    e.out << leader2 << "message->marshalChar(const_cast<char*>(_sessionID.c_str()), 36);\n";
    e.out << leader2 << "message->marshalInt(&_sessionID.iid);\n";
    e.out << leader2 << "message->marshalInt(&_sessionID.pid);\n";
    e.out << leader2 << "SCIRun::ProxyID _nextID=SCIRun::PRMI::peekProxyID();\n";
    e.out << leader2 << "message->marshalInt(&_nextID.iid);\n";
    e.out << leader2 << "message->marshalInt(&_nextID.pid);\n";
    e.out << leader2 << "message->marshalInt(&_numCalls);\n";
    //#endif
    e.out << leader2 << "//Marshal callID\n";
    e.out << leader2 << "int _callID = xr->getlineID();\n";
    e.out << leader2 << "message->marshalInt(&_callID);\n";
    e.out << leader2 << "// Send the message\n";
    e.out << leader2 << "int _handler=(_ref[0])->getVtableBase()+" << handlerOff << ";\n";
    e.out << leader2 << "message->sendMessage(_handler);\n";

    if (reply_required()) {
      e.out << leader2 << "message->waitReply();\n";
      //... emit unmarshal...;
      e.out << leader2 << "int _x_flag;\n";
      e.out << leader2 << "message->unmarshalInt(&_x_flag);\n";
      e.out << leader2 << "//Exceptions are ineffective for NOCALLRET\n";

      if (return_type) {
        if (!return_type->isvoid()) {
          e.out << leader2 << "// Unmarshal the return value\n";
          return_type->emit_unmarshal(e, "_ret", "1", handlerOff, ReturnType, false, false);
        }
      }

      if (list.size() != 0) {
        e.out << leader2 << "// Unmarshal the return arguments\n";
      }
      int argNum = 0;
      for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
        argNum++;
        Argument* arg=*iter;
        if (arg->getMode() != Argument::In) {
          std::ostringstream argname;
          argname << "_arg" << argNum;
          arg->emit_unmarshal(e, argname.str(), "1", handlerOff, ArgOut, false, false);
        }
      }
      //unmarshal returned UUID
      e.out << leader2 << "//unmarshal the next proxy ID and reset it for the current thread\n";
      e.out << leader2 << "SCIRun::ProxyID nextProxyID;\n";
      e.out << leader2 << "message->unmarshalInt(&nextProxyID.iid);\n";
      e.out << leader2 << "message->unmarshalInt(&nextProxyID.pid);\n";
      e.out << leader2 << "SCIRun::PRMI::setProxyID(nextProxyID);\n\n";

      e.out << leader2 << "message->destroyMessage();\n";
    }

    e.out.pop_leader(nocallret_ldr);
    e.out << leader2 << "}\n";
  } /*endif isCollective*/


  /***********************************/
  /*CALLNORET                        */
  /***********************************/
  std::string call_ldr;
  if (isCollective) {
    e.out << leader2 << "else {\n";

    call_ldr=e.out.push_leader();

    e.out << leader2 << "_ref = _rm->getCollectiveReference(SCIRun::CALLNORET);\n";
    e.out << leader2 << "std::vector< ::SCIRun::Reference*>::iterator iter = _ref.begin();\n";
    e.out << leader2 << "for (unsigned int i=0; i < _ref.size(); i++, iter++) {\n";
    e.out << leader2 << "  /*CALLNORET*/\n";
    std::string loop_leader1 = e.out.push_leader();
    e.out << leader2 << "::SCIRun::Message* message = (*iter)->chan->getMessage();\n";
    e.out << leader2 << "message->createMessage();\n";
    e.out << leader2 << "//Marshal flag which informs handler that\n";
    e.out << leader2 << "// this message is CALLNORET\n";
    e.out << leader2 << "_flag = ::SCIRun::CALLNORET;\n";
    e.out << leader2 << "message->marshalInt((int*)&_flag);\n";
    //#ifdef HAVE_MPI
    e.out << leader2 << "//Marshal the sessionID and number of actual calls from this proxy\n";
    //    e.out << leader2 << "message->marshalChar(const_cast<char*>(_sessionID.c_str()), 36);\n";
    e.out << leader2 << "message->marshalInt(&_sessionID.iid);\n";
    e.out << leader2 << "message->marshalInt(&_sessionID.pid);\n";
    e.out << leader2 << "SCIRun::ProxyID _nextID=SCIRun::PRMI::peekProxyID();\n";
    e.out << leader2 << "message->marshalInt(&_nextID.iid);\n";
    e.out << leader2 << "message->marshalInt(&_nextID.pid);\n";
    e.out << leader2 << "message->marshalInt(&_numCalls);\n";
    //#endif
    e.out << leader2 << "//Marshal callID\n";
    e.out << leader2 << "int _callID = xr->getlineID();\n";
    e.out << leader2 << "message->marshalInt(&_callID);\n";

    if (list.size() != 0)
      e.out << leader2 << "// Marshal the arguments\n";
    int argNum = 0;
    for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
      argNum++;
      Argument* arg=*iter;
      if (arg->getMode() != Argument::Out) {
        std::ostringstream argname;
        argname << "_arg" << argNum;
        arg->emit_marshal(e, argname.str(), "1", handlerOff , true, ArgIn, false);
      }
    }

    e.out << leader2 << "// Send the message\n";
    e.out << leader2 << "int _handler=(*iter)->getVtableBase()+" << handlerOff << ";\n";
    e.out << leader2 << "message->sendMessage(_handler);\n";
    if (throws_clause) {
      e.out << leader2 << "save_callnoret_msg.push_back(message);\n";
      e.out << leader2 << "//Later check if an exception was thrown\n";
    }
    else {
      e.out << leader2 << "save_callnoret_msg.push_back(message);\n";
      //e.out << leader2 << "message->destroyMessage();\n";
    }

    e.out.pop_leader(loop_leader1);
    e.out << leader2 << "}\n\n";

  }


  /***********************************/
  /*CALLONLY                         */
  /***********************************/
  if (isCollective) {
    e.out << leader2 << "/*CALLONLY*/\n";
    e.out << leader2 << "_ref = _rm->getCollectiveReference(SCIRun::CALLONLY);\n";
    e.out << leader2 << "if (_ref.size() <= 0) goto exitmethod;\n";
    e.out << leader2 << "::SCIRun::Message* message = (_ref[0])->chan->getMessage();\n";
  } else {
    e.out << leader2 << "::SCIRun::Reference* _i_ref = _rm->getIndependentReference();\n";
    e.out << leader2 << "::SCIRun::Message* message = _i_ref->chan->getMessage();\n";
  }

  e.out << leader2 << "message->createMessage();\n";

  int argNum = 0;
  if (isCollective) {
    e.out << leader2 << "//Marshal flag which informs handler that\n";
    e.out << leader2 << "// this message is CALLONLY:\n";
    e.out << leader2 << "_flag = ::SCIRun::CALLONLY;\n";
    e.out << leader2 << "message->marshalInt((int*)&_flag);\n";
    //#ifdef HAVE_MPI
    e.out << leader2 << "//Marshal the sessionID and number of actual calls from this proxy\n";
    //    e.out << leader2 << "message->marshalChar(const_cast<char*>(_sessionID.c_str()), 36);\n";
    e.out << leader2 << "message->marshalInt(&_sessionID.iid);\n";
    e.out << leader2 << "message->marshalInt(&_sessionID.pid);\n";
    e.out << leader2 << "SCIRun::ProxyID _nextID=SCIRun::PRMI::peekProxyID();\n";
    e.out << leader2 << "message->marshalInt(&_nextID.iid);\n";
    e.out << leader2 << "message->marshalInt(&_nextID.pid);\n";
    e.out << leader2 << "message->marshalInt(&_numCalls);\n";
    //#endif
    e.out << leader2 << "//Marshal callID\n";
    e.out << leader2 << "int _callID = xr->getlineID();\n";
    e.out << leader2 << "message->marshalInt(&_callID);\n";
  }

  if (list.size() != 0) {
    e.out << leader2 << "// Marshal the arguments\n";
  }
  if (!isCollective) {
    //Special subsetting for independent calls
    e.out << leader2 << "_rm->createSubset(1,0);\n";
  }
  argNum = 0;
  for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
    argNum++;
    Argument* arg=*iter;
    if (arg->getMode() != Argument::Out) {
      std::ostringstream argname;
      argname << "_arg" << argNum;
      arg->emit_marshal(e, argname.str(), "1", handlerOff , true, ArgIn, false);
    }
  }
  if (!isCollective) {
    //RESETTING Special subsetting for independent calls
    e.out << leader2 << "_rm->createSubset(0,0);\n";
  }

  e.out << leader2 << "// Send the message\n";
  if (isCollective) {
    e.out << leader2 << "int _handler=(_ref[0])->getVtableBase()+" << handlerOff << ";\n";
  }
  else {
    e.out << leader2 << "int _handler=_i_ref->getVtableBase()+" << handlerOff << ";\n";
  }
  e.out << leader2 << "message->sendMessage(_handler);\n";
  if (isCollective) {
    e.out << leader2 << "save_callonly_msg = message;\n";
    e.out << leader2 << "// CALLONLY reply to be continued...\n";
    e.out.pop_leader(call_ldr);
    e.out << leader2 << "}\n";
  }
  //***CALLONLY to be continued...*****

  //*****************************************
  // OUT args Redistribution code emitted here:
  if (doRedistribution) {
    int argNum = 0;
    for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
      argNum++;
      Argument* arg=*iter;
      if (arg->getMode() != Argument::In) {
        std::ostringstream argname;
        argname << "_arg" << argNum;
        //emit special redistribution code here:
        arg->emit_unmarshal(e, argname.str(), "1", handlerOff, ArgOut, true, false);
      }
    }
  }

  /********CALLONLY continuation******/
  if (reply_required()) {
    if (isCollective) {
      e.out << leader2 << "if (save_callonly_msg) {\n";
      call_ldr=e.out.push_leader();
      e.out << leader2 << "SCIRun::Message* message = save_callonly_msg;\n";
    }
    e.out << leader2 << "message->waitReply();\n";
    e.out << leader2 << "int _x_flag;\n";
    e.out << leader2 << "message->unmarshalInt(&_x_flag);\n";
    e.out << leader2 << "if (_x_flag != 0) {\n";
    if (throws_clause) {
      if (isCollective) {
        e.out << leader2 << "  xr->relayException(&_x_flag, &message);\n";
      }
      e.out << leader2 << "  _castException(_x_flag, &message);\n";
    }
    else {
      e.out << leader2 << "  throw ::SCIRun::InternalError(\"Unexpected user exception\\n\", __FILE__, __LINE__);\n";
    }
    e.out << leader2 << "}\n";



    if (return_type) {
      if (!return_type->isvoid()) {
        e.out << leader2 << "// Unmarshal the return value\n";
        return_type->emit_unmarshal(e, "_ret", "1", handlerOff, ReturnType, false, false);
      }
    }
    if (list.size() != 0) {
      e.out << leader2 << "// Unmarshal the return arguments\n";
    }
    argNum = 0;
    for (std::vector<Argument*>::const_iterator iter=list.begin();iter != list.end();iter++) {
      argNum++;
      Argument* arg=*iter;
      if (arg->getMode() != Argument::In) {
        std::ostringstream argname;
        argname << "_arg" << argNum;
        arg->emit_unmarshal(e, argname.str(), "1", handlerOff, ArgOut, false, false);
      }
    }

    if (isCollective) {
      //unmarshal returned UUID
      e.out << leader2 << "//unmarshal the next proxy ID and reset it for the current thread\n";
      e.out << leader2 << "SCIRun::ProxyID nextProxyID;\n";
      e.out << leader2 << "message->unmarshalInt(&nextProxyID.iid);\n";
      e.out << leader2 << "message->unmarshalInt(&nextProxyID.pid);\n";
      e.out << leader2 << "SCIRun::PRMI::setProxyID(nextProxyID);\n\n";
    }
    e.out << leader2 << "message->destroyMessage();\n";

    if (isCollective && !(throws_clause)) {
      e.out << leader2 << "/*CALLNORET reply*/\n";
      e.out << "for(unsigned int i=0; i<save_callnoret_msg.size(); i++){\n";
      e.out << "  save_callnoret_msg[i]->waitReply();\n";
      e.out << "  save_callnoret_msg[i]->unmarshalInt(&_x_flag);\n";
      e.out << "  save_callnoret_msg[i]->destroyMessage();\n";
      e.out << "  if (_x_flag != 0) {\n";
      e.out << "    throw ::SCIRun::InternalError(\"Unexpected user exception\", __FILE__, __LINE__);\n";
      e.out << "  }\n";
      e.out << "}\n";
    }

    if (isCollective) {
      e.out.pop_leader(call_ldr);
      e.out << leader2 << "}\n";
    }

    //CALLNORET ADDENDUM FOR EXCEPTIONS
    if ((isCollective)&&(throws_clause)) {
      e.out << "\n";
      e.out << leader2 << "//CALLNORET ADDENDUM FOR EXCEPTIONS\n";
      e.out << leader2 << "if (save_callnoret_msg.size() > 0) {\n";
      e.out << leader2 << "  std::vector< ::SCIRun::Message*>::iterator iter = save_callnoret_msg.begin();\n";
      e.out << leader2 << "  for (unsigned int i=0; i < save_callnoret_msg.size(); i++, iter++) {\n";
      e.out << leader2 << "    (*iter)->waitReply();\n";
      e.out << leader2 << "    int _x_flag;\n";
      e.out << leader2 << "    (*iter)->unmarshalInt(&_x_flag);\n";
      e.out << leader2 << "    if (_x_flag != 0) {\n";
      if (isCollective) {
        e.out << leader2 << "      xr->relayException(&_x_flag, &(*iter));\n";
      }
      e.out << leader2 << "      _castException(_x_flag, &(*iter));\n";
      e.out << leader2 << "    }\n";
      e.out << leader2 << "    (*iter)->destroyMessage();\n";
      e.out << leader2 << "  }\n";
      e.out << leader2 << "}\n";
    }

    if (isCollective) {
      e.out << "exitmethod:\n";
    }

    /**********************************************/
    if (reply_required()) {
      if (return_type) {
        if (!return_type->isvoid()) {
          e.out << "  return _ret;\n";
        } else {
          e.out << "  return;\n";
        }
      }
    } else {
      // A reply is not requred but we need to return something
      e.out << "  return -1;\n";
    }
  }

  e.out.pop_leader(oldleader);
  e.out << "}\n";
}

void Argument::emit_unmarshal(EmitState& e, const std::string& arg,
			      const std::string& qty, const int handler,
			      ArgContext ctx, const bool specialRedis,
			      bool declare) const
{
  e.out << leader2 << "// " << arg << ": " << fullsignature() << "\n";
  type->emit_unmarshal(e, arg, qty, handler, ctx, specialRedis, declare);
}

void Argument::emit_marshalsize(EmitState& e, const std::string& arg,
				const std::string& sizevar,
				const std::string& qty) const
{
  type->emit_marshalsize(e, arg, sizevar, qty);
}

void Argument::emit_declaration(EmitState& e, const std::string& arg) const
{
  type->emit_declaration(e, arg);
}

void Argument::emit_marshal(EmitState& e, const std::string& arg,
			    const std::string& qty, const int handler,
			    bool top, ArgContext ctx, bool specialRedis,
			    storageT bufferStore) const
{
  type->emit_marshal(e, arg, qty, handler, top, ctx, specialRedis, bufferStore);
}

void Argument::emit_prototype(SState& out, SymbolTable* localScope) const
{
  ArgContext ctx;
  switch(mode) {
  case In:
    ctx=ArgIn;
    break;
  case Out:
    ctx=ArgOut;
    break;
  default:
    ctx=ArgInOut;
    break;
  }
  type->emit_prototype(out, ctx, localScope);
  if (id != "" && id != "this") {
    out << " " << id;
  }
}

void Argument::emit_prototype_defin(SState& out, const std::string& arg,
				    SymbolTable* localScope) const
{
  ArgContext ctx;
  switch(mode) {
  case In:
    ctx=ArgIn;
    break;
  case Out:
    ctx=ArgOut;
    break;
  default:
    ctx=ArgInOut;
    break;
  }
  type->emit_prototype(out, ctx, localScope);
  out << " " << arg;
}

void ArrayType::emit_unmarshal(EmitState& e, const std::string& arg,
			       const std::string& qty, const int handler,
			       ArgContext ctx, const bool specialRedis,
			       bool declare) const
{
  if (specialRedis) return;

  if (qty != "1") {
    std::cerr << "ArrayType::emit_unmarshall, qty != 1: " << qty << '\n';
    exit(1);
  }
  e.out << leader2 << "int " << arg << "_dim[" << dim << "];\n";
  e.out << leader2 << "message->unmarshalInt(&" << arg << "_dim[0], " << dim << ");\n";
  if (declare) {
    e.out << leader2 << cppfullname(0) << " " << arg << "(";
  } else {
    e.out << leader2 << arg << ".resize(";
  }
  for (int i = 0; i < dim; i++) {
    if (i != 0) {
      e.out << ", ";
    }
    e.out << arg << "_dim[" << i << "]";
  }
  e.out << ");\n";
  if (subtype->array_use_pointer()) {
    std::string pname=arg+"_uptr";
    std::string sizename=arg+"_totalsize";
    e.out << leader2 << "int " << sizename << "=";
    for (int i = 0; i < dim; i++) {
      if (i != 0) {
        e.out << "*";
      }
      e.out << arg << "_dim[" << i << "]";
    }
    e.out << ";\n";
    //This is is wrong if the array dimension >= 2.
    //    e.out << leader2 << cppfullname(0) << "::pointer " << pname << "=const_cast< " << cppfullname(0) << "::pointer>(&" << arg << "[0]);\n";
    e.out << leader2 << cppfullname(0) << "::pointer " << pname << "=const_cast< " << cppfullname(0) << "::pointer>(" << arg << ".buffer());\n";
    subtype->emit_unmarshal(e, pname, sizename, handler, ctx, false, false);
  } else {
    std::string pname=arg+"_iter";
    e.out << leader2 << "for (" << cppfullname(0) << "::iterator " << pname << "=" << arg << ".begin();";
    e.out << pname << " != " << arg << ".end(); " << pname << "++) {\n";
    std::string oldleader=e.out.push_leader();
    e.out << leader2 << cppfullname(0) << "::reference " << arg << "_el = *" << pname << ";\n";
    subtype->emit_unmarshal(e, arg+"_el", "1", handler, ctx, false, false);
    e.out.pop_leader(oldleader);
    e.out << leader2 << "}\n";
  }
}

void ArrayType::emit_marshalsize(EmitState& e, const std::string& arg,
				 const std::string& /*sizevar*/,
				 const std::string& /* qty */) const
{
  if (subtype->uniformsize()) {
    std::string sizename=arg+"_mtotalsize";
    if (dim == 1) {
      e.out << leader2 << "int " << sizename << "=" << arg << ".size();\n";
    } else {
      std::string dimname=arg+"_mdim";
      e.out << leader2 << "int " << dimname << "[" << dim << "];\n";
      for (int i = 0; i < dim; i++) {
        e.out << leader2 << dimname << "[" << i << "]=" << arg << ".size" << i+1 << "();\n";
      }
      e.out << leader2 << "int " << sizename << "=" << dimname << "[0]";
      for (int i = 1; i < dim; i++) {
        e.out << "*" << dimname << "[" << i << "]";
      }
      e.out << ";\n";
    }
  }
}

void ArrayType::emit_declaration(EmitState& e, const std::string& arg) const
{
  e.out << leader2 << cppfullname(0) << " " << arg << ";\n";
}

bool ArrayType::array_use_pointer() const
{
  return false; // Always use iterator for array of array
}

bool ArrayType::uniformsize() const
{
  return false;
}

void ArrayType::emit_marshal(EmitState& e, const std::string& arg,
			     const std::string& /*qty*/, const int handler,
			     bool /*top*/, ArgContext ctx, bool specialRedis,
			     storageT bufferStore) const
{

  if (specialRedis) return;

  if (bufferStore == doRetrieve) {
    if (ctx == ReturnType) {
      e.out << leader2 << arg << " = *((" << cppfullname(0) << "*)(_sc->storage->get(" << e.handlerNum << ",0,_sessionID,_callID"
	    << ")));\n";
    }
    else {
      e.out << leader2 << cppfullname(0) << " " << arg << " = *((" << cppfullname(0) << "*)(_sc->storage->get("
	    << e.handlerNum << "," << arg[arg.size()-1] << ",_sessionID,_callID)));\n";
    }
  }

  this->emit_marshalsize(e, arg, "_rsize", "1");

  std::string pname;
  if (subtype->array_use_pointer()) {
    pname = arg + "_mptr";
    //This is wrong if the array dimension>=2
    //    e.out << leader2 << cppfullname(0) << "::pointer " << pname << "=const_cast< " << cppfullname(0) << "::pointer>(&" << arg << "[0]);\n";
    e.out << leader2 << cppfullname(0) << "::pointer " << pname << "=const_cast< " << cppfullname(0) << "::pointer>(" << arg << ".buffer());\n";
  } else {
    pname = arg + "_iter";
  }
  std::string sizename = arg + "_mtotalsize";
  std::string dimname = arg + "_mdim";

  if (!subtype->uniformsize()) {
    if (dim == 1) {
      e.out << leader2 << "int " << sizename << "=" << arg << ".size();\n";
    } else {
      e.out << leader2 << "int " << dimname << "[" << dim << "];\n";

      for (int i = 0;i<dim;i++) {
        e.out << leader2 << dimname << "[" << i << "]=" << arg << ".size" << i+1 << "();\n";
      }
      e.out << leader2 << "int " << sizename << "=" << dimname << "[0]";

      for (int i = 1;i<dim;i++) {
        e.out << "*" << dimname << "[" << i << "]";
      }
      e.out << ";\n";
    }
  }

  if (dim == 1) {
    e.out << leader2 << "message->marshalInt(&" << sizename << ");\n";
  } else {
    e.out << leader2 << "message->marshalInt(&" << dimname << "[0], " << dim << ");\n";
  }

  if (subtype->array_use_pointer()) {
    subtype->emit_marshal(e, pname, sizename, handler, false, ctx, false, noneStorage);
  } else {
    e.out << leader2 << "for (" << cppfullname(0) << "::const_iterator " << pname << "=" << arg << ".begin();";
    e.out << pname << " != " << arg << ".end(); " << pname << "++) {\n";
    e.out << leader2 << cppfullname(0) << "::const_reference " << arg << "_el = *" << pname << ";\n";
    std::string oldleader=e.out.push_leader();
    subtype->emit_marshal(e, arg+"_el", "1", handler, false, ctx, false, noneStorage);
    e.out.pop_leader(oldleader);
    e.out << leader2 << "}\n";
  }

  if (bufferStore == doStore) {
    if (ctx == ReturnType) {
      e.out << leader2 << "_sc->storage->add(" <<  e.handlerNum
	    <<",0, (void*)(new " << cppfullname(0) << "(" << arg << ")),_sessionID,_callID,_numCalls);\n";
    }
    else {
      e.out << leader2 << "_sc->storage->add(" <<  e.handlerNum  << "," << arg[arg.size()-1]
	    <<", (void*)(new " << cppfullname(0) << "(" << arg << ")),_sessionID,_callID,_numCalls);\n";
    }
  }
}

void ArrayType::emit_rettype(EmitState& e, const std::string& /*arg*/) const
{
  e.out << cppfullname(0) << " _ret";
}

void ArrayType::emit_prototype(SState& out, ArgContext ctx,
			       SymbolTable* /*localScope*/) const
{
  NamedType* ntype = dynamic_cast<NamedType*>(subtype);
  if (ntype) {
    ntype->getScopedName()->getSymbol()->emit_forward(*out.e);
  }
  if (ctx == ArgIn) {
    out << "const ";
  }
  out << cppfullname(0);
  if (ctx == ArgOut || ctx == ArgInOut || ctx == ArgIn) {
    out << "&";
  }
}

void BuiltinType::emit_unmarshal(EmitState& e, const std::string& arg,
				 const std::string& qty, const int /*handler*/,
				 ArgContext /*ctx*/, const bool specialRedis,
				 bool declare) const
{
  if (specialRedis) return;

  if (cname == "void") {
    // What?
    std::cerr << "Trying to unmarshal a void!\n";
    exit(1);
  } else if (cname == "bool") {
    if (qty != "1") {
      std::cerr << "emit_unmarshal called for bool with qty != 1:" << qty << "\n";
      exit(1);
    }
    e.out << leader2 << "char " << arg << "_tmp;\n";
    e.out << leader2 << "message->unmarshalByte(&" << arg << "_tmp);\n";
    if (declare) {
      e.out << leader2 << "bool ";
    }
    e.out << leader2 << arg << "=(bool)" << arg << "_tmp;\n";
  } else if (cname == "string") {
    if (qty != "1") {
      std::cerr << "emit_unmarshal call for std::string with qty != 1:" << qty << "\n";
      exit(1);
    }
    std::string arglen = produceLegalVar(arg) + "_length";
    e.out << leader2 << "int " << arglen << ";\n";
    e.out << leader2 << "message->unmarshalInt(&" << arglen << ");\n";
    if (declare) {
      e.out << leader2 << "std::string " << arg << "(" << arglen << ", ' ');\n";
    } else {
      e.out << leader2 << arg << ".resize(" << arglen << ");\n";
    }
    e.out << leader2 << "message->unmarshalChar(const_cast<char*>(" << arg << ".c_str()), " << arglen << ");\n";
  } else if (cname == BuiltinType::COMPLEX_FLOAT_NAME) {
    if (qty != "1") {
      e.out << leader2 << "float* " << arg << "_in = new float[2*" << qty << "];\n";
      e.out << leader2 << "message->unmarshalFloat(" << arg << "_in, 2*" << qty << ");\n";
      if (declare) {
        std::cerr << "Shouldn't declare arrays!\n";
        exit(1);
      }
      e.out << leader2 << "for (int _i=0;_i<" << qty << ";_i++)\n";
      e.out << leader2 << "  " << arg << "[_i]= std::complex<float>(" << arg << "_in[2*_i], " << arg << "_in[2*_i+1]);\n";
      e.out << leader2 << "delete[] " << arg << "_in;\n";
    } else {
      e.out << leader2 << "float " << arg << "_in[2];\n";
      e.out << leader2 << "message->unmarshalFloat(" << arg << "_in, 2);\n";
      if (declare) {
        e.out << leader2 << "std::complex<float> " << arg << "(" << arg << "_in[0], " << arg << "_in[1]);\n";
      } else {
        e.out << leader2 << arg << "= std::complex<float>(" << arg << "_in[0], " << arg << "_in[1]);\n";
      }
    }
  } else if (cname == BuiltinType::COMPLEX_DOUBLE_NAME) {
    if (qty != "1") {
      e.out << leader2 << "double* " << arg << "_in = new double[2*" << qty << "];\n";
      e.out << leader2 << "message->unmarshalDouble(" << arg << "_in, 2*" << qty << ");\n";
      if (declare) {
	std::cerr << "Shouldn't declare arrays!\n";
	exit(1);
      }
      e.out << leader2 << "for (int _i=0;_i<" << qty << ";_i++)\n";
      e.out << leader2 << "  " << arg << "[_i]= std::complex<double>(" << arg << "_in[2*_i], " << arg << "_in[2*_i+1]);\n";
      e.out << leader2 << "delete[] " << arg << "_in;\n";
    } else {
      e.out << leader2 << "double " << arg << "_in[2];\n";
      e.out << leader2 << "message->unmarshalDouble(" << arg << "_in, 2);\n";
      if (declare) {
        e.out << leader2 << "std::complex<double>" << " " << arg << "(" << arg << "_in[0], " << arg << "_in[1]);\n";
      } else {
        e.out << leader2 << arg << "= std::complex<double>(" << arg << "_in[0], " << arg << "_in[1]);\n";
      }
    }
  } else {
    std::string type_name(nexusname);
    if (declare) {
      e.out << leader2 << cname << " " << arg << ";\n";
    }
    type_name[0] += ('A' - 'a');
    e.out << leader2 << "message->unmarshal" << type_name << "(";
    if (qty == "1") {
      e.out << "&";
    }
    e.out << arg << ", " << qty << ");\n";
  }
}

void BuiltinType::emit_marshalsize(EmitState& /*e*/, const std::string& /*arg*/,
				   const std::string& /*sizevar*/,
				   const std::string& /*qty*/) const
{
}

void BuiltinType::emit_declaration(EmitState& e, const std::string& arg) const
{
  if (cname == "void") {
    // What?
    std::cerr << "Trying to declare a void!\n";
    exit(1);
  } else if (nexusname == "special") { // string, complex<float>, complex<double>
    e.out << leader2 << "std::" << cname << " " << arg << ";\n";
  } else if (cname == "bool") {
    e.out << leader2 << cname << " " << arg << " = false;\n";
  } else {
    e.out << leader2 << cname << " " << arg << ";\n";
  }
}


void BuiltinType::emit_marshal(EmitState& e, const std::string& arg,
                               const std::string& qty, const int /*handler*/, bool/* top*/,
                               ArgContext ctx, bool specialRedis,
                               storageT bufferStore) const
{
  if (specialRedis) return;

  std::string emitName;
  if (nexusname == "special") {
    emitName = "std::" + cname;
  } else {
    emitName = cname;
  }
  if (bufferStore == doRetrieve) {
    if (ctx == ReturnType) {
      e.out << leader2 << arg << " = *((" << emitName << "*)(_sc->storage->get(" << e.handlerNum << ",0,_sessionID,_callID"
	    << ")));\n";
    }
    else {
      e.out << leader2 << emitName << " " << arg << " = *((" << emitName << "*)(_sc->storage->get(" << e.handlerNum
	    << "," << arg[arg.size()-1] << "),_sessionID,_callID));\n";
    }
  }

  if (cname == "void") {
    // What?
    std::cerr << "Trying to unmarshal a void!\n";
    exit(1);
  } else if (cname == "bool") {
    if (qty != "1") {
      std::cerr << "marshal bool called with qty != 1: " << qty << '\n';
      exit(1);
    }
    e.out << leader2 << "message->marshalByte((char*) &" << arg << ");\n";
  } else if (cname == "string") {
    if (qty != "1") {
      std::cerr << "marshal string called with qty != 1: " << qty << '\n';
      exit(1);
    }
    std::string arglen = produceLegalVar(arg) + "_len";
    e.out << leader2 << "int " << arglen << "=" << arg << ".length();\n";
    e.out << leader2 << "message->marshalInt(&" << arglen << ");\n";
    e.out << leader2 << "message->marshalChar(const_cast<char*>(" << arg << ".c_str()), " << arglen << ");\n";
  } else if (cname == BuiltinType::COMPLEX_FLOAT_NAME) {
    if (qty != "1") {
      e.out << leader2 << "float* " << arg << "_out = new float[2*" << qty << "];\n";
      e.out << leader2 << "for (int _i=0;_i<" << qty << ";_i++) {\n";
      e.out << leader2 << "  " << arg << "_out[2*_i]=" << arg << "[_i].real();\n";
      e.out << leader2 << "  " << arg << "_out[2*_i+1]=" << arg << "[_i].imag();\n";
      e.out << leader2 << "}\n";
      e.out << leader2 << "message->marshalFloat(" << arg << "_out, 2*" << qty << ");\n";
      e.out << leader2 << "delete[] " << arg << "_out;\n";
    } else {
      e.out << leader2 << "float " << arg << "_out[2];\n";
      e.out << leader2 << arg << "_out[0]=" << arg << ".real();\n";
      e.out << leader2 << arg << "_out[1]=" << arg << ".imag();\n";
      e.out << leader2 << "message->marshalFloat(" << arg << "_out, 2);\n";
    }
  } else if (cname == BuiltinType::COMPLEX_DOUBLE_NAME) {
    if (qty != "1") {
      e.out << leader2 << "double* " << arg << "_out = new double[2*" << qty << "];\n";
      e.out << leader2 << "for (int _i=0;_i<" << qty << ";_i++) {\n";
      e.out << leader2 << "  " << arg << "_out[2*_i]=" << arg << "[_i].real();\n";
      e.out << leader2 << "  " << arg << "_out[2*_i+1]=" << arg << "[_i].imag();\n";
      e.out << leader2 << "}\n";
      e.out << leader2 << "message->marshalDouble(" << arg << "_out, 2*" << qty << ");\n";
      e.out << leader2 << "delete[] " << arg << "_out;\n";
    } else {
      e.out << leader2 << "double " << arg << "_out[2];\n";
      e.out << leader2 << arg << "_out[0]=" << arg << ".real();\n";
      e.out << leader2 << arg << "_out[1]=" << arg << ".imag();\n";
      e.out << leader2 << "message->marshalDouble(" << arg << "_out, 2);\n";
    }
  } else {
    std::string type_name(nexusname);
    type_name[0] += ('A' - 'a');
    e.out << leader2 << "message->marshal" << type_name << "(";
    if (qty == "1") {
      e.out << "&";
    }
    e.out << arg << ", " << qty << ");\n";
  }

  if (bufferStore == doStore) {
    if (ctx == ReturnType) {
      e.out << leader2 << "_sc->storage->add(" <<  e.handlerNum
	    <<",0, (void*)(new " << emitName << "(" << arg << ")),_sessionID,_callID,_numCalls);\n";
    } else {
      e.out << leader2 << "_sc->storage->add(" <<  e.handlerNum  << "," << arg[arg.size()-1]
	    <<", (void*)(new " << emitName << "(" << arg << ")),_sessionID,_callID,_numCalls);\n";
    }
  }
}

void BuiltinType::emit_rettype(EmitState& e, const std::string& arg) const
{
  if (cname == "void") {
    // Nothing
    return;
  } else if (nexusname == "special") { // string, complex<double>, complex<float>
    e.out << "std::" << cname << " " << arg;
  } else {
    e.out << cname << " " << arg;
  }
}

void BuiltinType::emit_prototype(SState& out, ArgContext ctx, SymbolTable*) const
{
  if (cname == "void") {
    // Nothing
    if (ctx == ReturnType) {
      out << "void";
    } else {
      std::cerr << "Illegal void type in argument list\n";
      exit(1);
    }
  } else if (nexusname == "special") { // string, complex<double>, complex<float>
    switch(ctx) {
    case ReturnType:
    case ArrayTemplate:
      out << "std::" << cname;
      break;
    case ArgIn:
      out << "const std::" << cname << "&";
      break;
    case ArgOut:
    case ArgInOut:
      out << "std::" << cname << "&";
      break;
    }
  } else {
    switch(ctx) {
    case ReturnType:
    case ArgIn:
    case ArrayTemplate:
      out << cname;
      break;
    case ArgOut:
    case ArgInOut:
      out << cname << "&";
      break;
    }
  }
}

bool BuiltinType::array_use_pointer() const
{
  if (cname == "string" || cname == "bool") {
    return false;
  } else {
    return true;
  }
}

bool BuiltinType::uniformsize() const
{
  if (cname == "string") {
    return false;
  } else {
    return true;
  }
}

void NamedType::emit_unmarshal(EmitState& e, const std::string& arg,
			       const std::string& qty, const int handler,
			       ArgContext ctx, const bool specialRedis,
			       bool declare) const
{
  Symbol::Type symtype = name->getSymbol()->getType();
  if (symtype == Symbol::EnumType) {
    if (specialRedis) return;
    if (qty != "1") {
      std::cerr << "NamedType::emit_unmarshal called with qty != 1: " << qty << '\n';
      exit(1);
    }
    e.out << leader2 << "int " << arg << "_unmarshal;\n";
    e.out << leader2 << "message->unmarshalInt(&" << arg << "_unmarshal);\n";
    e.out << leader2;
    if (declare) {
      e.out << name->cppfullname(0) << " ";
    }
    e.out << arg << "=(" << name->cppfullname(0) << ")" << arg << "_unmarshal;\n";
  } else if (symtype == Symbol::ClassType || symtype == Symbol::InterfaceType) {
    if (specialRedis) return;
    if (qty != "1") {
      std::cerr << "NamedType::emit_unmarshal called with qty != 1: " << qty << '\n';
      exit(1);
    }
    e.out << leader2 << "int " << arg << "_vtable_base;\n";
    e.out << leader2 << "message->unmarshalInt(&" << arg << "_vtable_base);\n";
    e.out << leader2 << "int " << arg << "_refno;\n";
    e.out << leader2 << "message->unmarshalInt(&" << arg << "_refno);\n";
    if (declare) {
      e.out << leader2 << name->cppfullname(0) << "::pointer " << arg << ";\n";
    }
    e.out << leader2 << "if (" << arg << "_vtable_base == -1) {\n";
    e.out << leader2 << "  " << arg << "=0;\n";
    e.out << leader2 << "} else {\n";
    e.out << leader2 << "  ::SCIRun::ReferenceMgr _refM;\n";
    e.out << leader2 << "  for (int i=0; i<" << arg << "_refno; i++) {\n";
    e.out << leader2 << "    //This may leak SPs\n";
    e.out << leader2 << "    ::SCIRun::Reference* _ref = new ::SCIRun::Reference();\n";
    e.out << leader2 << "    _ref->d_vtable_base=" << arg << "_vtable_base;\n";
    e.out << leader2 << "    message->unmarshalSpChannel(_ref->chan);\n";
    e.out << leader2 << "    _refM.insertReference(_ref);\n";
    e.out << leader2 << "  }\n";
    e.out << leader2 << "  ::SCIRun::Message* spmsg = _refM.getIndependentReference()->chan->getMessage();\n";
    e.out << leader2 << "  void* _ptr;\n";
    e.out << leader2 << "  if ((_ptr=spmsg->getLocalObj()) != NULL) {\n";
    e.out << leader2 << "    ::SCIRun::ServerContext* _sc=static_cast< ::SCIRun::ServerContext*>(_ptr);\n";
    e.out << leader2 << "    " << arg << "=dynamic_cast<" << name->cppfullname(0) << "*>(_sc->d_objptr);\n";
    e.out << leader2 << "    " << arg << "->_deleteReference();\n";
    e.out << leader2 << "  } else {\n";
    e.out << leader2 << "    " << arg << "=new " << name->cppfullname(0) << "_proxy(_refM);\n";
    e.out << leader2 << "  }\n";
    e.out << leader2 << "}\n";
  } else if (symtype == Symbol::DistArrayType) {
    //****************** DISTRIBUTION ARRAY ********************************
    Definition* defn = name->getSymbol()->getDefinition();
    DistributionArray* distarr = dynamic_cast<DistributionArray*>(defn);
    ArrayType* arr_t = distarr->getArrayType();
    if (specialRedis) {
      if (ctx == ArgIn) {
        // *********** IN arg -- Special Redis ******************************

        //the message to unmarshal contains solely a distribution
        e.out << leader2 << "if (dname == \"" << distarr->getName() << ".in\") {\n";
        e.out << leader2 << "  SCIRun::MxNArrayRep* _this_rep = _sc->d_sched->calleeGetCalleeRep(\"" << distarr->getName() << "\");\n";
        e.out << leader2 << "  SCIRun::MxNArrSynch* _synch = _sc->d_sched->getArrSynch(\"" << distarr->getName() << "\",_sessionID,_callID);\n";
        e.out << leader2 << "  //Unmarshal rank\n";
        e.out << leader2 << "  int rank;\n";
        e.out << leader2 << "  message->unmarshalInt(&rank, 1);\n";
        e.out << leader2 << "  " << arr_t->cppfullname(0) << "* _arr_ptr;\n";
        e.out << leader2 << "  _arr_ptr = static_cast< " << arr_t->cppfullname(0) << "*>(_synch->getArray());\n";
        e.out << leader2 << "  if (_arr_ptr == NULL) {\n";
        e.out << leader2 << "    _arr_ptr = new " << arr_t->cppfullname(0) << "(";
        for (int i=arr_t->dim; i > 0; i--) {
          e.out << "_this_rep->getSize(" << i << ")";
          if (i != 1) e.out << ", ";
        }
        e.out << ");\n";
        e.out << leader2 << "    _synch->setNewArray((void**)(&_arr_ptr));\n";
        e.out << leader2 << "  }\n";
        e.out << leader2 << "  //Unmarshal distribution metadata\n";
        e.out << leader2 << "  int _meta_rep_dim[2];\n";
        e.out << leader2 << "  message->unmarshalInt(&_meta_rep_dim[0], 2);\n";
        e.out << leader2 << "  ::SSIDL::array2< int> _meta_rep(_meta_rep_dim[0], _meta_rep_dim[1]);\n";
        e.out << leader2 << "  int _meta_rep_totalsize=_meta_rep_dim[0]*_meta_rep_dim[1];\n";
        e.out << leader2 << "  ::SSIDL::array2< int>::pointer _meta_rep_uptr=const_cast< ::SSIDL::array2< int>::pointer>(&_meta_rep[0][0]);\n";
        e.out << leader2 << "  message->unmarshalInt(_meta_rep_uptr, _meta_rep_totalsize);\n";
        e.out << leader2 << "  //Unmarshal received distribution\n";

        std::string templeader = e.out.leader;
        for (int i = arr_t->dim - 1; i >= 0; i--) {
	  e.out << leader2 << "  for (int adj" << i << "=_this_rep->getFirst(" << i+1
		<< "), str" << i << "=_this_rep->getStride(" << i+1 << "), "
		<< "i" << i << "=" << "_meta_rep[0][" << i << "]; " << "i" << i << "<="
		<< "_meta_rep[1][" << i << "]; " << "i" << i << "+=" << "(_meta_rep[2]["
		<< i << "] * _this_rep->getLocalStride(" << i+1 << ")) )\n";
	  e.out.push_leader();
        }
        e.out << leader2 << "{\n";
        std::string ttleader = e.out.leader;
        e.out.push_leader();
        std::ostringstream var2unmarshal2;
        var2unmarshal2 << "(*_arr_ptr)";
        for (int i=arr_t->dim-1; i >= 0; i--) {
          var2unmarshal2 << "[(i" << i << "-adj" << i << ")/str" << i << "]";
        }
        arr_t->subtype->emit_unmarshal(e, var2unmarshal2.str(), "1", handler, ctx, false, false);
        e.out.pop_leader(ttleader);
        e.out << leader2 << "}\n";
        e.out.pop_leader(templeader);
        e.out << leader2 << "  //Mark this representation as recieved\n";
        e.out << leader2 << "  _synch->doReceive(rank);\n";
        e.out << leader2 << "} /*endif \"" << distarr->getName() << "\"*/\n";
        // *********** END OF IN arg -- Special Redis ******************************
      } else if (ctx == ArgOut) {
        // *********** OUT arg -- Special Redis ******************************
        e.out << leader2 << "//Collect redistributions of out arguments:\n";
        e.out << leader2 << "if (1) { //Hack to prevent variable shadowing of _rl_out and _this_rep\n";
        e.out << leader2 << "SCIRun::descriptorList* _rl_out = d_sched->getRedistributionReps(\"" << distarr->getName() << "\",getProxyUUID() );\n";
        e.out << leader2 << "SCIRun::MxNArrayRep* _this_rep = d_sched->callerGetCallerRep(\"" << distarr->getName() << "\",getProxyUUID() );\n";
        e.out << leader2 << "//Resize array \n";
        e.out << leader2 << arg << ".resize(";
        for (int i=arr_t->dim; i > 0; i--) {
          e.out << "_this_rep->getSize(" << i << ")";
          if (i != 1) e.out << ", ";
        }
        e.out << ");\n";
        e.out << leader2 << "for (int i = 0; i < (int)_rl_out->size(); i++) {\n";
        e.out << leader2 << "  SCIRun::Message* message = (*_rl_out)[i]->getReference()->chan->getMessage();\n";
        //std::string dimname=arg+"_mdim";
        e.out << leader2 << "  message->createMessage();\n";
        e.out << leader2 << "  //Marshal the redistribution call flag\n";
        e.out << leader2 << "  ::SCIRun::callType _r_flag = ::SCIRun::REDIS;\n";
        e.out << leader2 << "  message->marshalInt((int*)&_r_flag);\n";
        //#ifdef HAVE_MPI
        e.out << leader2 << "  //Marshal the sessionID and number of actual calls from this proxy\n";
        //  e.out << leader2 << "  message->marshalChar(const_cast<char*>(_sessionID.c_str()), 36);\n";
        e.out << leader2 << "message->marshalInt(&_sessionID.iid);\n";
        e.out << leader2 << "message->marshalInt(&_sessionID.pid);\n";
        e.out << leader2 << "SCIRun::ProxyID _nextID=SCIRun::PRMI::peekProxyID();\n";
        e.out << leader2 << "message->marshalInt(&_nextID.iid);\n";
        e.out << leader2 << "message->marshalInt(&_nextID.pid);\n";
        e.out << leader2 << "  message->marshalInt(&_numCalls);\n";
        //#endif
        e.out << leader2 << "  //Marshal callID\n";
        e.out << leader2 << "  int _callID = xr->getlineID();\n";
        e.out << leader2 << "  message->marshalInt(&_callID);\n";
        e.out << leader2 << "  //Marshal the distribution name:\n";
        e.out << leader2 << "  int namesize = " << distarr->getName().size()+4 << ";\n";
        e.out << leader2 << "  message->marshalInt(&namesize);\n";
        e.out << leader2 << "  message->marshalChar(\"" << distarr->getName() << ".out\",namesize);\n";
        e.out << leader2 << "  //Marshal the rank\n";
        e.out << leader2 << "  int rank = _rm->getRank();\n";
        e.out << leader2 << "  message->marshalInt(&rank);\n";
        e.out << leader2 << "  //Intersect and create distribution representation\n";
        e.out << leader2 << "  ::SSIDL::array2< int> _meta_arr = (*_rl_out)[i]->getArray();\n";
        e.out << leader2 << "  //Marshal distribution representation array\n";
        e.out << leader2 << "  int _rep_mdim[2];\n";
        e.out << leader2 << "  _rep_mdim[0]=_meta_arr.size1();\n";
        e.out << leader2 << "  _rep_mdim[1]=_meta_arr.size2();\n";
        e.out << leader2 << "  int _rep_mtotalsize=_rep_mdim[0]*_rep_mdim[1];\n";
        e.out << leader2 << "  ::SSIDL::array2< int>::pointer _rep_mptr=const_cast< ::SSIDL::array2< int>::pointer>(&_meta_arr[0][0]);\n";
        e.out << leader2 << "  message->marshalInt(&_rep_mdim[0], 2);\n";
        e.out << leader2 << "  message->marshalInt(_rep_mptr, _rep_mtotalsize);\n";
        e.out << leader2 << "  //Send Message \n";
        e.out << leader2 << "  int _handler=(*_rl_out)[i]->getReference()->getVtableBase()+" << handler << ";\n";
        e.out << leader2 << "  message->sendMessage(_handler);\n";
        e.out << leader2 << "  message->waitReply();\n";
        std::string templeader = e.out.leader;
        for (int i=arr_t->dim-1; i >= 0; i--) {
          e.out << leader2 << "  for (int adj" << i << "=_this_rep->getFirst(" << i+1
                << "), str" << i << "=_this_rep->getStride(" << i+1 << "), "
                << "i" << i << "=" << "_meta_arr[0][" << i << "]; " << "i" << i << "<="
                << "_meta_arr[1][" << i << "]; " << "i" << i << "+=" << "(_meta_arr[2]["
                << i << "] * _this_rep->getLocalStride(" << i+1 << ")) )\n";
          e.out.push_leader();
        }
        e.out << leader2 << "{\n";
        std::string ttleader = e.out.leader;
        e.out.push_leader();
        std::ostringstream var2unmarshal2;
        var2unmarshal2 << "(" << arg;
        for (int i=arr_t->dim-1; i >= 0; i--) {
          var2unmarshal2 << "[(i" << i << "-adj" << i << ")/str" << i << "]";
        }
        var2unmarshal2 << ")";
        arr_t->subtype->emit_unmarshal(e, var2unmarshal2.str(), "1", handler, ctx, false, false);
        e.out.pop_leader(ttleader);
        e.out << leader2 << "}\n";
        e.out.pop_leader(templeader);
        e.out << leader2 << "}\n";
        e.out << leader2 << "}\n";

#ifdef MxNDEBUG
        //TEMPORARY TEST:
        e.out << leader2 << "//Test\n";
        e.out << leader2 << "if (1) {\n";
        std::string testleader = e.out.push_leader();
        e.out << leader2 << "std::ostringstream fname;\n";
        e.out << leader2 << "fname << \"" << distarr->getName() << "\" << \"_\" << rm.getRank() << \".caller.out\";\n";
        e.out << leader2 << "d_sched->dbg.open(fname.str().c_str(), std::ios_base::app);\n";
        e.out << leader2 << "d_sched->dbg << \"Complete distribution received;\\n\";\n";
        if (arr_t->dim == 1) {
          e.out << leader2 << "for (unsigned int k = 0; k < " << arg << ".size(); k++)\n";
          e.out << leader2 << "  d_sched->dbg << k << \"arr = \" << " << arg << "[k] << \"\\n\";\n";
        } else {
          e.out << leader2 << "for (unsigned int k = 0; k < " << arg << ".size1(); k++)\n";
          e.out << leader2 << "  for (unsigned int i = 0; i < " << arg << ".size2(); i++)\n";
          e.out << leader2 << "    d_sched->dbg << k << \",\" << i << \"out_arr = \" << "
                << arg << "[k][i] << \"\\n\";\n";
        }
        e.out << leader2 << "if (d_sched->dbg)\n";
        e.out << leader2 << "  d_sched->dbg.close();\n";
        e.out.pop_leader(testleader);
        e.out << leader2 << "}\n";
        //EOF TEMPORARY TEST
#endif

        // *********** END OF OUT arg -- Special Redis ******************************
      }
    } else {
      if (ctx == ArgIn) {
        // *********** IN arg -- No Special Redis ****************************
        std::string Dname = distarr->getName();
        std::string arr_ptr_name = arg + "_ptr";
        e.out << leader2 << arr_t->cppfullname(0) << "* " << arr_ptr_name << ";\n";
        e.out << leader2 << arr_ptr_name << " = static_cast< " << arr_t->cppfullname(0)
	      << "*>(_sc->d_sched->waitCompleteInArray(\"" << Dname << "\",_sessionID,_callID));\n";
        e.out << leader2 << "#define " << arg << " (* " << arr_ptr_name << ")\n";

#ifdef MxNDEBUG
        //TEMPORARY TEST
        e.out << leader2 << "//Test\n";
        e.out << leader2 << "if (1) {\n";
        e.out << leader2 << "std::ostringstream fname;\n";
        e.out << leader2 << "int rank = 0;\n";
        e.out << leader2 << "MPI_Comm_rank(MPI_COMM_WORLD,&rank);\n";
        e.out << leader2 << "fname << \"" << Dname << "\" << \"_\" << rank << \".callee.out\";\n";
        e.out << leader2 << "_sc->d_sched->dbg.open(fname.str().c_str(), std::ios_base::app);\n";
        e.out << leader2 << "_sc->d_sched->dbg << \"Complete distribution received;\\n\";\n";
        if (arr_t->dim == 1) {
          e.out << leader2 << "for (unsigned int k = 0; k < " << arr_ptr_name << "->size(); k++)\n";
          e.out << leader2 << "  _sc->d_sched->dbg << k << \"arr = \" << (*" << arr_ptr_name << ")[k] << \"\\n\";\n";
        } else {
          e.out << leader2 << "for (unsigned int k = 0; k < " << arr_ptr_name << "->size1(); k++)\n";
          e.out << leader2 << "  for (unsigned int i = 0; i < " << arr_ptr_name << "->size2(); i++)\n";
          e.out << leader2 << "    _sc->d_sched->dbg << k << \",\" << i << \"arr = \" << (*"
                << arr_ptr_name << ")[k][i] << \"\\n\";\n";
        }
        e.out << leader2 << "if (_sc->d_sched->dbg)\n";
        e.out << leader2 << " _sc->d_sched->dbg.close();\n";
        e.out << leader2 << "}\n";
        //EOF TEMPORARY TEST
#endif
        // *********** END OF IN arg -- No Special Redis ******************************
      } else if (ctx == ArgOut) {
        // *********** OUT arg -- No Special Redis ********************************
        /*
	  std::string Dname = distarr->getName();
	  e.out << leader2 << "//OUT redistribution array detected. Get it afterwards\n";
	  e.out << leader2 << "//Resize array \n";
	  e.out << leader2 << "SCIRun::MxNArrayRep* _d_rep_" << Dname << " = d_sched->callerGetCallerRep(\""
	  << Dname << "\");\n";
	  e.out << leader2 << arg << ".resize(";
	  for (int i=arr_t->dim; i > 0; i--) {
          e.out << "_d_rep_" << Dname << "->getSize(" << i << ")";
          if (i != 1) e.out << ", ";
	  }
	  e.out << ");\n";
        */
        // *********** END OF OUT arg -- No Special Redis ****************************
      }
    }
    //****************** eof DISTRIBUTION ARRAY ********************************
  }
}

void
NamedType::emit_marshalsize( EmitState& /*e*/, const std::string& /*arg*/,
                             const std::string& /*sizevar*/,
			     const std::string& /*qty*/) const
{
}

void NamedType::emit_declaration(EmitState& e, const std::string& arg) const
{
  Symbol::Type symtype = name->getSymbol()->getType();
  if (symtype == Symbol::DistArrayType) {
    Definition* defn = name->getSymbol()->getDefinition();
    DistributionArray* distarr = dynamic_cast<DistributionArray*>(defn);
    ArrayType* arr_t = distarr->getArrayType();
    e.out << leader2 << arr_t->cppfullname(0) << "* " << arg << "_ptr = new " << arr_t->cppfullname(0) << ";\n";
    e.out << leader2 << "#define " << arg << " (*" << arg << "_ptr)\n";
  }
  else {
    e.out << leader2 << cppfullname(0) << " " << arg << ";\n";
  }
}

void NamedType::emit_marshal(EmitState& e, const std::string& arg,
			     const std::string& qty, const int handler,
			     bool top, ArgContext ctx, bool specialRedis,
			     storageT bufferStore) const
{
  Symbol::Type symtype = name->getSymbol()->getType();
  if (symtype == Symbol::EnumType) {
    if (specialRedis) return;
    if (qty != "1") {
      std::cerr << "NamedType::emit_marshal called with qty != 1: " << qty << '\n';
      exit(1);
    }
    if (bufferStore == doRetrieve) {
      if (ctx == ReturnType) {
        e.out << leader2 << arg << " = *((" << name->cppfullname(0) << "*)(_sc->storage->get("
	      << e.handlerNum << ",0" << ")));\n";
      } else {
        e.out << leader2 << name->cppfullname(0) << " " << arg << " = *((" << name->cppfullname(0)
	      << "*)(_sc->storage->get(" << e.handlerNum << "," << arg[arg.size()-1] << ",_sessionID,_callID)));\n";
      }
    }
    e.out << leader2 << "int " << arg << "_marshal = (int)" << arg << ";\n";
    e.out << leader2 << "message->marshalInt(&" << arg << "_marshal);\n";
    if (bufferStore == doStore) {
      if (ctx == ReturnType) {
        e.out << leader2 << "_sc->storage->add(" <<  e.handlerNum  << ", 0, (void*)(new "
              << name->cppfullname(0) << "(" << arg << "),_sessionID,_callID,_numCalls));\n";
      } else {
        e.out << leader2 << "_sc->storage->add(" <<  e.handlerNum  << "," << arg[arg.size()-1]
	      << ", (void*)(new " << name->cppfullname(0) << "(" << arg << "),_sessionID,_callID,_numCalls));\n";
      }
    }
  } else if (symtype == Symbol::ClassType || symtype == Symbol::InterfaceType) {
    if (specialRedis) return;
    if (qty != "1") {
      std::cerr << "NamedType::emit_marshal called with qty != 1: " << qty << '\n';
      exit(1);
    }
    if (bufferStore == doRetrieve) {
      if (ctx == ReturnType) {
        e.out << leader2 << arg << " = *((" << name->cppfullname(0) << "::pointer*)(_sc->storage->get("
	      << e.handlerNum << ",0,_sessionID,_callID)));\n";
      } else {
        e.out << leader2 << name->cppfullname(0) << "::pointer " << arg << " = *((" << name->cppfullname(0)
	      << "::pointer*)(_sc->storage->get(" << e.handlerNum << "," << arg[arg.size()-1] << ",_sessionID,_callID)));\n";
      }
    }
    e.out << leader2 << "if (!" << arg << ".isNull()) {\n";

    e.out << leader2 << "  " << arg << "->addReference();\n";
    e.out << leader2 << "  const ::SCIRun::TypeInfo* _dt=" << arg << "->_virtual_getTypeInfo();\n";
    e.out << leader2 << "  const ::SCIRun::TypeInfo* _bt=" << name->cppfullname(0) << "::_static_getTypeInfo();\n";
    e.out << leader2 << "  int _vtable_offset=_dt->computeVtableOffset(_bt);\n";

    e.out << leader2 << "  ::SCIRun::ReferenceMgr* " << arg << "_rm;\n";
    e.out << leader2 << "  " << arg << "->_getReferenceCopy(&" << arg << "_rm);\n";
    e.out << leader2 << "  ::SCIRun::refList* _refL = " << arg << "_rm->getAllReferences();\n";
    e.out << leader2 << "  int _vtable_base=(*_refL)[0]->getVtableBase()+_vtable_offset;\n";
    e.out << leader2 << "  message->marshalInt(&_vtable_base);\n";
    e.out << leader2 << "  int " << arg << "_refno=_refL->size();\n";
    e.out << leader2 << "  message->marshalInt(&" << arg << "_refno);\n";
    e.out << leader2 << "  for (::SCIRun::refList::iterator iter = _refL->begin(); iter != _refL->end(); iter++) {\n";
    e.out << leader2 << "    message->marshalSpChannel((*iter)->chan);\n";
    e.out << leader2 << "  }\n";

    e.out << leader2 << "} else {\n";
    e.out << leader2 << "  int _vtable_base=-1; // Null ptr\n";
    e.out << leader2 << "  message->marshalInt(&_vtable_base);\n";
    e.out << leader2 << "}\n";
    if (bufferStore == doStore) {
      if (ctx == ReturnType) {
        e.out << leader2 << "_sc->storage->add(" <<  e.handlerNum  << ",0, (void*)(new "
	      << name->cppfullname(0) << "::pointer(" << arg << ")),_sessionID,_callID,_numCalls);\n";
      } else {
        e.out << leader2 << "_sc->storage->add(" <<  e.handlerNum  << "," << arg[arg.size()-1]
	      << ", (void*)(new " << name->cppfullname(0) << "::pointer(" << arg << ")),_sessionID,_callID,_numCalls);\n";
      }
    }
  } else if (symtype == Symbol::DistArrayType) {
    Definition* defn = name->getSymbol()->getDefinition();
    DistributionArray* distarr = dynamic_cast<DistributionArray*>(defn);
    ArrayType* arr_t = distarr->getArrayType();
    if (specialRedis) {
      if (ctx == ArgIn) {
        // *********** IN arg -- Special Redis ******************************
        e.out << "\n";
        e.out << leader2 << "//Redistribute the array:\n";
        e.out << leader2 << "if (1) { //Hack to prevent varable shadowing on rl & this_rep\n";
        std::string ifone = e.out.push_leader();
        e.out << leader2 << "SCIRun::descriptorList* rl = d_sched->getRedistributionReps(\""
              << distarr->getName() << "\",getProxyUUID() );\n";
        e.out << leader2 << "SCIRun::MxNArrayRep* this_rep = d_sched->callerGetCallerRep(\""
              << distarr->getName() << "\",getProxyUUID() );\n";
        e.out << leader2 << "for (int i = 0; i < (int)rl->size(); i++) {\n";
        e.out << leader2 << "  SCIRun::Message* message = (*rl)[i]->getReference()->chan->getMessage();\n";
        //std::string dimname=arg+"_mdim";
        e.out << leader2 << "  message->createMessage();\n";
        e.out << leader2 << "  //Marshal the distribution flag\n";
        e.out << leader2 << "  ::SCIRun::callType _r_flag = ::SCIRun::REDIS;\n";
        e.out << leader2 << "  message->marshalInt((int*)&_r_flag);\n";
        //#ifdef HAVE_MPI
        e.out << leader2 << "  //Marshal the sessionID and number of actual calls from this proxy\n";
        //  e.out << leader2 << "  message->marshalChar(const_cast<char*>(_sessionID.c_str()), 36);\n";
        e.out << leader2 << "message->marshalInt(&_sessionID.iid);\n";
        e.out << leader2 << "message->marshalInt(&_sessionID.pid);\n";
        e.out << leader2 << "SCIRun::ProxyID _nextID=SCIRun::PRMI::peekProxyID();\n";
        e.out << leader2 << "message->marshalInt(&_nextID.iid);\n";
        e.out << leader2 << "message->marshalInt(&_nextID.pid);\n";
        e.out << leader2 << "  message->marshalInt(&_numCalls);\n";
        //#endif
        e.out << leader2 << "  //Marshal callID\n";
        e.out << leader2 << "  int _callID = xr->getlineID();\n";
        e.out << leader2 << "  message->marshalInt(&_callID);\n";
        e.out << leader2 << "  //Marshal the distribution name:\n";
        e.out << leader2 << "  int namesize = " << distarr->getName().size()+3 << ";\n";
        e.out << leader2 << "  message->marshalInt(&namesize);\n";
        e.out << leader2 << "  message->marshalChar(\"" << distarr->getName() << ".in\",namesize);\n";
        e.out << leader2 << "  //Marshal the rank\n";
        e.out << leader2 << "  int rank = _rm->getRank();\n";
        e.out << leader2 << "  message->marshalInt(&rank);\n";
        e.out << leader2 << "  //Intersect and create distribution representation\n";
        e.out << leader2 << "  ::SSIDL::array2< int> _meta_arr = (*rl)[i]->getArray();\n";
        e.out << leader2 << "  //Marshal distribution representation array\n";
        e.out << leader2 << "  int _rep_mdim[2];\n";
        e.out << leader2 << "  _rep_mdim[0]=_meta_arr.size1();\n";
        e.out << leader2 << "  _rep_mdim[1]=_meta_arr.size2();\n";
        e.out << leader2 << "  int _rep_mtotalsize=_rep_mdim[0]*_rep_mdim[1];\n";
        e.out << leader2 << "  ::SSIDL::array2< int>::pointer _rep_mptr=const_cast< ::SSIDL::array2< int>::pointer>(&_meta_arr[0][0]);\n";
        e.out << leader2 << "  message->marshalInt(&_rep_mdim[0], 2);\n";
        e.out << leader2 << "  message->marshalInt(_rep_mptr, _rep_mtotalsize);\n";
        e.out << leader2 << "  //Marshal the data:\n";
        std::string templeader = e.out.leader;
        for (int i=arr_t->dim-1; i >= 0; i--) {
          e.out << leader2 << "  for (int adj" << i << "=this_rep->getFirst(" << i+1
                << "), str" << i << "=this_rep->getStride(" << i+1 << "), "
                << "i" << i << "=" << "_meta_arr[0][" << i << "]; " << "i" << i << "<="
                << "_meta_arr[1][" << i << "]; " << "i" << i << "+=" << "(_meta_arr[2]["
                << i << "] * this_rep->getLocalStride(" << i+1 << ")) )\n";
          e.out.push_leader();
        }
        e.out << leader2 << "{\n";
        std::string ttleader = e.out.leader;
        e.out.push_leader();
        std::ostringstream var2marshal;
        var2marshal << arg;
        for (int i=arr_t->dim-1; i >= 0; i--) {
          var2marshal << "[(i" << i << "-adj" << i << ")/str" << i << "]";
        }
        arr_t->subtype->emit_marshal(e, var2marshal.str(), "1", handler, top, ctx, false);
        e.out.pop_leader(ttleader);
        e.out << leader2 << "}\n";
        e.out.pop_leader(templeader);
        e.out << leader2 << "  int _handler=(*rl)[i]->getReference()->getVtableBase()+" << handler << ";\n";
        e.out << leader2 << "  message->sendMessage(_handler);\n";
        e.out << leader2 << "  message->destroyMessage();\n";
        e.out << leader2 << "}\n";
        e.out.pop_leader(ifone);
        e.out << leader2 << "}\n"; //if (1) ...
        // *********** END OF IN arg -- Special Redis ******************************
      } else if (ctx == ArgOut) {
        // *********** OUT arg -- Special Redis ******************************
        e.out << leader2 << "if (dname == \"" << distarr->getName() << ".out\") {\n";
        e.out << leader2 << "  SCIRun::MxNArrayRep* _this_rep = _sc->d_sched->calleeGetCalleeRep(\"" << distarr->getName() << "\");\n";
        e.out << leader2 << "  SCIRun::MxNArrSynch* _synch = _sc->d_sched->getArrSynch(\"" << distarr->getName() << "\",_sessionID,_callID);\n";
        e.out << leader2 << "  //Unmarshal rank\n";
        e.out << leader2 << "  int rank;\n";
        e.out << leader2 << "  message->unmarshalInt(&rank, 1);\n";
        e.out << leader2 << "  " << arr_t->cppfullname(0) << "* _arr_ptr;\n";
        e.out << leader2 << "  _arr_ptr = static_cast< " << arr_t->cppfullname(0) << "*>(_synch->getArrayWait());\n";
        e.out << leader2 << "  //Unmarshal distribution metadata\n";
        e.out << leader2 << "  int _meta_rep_dim[2];\n";
        e.out << leader2 << "  message->unmarshalInt(&_meta_rep_dim[0], 2);\n";
        e.out << leader2 << "  ::SSIDL::array2< int> _meta_rep(_meta_rep_dim[0], _meta_rep_dim[1]);\n";
        e.out << leader2 << "  int _meta_rep_totalsize=_meta_rep_dim[0]*_meta_rep_dim[1];\n";
        e.out << leader2 << "  ::SSIDL::array2< int>::pointer _meta_rep_uptr=const_cast< ::SSIDL::array2< int>::pointer>(&_meta_rep[0][0]);\n";
        e.out << leader2 << "  message->unmarshalInt(_meta_rep_uptr, _meta_rep_totalsize);\n";
        e.out << leader2 << "  message->unmarshalReply();\n";
        e.out << leader2 << "  message->createMessage();\n";
        e.out << leader2 << "  //Marshal the data:\n";
        std::string templeader = e.out.leader;
        for (int i=arr_t->dim-1; i >= 0; i--) {
          e.out << leader2 << "  for (int adj" << i << "=_this_rep->getFirst(" << i+1
                << "), str" << i << "=_this_rep->getStride(" << i+1 << "), "
                << "i" << i << "=" << "_meta_rep[0][" << i << "]; " << "i" << i << "<="
                << "_meta_rep[1][" << i << "]; " << "i" << i << "+=" << "(_meta_rep[2]["
                << i << "] * _this_rep->getLocalStride(" << i+1 << ")) )\n";
          e.out.push_leader();
        }
        e.out << leader2 << "{\n";
        std::string ttleader = e.out.leader;
        e.out.push_leader();
        std::ostringstream var2marshal;
        var2marshal << "((*_arr_ptr)";
        for (int i=arr_t->dim-1; i >= 0; i--) {
          var2marshal << "[(i" << i << "-adj" << i << ")/str" << i << "]";
        }
        var2marshal << ")";
        arr_t->subtype->emit_marshal(e, var2marshal.str(), "1", handler, top, ctx, false);
        e.out.pop_leader(ttleader);
        e.out << leader2 << "}\n";
        e.out.pop_leader(templeader);
        e.out << leader2 << "  //Send Message\n";
        e.out << leader2 << "  message->sendMessage(0);\n";
        e.out << leader2 << "  message->destroyMessage();\n";
        e.out << leader2 << "  //One redistribution is sent back\n";
        e.out << leader2 << "  _synch->doSend(rank);\n";
        e.out << leader2 << "} /*endif \"" << distarr->getName() << "\"*/\n";
        // *********** END OF OUT arg -- Special Redis ******************************
      }
    } else {
      if ((ctx == ArgOut)&&(bufferStore != doRetrieve)) {
	// *********** OUT arg -- No Special Redis ******************************
	e.out << leader2 << "_sc->d_sched->setArray(\"" <<  distarr->getName() << "\",_sessionID, _callID, (void**)(&" << arg << "_ptr));\n";
	e.out << leader2 << "_sc->d_sched->waitCompleteOutArray(\"" << distarr->getName() << "\",_sessionID, _callID);\n";
	// *********** OUT arg -- No Special Redis ******************************
      }
    }
  } else {
    std::cerr << "Emit marshal shouldn't be called for packages/methods\n";
    exit(1);
  }
}

void NamedType::emit_rettype(EmitState& e, const std::string& arg) const
{
  Symbol::Type symtype = name->getSymbol()->getType();
  if (symtype == Symbol::EnumType) {
    e.out << name->cppfullname(0) << " " << arg;
  } else if (symtype == Symbol::DistArrayType) {
    Definition* defn = name->getSymbol()->getDefinition();
    DistributionArray* distarr = dynamic_cast<DistributionArray*>(defn);
    ArrayType* arr_t = distarr->getArrayType();
    arr_t->emit_rettype(e,arg);
  } else {
    e.out << name->cppfullname(0) << "::pointer " << arg;
  }
}

void NamedType::emit_prototype(SState& out, ArgContext ctx,
			       SymbolTable* localScope) const
{
  Symbol::Type symtype = name->getSymbol()->getType();
  if (symtype == Symbol::DistArrayType) {
    Definition* defn = name->getSymbol()->getDefinition();
    DistributionArray* distarr = dynamic_cast<DistributionArray*>(defn);
    ArrayType* arr_t = distarr->getArrayType();
    arr_t->emit_prototype(out, ctx, localScope);
  } else {
    // Ensure that it is forward declared...
    name->getSymbol()->emit_forward(*out.e);

    switch(ctx) {
    case ReturnType:
    case ArrayTemplate:
      out << cppfullname(localScope);
      break;
    case ArgIn:
      if (name->getSymbol()->getType() == Symbol::EnumType) {
        out << cppfullname(localScope);
      }
      else
	out << "const " << cppfullname(localScope) << "&";
      break;
    case ArgOut:
    case ArgInOut:
      out << cppfullname(localScope) << "&";
      break;
    }
  }
}

bool NamedType::array_use_pointer() const
{
  return false; // Always use iterator for array of references
}

bool NamedType::uniformsize() const
{
  return false; // Startpoints can vary in size
}

void Enum::emit(EmitState& e)
{
  if (emitted_declaration) {
    return;
  }

  e.fwd.begin_namespace(symbols->getParent());
  e.fwd << leader2 << "enum " << name << " {";
  bool first=true;
  for (std::vector<Enumerator*>::iterator iter = list.begin();
       iter != list.end(); iter++) {
    (*iter)->emit(e, first);
    first=false;
  }
  e.fwd << "\n";
  e.fwd << leader2 << "};\n";
  emitted_declaration=true;
}

void Enumerator::emit(EmitState& e, bool first)
{
  if (!first) {
    e.fwd << ",";
  }
  e.fwd << "\n";
  e.fwd << leader2 << "  " << name << "=" << value;
}

void DistributionArray::emit(EmitState& /*e*/)
{
}
