
/*
 *  CrosshairWidget.h
 *
 *  Written by:
 *   James Purciful
 *   Department of Computer Science
 *   University of Utah
 *   Jan. 1995
 *
 *  Copyright (C) 1995 SCI Group
 */


#ifndef SCI_project_Crosshair_Widget_h
#define SCI_project_Crosshair_Widget_h 1

#include <Dataflow/Widgets/BaseWidget.h>

namespace SCIRun {

#if defined(__sgi) && !defined(__GNUC__) && (_MIPS_SIM != _MIPS_SIM_ABI32)
// Turn off warnings about partially overridden virtual functions
#pragma set woff 1682
#endif

class CrosshairWidget : public BaseWidget {
public:
   CrosshairWidget( Module* module, CrowdMonitor* lock, double widget_scale );
   CrosshairWidget( const CrosshairWidget& );
   virtual ~CrosshairWidget();

   virtual void redraw();
   virtual void geom_moved(GeomPick*, int, double, const Vector&, int, const BState&);

   virtual void MoveDelta( const Vector& delta );
   virtual Point ReferencePoint() const;

   void SetPosition( const Point& );
   Point GetPosition() const;

   // They should be orthogonal.
   void SetAxes( const Vector& v1, const Vector& v2, const Vector& v3 );
   void GetAxes( Vector& v1, Vector& v2, Vector& v3 ) const;

   virtual void widget_tcl( TCLArgs& );

   // Variable indexs
   enum { CenterVar };

   // Material indexs
   enum { PointMatl, AxesMatl };
   
protected:
   virtual clString GetMaterialName( const Index mindex ) const;   
   
private:
   Vector axis1, axis2, axis3;
};

} // End namespace SCIRun

#if defined(__sgi) && !defined(__GNUC__) && (_MIPS_SIM != _MIPS_SIM_ABI32)
#pragma reset woff 1682
#endif


#endif
