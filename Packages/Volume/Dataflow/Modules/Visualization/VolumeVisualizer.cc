//  
//  For more information, please see: http://software.sci.utah.edu
//  
//  The MIT License
//  
//  Copyright (c) 2004 Scientific Computing and Imaging Institute,
//  University of Utah.
//  
//  License for the specific language governing rights and limitations under
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//  
//  The above copyright notice and this permission notice shall be included
//  in all copies or substantial portions of the Software.
//  
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//  
//    File   : VolumeVisualizer.cc
//    Author : Milan Ikits
//    Author : Kurt Zimmerman
//    Date   : Sat Jul 10 21:55:34 2004

#include <Dataflow/Network/Module.h>
#include <Core/Malloc/Allocator.h>
#include <Core/Geom/ColorMap.h>
#include <Dataflow/Ports/ColorMapPort.h>
#include <Dataflow/Ports/GeometryPort.h>

#include <Dataflow/share/share.h>

#include <Core/GuiInterface/GuiVar.h>
#include <Core/Thread/CrowdMonitor.h>
#include <Dataflow/Widgets/PointWidget.h>

#include <Packages/Volume/Core/Geom/VolumeRenderer.h>
#include <Packages/Volume/Core/Datatypes/Texture.h>
#include <Packages/Volume/Dataflow/Ports/TexturePort.h>
#include <Packages/Volume/Dataflow/Ports/Colormap2Port.h>

#include <iostream>
#ifdef __sgi
#include <ios>
#endif
#include <algorithm>
#include <Packages/Volume/Core/Util/Utils.h>

namespace Volume {

using namespace SCIRun;

class PSECORESHARE VolumeVisualizer : public Module {
public:
  VolumeVisualizer(GuiContext*);
  virtual ~VolumeVisualizer();
  virtual void execute();
  virtual void tcl_command(GuiArgs&, void*);

private:
  TextureHandle tex;

  ColorMapIPort* icmap1;
  Colormap2IPort* icmap2;
  TextureIPort* intexture;
  GeometryOPort* ogeom;
  ColorMapOPort* ocmap;
  int cmap1_prevgen;
  int cmap2_prevgen;
   
  CrowdMonitor control_lock; 
  PointWidget* control_widget;
  GeomID control_id;

  GuiDouble gui_sampling_rate_hi_;
  GuiDouble gui_sampling_rate_lo_;
  GuiInt gui_adaptive_;
  GuiInt gui_cmap_size_;
  GuiInt gui_render_style_;
  GuiDouble gui_alpha_scale_;
  GuiInt gui_interp_mode_;
  GuiInt gui_shading_;
  GuiDouble gui_ambient_;
  GuiDouble gui_diffuse_;
  GuiDouble gui_specular_;
  GuiDouble gui_shine_;
  GuiInt gui_light_;
  
  VolumeRenderer *volren_;
};


DECLARE_MAKER(VolumeVisualizer)
VolumeVisualizer::VolumeVisualizer(GuiContext* ctx)
  : Module("VolumeVisualizer", ctx, Source, "Visualization", "Volume"),
    tex(0),
    cmap1_prevgen(0),
    cmap2_prevgen(0),
    control_lock("VolumeVisualizer resolution lock"),
    control_widget(0),
    control_id(-1),
    gui_sampling_rate_hi_(ctx->subVar("sampling_rate_hi")),
    gui_sampling_rate_lo_(ctx->subVar("sampling_rate_lo")),
    gui_adaptive_(ctx->subVar("adaptive")),
    gui_cmap_size_(ctx->subVar("cmap_size")),
    gui_render_style_(ctx->subVar("render_style")),
    gui_alpha_scale_(ctx->subVar("alpha_scale")),
    gui_interp_mode_(ctx->subVar("interp_mode")),
    gui_shading_(ctx->subVar("shading")),
    gui_ambient_(ctx->subVar("ambient")),
    gui_diffuse_(ctx->subVar("diffuse")),
    gui_specular_(ctx->subVar("specular")),
    gui_shine_(ctx->subVar("shine")),
    gui_light_(ctx->subVar("light")),
    volren_(0)
{
}

VolumeVisualizer::~VolumeVisualizer(){
}

void
VolumeVisualizer::execute(){

  static Point oldmin(0,0,0), oldmax(0,0,0);
  static int oldni = 0, oldnj = 0, oldnk = 0;
  static GeomID geomID  = 0;
  
  intexture = (TextureIPort*)get_iport("Texture");
  icmap1 = (ColorMapIPort*)get_iport("ColorMap");
  icmap2 = (Colormap2IPort*)get_iport("ColorMap2");
  ogeom = (GeometryOPort*)get_oport("Geometry");
  ocmap = (ColorMapOPort*)get_oport("ColorMap");
  if (!intexture) {
    error("Unable to initialize iport 'GL Texture'.");
    return;
  }
  if (!icmap1 && !icmap2) {
    error("Unable to initialize iport 'ColorMap' or 'ColorMap2'.");
    return;
  }
  if (!ogeom) {
    error("Unable to initialize oport 'Geometry'.");
    return;
  }
  
  if (!intexture->get(tex)) {
    return;
  }
  else if (!tex.get_rep()) {
    return;
  }
  
  ColorMapHandle cmap1;
  Colormap2Handle cmap2;
  bool c1 = icmap1->get(cmap1);
  bool c2 = icmap2->get(cmap2);
  if(!c1 && !c2) return;

  bool cmap1_dirty = false;
  bool cmap2_dirty = false;
  if(c1 && cmap1->generation != cmap1_prevgen) {
    cmap1_dirty = true;
  }    
  if(c2 && cmap2->generation != cmap2_prevgen) {
    cmap2_dirty = true;
  }    
  if(c1) cmap1_prevgen = cmap1->generation;
  if(c2) cmap2_prevgen = cmap2->generation;
  
//   if(!control_widget){
//     control_widget=scinew PointWidget(this, &control_lock, 0.2);
//     Transform trans = tex->get_field_transform();
//     Point Smin(trans.project(tex->min()));
//     Point Smax(trans.project(tex->max()));

//     double max =  std::max(Smax.x() - Smin.x(), Smax.y() - Smin.y());
//     max = std::max( max, Smax.z() - Smin.z());
//     control_widget->SetPosition(Interpolate(Smin,Smax,0.5));
//     control_widget->SetScale(max/80.0);
//   }

  //AuditAllocator(default_allocator);
  if(!volren_) {
    volren_ = new VolumeRenderer(tex, cmap1, cmap2);
    oldmin = tex->min();
    oldmax = tex->max();
    tex->get_dimensions(oldni, oldnj, oldnk);
    //    ogeom->delAll();
    geomID = ogeom->addObj(volren_, "VolumeRenderer TransParent");
  } else {
    volren_->set_texture(tex);
    if(cmap1_dirty)
      volren_->set_colormap1(cmap1);
    if(cmap2_dirty)
      volren_->set_colormap2(cmap2);
    int ni, nj, nk;
    tex->get_dimensions(ni, nj, nk);
    if(oldmin != tex->min() || oldmax != tex->max() ||
       ni != oldni || nj != oldnj || nk != oldnk) {
      ogeom->delObj(geomID);
      geomID = ogeom->addObj(volren_, "VolumeRenderer TransParent");
      oldni = ni; oldnj = nj; oldnk = nk;
      oldmin = tex->min();
      oldmax = tex->max();
    }
  }
 
  //AuditAllocator(default_allocator);
  volren_->set_interp(bool(gui_interp_mode_.get()));
  //AuditAllocator(default_allocator);

  switch(gui_render_style_.get()) {
  case 0:
    volren_->set_mode(VolumeRenderer::MODE_OVER);
    break;
  case 1:
    volren_->set_mode(VolumeRenderer::MODE_MIP);
    break;
  }
  
  //AuditAllocator(default_allocator);
  volren_->set_sampling_rate(gui_sampling_rate_hi_.get());
  volren_->set_interactive_rate(gui_sampling_rate_lo_.get());
  volren_->set_adaptive(gui_adaptive_.get());
  volren_->set_colormap_size(1 << gui_cmap_size_.get());
  volren_->set_slice_alpha(gui_alpha_scale_.get());
  volren_->set_shading(gui_shading_.get());
  volren_->set_material(gui_ambient_.get(), gui_diffuse_.get(),
                        gui_specular_.get(), gui_shine_.get());
  volren_->set_light(gui_light_.get());
  
  //AuditAllocator(default_allocator);
  ogeom->flushViews();				  
  //AuditAllocator(default_allocator);

  if (!ocmap) {
    error("Unable to initialize oport 'Color Map'.");
    return;
  } else {
    if(c1) {
      ColorMapHandle outcmap;
      outcmap = new ColorMap(*cmap1.get_rep()); 
      double vmin, vmax, gmin, gmax;
      tex->get_min_max(vmin, vmax, gmin, gmax);
      outcmap->Scale(vmin, vmax);
      ocmap->send(outcmap);
    }
  }    
}

void
VolumeVisualizer::tcl_command(GuiArgs& args, void* userdata)
{
  Module::tcl_command(args, userdata);
}

} // End namespace Volume
