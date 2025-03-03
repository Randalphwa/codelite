//////////////////////////////////////////////////////////////////////
// This file was auto-generated by codelite's wxCrafter Plugin
// wxCrafter project file: clSelectSymbolDialogBase.wxcp
// Do not modify this file by hand!
//////////////////////////////////////////////////////////////////////

#include "clSelectSymbolDialogBase.h"

#include "codelite_exports.h"

// Declare the bitmap loading function
extern void wxCrafterWmuZfdInitBitmapResources();

static bool bBitmapLoaded = false;

clSelectSymbolDialogBase::clSelectSymbolDialogBase(wxWindow* parent, wxWindowID id, const wxString& title,
                                                   const wxPoint& pos, const wxSize& size, long style)
    : wxDialog(parent, id, title, pos, size, style)
{
    if(!bBitmapLoaded) {
        // We need to initialise the default bitmap handler
        wxXmlResource::Get()->AddHandler(new wxBitmapXmlHandler);
        wxCrafterWmuZfdInitBitmapResources();
        bBitmapLoaded = true;
    }

    boxSizer2 = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(boxSizer2);

    boxSizer10 = new wxBoxSizer(wxVERTICAL);

    boxSizer2->Add(boxSizer10, 1, wxALL | wxEXPAND, 5);

    m_dvListCtrl = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(500, 200),
                                          wxDV_VERT_RULES | wxDV_ROW_LINES | wxDV_SINGLE);
    m_dvListCtrl->SetFocus();

    boxSizer10->Add(m_dvListCtrl, 1, wxALL | wxEXPAND, 5);

    m_dvListCtrl->AppendIconTextColumn(_("Name"), wxDATAVIEW_CELL_INERT, 500, wxALIGN_LEFT);
    m_dvListCtrl->AppendTextColumn(_("Ext"), wxDATAVIEW_CELL_INERT, 200, wxALIGN_LEFT);
    m_stdBtnSizer4 = new wxStdDialogButtonSizer();

    boxSizer2->Add(m_stdBtnSizer4, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

    m_buttonOK = new wxButton(this, wxID_OK, wxT(""), wxDefaultPosition, wxSize(-1, -1), 0);
    m_buttonOK->SetDefault();
    m_stdBtnSizer4->AddButton(m_buttonOK);

    m_buttonCancel = new wxButton(this, wxID_CANCEL, wxT(""), wxDefaultPosition, wxSize(-1, -1), 0);
    m_stdBtnSizer4->AddButton(m_buttonCancel);
    m_stdBtnSizer4->Realize();

    SetName(wxT("clSelectSymbolDialogBase"));
    SetSizeHints(-1, -1);
    if(GetSizer()) {
        GetSizer()->Fit(this);
    }
    CentreOnParent(wxBOTH);
#if wxVERSION_NUMBER >= 2900
    if(!wxPersistenceManager::Get().Find(this)) {
        wxPersistenceManager::Get().RegisterAndRestore(this);
    } else {
        wxPersistenceManager::Get().Restore(this);
    }
#endif
    // Connect events
    m_dvListCtrl->Connect(wxEVT_COMMAND_DATAVIEW_ITEM_ACTIVATED,
                          wxDataViewEventHandler(clSelectSymbolDialogBase::OnItemActivated), NULL, this);
    m_buttonOK->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(clSelectSymbolDialogBase::OnOKUI), NULL, this);
}

clSelectSymbolDialogBase::~clSelectSymbolDialogBase()
{
    m_dvListCtrl->Disconnect(wxEVT_COMMAND_DATAVIEW_ITEM_ACTIVATED,
                             wxDataViewEventHandler(clSelectSymbolDialogBase::OnItemActivated), NULL, this);
    m_buttonOK->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(clSelectSymbolDialogBase::OnOKUI), NULL, this);
}
