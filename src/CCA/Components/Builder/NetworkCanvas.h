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


#ifndef NetworkCanvas_h
#define NetworkCanvas_h

#include <Core/CCA/spec/cca_sidl.h>

#include <map>
#include <vector>
#include <string>

class wxScrolledWindow;
class wxMenu;
class wxMenuItem;

namespace GUIBuilder {

class BuilderWindow;
class PortIcon;
class ComponentIcon;
class Connection;
class MiniCanvas;

class NetworkCanvas : public wxScrolledWindow {
public:
  typedef std::map<std::string, ComponentIcon*> ComponentMap;
  typedef std::multimap<PortIcon*, Connection*> ConnectionMap;

  friend class MiniCanvas;

  // identifiers are local to each wxWindow, so duplication between different windows is OK
  enum {
    ID_MENU_CLEAR = wxID_HIGHEST,
    ID_MENU_DISCONNECT,
    ID_MENU_COMPONENTS,
  };

  NetworkCanvas(const sci::cca::BuilderComponent::pointer& bc, BuilderWindow* bw, wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size);
  virtual ~NetworkCanvas();

  bool Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style = wxHSCROLL|wxVSCROLL|wxSUNKEN_BORDER|wxRETAINED /*|wxCLIP_CHILDREN*/);

  virtual void OnDraw(wxDC& dc);

  void OnPaint(wxPaintEvent& event);
  void PaintBackground(wxDC& dc);

  // Empty implementation, to prevent flicker - from wxWidgets book
  // Handle background paint in paint event handler (on GTK+ might also
  // want to use background style wxBG_STYLE_CUSTOM).
  void OnEraseBackground(wxEraseEvent& event) {}
  void OnLeftDown(wxMouseEvent& event);
  void OnLeftUp(wxMouseEvent& event);
  void OnMouseMove(wxMouseEvent& event);
  void OnRightClick(wxMouseEvent& event);
  void OnScroll(wxScrollWinEvent& event);
  void OnClear(wxCommandEvent& event);

  //void DrawIcons(wxDC& dc);
  void DrawConnections(wxDC& dc);

  void OnConnect(PortIcon* pUsed);
  void OnDisconnect(Connection* connection);
  bool ShowPossibleConnections(PortIcon* usesPort);
  void HighlightConnection(const wxPoint& point);

  void Clear();
  void ClearPossibleConnections();
  void ClearConnections();

  void SetMovingIcon(ComponentIcon* ci) { movingIcon = ci; }
  void AddIcon(sci::cca::ComponentID::pointer& compID);

  void GetUnscrolledPosition(const wxPoint& p, wxPoint& position);
  void GetUnscrolledMousePosition(wxPoint& position);
  void GetScrolledPosition(const wxPoint& p, wxPoint& position);

  ComponentIcon* FindIconAtPointer(wxPoint& position);
  PortIcon* FindPortIconAtPointer(wxPoint& position);

  void GetComponentRects(std::vector<wxRect>& rv);
  void GetConnections(std::vector<Connection*>& cv);

//   const ComponentMap& getComponentIcons() const { return components; }
//   const std::vector<Connection*>& getConnections() const { return connections; }

protected:
  NetworkCanvas();
  void Init();
  void SetMenus();

  // Only call this from a paint event handler or event handler helper function!
  wxRect GetClientRect();

  // canvas menu
  wxMenu *popupMenu;
  wxMenu* componentMenu;
  wxMenuItem* clearMenuItem;

  // connection menu
  wxMenu *connectionPopupMenu;
  wxMenuItem* deleteConnection;

private:
  sci::cca::BuilderComponent::pointer builder;
  ComponentMap components;
  ConnectionMap connections;
  ConnectionMap possibleConnections;
//   std::vector<Connection*> connections;
//   std::vector<Connection*> possibleConnections;

  // default scroll window virtual size, scroll rates
  const static int DEFAULT_VWIDTH = 2500;
  const static int DEFAULT_VHEIGHT = 2000;
  const static int DEFAULT_SCROLLX = 10;
  const static int DEFAULT_SCROLLY = 10;

  BuilderWindow* builderWindow;
  ComponentIcon* movingIcon;
//   ComponentIcon* connectingIcon;
//   wxPoint movingStart;
//   Connection* highlightedConnection;

  wxCursor *handCursor;
  wxCursor *arrowCursor;

  DECLARE_EVENT_TABLE()
  DECLARE_DYNAMIC_CLASS(NetworkCanvas)
};

}

#endif
