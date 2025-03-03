#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include "macros.h"

#include <vector>
#include <wx/arrstr.h>
#include <wx/filename.h>

using namespace std;
class CTagsdSettings
{
    wxString m_file_mask = "*.cpp;*.h;*.hpp;*.cxx;*.cc;*.hxx";
    wxArrayString m_search_path;
    vector<pair<wxString, wxString>> m_tokens;
    vector<pair<wxString, wxString>> m_types;
    wxString m_codelite_indexer;
    wxString m_ignore_spec = ".git/;.svn/;/build/;/build-;CPack_Packages/;CMakeFiles/";
    size_t m_limit_results = 250;

private:
    void build_search_path(const wxFileName& filepath);
    void CreateDefault(const wxFileName& filepath);

public:
    CTagsdSettings();
    ~CTagsdSettings();

    void Load(const wxFileName& filepath);
    void Save(const wxFileName& filepath);

    void SetLimitResults(size_t limit_results) { this->m_limit_results = limit_results; }
    size_t GetLimitResults() const { return m_limit_results; }
    void SetCodeliteIndexer(const wxString& codelite_indexer) { this->m_codelite_indexer = codelite_indexer; }
    void SetFileMask(const wxString& file_mask) { this->m_file_mask = file_mask; }
    void SetIgnoreSpec(const wxString& ignore_spec) { this->m_ignore_spec = ignore_spec; }
    void SetSearchPath(const wxArrayString& search_path) { this->m_search_path = search_path; }
    void SetTokens(const vector<pair<wxString, wxString>>& tokens) { this->m_tokens = tokens; }
    const wxString& GetCodeliteIndexer() const { return m_codelite_indexer; }
    const wxString& GetFileMask() const { return m_file_mask; }
    const wxString& GetIgnoreSpec() const { return m_ignore_spec; }
    const wxArrayString& GetSearchPath() const { return m_search_path; }
    const vector<pair<wxString, wxString>>& GetTokens() const { return m_tokens; }
    void SetTypes(const vector<pair<wxString, wxString>>& types) { this->m_types = types; }
    const vector<pair<wxString, wxString>>& GetTypes() const { return m_types; }
};

#endif // SETTINGS_HPP
