
#ifndef Uintah_ArchesTable_h
#define Uintah_ArchesTable_h

#include <Packages/Uintah/CCA/Components/Models/test/TableInterface.h>

namespace Uintah {

/****************************************

CLASS
   ArchesTable
   
   Short description...

GENERAL INFORMATION

   ArchesTable.h

   Steven G. Parker
   Department of Computer Science
   University of Utah

   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
  
   Copyright (C) 2000 SCI Group

KEYWORDS
   ArchesTable

DESCRIPTION
   Long description...
  
WARNING
  
****************************************/

  class ArchesTable : public TableInterface {
  public:
    ArchesTable(ProblemSpecP& ps);
    virtual ~ArchesTable();

    virtual void addIndependentVariable(const string&);
    virtual int addDependentVariable(const string&);
    
    virtual void setup();
    
    virtual void interpolate(int index, CCVariable<double>& result,
			     const CellIterator&,
			     vector<constCCVariable<double> >& independents);
    virtual double interpolate(int index, vector<double>& independents);
  private:

    struct Ind {
      string name;
      vector<double> weights;
      vector<long> offset;

      bool uniform;
      double dx;
    };
    vector<Ind*> inds;

    struct Expr {
      char op;
      Expr(char op, Expr* child1, Expr* child2)
        : op(op), child1(child1), child2(child2)
      {
      }
      Expr(int dep)
        : op('i'), child1(0), child2(0), dep(dep)
      {
      }
      Expr(double constant)
        : op('c'), child1(0), child2(0), constant(constant)
      {
      }
      Expr* child1;
      Expr* child2;
      int dep;
      double constant;
      ~Expr() {
        if(child1)
          delete child1;
        if(child2)
          delete child2;
      }
    };

    struct Dep {
      string name;
      enum Type {
        ConstantValue,
        DerivedValue,
        TableValue
      } type;
      double constantValue;
      double* data;
      Expr* expression;
      Dep(Type type) : type(type) { data = 0; expression = 0; }
    };
    vector<Dep*> deps;

    Expr* parse_addsub(string::iterator&  begin, string::iterator& end);
    Expr* parse_muldiv(string::iterator&  begin, string::iterator& end);
    Expr* parse_sign(string::iterator&  begin, string::iterator& end);
    Expr* parse_idorconstant(string::iterator&  begin, string::iterator& end);
    void evaluate(Expr* expr, double* data, int size);

    string filename;
    bool file_read;

    struct DefaultValue {
      string name;
      double value;
    };
    vector<DefaultValue*> defaults;

    int getInt(istream&);
    double getDouble(istream&);
    bool getBool(istream&);
    string getString(istream&);

    bool startline;
    void error(istream& in);
    void skipComments(istream& in);
    void eatWhite(istream& in);
  };
} // End namespace Uintah
    
#endif
