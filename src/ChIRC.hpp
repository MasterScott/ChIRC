#include "IRCClient.h"
#include <thread>
#include <atomic>
#include <unordered_map>

namespace ChIRC
{
// Used for storing IRC Client data
struct IRCData
{
    std::string user;
    std::string nick;
    std::string comms_channel;
    std::string commandandcontrol_channel;
    std::string commandandcontrol_password;
    std::string address;
    int port{};
    bool is_commandandcontrol{ false };
    bool is_partying{ false };
    int id{};
};
// Used for storing data of C&C clients
struct PeerData
{
    // unsigned int steamid;
    std::chrono::time_point<std::chrono::system_clock> heartbeat{};
    std::string nickname;
    bool can_party{ false };
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
    std::unordered_map<int, PeerData> peers;
    std::vector<
        std::pair<std::string, std::function<void(IRCMessage, IRCClient *)>>>
        callbacks;

    void IRCThread();
    void ChangeState(bool state);
    static void basicHandler(IRCMessage msg, IRCClient *irc, void *ptr);
    void updateID();

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
                    std::string commandandcontrol_channel,
                    std::string commandandcontrol_password, std::string address,
                    int port);
    void UpdateState(bool partying);
    bool sendraw(std::string msg);
    bool privmsg(std::string msg, bool command = false);
    void sendSignon(bool reply)
    {
        if (!data.is_commandandcontrol)
            return;
        std::string msg;
        if (reply)
            msg = "cc_signonrep";
        else
            msg = "cc_signon";
        msg += "$id";
        msg += std::to_string(data.id);
        privmsg(msg, true);
    }
    void sendHeartbeat()
    {
        if (!data.is_commandandcontrol)
            return;
        std::string msg = "cc_heartbeat";
        msg += std::to_string(data.id);
        msg += "-";
        msg += std::to_string(data.is_partying);
        privmsg(msg, true);
    }

    void Update();

    void installCallback(std::string cmd,
                         std::function<void(IRCMessage, IRCClient *)> func)
    {
        callbacks.emplace_back(cmd, func);
    }
    const IRCData &getData() const
    {
        return data;
    }
    const std::unordered_map<int, PeerData> getPeers()
    {
        return peers;
    }
    ChIRC()
    {
        IRC.HookIRCCommand("PRIVMSG", this, basicHandler);
    }
    ~ChIRC()
    {
        shouldrun = false;
        ChangeState(false);
    }
};
} // namespace ChIRC
