
/*
 *  BaseWidget.h
 *
 *  Written by:
 *   James Purciful
 *   Department of Computer Science
 *   University of Utah
 *   Aug. 1994
 *
 *  Copyright (C) 1994 SCI Group
 */


#ifndef SCI_project_Base_Widget_h
#define SCI_project_Base_Widget_h 1

#include <Constraints/manifest.h>
#include <Constraints/BaseConstraint.h>
#include <Geometry/Point.h>
#include <Geometry/Vector.h>
#include <Geom/Geom.h>
#include <Geom/Material.h>

class GeomGroup;
class Module;

class BaseWidget {
public:
   BaseWidget( Module* module,
	       const Index vars, const Index cons,
	       const Index geoms, const Index mats,
	       const Real widget_scale );
   BaseWidget( const BaseWidget& );
   ~BaseWidget();

   inline void SetScale( const Real scale );
   inline double GetScale() const;

   inline void SetEpsilon( const Real Epsilon );

   inline GeomGroup* GetWidget();

   virtual void execute();
   const Point& GetVar( const Index vindex ) const;
   
   virtual void geom_pick(void*);
   virtual void geom_release(void*);
   virtual void geom_moved(int, double, const Vector&, void*);

   BaseWidget& operator=( const BaseWidget& );
   int operator==( const BaseWidget& );

   void print( ostream& os=cout ) const;

protected:
   Index NumConstraints;
   Index NumVariables;
   Index NumGeometries;
   Index NumMaterials;

   Array1<BaseConstraint*> constraints;
   Array1<Variable*> variables;
   Array1<GeomObj*> geometries;
   Array1<MaterialHandle> materials;

   GeomGroup* widget;
   Real widget_scale;

   Module* module;
};

inline ostream& operator<<( ostream& os, BaseWidget& w );


inline void
BaseWidget::SetScale( const double scale )
{
   widget_scale = scale;
}


inline double
BaseWidget::GetScale() const
{
   return widget_scale;
}


inline void
BaseWidget::SetEpsilon( const Real Epsilon )
{
   for (Index i=0; i<NumVariables; i++)
      variables[i]->SetEpsilon(Epsilon);
}


inline GeomGroup*
BaseWidget::GetWidget()
{
   return widget;
}


inline ostream&
operator<<( ostream& os, BaseWidget& w )
{
   w.print(os);
   return os;
}


#endif
