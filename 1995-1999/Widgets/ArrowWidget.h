
/*
 *  ArrowWidget.h
 *
 *  Written by:
 *   James Purciful
 *   Department of Computer Science
 *   University of Utah
 *   Aug. 1994
 *
 *  Copyright (C) 1994 SCI Group
 */


#ifndef SCI_project_Arrow_Widget_h
#define SCI_project_Arrow_Widget_h 1

#include <Widgets/BaseWidget.h>


class ArrowWidget : public BaseWidget {
public:
   ArrowWidget( Module* module, CrowdMonitor* lock, double widget_scale );
   ArrowWidget( const ArrowWidget& );
   virtual ~ArrowWidget();

   virtual void redraw();
   virtual void geom_moved(GeomPick*, int, double, const Vector&, int, const BState&);

   virtual void MoveDelta( const Vector& delta );
   virtual Point ReferencePoint() const;

   void SetPosition( const Point& );
   Point GetPosition() const;
   
   void SetDirection( const Vector& v );
   const Vector& GetDirection() const;

   virtual void widget_tcl( TCLArgs& );

   // Variable indexs
   enum { PointVar };

   // Material indexs
   enum { PointMatl, ShaftMatl, HeadMatl };

protected:
   virtual clString GetMaterialName( const Index mindex ) const;   
   
private:
   Vector direction;
};


#endif
