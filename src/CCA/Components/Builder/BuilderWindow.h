/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2004 Scientific Computing and Imaging Institute,
   University of Utah.

   License for the specific language governing rights and limitations under
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

#ifndef BuilderWindow_h
#define BuilderWindow_h


#include <wx/wxprec.h>
#ifndef WX_PRECOMP
 #include <wx/wx.h>
#endif

#include <Core/CCA/spec/cca_sidl.h>

#include <string>

class wxEvtHandler;
class wxFrame;
class wxSashLayoutWindow;
class wxSashEvent;
class wxTextCtrl;
class wxMenuBar;

namespace GUIBuilder {

class BuilderWindow;
class MiniCanvas;
class NetworkCanvas;

class MenuTree : public wxEvtHandler {
public:
  enum { // user specified ids for widgets, menus
    ID_MENU_COMPONENTS = wxID_HIGHEST,
    ID_MENUTREE_HIGHEST = ID_MENU_COMPONENTS + 1,
  };

  MenuTree(BuilderWindow* builder, const std::string &url);
  virtual ~MenuTree();

  void add(const std::vector<std::string>& name, int nameindex, const sci::cca::ComponentClassDescription::pointer& desc, const std::string& fullname);
  void coalesce();
  void populateMenu(wxMenu* menu);
  void clear();

  void OnInstantiateComponent(wxCommandEvent& event);


private:
  BuilderWindow* builder;
  sci::cca::ComponentClassDescription::pointer cd;
  std::map<std::string, MenuTree*> child;
  std::string url;
  int id;
};

class BuilderWindow : public wxFrame /*, public sci::cca::ports::ComponentEventListener */ {
public:
  //typedef std::map<std::string, int> IntMap;
  typedef std::map<std::string, MenuTree*> MenuMap;

  enum { // user specified ids for widgets, menus
    ID_WINDOW_LEFT = MenuTree::ID_MENUTREE_HIGHEST,
    ID_WINDOW_RIGHT,
    ID_WINDOW_BOTTOM,
    ID_NET_WINDOW,
    ID_MINI_WINDOW,
    ID_TEXT_WINDOW,
    ID_MENU_TEST, // temporary
    ID_MENU_LOAD,
    ID_MENU_INSERT,
    ID_MENU_CLEAR,
    //ID_MENU_EXECALL,
    ID_MENU_COMPONENT_WIZARD,
    ID_MENU_WIZARDS,
    ID_MENU_ADDINFO,
    ID_BUILDERWINDOW_HIGHEST = ID_MENU_ADDINFO + 1,
  };

  BuilderWindow(const sci::cca::BuilderComponent::pointer& bc, wxWindow *parent);
  virtual ~BuilderWindow();

  // two-step creation
  bool Create(wxWindow* parent, wxWindowID id,
              const wxString& title = wxString(wxT("SCIRun2 GUI Builder")),
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxSize(WIDTH, HEIGHT),
              long style = wxDEFAULT_FRAME_STYLE,
              const wxString& name = wxString(wxT("SCIRun2")));

  // set builder only if builder is null
  bool SetBuilder(const sci::cca::BuilderComponent::pointer& bc);

  // Event handlers
  void OnQuit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& event);
  void OnSize(wxSizeEvent& event);
  void OnSashDrag(wxSashEvent& event);
  void OnTest(wxCommandEvent& event);

  void InstantiateComponent(const sci::cca::ComponentClassDescription::pointer& cd);

  void RefreshMiniCanvas();

  static int GetNextID() { return ++IdCounter; }
  static int GetCurrentID() { return IdCounter; }

  static const wxColor BACKGROUND_COLOUR;

protected:
  BuilderWindow() { Init(); }
  // common initialization
  void Init();

  MiniCanvas* miniCanvas;
  wxTextCtrl* textCtrl;
  NetworkCanvas* networkCanvas;

  wxSashLayoutWindow* leftWindow;
  wxSashLayoutWindow* rightWindow;
  wxSashLayoutWindow* bottomWindow;

  wxMenuBar* menuBar;
  wxStatusBar* statusBar;

  MenuMap menus;

private:
  DECLARE_DYNAMIC_CLASS(BuilderWindow)
  // This class handles events
  DECLARE_EVENT_TABLE()

  static const int MIN = 4;
  static const int WIDTH = 1000;
  static const int HEIGHT = 800;
  static const int TOP_HEIGHT = 300;
  static const int BOTTOM_HEIGHT = 500;
  static const int MINI_WIDTH = 350;
  static const int TEXT_WIDTH = 650;
  static int IdCounter;

  sci::cca::BuilderComponent::pointer builder;
  std::string url;

  void buildPackageMenus();
};

}

#endif
