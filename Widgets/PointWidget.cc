
/*
 *  PointWidget.h
 *
 *  Written by:
 *   James Purciful
 *   Department of Computer Science
 *   University of Utah
 *   Jan. 1995
 *
 *  Copyright (C) 1995 SCI Group
 */


#include <Widgets/PointWidget.h>
#include <Geom/Sphere.h>

const Index NumCons = 0;
const Index NumVars = 1;
const Index NumGeoms = 1;
const Index NumMatls = 2;
const Index NumPcks = 1;
// const Index NumSchemes = 1;

enum { PointW_GeomPoint };
enum { PointW_Pick };

PointWidget::PointWidget( Module* module, CrowdMonitor* lock, double widget_scale )
: BaseWidget(module, lock, NumVars, NumCons, NumGeoms, NumMatls, NumPcks, widget_scale)
{
   variables[PointW_Point] = new Variable("Point", Scheme1, Point(0, 0, 0));

   materials[PointW_PointMatl] = PointWidgetMaterial;
   materials[PointW_HighMatl] = HighlightWidgetMaterial;

   geometries[PointW_GeomPoint] = new GeomSphere;
   GeomMaterial* sphm = new GeomMaterial(geometries[PointW_GeomPoint], materials[PointW_PointMatl]);
   picks[PointW_Pick] = new GeomPick(sphm, module);
   picks[PointW_Pick]->set_highlight(materials[PointW_HighMatl]);
   picks[PointW_Pick]->set_cbdata((void*)PointW_Pick);

   FinishWidget(picks[PointW_Pick]);
}


PointWidget::~PointWidget()
{
}


void
PointWidget::widget_execute()
{
   ((GeomSphere*)geometries[PointW_GeomPoint])->move(variables[PointW_Point]->Get(),
						     1*widget_scale);
}

void
PointWidget::geom_moved( int /* axis */, double /* dist */, const Vector& delta,
			 void* cbdata )
{
   switch((int)cbdata){
   case PointW_Pick:
      variables[PointW_Point]->MoveDelta(delta);
      break;
   }
}

