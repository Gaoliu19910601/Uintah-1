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
//    File   : EditTransferFunc2.cc
//    Author : Milan Ikits
//    Author : Michael Callahan
//    Date   : Thu Jul  8 01:50:58 2004

#include <Dataflow/Network/Module.h>
#include <Core/GuiInterface/GuiVar.h>
#include <Core/Containers/StringUtil.h>
#include <Core/Containers/Array3.h>
#include <Core/Persistent/Pstreams.h>

#include <sci_gl.h>
#include <Core/Volume/Pbuffer.h>
#include <Core/Volume/CM2Shader.h>
#include <Core/Volume/CM2Widget.h>
#include <Core/Volume/ShaderProgramARB.h>
#include <Dataflow/Ports/Colormap2Port.h>
#include <Dataflow/Ports/NrrdPort.h>

#include <Core/Geom/GeomOpenGL.h>
#include <Core/Geom/TkOpenGLContext.h>
#include <stdio.h>
#include <stack>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>

// tcl interpreter corresponding to this module
extern Tcl_Interp* the_interp;

using std::stack;

namespace SCIRun {

struct UndoItem
{
  enum Action { UNDO_CHANGE, UNDO_ADD, UNDO_DELETE };

  int action_;
  int selected_;
  CM2WidgetHandle widget_;

  UndoItem(int a, int s, CM2WidgetHandle w)
    : action_(a), selected_(s), widget_(w)
  {
  }

};


class EditTransferFunc2 : public Module {

public:
  EditTransferFunc2(GuiContext* ctx);
  virtual ~EditTransferFunc2();

  virtual void execute();

  virtual void tcl_command(GuiArgs&, void*);
  virtual void presave();
  
  void resize_gui(int n = -1);
  void update_from_gui();
  void update_to_gui(bool forward = true);
  void tcl_unpickle();

  void undo();

  bool make_current();
  void init_pbuffer();
  void rasterize_widgets();
  void draw_colormap();
  void save_ppm(const char * const filename,
              int sx, int sy,
              int bpp, const unsigned char * buf);
  void update_histo();
  void draw_histo();
  void redraw();

  void push(int x, int y, int button, int modifier);
  void motion(int x, int y);
  void release(int x, int y, int button);

private:
  TkOpenGLContext *                     ctx_;
  int                                   width_;
  int                                   height_;
  bool                                  button_;
  vector<CM2WidgetHandle>               widgets_;
  stack<UndoItem>                       undo_stack_;
  CM2ShaderFactory*                     shader_factory_;
  Pbuffer*                              pbuffer_;
  Array3<float>                         array_;
  bool                                  use_pbuffer_;
  bool                                  use_back_buffer_;
  
  int                                   icmap_gen_;

  Nrrd*                                 histo_;
  bool                                  histo_dirty_;
  GLuint                                histo_tex_;

  bool                                  cmap_dirty_;
  bool                                  cmap_size_dirty_;
  GLuint                                cmap_tex_;
  
  // Which widget is selected.
  int                                   pick_widget_; 
  // The part of the widget that is selected.
  int                                   pick_object_; 
  // Push on undo when motion occurs, not on select.
  bool                                  first_motion_;
  bool save_ppm_;


  //! functions and for panning.
  void translate_start(int x, int y);
  void translate_motion(int x, int y);
  void translate_end(int x, int y);

  //! functions and for zooming.
  void scale_start(int x, int y);
  void scale_motion(int x, int y);
  void scale_end(int x, int y);

  void screen_val(int &x, int &y);

  int                                   mouse_last_x_;
  int                                   mouse_last_y_;
  GuiDouble                             pan_x_;
  GuiDouble                             pan_y_;
  GuiDouble                             scale_;

  bool                                  updating_; // updating the tf or not

  GuiInt                                gui_faux_;
  GuiDouble                             gui_histo_;


  GuiInt			        gui_num_entries_;
  vector<GuiString *>		        gui_name_;
  vector<GuiDouble *>		        gui_color_r_;
  vector<GuiDouble *>		        gui_color_g_;
  vector<GuiDouble *>		        gui_color_b_;
  vector<GuiDouble *>		        gui_color_a_;
  vector<GuiString *>                   gui_wstate_;
  vector<GuiInt *>		        gui_sstate_;
  vector<GuiInt *>                      gui_onstate_;

  // variables for file loading and saving
  GuiFilename                           filename_;
  string                                old_filename_;
  time_t 	                        old_filemodification_;
  GuiString *				end_marker_;
};



DECLARE_MAKER(EditTransferFunc2)

EditTransferFunc2::EditTransferFunc2(GuiContext* ctx)
  : Module("EditTransferFunc2", ctx, Filter, "Visualization", "SCIRun"),
    ctx_(0), 
    button_(0), 
    shader_factory_(0),
    pbuffer_(0), 
    use_pbuffer_(true), 
    use_back_buffer_(true),
    icmap_gen_(-1),
    histo_(0), 
    histo_dirty_(false), 
    histo_tex_(0),
    cmap_dirty_(true), 
    cmap_tex_(0),
    pick_widget_(-1), 
    pick_object_(0), 
    first_motion_(true),
    save_ppm_(false),
    mouse_last_x_(0),
    mouse_last_y_(0),
    pan_x_(ctx->subVar("panx")),
    pan_y_(ctx->subVar("pany")),
    scale_(ctx->subVar("scale_factor")),
    updating_(false),
    gui_faux_(ctx->subVar("faux")),
    gui_histo_(ctx->subVar("histo")),
    gui_num_entries_(ctx->subVar("num-entries")),
    filename_(ctx->subVar("filename")),
    old_filemodification_(0),
    end_marker_(0)
{
  pan_x_.set(0.0);
  pan_y_.set(0.0);
  scale_.set(1.0);
  widgets_.push_back(scinew TriangleCM2Widget());
  widgets_.push_back(scinew RectangleCM2Widget());
  resize_gui(2);
  update_to_gui(false);
}


EditTransferFunc2::~EditTransferFunc2()
{
  delete pbuffer_;
  delete shader_factory_;
  if (end_marker_) 
    delete end_marker_;
}


void
EditTransferFunc2::translate_start(int x, int y)
{
  mouse_last_x_ = x;
  mouse_last_y_ = y;
}

void
EditTransferFunc2::translate_motion(int x, int y)
{
  float xmtn = float(mouse_last_x_ - x) / float(width_);
  float ymtn = -float(mouse_last_y_ - y) / float(height_);
  mouse_last_x_ = x;
  mouse_last_y_ = y;

  pan_x_.set(pan_x_.get() + xmtn / scale_.get());
  pan_y_.set(pan_y_.get() + ymtn / scale_.get());

  redraw();
}

void
EditTransferFunc2::translate_end(int x, int y)
{
  redraw();
}

void
EditTransferFunc2::scale_start(int x, int y)
{
  mouse_last_y_ = y;
}

void
EditTransferFunc2::scale_motion(int x, int y)
{
  float ymtn = -float(mouse_last_y_ - y) / float(height_);
  mouse_last_y_ = y;
  scale_.set(scale_.get() + -ymtn);

  if (scale_.get() < 0.0) scale_.set(0.0);

  redraw();
}

void
EditTransferFunc2::scale_end(int x, int y)
{
  redraw();
}

int
do_round(float d)
{
  if (d > 0.0) return (int)(d + 0.5);
  else return -(int)(-d + 0.5); 
}

void
EditTransferFunc2::screen_val(int &x, int &y)
{
  const float cx = width_ * 0.5;
  const float cy = height_ * 0.5;
  const float sf_inv = 1.0 / scale_.get();

  x = do_round((x - cx) * sf_inv + cx + (pan_x_.get() * width_));
  y = do_round((y - cy) * sf_inv + cy - (pan_y_.get() * height_));
}

void
EditTransferFunc2::tcl_command(GuiArgs& args, void* userdata)
{
  if (args.count() < 2) {
    args.error("No command for EditTransferFunc");
    return;
  }

  // mouse commands are motion, down, release
  if (args[1] == "mouse") {
    int x, y, b, m;
    string_to_int(args[3], x);
    string_to_int(args[4], y);
    screen_val(x,y);
    if (args[2] == "motion") {
      if (button_ == 0) // not buttons down!
	return;
      motion(x, y);
    } else {
      string_to_int(args[5], b); // which button it was
      if (args[2] == "push") {
	string_to_int(args[6], m); // which button it was
	push(x, y, b, m);
      } else if (args[2] == "release") {
	release(x, y, b);
      } else if (args[2] == "translate") {
	string_to_int(args[4], x);
	string_to_int(args[5], y);
	if (args[3] == "start") {
	  translate_start(x, y);
	} else if (args[3] == "move") {
	  translate_motion(x, y);
	} else {
	  // end
	  translate_end(x, y);
	}	
      } else if (args[2] == "reset") {
	pan_x_.set(0.0);
	pan_y_.set(0.0);
	scale_.set(1.0);
	redraw();
      } else if (args[2] == "scale") {
	string_to_int(args[4], x);
	string_to_int(args[5], y);
	if (args[3] == "start") {
	  scale_start(x, y);
	} else if (args[3] == "move") {
	  scale_motion(x, y);
	} else {
	  // end
	  scale_end(x, y);
	}
      }
    }
  } else if (args[1] == "resize") {
    string_to_int(args[2], width_);
    string_to_int(args[3], height_);
    redraw();
  } else if (args[1] == "expose") {
    redraw();
  } else if (args[1] == "redraw") {
    reset_vars();
    if (args.count() > 2)
    {
      cmap_dirty_ = true;
    }
    redraw();
  } else if (args[1] == "redrawcmap") {
    cmap_dirty_ = true;
    redraw();
  } else if (args[1] == "destroygl") {
    if (ctx_) {
      delete ctx_;
      ctx_ = 0;
    }
  } else if (args[1] == "setgl") {
    ASSERT(args.count() == 3);
    if (ctx_) {
      delete ctx_;
    }
    ctx_ = scinew TkOpenGLContext(args[2], 0, 512, 256);
    width_ = 512;
    height_ = 256;
  } else if (!args[1].compare(0, 13, "color_change-")) {
    int n;
    string_to_int(args[1].substr(13), n);

    gui_num_entries_.reset();
    resize_gui();
    gui_color_r_[n]->reset();
    gui_color_g_[n]->reset();
    gui_color_b_[n]->reset();
    gui_color_a_[n]->reset();
    const double r = gui_color_r_[n]->get();
    const double g = gui_color_g_[n]->get();
    const double b = gui_color_b_[n]->get();
    const double a = gui_color_a_[n]->get();
    Color c(widgets_[n]->color());
    if (r != c.r() || g != c.g() || b != c.b() || a != widgets_[n]->alpha())
    {
      undo_stack_.push(UndoItem(UndoItem::UNDO_CHANGE, n,
				widgets_[n]->clone()));
      update_from_gui();
      want_to_execute();
    }
  } else if (args[1] == "unpickle") {
    tcl_unpickle();
  } else if (args[1] == "addtriangle") {
    widgets_.push_back(scinew TriangleCM2Widget());
    undo_stack_.push(UndoItem(UndoItem::UNDO_ADD, widgets_.size()-1, NULL));
    cmap_dirty_ = true;
    redraw();
    update_to_gui();
    want_to_execute();
  } else if (args[1] == "addrectangle") {
    widgets_.push_back(scinew RectangleCM2Widget());
    undo_stack_.push(UndoItem(UndoItem::UNDO_ADD, widgets_.size()-1, NULL));
    cmap_dirty_ = true;
    redraw();
    update_to_gui();
    want_to_execute();
  } else if (args[1] == "deletewidget") {
    if (pick_widget_ != -1 && pick_widget_ < (int)widgets_.size())
    {
      // Delete widget.
      undo_stack_.push(UndoItem(UndoItem::UNDO_DELETE,
				pick_widget_, widgets_[pick_widget_]));
      widgets_.erase(widgets_.begin() + pick_widget_);
      pick_widget_ = -1;
      cmap_dirty_ = true;
      redraw();
      update_to_gui();
      want_to_execute();
    }
  } else if (!args[1].compare(0, 12, "shadewidget-")) {
    int i;
    string_to_int(args[1].substr(12), i);

    resize_gui();  // make sure the guivar vector exists
    reset_vars();  // sync the guivar vectors to the TCL side
    // Toggle the shading type from flat to normal and vice-versa
    widgets_[i]->set_shadeType(gui_sstate_[i]->get());
    cmap_dirty_ = true;
    redraw();
    update_to_gui();
    want_to_execute();
  } else if(!args[1].compare(0, 9, "toggleon-")) {
    int i;
    string_to_int(args[1].substr(9), i);
    resize_gui();  // make sure the guivar vector exists
    reset_vars();  // sync the guivar vectors to the TCL side
    widgets_[i]->set_onState(gui_onstate_[i]->get());  // toggle on/off state.
    
    cmap_dirty_ = true;
    redraw();
    update_to_gui();
    want_to_execute();
    
  } else if (args[1] == "undowidget") {
    undo();
  } else if (args[1] == "reset_gui") {
    
  } else if (args[1] == "load") {
    // The implementation of this was taken almost directly from
    // NrrdReader Module.  
    filename_.reset();
    string fn(filename_.get());
    if(fn == "") {
      error("Please Specify a Transfer Function filename.");
      return;
    }

    struct stat buf;
    if(stat(fn.c_str(), &buf) == -1) {
      error(string("EditTransferFunc2 error - file not found: '")+fn+"'");
      return;
    }

    // if we haven't read yet, or if it's new, or if time has changed, read.
#ifdef __sgi
    time_t new_filemodification = buf.st_mtim.tv_sec;
#else 
    time_t new_filemodification = buf.st_mtime;
#endif
    if( new_filemodification != old_filemodification_)
    {
      old_filemodification_ = new_filemodification;
      old_filename_ = fn;
      
      int len = fn.size();
      const string suffix(".xff");

      // Check the suffix.
      if (fn.substr(len - 4, 4) == suffix) {
        BinaryPiostream *stream = new BinaryPiostream(fn, Piostream::Read);
        if (!stream) {
          error("Error reading file '" + fn + "'.");
          return;
        }

        // read the file.
        int size;
        stream->io(size);
        if(stream->reading()) widgets_.resize(size);
        for(unsigned int i = 0; i < widgets_.size(); i++) {
          Pio(*stream, widgets_[i]);
        }
        delete stream;

        cmap_dirty_ = true;
        redraw();
        update_to_gui();
        want_to_execute();
      }
    }    
  } else if (args[1] == "save") {
    const string fn(filename_.get());
    if (fn == "") {
      error("Warning;  No filename provided to EditTransferFunc2");
      return;
    }

    // Open ostream
    Piostream* stream;
    stream = scinew BinaryPiostream(fn, Piostream::Write);
    if (stream->error())
      error("Could not open file for writing" + fn);
    else { 
      int size = widgets_.size();
      Pio(*stream, size);
      for(unsigned int i = 0; i < widgets_.size(); i++) {
        Pio(*stream, widgets_[i]);
      }
      delete stream;
    }

  } else if (args[1] == "saveppm") { 
    save_ppm_ = true;
    redraw();
  }
  else {
    Module::tcl_command(args, userdata);
  }
}


void
EditTransferFunc2::presave()
{
  unsigned int i;

  resize_gui();
  update_to_gui(false);

  // Pickle up the tcl states.
  for (i = 0; i < widgets_.size(); i++)
  {
    gui_wstate_[i]->set(widgets_[i]->tcl_pickle());
  }

  // Delete all of the unused variables.
  for (i = widgets_.size(); i < gui_name_.size(); i++)
  {
    const string num = to_string(i);
    //    ctx->erase("name-" + num);
    //    ctx->erase(num +"-color-r");
    //    ctx->erase(num +"-color-g");
    //    ctx->erase(num +"-color-b");
    //    ctx->erase(num +"-color-a");
    //    ctx->erase("state-" + num);
    //    ctx->erase("shadeType-" + num);
    //    ctx->erase("on-" + num);

    delete gui_name_[i];
    delete gui_color_r_[i];
    delete gui_color_g_[i];
    delete gui_color_b_[i];
    delete gui_color_a_[i];
    delete gui_wstate_[i];
    delete gui_sstate_[i];
    delete gui_onstate_[i];
  }
  if (widgets_.size() < gui_name_.size())
  {
    const unsigned int ws = widgets_.size();
    gui_name_.erase(gui_name_.begin() + ws, gui_name_.end());
    gui_color_r_.erase(gui_color_r_.begin() + ws, gui_color_r_.end());
    gui_color_g_.erase(gui_color_g_.begin() + ws, gui_color_g_.end());
    gui_color_b_.erase(gui_color_b_.begin() + ws, gui_color_b_.end());
    gui_color_a_.erase(gui_color_a_.begin() + ws, gui_color_a_.end());
    gui_wstate_.erase(gui_wstate_.begin() + ws, gui_wstate_.end());
    gui_sstate_.erase(gui_sstate_.begin() + ws, gui_sstate_.end());
    gui_onstate_.erase(gui_onstate_.begin() + ws, gui_onstate_.end());
  }
}


void
EditTransferFunc2::undo()
{
  bool gui_update = false;
  if (!undo_stack_.empty())
  {
    const UndoItem &item = undo_stack_.top();
    
    switch (item.action_)
    {
    case UndoItem::UNDO_CHANGE:
      widgets_[item.selected_] = item.widget_;
      gui_update = true;
      break;

    case UndoItem::UNDO_ADD:
      widgets_.erase(widgets_.begin() + item.selected_);
      gui_update = true;
      break;
   
    case UndoItem::UNDO_DELETE:
      widgets_.insert(widgets_.begin() + item.selected_, item.widget_);
      gui_update = true;
      break;
    }
    undo_stack_.pop();
    cmap_dirty_ = true;
    redraw();
    want_to_execute();
    if (gui_update)
    {
      update_to_gui();
    }
  }
}


void
EditTransferFunc2::resize_gui(int n)
{
  gui_num_entries_.set(n==-1?widgets_.size():n);
  unsigned int i = 0;
  // Expand the gui elements.
  for (i = gui_name_.size(); i < (unsigned int)gui_num_entries_.get(); i++)
  {
    const string num = to_string(i);
    gui_name_.push_back(new GuiString(ctx->subVar("name-" + num)));
    gui_color_r_.push_back(new GuiDouble(ctx->subVar(num +"-color-r")));
    gui_color_g_.push_back(new GuiDouble(ctx->subVar(num +"-color-g")));
    gui_color_b_.push_back(new GuiDouble(ctx->subVar(num +"-color-b")));
    gui_color_a_.push_back(new GuiDouble(ctx->subVar(num +"-color-a")));
    gui_wstate_.push_back(new GuiString(ctx->subVar("state-" + num)));
    gui_sstate_.push_back(new GuiInt(ctx->subVar("shadeType-" + num)));
    gui_onstate_.push_back(new GuiInt(ctx->subVar("on-" + num)));

  }
  // Delete the old variable that marked the end of the variables
  if (end_marker_) 
    delete end_marker_;
  // Create a new marker that marks the end of the variables
  if (i != 0) 
    end_marker_ = scinew GuiString(ctx->subVar("marker"), "end");
}


void
EditTransferFunc2::update_to_gui(bool forward)
{
  // Update GUI
  resize_gui();
  for (unsigned int i = 0; i < widgets_.size(); i++)
  {
    gui_name_[i]->set("Generic");
    Color c(widgets_[i]->color());
    gui_color_r_[i]->set(c.r());
    gui_color_g_[i]->set(c.g());
    gui_color_b_[i]->set(c.b());
    gui_color_a_[i]->set(widgets_[i]->alpha());
    gui_sstate_[i]->set(widgets_[i]->get_shadeType());
    gui_onstate_[i]->set(widgets_[i]->get_onState());
  }
  if (forward) { gui->execute(id + " create_entries"); }
}


void
EditTransferFunc2::update_from_gui()
{
  gui_num_entries_.reset();
  resize_gui();
  for (unsigned int i = 0; i < widgets_.size(); i++)
  {
    gui_color_r_[i]->reset();
    gui_color_g_[i]->reset();
    gui_color_b_[i]->reset();
    gui_color_a_[i]->reset();
    widgets_[i]->set_color(Color(gui_color_r_[i]->get(),
				 gui_color_g_[i]->get(),
				 gui_color_b_[i]->get()));
    widgets_[i]->set_alpha(gui_color_a_[i]->get());
    widgets_[i]->set_shadeType(gui_sstate_[i]->get());
    widgets_[i]->set_onState(gui_onstate_[i]->get());
  }
  cmap_dirty_ = true;
  redraw();
}


void
EditTransferFunc2::tcl_unpickle()
{
  widgets_.clear();

  gui_num_entries_.reset();
  resize_gui(gui_num_entries_.get());
  for (int i=0; i < gui_num_entries_.get(); i++)
  {
    gui_wstate_[i]->reset();
    if (gui_wstate_[i]->get()[0] == 't')
    {
      widgets_.push_back(scinew TriangleCM2Widget());
      widgets_[widgets_.size()-1]->tcl_unpickle(gui_wstate_[i]->get());
    }
    else if (gui_wstate_[i]->get()[0] == 'r')
    {
      widgets_.push_back(scinew RectangleCM2Widget());
      widgets_[widgets_.size()-1]->tcl_unpickle(gui_wstate_[i]->get());
    }
    else if (gui_wstate_[i]->get()[0] == 'i') {
      widgets_.push_back(scinew ImageCM2Widget());
      widgets_[widgets_.size()-1]->tcl_unpickle(gui_wstate_[i]->get());
    }
  }

  // Grab colors
  resize_gui();
  update_from_gui();
}


void
EditTransferFunc2::push(int x, int y, int button, int modifier)
{
  int i;

  button_ = button;
  first_motion_ = true;

  for (i = 0; i < (int)widgets_.size(); i++)
  {
    widgets_[i]->unselect_all();
  }

  pick_widget_ = -1;
  pick_object_ = 0;
  if (modifier == 0)
  {
    for (i = widgets_.size()-1; i >= 0; i--)
    {
      const int tmp = widgets_[i]->pick1(x, height_-1-y, width_, height_);
      if (tmp)
      {
	pick_widget_ = i;
	pick_object_ = tmp;
	widgets_[i]->select(tmp);
	break;
      }
    }
  }
  if (pick_widget_ == -1)
  {
    for (i = widgets_.size()-1; i >= 0; i--)
    {
      const int m = modifier;
      const int tmp = widgets_[i]->pick2(x, height_-1-y, width_, height_, m);
      if (tmp)
      {
	pick_widget_ = i;
	pick_object_ = tmp;
	widgets_[i]->select(tmp);
	break;
      }
    }
  }
  if (pick_widget_ != -1)
  {
    redraw();
  }
}



void
EditTransferFunc2::motion(int x, int y)
{
  if (pick_widget_ != -1)
  {
    if (first_motion_)
    {
      undo_stack_.push(UndoItem(UndoItem::UNDO_CHANGE, pick_widget_,
				widgets_[pick_widget_]->clone()));
      first_motion_ = false;
    }

    widgets_[pick_widget_]->move(pick_object_, x, height_-1-y, width_, height_);
    cmap_dirty_ = true;
    updating_ = true;
    update_to_gui(true);
    redraw();
    want_to_execute();
  }
}



void
EditTransferFunc2::release(int x, int y, int button)
{
  button_ = 0;
  if (pick_widget_ != -1)
  {
    widgets_[pick_widget_]->release(pick_object_, x, height_-1-y, width_, height_);
    updating_ = false;
    cmap_dirty_ = true;

    redraw();
    update_to_gui(true);
    want_to_execute();
  }
}

void
EditTransferFunc2::execute()
{
  ColorMap2IPort* cmap_iport = (ColorMap2IPort*)get_iport("Input Colormap");
  if (cmap_iport) {
    ColorMap2Handle icmap_handle;
    cmap_iport->get(icmap_handle);

    bool no_input = ! icmap_handle.get_rep();
    bool new_input = ! no_input && icmap_handle->generation != icmap_gen_;

    if ((new_input || no_input) && widgets_[0]->is_empty()) {
      widgets_.erase(widgets_.begin());
    }
    if (icmap_handle.get_rep() && icmap_handle->generation != icmap_gen_) {
      // add the input widgets to our existing set.
      widgets_.insert(widgets_.begin(), icmap_handle->widgets().begin(), 
		      icmap_handle->widgets().end());
      icmap_gen_ = icmap_handle->generation;
      cmap_dirty_ = true;
      redraw();
    }
  }

  gui_num_entries_.reset();
  if ((unsigned)gui_num_entries_.get() != widgets_.size()) {  
    resize_gui();
    update_to_gui();
  }

  NrrdIPort* histo_port = (NrrdIPort*)get_iport("Histogram");
  if(histo_port) {
    NrrdDataHandle h;
    histo_port->get(h);
    if(h.get_rep()) {
      if(h->nrrd->dim != 2 && h->nrrd->dim != 3) {
        error("Invalid input dimension.");
      }
      if(histo_ != h->nrrd) {
        histo_ = h->nrrd;
        histo_dirty_ = true;
      }
    } else {
      if(histo_ != 0)
        histo_dirty_ = true;
      histo_ = 0;
    }
  } else {
    if(histo_ != 0)
      histo_dirty_ = true;
    histo_ = 0;
  }

  if(histo_dirty_) {
    redraw();
  }
  
  ColorMap2OPort* cmap_port = (ColorMap2OPort*)get_oport("Output Colormap");
  if (cmap_port) {
    ColorMap2Handle cmap(scinew ColorMap2(widgets_, updating_, gui_faux_.get()));
    cmap_port->send(cmap);
  }
}


bool
EditTransferFunc2::make_current()
{
  // check if it was created
  if(!ctx_) {
    return false;
  }

  ctx_->make_current();

  return true;
}

void
EditTransferFunc2::init_pbuffer() 
{
  //----------------------------------------------------------------
  // decide what rasterization to use
  if(use_pbuffer_ || use_back_buffer_) {
    if(!shader_factory_) {
      shader_factory_ = new CM2ShaderFactory();
      FragmentProgramARB dummy("!!ARBfp1.0\nMOV result.color, 0.0;\nEND");
      if(dummy.create()) {
        dummy.destroy();
        use_pbuffer_ = false;
        use_back_buffer_ = false;
        warning ("[EditTransferFunction2] Shaders not supported;");
	warning ("  switching to software rasterization.");
      }
      dummy.destroy();
    }
  }

  //! init/resize the pbuffer
  if(use_pbuffer_ && (!pbuffer_ || pbuffer_->width() != width_ || 
		      pbuffer_->height() != height_)) 
  {
    if(pbuffer_) {
      pbuffer_->destroy();
      delete pbuffer_;
    }
    pbuffer_ = new Pbuffer(width_, height_, GL_INT, 8, true, GL_FALSE);
    if(pbuffer_->create()) {
      pbuffer_->destroy();
      delete pbuffer_;
      pbuffer_ = 0;
      use_pbuffer_ = false;
      warning("[EditTransferFunction2] Pbuffers not supported;");
      warning("  switching to back buffer rasterization.");
    } else {
      use_back_buffer_ = false;
      remark("[EditTransferFunction2] Using Pbuffer rasterization.");
    }
  }
}

void
EditTransferFunc2::rasterize_widgets()
{
  //----------------------------------------------------------------
  // update local array
  if(use_pbuffer_) {
    // pbuffer rasterization
    if(cmap_dirty_) {
      pbuffer_->makeCurrent();
      glDrawBuffer(GL_FRONT);
      glViewport(0, 0, width_, height_);

      glClearColor(0.0, 0.0, 0.0, 0.0);
      glClear(GL_COLOR_BUFFER_BIT);

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      glTranslatef(-1.0, -1.0, 0.0);
      glScalef(2.0, 2.0, 2.0);

      glDisable(GL_DEPTH_TEST);
      glDisable(GL_LIGHTING);
      glDisable(GL_CULL_FACE);

      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    
      // Rasterize widgets
      for (unsigned int i=0; i<widgets_.size(); i++) {
        widgets_[i]->rasterize(*shader_factory_, gui_faux_.get(), 0);
      }

      glDisable(GL_BLEND);
      pbuffer_->swapBuffers();
      ctx_->make_current();
      cmap_dirty_ = false;
    }
  } else {
    // software rasterization
    bool cmap_size_dirty = width_ != array_.dim2() || height_ != array_.dim1();
    if(cmap_dirty_ || cmap_size_dirty) {
      // realloc cmap
      if(cmap_size_dirty)
        array_.resize(height_, width_, 4);
      // clear cmap
      for(int i=0; i<array_.dim1(); i++) {
        for(int j=0; j<array_.dim2(); j++) {
          array_(i,j,0) = 0.0;
          array_(i,j,1) = 0.0;
          array_(i,j,2) = 0.0;
          array_(i,j,3) = 0.0;
        }
      }
      // rasterize widgets
      for (unsigned int i=0; i<widgets_.size(); i++) {
	if (! widgets_[i]->is_empty()) {
	  widgets_[i]->rasterize(array_, gui_faux_.get());
	}
      }
      // update texture
      if(cmap_size_dirty) {
        if(glIsTexture(cmap_tex_)) {
          glDeleteTextures(1, &cmap_tex_);
          cmap_tex_ = 0;
        }
        glGenTextures(1, &cmap_tex_);
        glBindTexture(GL_TEXTURE_2D, cmap_tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, array_.dim2(), array_.dim1(),
                     0, GL_RGBA, GL_FLOAT, &array_(0,0,0));
        glBindTexture(GL_TEXTURE_2D, 0);
      } else {
        glBindTexture(GL_TEXTURE_2D, cmap_tex_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, array_.dim2(), array_.dim1(),
                        GL_RGBA, GL_FLOAT, &array_(0,0,0));
        glBindTexture(GL_TEXTURE_2D, 0);
      }
      cmap_dirty_ = false;
    }
  }
} 
void
EditTransferFunc2::draw_histo()
{
#ifdef HAVE_AVR_SUPPORT
  glActiveTexture(GL_TEXTURE0);
#endif
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, histo_tex_);
  double alpha = gui_histo_.get();
  glColor4f(alpha, alpha, alpha, 1.0);
  glBegin(GL_QUADS);
  {
    glTexCoord2f( 0.0,  0.0);
    glVertex2f( 0.0,  0.0);
    glTexCoord2f( 1.0,  0.0);
    glVertex2f( 1.0,  0.0);
    glTexCoord2f( 1.0,  1.0);
    glVertex2f( 1.0,  1.0);
    glTexCoord2f( 0.0,  1.0);
    glVertex2f( 0.0,  1.0);
  }
  glEnd();
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);
}

void EditTransferFunc2::save_ppm(const char * const filename,
              int sx, int sy,
              int bpp, const unsigned char * buf)
{
  int R = 3;
  int G = 2;
  int B = 1;
  int A = 0;

  ofstream output(filename, ios::out);
  
  if ( ! output )
    {
      cerr << "ERROR: can't open file " << filename <<"\n"; // endl;
      return;
    }
  
  if ( bpp == 1 || bpp == 2 )
    output << "P2\n# CREATOR: " << "\n"; // endl;
  else if ( bpp == 3 || bpp == 4 )
    output << "P3\n# CREATOR: " << "\n"; // endl;
  else {
    cerr << "ERROR: unknown number of bytes per pixel " << bpp << "\n"; // endl;
    return;
  }
  
  output << sx << " " << sy << "\n"; // endl;
  output << 255 << "\n"; // endl;
  
  for ( int row = sy - 1; row >= 0; --row )
    {
      for ( int col = 0; col < sx; ++col )
	{
	  int p = bpp * ( row * sx + col );
	  switch (bpp) {
	  case 2:
	  case 1:
	    output << (int) buf[p] << "\n"; // endl;
	    break;
	  default:
	    output << (int) buf[p + R] << " " << (int) buf[p + G]
		   << " " << (int) buf[p + B] << "\n"; // endl;
	    break;
	  }
	}
    }
  
  return;
}

void
EditTransferFunc2::draw_colormap()
{
  // draw cmap
  if(use_back_buffer_) {
    // rasterize widgets into back buffer
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    for(unsigned int i=0; i<widgets_.size(); i++) {
      if (! widgets_[i]->is_empty()) {
	widgets_[i]->rasterize(*shader_factory_, gui_faux_.get(), 0);
      }
    }
    
    glBlendFunc(GL_ONE, GL_DST_ALPHA);
    // draw histo
    if(histo_) {
      draw_histo();
    } else {
      glColor4f(0.0, 0.0, 0.0, 0.0); 
      glBegin(GL_QUADS);
      {
        glVertex2f( 0.0,  0.0);
        glVertex2f( 1.0,  0.0);
        glVertex2f( 1.0,  1.0);
        glVertex2f( 0.0,  1.0);
      }
      glEnd();
    }
    glDisable(GL_BLEND);
  } else {
    // draw histo
    if(histo_) {
      draw_histo();
    }
    // draw textures
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    if(use_pbuffer_) {
      pbuffer_->bind(GL_FRONT);
    } else {
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, cmap_tex_);
    }
    glBegin(GL_QUADS);
    {
      glTexCoord2f( 0.0,  0.0);
      glVertex2f( 0.0,  0.0);
      glTexCoord2f( 1.0,  0.0);
      glVertex2f( 1.0,  0.0);
      glTexCoord2f( 1.0,  1.0);
      glVertex2f( 1.0,  1.0);
      glTexCoord2f( 0.0,  1.0);
      glVertex2f( 0.0,  1.0);
    }
    glEnd();
    if(use_pbuffer_) {
      if (save_ppm_){
	unsigned int* FrameBuffer = scinew unsigned int[pbuffer_->width()*pbuffer_->height()];
	glFlush();
	glReadBuffer(GL_FRONT);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
	glReadPixels(0, 0,pbuffer_->width(), pbuffer_->height(),
		     GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, FrameBuffer);
	char tmp[256];
	sprintf(tmp, "/tmp/FrameBuffer%d.ppm", 1);
	save_ppm(tmp, pbuffer_->width(), pbuffer_->height(),
		 4, (const unsigned char *)(FrameBuffer));
	cout<<"Done saving framebuffer..."<<tmp<<"\n"; // endl;
	delete FrameBuffer;
	save_ppm_ = false;
      }
	
      pbuffer_->release(GL_FRONT);
    } else {
      glBindTexture(GL_TEXTURE_2D, 0);
      glDisable(GL_TEXTURE_2D);
    }
    glDisable(GL_BLEND);

  }
}

void
EditTransferFunc2::update_histo()
{
  //----------------------------------------------------------------
  // update histo tex
  if(histo_dirty_) {
    if(glIsTexture(histo_tex_)) {
      glDeleteTextures(1, &histo_tex_);
      histo_tex_ = 0;
    }
    glGenTextures(1, &histo_tex_);
    glBindTexture(GL_TEXTURE_2D, histo_tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    int axis_size[3];
    nrrdAxisInfoGet_nva(histo_, nrrdAxisInfoSize, axis_size);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, axis_size[histo_->dim-2],
                 axis_size[histo_->dim-1], 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 histo_->data);
    glBindTexture(GL_TEXTURE_2D, 0);
    histo_dirty_ = false;
  }
}

void
EditTransferFunc2::redraw()
{
  gui->lock();

  if (! make_current()) { gui->unlock(); return; }
  init_pbuffer();
  rasterize_widgets();
  update_histo();
  
  //----------------------------------------------------------------
  // draw
  glDrawBuffer(GL_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);
  
  glViewport(0, 0, width_, height_);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  const float scale_factor = 2.0 * scale_.get();
  glScalef(scale_factor, scale_factor, scale_factor);
  glTranslatef(-0.5 - pan_x_.get(), -0.5 - pan_y_.get() , 0.0);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glDisable(GL_CULL_FACE);

  draw_colormap();
  
  // draw widgets
  for(unsigned int i=0; i<widgets_.size(); i++) {
    if (! widgets_[i]->is_empty()) {
      widgets_[i]->draw();
    }
  }
  
  // draw outline of the image space.
  glColor4f(0.25, 0.35, 0.25, 1.0); 
  glBegin(GL_LINES);
  {
    glVertex2f( 0.0,  0.0);
    glVertex2f( 1.0,  0.0);

    glVertex2f( 1.0,  0.0);
    glVertex2f( 1.0,  1.0);

    glVertex2f( 1.0,  1.0);
    glVertex2f( 0.0,  1.0);

    glVertex2f( 0.0,  1.0);
     glVertex2f( 0.0,  0.0);
  }
  glEnd();
  ctx_->swap();
  ctx_->release();

  gui->unlock();
}


} // end namespace SCIRun
