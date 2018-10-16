#include "IRCClient.h"
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>

namespace ChIRC
{
// Used for storing IRC Client data, should never be modifed while irc thread
// running
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
    int id{};
    bool is_bot = 0;
};

struct GameState
{
    int party_size = -1;
};

// Used for storing data of C&C clients
struct PeerData
{
    std::chrono::time_point<std::chrono::system_clock> heartbeat{};
    std::string nickname;
    bool is_bot    = false;
    int party_size = -1;
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
    // Thread for IRC socket etc etc
    std::thread thread;
    // Status of IRC thread
    std::atomic<statusenum> status{ off };
    // If IRC is supposed to run, used for autorestart
    bool shouldrun{ false };
    // Contains core irc data, should'nt be modified while main thread is
    // running
    IRCData data;
    // IRC client itself
    IRCClient IRC;
    // Unordered map containing peers
    std::unordered_map<int, PeerData> peers;
    std::mutex peers_lock;
    // Contains backwards compatible callbacks
    std::vector<
        std::pair<std::string, std::function<void(IRCMessage, IRCClient *)>>>
        callbacks;
    // Contains game data that might change at any moment. Thread safe.
    GameState game_state;
    std::mutex game_state_lock;

    void IRCThread();
    void ChangeState(bool state);
    static void basicHandler(IRCMessage msg, IRCClient *irc, void *ptr);
    void updateID();
    void sendHeartbeat()
    {
        if (!data.is_commandandcontrol)
            return;
        std::string msg = "cc_heartbeat";
        msg += std::to_string(data.id);
        msg += '$' + std::to_string(game_state.party_size);
        privmsg(msg, true);
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
                    std::string commandandcontrol_channel,
                    std::string commandandcontrol_password, std::string address,
                    int port, bool is_bot = false);
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
        msg += '$' + std::to_string(data.id);
        msg += '$' + std::to_string(data.is_bot);
        privmsg(msg, true);
    }
    void setState(GameState &state)
    {
        std::lock_guard<std::mutex> lock(game_state_lock);
        game_state = state;
    }
    GameState getState()
    {
        std::lock_guard<std::mutex> lock(game_state_lock);
        return game_state;
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
        std::lock_guard<std::mutex> lock(peers_lock);
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
