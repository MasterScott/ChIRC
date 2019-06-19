/*
 * Copyright (C) 2011 Fredi Machado <https://github.com/fredimachado>
 * IRCClient is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3.0 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <cstring>
#include <fcntl.h>
#include "IRCSocket.h"

#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXDATASIZE 4096

bool IRCSocket::Init()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        std::cout << "Unable to initialize Winsock." << std::endl;
        return false;
    }
#endif

    if ((_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
    {
        std::cout << "Socket error." << std::endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    int on = 1;
    if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (char const *) &on, sizeof(on)) == -1)
    {
        std::cout << "Invalid socket." << std::endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

#ifdef _WIN32
    u_long mode = 0;
    ioctlsocket(_socket, FIONBIO, &mode);
#else
    fcntl(_socket, F_SETFL, O_NONBLOCK);
    fcntl(_socket, F_SETFL, O_ASYNC);
#endif

    return true;
}

std::vector<std::string> getipfromhostname(std::string host)
{
    struct addrinfo hints, *res;
    int errcode;
    char addrstr[100];
    void *ptr;
    std::vector<std::string> addresses;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;

    errcode = getaddrinfo(host.c_str(), NULL, &hints, &res);
    if (errcode != 0)
        return std::vector<std::string>();
    while (res)
    {
        inet_ntop(res->ai_family, res->ai_addr->sa_data, addrstr, 100);

        switch (res->ai_family)
        {
        case AF_INET:
            ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
            break;
        case AF_INET6:
            ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
            break;
        }
        inet_ntop(res->ai_family, ptr, addrstr, 100);
        addresses.push_back(addrstr);
        res = res->ai_next;
    }
    return addresses;
}

bool IRCSocket::Connect(char const *host, int port)
{
    auto res = getipfromhostname(host);

    if (res.size() == 0)
    {
        std::cout << "IRC: Dns entry not found!" << std::endl;
        return false;
    }
    std::cout << res[0].c_str() << std::endl;

    char buf[4];

    inet_pton(AF_INET, res[0].c_str(), buf);

    sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    addr.sin_addr   = *((const in_addr *) buf);
    memset(&(addr.sin_zero), '\0', 8);

    _connected = true;
    if (connect(_socket, (sockaddr *) &addr, sizeof(addr)) == SOCKET_ERROR)
    {
        std::cout << "Could not connect to: " << host << std::endl;
        closesocket(_socket);
        return false;
    }

    return true;
}

void IRCSocket::Disconnect()
{
    if (_connected)
    {
#ifdef _WIN32
        shutdown(_socket, 2);
#endif
        closesocket(_socket);
        _connected = false;
    }
}

bool IRCSocket::SendData(char const *data)
{
    if (_connected)
        if (send(_socket, data, strlen(data), 0) == -1)
            return false;

    return true;
}

std::string IRCSocket::ReceiveData()
{
    char buffer[MAXDATASIZE];

    memset(buffer, 0, MAXDATASIZE);

    int bytes = recv(_socket, buffer, MAXDATASIZE - 1, 0);

    if (bytes > 0)
        return std::string(buffer);
    else
        Disconnect();

    return "";
}
