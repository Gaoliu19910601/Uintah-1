//static char *id="@(#) $Id$";

/*
 *  WidgetTest.cc:  
 *
 *  Written by:
 *   James Purciful
 *   Department of Computer Science
 *   University of Utah
 *   Jan. 1995
 *
 *  Copyright (C) 1995 SCI Group
 */

#include <Util/NotFinished.h>
#include <Dataflow/Module.h>
#include <CommonDatatypes/GeometryPort.h>
#include <Geometry/Point.h>
#include <Malloc/Allocator.h>
#include <TclInterface/TCLvar.h>

#include <Widgets/PointWidget.h>
#include <Widgets/ArrowWidget.h>
#include <Widgets/CriticalPointWidget.h>
#include <Widgets/CrosshairWidget.h>
#include <Widgets/GaugeWidget.h>
#include <Widgets/RingWidget.h>
#include <Widgets/FrameWidget.h>
#include <Widgets/ScaledFrameWidget.h>
#include <Widgets/BoxWidget.h>
#include <Widgets/ScaledBoxWidget.h>
#include <Widgets/ViewWidget.h>
#include <Widgets/LightWidget.h>
#include <Widgets/PathWidget.h>

#include <iostream.h>

namespace PSECommon {
namespace Modules {

using namespace PSECommon::Dataflow;
using namespace PSECommon::CommonDatatypes;
using namespace PSECommon::Widgets;
using namespace SCICore::TclInterface;
using namespace SCICore::GeomSpace;

enum WidgetTypes { WT_Point, WT_Arrow, WT_Crit, WT_Cross, WT_Gauge, WT_Ring,
		   WT_Frame, WT_SFrame, WT_Box, WT_SBox, WT_View, WT_Light,
		   WT_Path, NumWidgetTypes };

class WidgetTest : public Module {
   GeometryOPort* ogeom;
   CrowdMonitor widget_lock;

private:
   int init;
   int widget_id;
   TCLdouble widget_scale;
   TCLint widget_type;

   BaseWidget* widgets[NumWidgetTypes];

   virtual void widget_moved(int);

public:
   WidgetTest(const clString& id);
   WidgetTest(const WidgetTest&, int deep);
   virtual ~WidgetTest();
   virtual Module* clone(int deep);
   virtual void execute();

   virtual void tcl_command(TCLArgs&, void*);
};

Module* make_WidgetTest(const clString& id) {
  return new WidgetTest(id);
}

static clString module_name("WidgetTest");

WidgetTest::WidgetTest(const clString& id)
: Module("WidgetTest", id, Source), widget_scale("widget_scale", id, this),
  widget_type("widget_type", id, this),
  init(1)
{
   // Create the output port
   ogeom = scinew GeometryOPort(this, "Geometry", GeometryIPort::Atomic);
   add_oport(ogeom);

   float INIT(0.01);

   widgets[WT_Point] = scinew PointWidget(this, &widget_lock, INIT);
   widgets[WT_Arrow] = scinew ArrowWidget(this, &widget_lock, INIT);
   widgets[WT_Crit] = scinew CriticalPointWidget(this, &widget_lock, INIT);
   widgets[WT_Cross] = scinew CrosshairWidget(this, &widget_lock, INIT);
   widgets[WT_Gauge] = scinew GaugeWidget(this, &widget_lock, INIT);
   widgets[WT_Ring] = scinew RingWidget(this, &widget_lock, INIT);
   widgets[WT_Frame] = scinew FrameWidget(this, &widget_lock, INIT);
   widgets[WT_SFrame] = scinew ScaledFrameWidget(this, &widget_lock, INIT);
   widgets[WT_Box] = scinew BoxWidget(this, &widget_lock, INIT);
   widgets[WT_SBox] = scinew ScaledBoxWidget(this, &widget_lock, INIT);
   widgets[WT_View] = scinew ViewWidget(this, &widget_lock, INIT);
   widgets[WT_Light] = scinew LightWidget(this, &widget_lock, INIT);
   widgets[WT_Path] = scinew PathWidget(this, &widget_lock, INIT);
}

WidgetTest::WidgetTest(const WidgetTest& copy, int deep)
: Module(copy, deep),
  widget_scale("widget_scale", id, this),
  widget_type("widget_type", id, this)
{
   NOT_FINISHED("WidgetTest::WidgetTest");
}

WidgetTest::~WidgetTest()
{
}

Module* WidgetTest::clone(int deep)
{
   return scinew WidgetTest(*this, deep);
}

void WidgetTest::execute()
{
   if (init == 1) {
      init = 0;
      GeomGroup* w = scinew GeomGroup;
      int i;
      for(i = 0; i < NumWidgetTypes; i++)
	 w->add(widgets[i]->GetWidget());
      widget_id = ogeom->addObj(w, module_name, &widget_lock);
      for(i = 0; i < NumWidgetTypes; i++)
	 widgets[i]->Connect(ogeom);
   }
   cerr << "There are currently " << ogeom->getNRoe() << " active roe\n";
   GeometryData* data=ogeom->getData(0, GEOM_ALLDATA);
   cerr << "data=" << data << endl;
}

void WidgetTest::widget_moved(int last)
{
    if(last)
	cerr << "Last callback...\n";
   cerr << "WidgetTest: begin widget_moved" << endl;
   // Update Arrow/Critical point widget
   if ((widgets[WT_Arrow]->ReferencePoint()-Point(0,0,0)).length2() >= 1e-6)
      ((ArrowWidget*)widgets[WT_Arrow])->SetDirection((Point(0,0,0)-widgets[WT_Arrow]->ReferencePoint()).normal());
   if ((widgets[WT_Crit]->ReferencePoint()-Point(0,0,0)).length2() >= 1e-6)
      ((CriticalPointWidget*)widgets[WT_Crit])->SetDirection((Point(0,0,0)-widgets[WT_Crit]->ReferencePoint()).normal());
   
   widget_lock.read_lock();
   cout << "Gauge ratio " << ((GaugeWidget*)widgets[WT_Gauge])->GetRatio() << endl;
   cout << "Ring angle " << ((RingWidget*)widgets[WT_Ring])->GetRatio() << endl;
   cout << "FOV " << ((ViewWidget*)widgets[WT_View])->GetFOV() << endl;
   widget_lock.read_unlock();

   // If your module needs to execute when the widget moves, add these lines:
   //if(last && !abort_flag){
   //    abort_flag=1;
   //    want_to_execute();
   //}
   cerr << "WidgetTest: end widget_moved" << endl;
}


void WidgetTest::tcl_command(TCLArgs& args, void* userdata)
{
#if 0
   if(args.count() < 2){
      args.error("WidgetTest needs a minor command");
      return;
   }
#endif
   if (args[1] == "nextmode") {
      widgets[widget_type.get()]->NextMode();
   } else if(args[1] == "select"){
       // Select the appropriate widget
       reset_vars();
       widget_lock.write_lock();
       for(int i = 0; i < NumWidgetTypes; i++)
	   widgets[i]->SetState(0);
       widgets[widget_type.get()]->SetState(1);
       widget_lock.write_unlock();
       widgets[widget_type.get()]->SetScale(widget_scale.get());
   } else if(args[1] == "scale"){
      reset_vars();
      widgets[widget_type.get()]->SetScale(widget_scale.get());
   } else if(args[1] == "ui"){
      widgets[widget_type.get()]->ui();
   } else {
      Module::tcl_command(args, userdata);
   }
}

} // End namespace Modules
} // End namespace PSECommon

//
// $Log$
// Revision 1.1  1999/07/27 16:58:18  mcq
// Initial commit
//
// Revision 1.1.1.1  1999/04/24 23:12:34  dav
// Import sources
//
//
