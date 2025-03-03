#include "clTempFile.hpp"
#include "cl_standard_paths.h"

clTempFile::clTempFile(const wxString& ext)
{
    m_filename = FileUtils::CreateTempFileName(clStandardPaths::Get().GetTempDir(), "cltmp", ext);
}

clTempFile::~clTempFile()
{
    if(m_deleteOnDestruct) {
        FileUtils::RemoveFile(m_filename);
    }
}

bool clTempFile::Write(const wxString& content, wxMBConv& conv)
{
    return FileUtils::WriteFileContent(GetFileName(), content, conv);
}

wxString clTempFile::GetFullPath(bool wrapped_with_quotes) const
{
    wxString fullpath = GetFileName().GetFullPath();
    if(fullpath.Contains(" ")) {
        fullpath.Prepend("\"").Append("\"");
    }
    return fullpath;
}

void clTempFile::Persist() { m_deleteOnDestruct = false; }
