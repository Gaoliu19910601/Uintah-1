
/*
 *  CubeWidget.h
 *
 *  Written by:
 *   James Purciful
 *   Department of Computer Science
 *   University of Utah
 *   Aug. 1994
 *
 *  Copyright (C) 1994 SCI Group
 */


#ifndef SCI_project_Cube_Widget_h
#define SCI_project_Cube_Widget_h 1

#include <Widgets/BaseWidget.h>


// Variable indexs
enum { CubeW_PointIUL, CubeW_PointIUR, CubeW_PointIDR, CubeW_PointIDL,
       CubeW_PointOUL, CubeW_PointOUR, CubeW_PointODR, CubeW_PointODL,
       CubeW_Dist, CubeW_Hypo, CubeW_Diag };
// Material indexs
enum { CubeW_PointMatl, CubeW_EdgeMatl, CubeW_HighMatl };


class CubeWidget : public BaseWidget {
public:
   CubeWidget( Module* module, CrowdMonitor* lock, double widget_scale );
   CubeWidget( const CubeWidget& );
   ~CubeWidget();

   virtual void widget_execute();
   virtual void geom_moved(int, double, const Vector&, void*);
};


#endif
