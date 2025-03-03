#include "ColoursAndFontsManager.h"
#include "ThemeImporterASM.hpp"
#include "ThemeImporterBatch.hpp"
#include "ThemeImporterCMake.hpp"
#include "ThemeImporterCSS.hpp"
#include "ThemeImporterCXX.hpp"
#include "ThemeImporterCobra.hpp"
#include "ThemeImporterCobraAlt.hpp"
#include "ThemeImporterDiff.hpp"
#include "ThemeImporterDockerfile.hpp"
#include "ThemeImporterFortran.hpp"
#include "ThemeImporterINI.hpp"
#include "ThemeImporterInnoSetup.hpp"
#include "ThemeImporterJava.hpp"
#include "ThemeImporterJavaScript.hpp"
#include "ThemeImporterLUA.hpp"
#include "ThemeImporterMakefile.hpp"
#include "ThemeImporterPHP.hpp"
#include "ThemeImporterPython.hpp"
#include "wx/versioninfo.h"
#if wxCHECK_VERSION(3, 1, 0)
#include "ThemeImporterJSON.hpp"
#include "ThemeImporterRust.hpp"
#endif
#include "ThemeImporterManager.hpp"
#include "ThemeImporterRuby.hpp"
#include "ThemeImporterSCSS.hpp"
#include "ThemeImporterSQL.hpp"
#include "ThemeImporterScript.hpp"
#include "ThemeImporterText.hpp"
#include "ThemeImporterXML.hpp"
#include "ThemeImporterYAML.hpp"

ThemeImporterManager::ThemeImporterManager()
{
    m_importers.push_back(new ThemeImporterCXX());
    m_importers.push_back(new ThemeImporterCMake());
    m_importers.push_back(new ThemeImporterText());
    m_importers.push_back(new ThemeImporterMakefile());
    m_importers.push_back(new ThemeImporterDiff());
    m_importers.push_back(new ThemeImporterPHP());
    m_importers.push_back(new ThemeImporterCSS());
    m_importers.push_back(new ThemeImporterXML());
    m_importers.push_back(new ThemeImporterJavaScript());
    m_importers.push_back(new ThemeImporterINI());
    m_importers.push_back(new ThemeImporterASM());
    m_importers.push_back(new ThemeImporterBatch());
    m_importers.push_back(new ThemeImporterPython());
    m_importers.push_back(new ThemeImporterCobra());
    m_importers.push_back(new ThemeImporterCobraAlt());
    m_importers.push_back(new ThemeImporterFortran());
    m_importers.push_back(new ThemeImporterInnoSetup());
    m_importers.push_back(new ThemeImporterJava());
    m_importers.push_back(new ThemeImporterLua());
    m_importers.push_back(new ThemeImporterScript());
    m_importers.push_back(new ThemeImporterSQL());
    m_importers.push_back(new ThemeImporterSCSS());
    m_importers.push_back(new ThemeImporterDockerfile());
    m_importers.push_back(new ThemeImporterYAML());
    m_importers.push_back(new ThemeImporterRuby());
#if wxCHECK_VERSION(3, 1, 0)
    m_importers.push_back(new ThemeImporterRust());
    m_importers.push_back(new ThemeImporterJson());
#endif
}

ThemeImporterManager::~ThemeImporterManager() {}

wxString ThemeImporterManager::Import(const wxString& theme_file)
{
    wxString name;
    ThemeImporterBase::List_t::iterator iter = m_importers.begin();
    for(; iter != m_importers.end(); ++iter) {
        auto lexer = (*iter)->Import(theme_file);
        if(name.empty()) {
            name = lexer->GetThemeName();
        }
        ColoursAndFontsManager::Get().AddLexer(lexer);
    }
    return name;
}
