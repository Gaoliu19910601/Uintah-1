
/*
 *  FFrameWidget.h
 *
 *  Written by:
 *   James Purciful
 *   Department of Computer Science
 *   University of Utah
 *   Jan. 1995
 *
 *  Copyright (C) 1995 SCI Group
 */


#include <Widgets/FixedFrameWidget.h>
#include <Constraints/DistanceConstraint.h>
#include <Constraints/PythagorasConstraint.h>
#include <Constraints/PlaneConstraint.h>
#include <Constraints/RatioConstraint.h>
#include <Geom/Cylinder.h>
#include <Geom/Sphere.h>

const Index NumCons = 9;
const Index NumVars = 8;
const Index NumGeoms = 10;
const Index NumMatls = 4;
const Index NumPcks = 7;
const Index NumSchemes = 5;

enum { ConstULDR, ConstURDL, ConstPyth, ConstPlane,
       ConstULUR, ConstULDL, ConstDRUR, ConstDRDL,
       ConstRatio };
enum { GeomPointUL, GeomPointUR, GeomPointDR, GeomPointDL,
       GeomCylU, GeomCylR, GeomCylD, GeomCylL,
       GeomResizeU, GeomResizeD };
enum { PickSphUL, PickSphUR, PickSphDR, PickSphDL, PickCyls,
       PickResizeU, PickResizeD };

FixedFrameWidget::FixedFrameWidget( Module* module, CrowdMonitor* lock, Real widget_scale )
: BaseWidget(module, lock, NumVars, NumCons, NumGeoms, NumMatls, NumPcks, widget_scale*0.1),
  oldaxis1(1, 0, 0), oldaxis2(0, 1, 0)
{
   Real INIT = 1.0*widget_scale;
   // Scheme5 is used by the resize picks in GeomMoved!!
   variables[PointULVar] = new PointVariable("PntUL", solve, Scheme1, Point(0, 0, 0));
   variables[PointURVar] = new PointVariable("PntUR", solve, Scheme2, Point(INIT, 0, 0));
   variables[PointDRVar] = new PointVariable("PntDR", solve, Scheme3, Point(INIT, INIT, 0));
   variables[PointDLVar] = new PointVariable("PntDL", solve, Scheme4, Point(0, INIT, 0));
   variables[Dist1Var] = new RealVariable("Dist1", solve, Scheme1, INIT);
   variables[Dist2Var] = new RealVariable("Dist2", solve, Scheme1, INIT);
   variables[RatioVar] = new RealVariable("Ratio", solve, Scheme1, 1.0);
   variables[HypoVar] = new RealVariable("HYPO", solve, Scheme1, sqrt(2*INIT*INIT));

   constraints[ConstRatio] = new RatioConstraint("ConstRatio",
						 NumSchemes,
						 variables[Dist1Var],
						 variables[Dist2Var],
						 variables[RatioVar]);
   constraints[ConstRatio]->VarChoices(Scheme1, 1, 0, 1);
   constraints[ConstRatio]->VarChoices(Scheme2, 1, 0, 0);
   constraints[ConstRatio]->VarChoices(Scheme3, 1, 0, 1);
   constraints[ConstRatio]->VarChoices(Scheme4, 1, 0, 0);
   constraints[ConstRatio]->VarChoices(Scheme5, 1, 0, 0);
   constraints[ConstRatio]->Priorities(P_Highest, P_Highest, P_Default);
   constraints[ConstPlane] = new PlaneConstraint("ConstPlane",
						 NumSchemes,
						 variables[PointULVar],
						 variables[PointURVar],
						 variables[PointDRVar],
						 variables[PointDLVar]);
   constraints[ConstPlane]->VarChoices(Scheme1, 2, 3, 2, 1);
   constraints[ConstPlane]->VarChoices(Scheme2, 2, 3, 0, 3);
   constraints[ConstPlane]->VarChoices(Scheme3, 0, 3, 0, 1);
   constraints[ConstPlane]->VarChoices(Scheme4, 2, 1, 0, 1);
   constraints[ConstPlane]->VarChoices(Scheme5, 2, 3, 0, 1);
   constraints[ConstPlane]->Priorities(P_Highest, P_Highest,
				       P_Highest, P_Highest);
   constraints[ConstULDR] = new DistanceConstraint("Const13",
						   NumSchemes,
						   variables[PointULVar],
						   variables[PointDRVar],
						   variables[HypoVar]);
   constraints[ConstULDR]->VarChoices(Scheme1, 1, 1, 1);
   constraints[ConstULDR]->VarChoices(Scheme2, 1, 0, 1);
   constraints[ConstULDR]->VarChoices(Scheme3, 0, 0, 0);
   constraints[ConstULDR]->VarChoices(Scheme4, 1, 0, 1);
   constraints[ConstULDR]->VarChoices(Scheme5, 1, 0, 1);
   constraints[ConstULDR]->Priorities(P_HighMedium, P_HighMedium, P_Default);
   constraints[ConstURDL] = new DistanceConstraint("Const24",
						   NumSchemes,
						   variables[PointURVar],
						   variables[PointDLVar],
						   variables[HypoVar]);
   constraints[ConstURDL]->VarChoices(Scheme1, 1, 0, 1);
   constraints[ConstURDL]->VarChoices(Scheme2, 1, 1, 1);
   constraints[ConstURDL]->VarChoices(Scheme3, 1, 0, 1);
   constraints[ConstURDL]->VarChoices(Scheme4, 0, 0, 0);
   constraints[ConstURDL]->VarChoices(Scheme5, 2, 2, 0);
   constraints[ConstURDL]->Priorities(P_HighMedium, P_HighMedium, P_Default);
   constraints[ConstPyth] = new PythagorasConstraint("ConstPyth",
						     NumSchemes,
						     variables[Dist1Var],
						     variables[Dist2Var],
						     variables[HypoVar]);
   constraints[ConstPyth]->VarChoices(Scheme1, 1, 0, 1);
   constraints[ConstPyth]->VarChoices(Scheme2, 1, 0, 1);
   constraints[ConstPyth]->VarChoices(Scheme3, 1, 0, 1);
   constraints[ConstPyth]->VarChoices(Scheme4, 1, 0, 1);
   constraints[ConstPyth]->VarChoices(Scheme5, 1, 0, 1);
   constraints[ConstPyth]->Priorities(P_Default, P_Default, P_HighMedium);
   constraints[ConstULUR] = new DistanceConstraint("Const12",
						   NumSchemes,
						   variables[PointULVar],
						   variables[PointURVar],
						   variables[Dist1Var]);
   constraints[ConstULUR]->VarChoices(Scheme1, 1, 1, 1);
   constraints[ConstULUR]->VarChoices(Scheme2, 0, 0, 0);
   constraints[ConstULUR]->VarChoices(Scheme3, 1, 0, 0);
   constraints[ConstULUR]->VarChoices(Scheme4, 1, 0, 1);
   constraints[ConstULUR]->VarChoices(Scheme5, 0, 0, 0);
   constraints[ConstULUR]->Priorities(P_Default, P_Default, P_LowMedium);
   constraints[ConstULDL] = new DistanceConstraint("Const14",
						   NumSchemes,
						   variables[PointULVar],
						   variables[PointDLVar],
						   variables[Dist2Var]);
   constraints[ConstULDL]->VarChoices(Scheme1, 1, 1, 1);
   constraints[ConstULDL]->VarChoices(Scheme2, 1, 0, 0);
   constraints[ConstULDL]->VarChoices(Scheme3, 1, 0, 1);
   constraints[ConstULDL]->VarChoices(Scheme4, 0, 0, 0);
   constraints[ConstULDL]->VarChoices(Scheme5, 0, 0, 0);
   constraints[ConstULDL]->Priorities(P_Default, P_Default, P_LowMedium);
   constraints[ConstDRUR] = new DistanceConstraint("Const32",
						   NumSchemes,
						   variables[PointDRVar],
						   variables[PointURVar],
						   variables[Dist2Var]);
   constraints[ConstDRUR]->VarChoices(Scheme1, 1, 0, 1);
   constraints[ConstDRUR]->VarChoices(Scheme2, 0, 0, 0);
   constraints[ConstDRUR]->VarChoices(Scheme3, 1, 1, 1);
   constraints[ConstDRUR]->VarChoices(Scheme4, 1, 0, 0);
   constraints[ConstDRUR]->VarChoices(Scheme5, 0, 0, 0);
   constraints[ConstDRUR]->Priorities(P_Default, P_Default, P_LowMedium);
   constraints[ConstDRDL] = new DistanceConstraint("Const34",
						   NumSchemes,
						   variables[PointDRVar],
						   variables[PointDLVar],
						   variables[Dist1Var]);
   constraints[ConstDRDL]->VarChoices(Scheme1, 1, 0, 1);
   constraints[ConstDRDL]->VarChoices(Scheme2, 1, 0, 0);
   constraints[ConstDRDL]->VarChoices(Scheme3, 1, 1, 1);
   constraints[ConstDRDL]->VarChoices(Scheme4, 0, 0, 0);
   constraints[ConstDRDL]->VarChoices(Scheme5, 0, 0, 0);
   constraints[ConstDRDL]->Priorities(P_Default, P_Default, P_LowMedium);

   materials[PointMatl] = PointWidgetMaterial;
   materials[EdgeMatl] = EdgeWidgetMaterial;
   materials[ResizeMatl] = ResizeWidgetMaterial;
   materials[HighMatl] = HighlightWidgetMaterial;

   Index geom, pick;
   GeomGroup* pts = new GeomGroup;
   for (geom = GeomPointUL, pick = PickSphUL;
	geom <= GeomPointDL; geom++, pick++) {
      geometries[geom] = new GeomSphere;
      picks[pick] = new GeomPick(geometries[geom], module);
      picks[pick]->set_highlight(materials[HighMatl]);
      picks[pick]->set_cbdata((void*)pick);
      pts->add(picks[pick]);
   }
   GeomMaterial* ptsm = new GeomMaterial(pts, materials[PointMatl]);
   
   GeomGroup* resizes = new GeomGroup;
   geometries[GeomResizeU] = new GeomCappedCylinder;
   picks[PickResizeU] = new GeomPick(geometries[GeomResizeU], module);
   picks[PickResizeU]->set_highlight(materials[HighMatl]);
   picks[PickResizeU]->set_cbdata((void*)PickResizeU);
   resizes->add(picks[PickResizeU]);
   geometries[GeomResizeD] = new GeomCappedCylinder;
   picks[PickResizeD] = new GeomPick(geometries[GeomResizeD], module);
   picks[PickResizeD]->set_highlight(materials[HighMatl]);
   picks[PickResizeD]->set_cbdata((void*)PickResizeD);
   resizes->add(picks[PickResizeD]);
   GeomMaterial* resizem = new GeomMaterial(resizes, materials[ResizeMatl]);
   
   GeomGroup* cyls = new GeomGroup;
   for (geom = GeomCylU; geom <= GeomCylL; geom++) {
      geometries[geom] = new GeomCylinder;
      cyls->add(geometries[geom]);
   }
   picks[PickCyls] = new GeomPick(cyls, module);
   picks[PickCyls]->set_highlight(materials[HighMatl]);
   picks[PickCyls]->set_cbdata((void*)PickCyls);
   GeomMaterial* cylsm = new GeomMaterial(picks[PickCyls], materials[EdgeMatl]);

   GeomGroup* w = new GeomGroup;
   w->add(ptsm);
   w->add(resizem);
   w->add(cylsm);

   SetEpsilon(widget_scale*1e-6);
   
   FinishWidget(w);
}


FixedFrameWidget::~FixedFrameWidget()
{
}


void
FixedFrameWidget::widget_execute()
{
   ((GeomSphere*)geometries[GeomPointUL])->move(variables[PointULVar]->point(),
						   1*widget_scale);
   ((GeomSphere*)geometries[GeomPointUR])->move(variables[PointURVar]->point(),
						   1*widget_scale);
   ((GeomSphere*)geometries[GeomPointDR])->move(variables[PointDRVar]->point(),
						   1*widget_scale);
   ((GeomSphere*)geometries[GeomPointDL])->move(variables[PointDLVar]->point(),
						   1*widget_scale);
   Point p(variables[PointULVar]->point() + (variables[PointURVar]->point()
						- variables[PointULVar]->point()) / 2.0);
   ((GeomCappedCylinder*)geometries[GeomResizeU])->move(p - (GetAxis2() * 0.6 * widget_scale),
							p + (GetAxis2() * 0.6 * widget_scale),
							0.75*widget_scale);
   p = variables[PointDRVar]->point() + (variables[PointDLVar]->point()
					    - variables[PointDRVar]->point()) / 2.0;
   ((GeomCappedCylinder*)geometries[GeomResizeD])->move(p - (GetAxis2() * 0.6 * widget_scale),
							p + (GetAxis2() * 0.6 * widget_scale),
							0.75*widget_scale);
   ((GeomCylinder*)geometries[GeomCylU])->move(variables[PointULVar]->point(),
					       variables[PointURVar]->point(),
					       0.5*widget_scale);
   ((GeomCylinder*)geometries[GeomCylR])->move(variables[PointURVar]->point(),
					       variables[PointDRVar]->point(),
					       0.5*widget_scale);
   ((GeomCylinder*)geometries[GeomCylD])->move(variables[PointDRVar]->point(),
					       variables[PointDLVar]->point(),
					       0.5*widget_scale);
   ((GeomCylinder*)geometries[GeomCylL])->move(variables[PointDLVar]->point(),
					       variables[PointULVar]->point(),
					       0.5*widget_scale);

   ((DistanceConstraint*)constraints[ConstULUR])->SetMinimum(3.2*widget_scale);
   ((DistanceConstraint*)constraints[ConstDRDL])->SetMinimum(3.2*widget_scale);
   ((DistanceConstraint*)constraints[ConstULDL])->SetMinimum(3.2*widget_scale);
   ((DistanceConstraint*)constraints[ConstDRUR])->SetMinimum(3.2*widget_scale);
   ((DistanceConstraint*)constraints[ConstULDR])->SetMinimum(sqrt(2*3.2*3.2)*widget_scale);
   ((DistanceConstraint*)constraints[ConstURDL])->SetMinimum(sqrt(2*3.2*3.2)*widget_scale);

   SetEpsilon(widget_scale*1e-6);

   Vector spvec1(variables[PointURVar]->point() - variables[PointULVar]->point());
   Vector spvec2(variables[PointDLVar]->point() - variables[PointULVar]->point());
   if ((spvec1.length2() > 0.0) && (spvec2.length2() > 0.0)) {
      spvec1.normalize();
      spvec2.normalize();
      Vector v = Cross(spvec1, spvec2);
      for (Index geom = 0; geom < NumPcks; geom++) {
	 if ((geom == PickResizeU) || (geom == PickResizeD))
	    picks[geom]->set_principal(spvec2);
	 else
	    picks[geom]->set_principal(spvec1, spvec2, v);
      }
   }
}

void
FixedFrameWidget::geom_moved( int /* axis */, double /* dist */, const Vector& delta,
			      void* cbdata )
{
   Vector delt(delta);
   Real t;
   
   ((DistanceConstraint*)constraints[ConstULUR])->SetDefault(GetAxis1());
   ((DistanceConstraint*)constraints[ConstDRDL])->SetDefault(GetAxis1());
   ((DistanceConstraint*)constraints[ConstULDL])->SetDefault(GetAxis2());
   ((DistanceConstraint*)constraints[ConstDRUR])->SetDefault(GetAxis2());

   switch((int)cbdata){
   case PickSphUL:
      variables[PointULVar]->SetDelta(delta);
      break;
   case PickSphUR:
      variables[PointURVar]->SetDelta(delta);
      break;
   case PickSphDR:
      variables[PointDRVar]->SetDelta(delta);
      break;
   case PickSphDL:
      variables[PointDLVar]->SetDelta(delta);
      break;
   case PickResizeU:
      if (((variables[PointULVar]->point()+delta)-variables[PointDLVar]->point()).length()
	  < 3.2*widget_scale) {
	 delt = ((variables[PointDLVar]->point() + delta.normal()*3.2*widget_scale)
		 - variables[PointULVar]->point());
      }
      t = delt.length();
      if (Dot(delt, GetAxis2()) < 0.0)
	 t = -t;
      variables[PointDLVar]->MoveDelta(GetAxis1()*t/2.0);
      variables[PointURVar]->SetDelta(delt-GetAxis1()*t/2.0, Scheme5);
      break;
   case PickResizeD:
      if (((variables[PointDRVar]->point()+delta)-variables[PointURVar]->point()).length()
	  < 3.2*widget_scale) {
	 delt = ((variables[PointURVar]->point() + delta.normal()*3.2*widget_scale)
		 - variables[PointDRVar]->point());
      }
      t = delt.length();
      if (Dot(delt, GetAxis2()) < 0.0)
	 t = -t;
      variables[PointURVar]->MoveDelta(GetAxis1()*t/2.0);
      variables[PointDLVar]->SetDelta(delt-GetAxis1()*t/2.0, Scheme5);
      break;
   case PickCyls:
      MoveDelta(delta);
      break;
   }
}


void
FixedFrameWidget::MoveDelta( const Vector& delta )
{
   variables[PointULVar]->MoveDelta(delta);
   variables[PointURVar]->MoveDelta(delta);
   variables[PointDRVar]->MoveDelta(delta);
   variables[PointDLVar]->MoveDelta(delta);
}


Point
FixedFrameWidget::ReferencePoint() const
{
   return (variables[PointULVar]->point()
	   + (variables[PointDRVar]->point()
	      -variables[PointULVar]->point())/2.0);
}
