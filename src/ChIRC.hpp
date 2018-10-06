#include "ucccccp.hpp"
#include "IRCClient.h"
#include <thread>
#include <atomic>
#include <algorithm>
#include <random>
// temp
#include "core/logging.hpp"

namespace ChIRC
{
struct IRCData
{
    std::string user;
    std::string nick;
    std::string comms_channel;
    std::string commandandcontrol_channel;
    std::string address;
    int port{};
};

enum statusenum
{
    off = 0,
    // IRC is initing and not ready to take commands
    initing,
    // IRC is running and ready to take commands
    running,
    // ChIRC ordered IRC to stop
    stopping,
    // IRC has canceled itself and is waiting to be joined
    joining
};

class ChIRC
{
    std::thread thread;
    std::atomic<statusenum> status{ off };
    bool shouldrun{ false };

    void IRCThread()
    {
        logging::Info("Thread");
        if (!IRC.InitSocket() ||
            !IRC.Connect(data.address.c_str(), data.port) ||
            !IRC.Login(data.nick, data.user))
        {
            logging::Info(
                "Failed to connect, user %s, nick %s, address %s, port %i",
                data.user.c_str(), data.nick.c_str(), data.address.c_str(),
                data.port);
            status = joining;
            return;
        }
        IRC.ReceiveData();
        sendraw("JOIN " + data.comms_channel);
        statusenum compare = initing;
        if (!status.compare_exchange_strong(compare, running))
            return;
        logging::Info("Ready, %s %s %s %s", data.user.c_str(),
                      data.nick.c_str(), data.comms_channel.c_str(),
                      data.address.c_str());
        while (IRC.Connected() && status == running)
            IRC.ReceiveData();
        status.store(joining);
    }
    void ChangeState(bool state)
    {
        if (state)
        {
            if (status == off)
            {
                status = initing;
                thread = std::thread(&ChIRC::IRCThread, this);
            }
        }
        else
        {
            status = stopping;
            IRC.Disconnect();
            if (thread.joinable())
                thread.join();
            status = off;
        }
    }

public:
    IRCClient IRC;
    IRCData data;
    void Disconnect()
    {
        shouldrun = false;
        ChangeState(false);
    }
    void Connect()
    {
        shouldrun = true;
        ChangeState(true);
    }
    void UpdateData(std::string user, std::string nick,
                    std::string comms_channel,
                    std::string commandandcontrol_channel, std::string address,
                    int port)
    {
        std::random_device rd;
        std::mt19937 e{ rd() };
        std::uniform_int_distribution<int> dist{ 1, 10000 };
        // Prevent dupes
        nick.append("-" + std::to_string(dist(e)));

        // Fix spaces
        std::replace(nick.begin(), nick.end(), ' ', '_');
        std::replace(nick.begin(), nick.end(), ' ', '_');

        data.user                      = user;
        data.nick                      = nick;
        data.comms_channel             = comms_channel;
        data.commandandcontrol_channel = commandandcontrol_channel;
        data.address                   = address;
        data.port                      = port;
    }
    bool sendraw(std::string msg)
    {
        if (msg.empty())
            return false;
        if (status.load() == running)
        {
            if (IRC.SendIRC(msg))
                return true;
        }
        return false;
    }
    bool privmsg(std::string msg, bool command = false)
    {
        if (command)
            return sendraw("PRIVMSG " + data.commandandcontrol_channel + " :" +
                           msg);
        else
            return sendraw("PRIVMSG " + data.comms_channel + " :" + msg);
    }
    void Update()
    {
        if (status == joining)
        {
            IRC.Disconnect();
            thread.join();
            status = off;
        }
        if (shouldrun && status == off)
            ChangeState(true);
        else if (!shouldrun && status == running)
        {
            ChangeState(false);
        }
    }
    ChIRC()
    {
        //IRC.HookIRCCommand("PRIVMSG", func);
    }
    ~ChIRC()
    {
        shouldrun = false;
        ChangeState(false);
    }
};
} // namespace ChIRC