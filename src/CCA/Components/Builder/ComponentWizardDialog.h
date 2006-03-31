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

#ifndef ComponentWizardDialog_h
#define ComponentWizardDialog_h

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <CCA/Components/Builder/ComponentSkeletonWriter.h>
#include <vector>

class wxWindow;
class wxButton;
class wxStaticText;
class wxTextCtrl;

namespace GUIBuilder {

class AddPortDialog;

class ComponentWizardDialog: public wxDialog
{
public:
  enum {
    ID_AddProvidesPort=1,
    ID_AddUsesPort,
  };

  ComponentWizardDialog(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE, const wxString& name = "ComponentWizardDialogBox");

  ~ComponentWizardDialog()
    {
      for(int i=0;i<pp.size();i++)
	delete pp[i];
      for(int i=0;i<up.size();i++)
	delete up[i];
    }

  void OnOk( wxCommandEvent &event );
  void OnAddProvidesPort( wxCommandEvent &event );
  void OnAddUsesPort( wxCommandEvent &event );

  virtual bool Validate();

private:
  wxTextCtrl  *componentName;
  wxStaticText *lcomponentName;
  wxButton *AddProvidesPort;
  wxButton *AddUsesPort;
  wxString GetText();

  std::vector <PortDescriptor*> pp;
  std::vector <PortDescriptor*> up;

  DECLARE_EVENT_TABLE()

};

class AddPortDialog: public wxDialog
{
public:
  AddPortDialog(wxWindow *parent,wxWindowID id,const wxString &title,const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE, const wxString& name = "Port dialog box");

  std::string GetPortNameText() const;
  std::string GetDataTypeText() const;

private:
  wxTextCtrl  *pname;
  wxTextCtrl  *dtype;
  wxStaticText *lname;
  wxStaticText *ldtype;
  wxButton *ok;
  wxButton *cancel;

  // DECLARE_EVENT_TABLE()
};


}
#endif
