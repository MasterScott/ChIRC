#include "../ucccccp/ucccccp.hpp"
#include "IRCClient.h"
#include <thread>
#include <atomic>
#include <algorithm>
#include <random>

namespace ChIRC
{
struct IRCData
{
    std::string user;
    std::string nick;
    std::string comms_channel;
    std::string commandandcontrol_channel;
    std::string commandandcontrol_password;
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
    IRCData data;
    IRCClient IRC;

    void IRCThread()
    {
        if (!IRC.InitSocket() ||
            !IRC.Connect(data.address.c_str(), data.port) ||
            !IRC.Login(data.nick, data.user))
        {
            status = joining;
            return;
        }
        statusenum compare = initing;
        if (!status.compare_exchange_strong(compare, running))
            return;
        std::thread joinChannel([=]() {
            std::this_thread::sleep_for(std::chrono_literals::operator""s(1));
            if (this && IRC.Connected())
            {
                sendraw("JOIN " + data.comms_channel);
                if (!data.commandandcontrol_channel.empty())
                {
                    sendraw("JOIN " + data.commandandcontrol_channel + " " + data.commandandcontrol_password);
                    sendraw("MODE " + data.commandandcontrol_channel + " +k " + data.commandandcontrol_password);
                    sendraw("MODE " + data.commandandcontrol_channel + " +s");
                }
            }
        });
        joinChannel.detach();
        while (IRC.Connected() && status == running)
        {
            IRC.ReceiveData();
        }
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
                    std::string commandandcontrol_channel,std::string commandandcontrol_password, std::string address,
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
        data.commandandcontrol_password = commandandcontrol_password;
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
        msg = ucccccp::encrypt(msg, 'B');
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
    void installCallback(std::string cmd, void (*func)(IRCMessage, IRCClient *))
    {
        IRC.HookIRCCommand(cmd, func);
    }
    const IRCData &getData() const
    {
        return data;
    }
    ChIRC()
    {
    }
    ~ChIRC()
    {
        shouldrun = false;
        ChangeState(false);
    }
};
} // namespace ChIRC
