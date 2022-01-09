#ifndef CLSOCKETCLIENT_H
#define CLSOCKETCLIENT_H

// [Randalph - 01-08-2022] removed the wxcLib/ prefix since they are in the same directory
#include "clSocketBase.h"
#include <wx/string.h>

class clSocketClient : public clSocketBase
{
    wxString m_path;

public:
    clSocketClient();
    virtual ~clSocketClient();

    /**
     * @brief connect to a remote socket, using unix-domain socket
     */
    bool ConnectLocal(const wxString &socketPath);
    /**
     * @brief connect to a remote server using ip/port
     */
    bool ConnectRemote(const wxString &address, int port);
    /**
     * @brief for wx2.8 ...
     */
    bool ConnectRemote(const std::string &address, int port);
};

#endif // CLSOCKETCLIENT_H
