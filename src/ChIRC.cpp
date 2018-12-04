#include "ChIRC.hpp"
#include <algorithm>
#include <random>
#include "../ucccccp/ucccccp.hpp"
#include "timer.hpp"

constexpr std::string_view heartbeat = "cc_hb";
constexpr std::string_view reqauth   = "cc_reqauth";
constexpr std::string_view auth      = "cc_auth";

void ChIRC::ChIRC::basicHandler(IRCMessage msg, IRCClient *irc, void *context)
{
    ChIRC *this_ChIRC = static_cast<ChIRC *>(context);
    if (!this_ChIRC)
        return;
    if (msg.parameters.empty())
        return;
    try
    {
        msg.parameters.at(0);
        msg.parameters.at(1);
    }
    catch (std::out_of_range)
    {
        return;
    }
    auto &callbacks = this_ChIRC->callbacks;
    for (auto &i : callbacks)
    {
        if (msg.command == i.first)
        {
            i.second(msg, irc);
        }
    }
    if (msg.command == "PRIVMSG")
    {
        std::string rawmsg   = msg.parameters.at(1);
        std::string &channel = msg.parameters.at(0);
        if (!ucccccp::validate(rawmsg))
            return;
        rawmsg = ucccccp::decrypt(rawmsg);
        if (channel == this_ChIRC->data.commandandcontrol_channel && this_ChIRC->data.is_commandandcontrol)
        {
            if (rawmsg.find(heartbeat.data()) == 0)
            {
                int id                = 0;
                int party_size        = 0;
                size_t id_loc         = rawmsg.find('$') + 1;
                size_t party_size_loc = rawmsg.find('$', id_loc) + 1;

                try
                {
                    id         = std::stoi(rawmsg.substr(id_loc, party_size_loc - id_loc));
                    party_size = std::stoi(rawmsg.substr(party_size_loc));
                }
                catch (std::invalid_argument)
                {
                    std::cout << "ChIRC: Recieved invalid heartbeat" << std::endl;
                    return;
                }

                std::lock_guard<std::mutex> lock(this_ChIRC->peers_lock);
                if (this_ChIRC->peers.find(id) == this_ChIRC->peers.end())
                {
                    // Not found in peers. Ask for auth.
                    this_ChIRC->privmsg(std::string(reqauth) + '$' + std::to_string(id), true);
                }
                else
                {
                    // Found in peers. Update peer.
                    auto &peer      = this_ChIRC->peers[id];
                    peer.heartbeat  = std::chrono::system_clock::now();
                    peer.party_size = party_size;
                }
            }
            else if (rawmsg.find(auth.data()) == 0)
            {
                int id            = 0;
                bool is_bot       = false;
                size_t id_loc     = rawmsg.find('$') + 1;
                size_t is_bot_loc = rawmsg.find('$', id_loc) + 1;

                try
                {
                    id     = std::stoi(rawmsg.substr(id_loc, is_bot_loc - id_loc));
                    is_bot = std::stoi(rawmsg.substr(is_bot_loc));
                }
                catch (std::invalid_argument)
                {
                    std::cout << "ChIRC: Recieved invalid auth" << std::endl;
                    return;
                }
                std::lock_guard<std::mutex> lock(this_ChIRC->peers_lock);
                PeerData peer         = {};
                peer.heartbeat        = std::chrono::system_clock::now();
                peer.is_bot           = is_bot;
                peer.nickname         = msg.prefix.nick;
                this_ChIRC->peers[id] = std::move(peer);
            }
            else if (rawmsg.find(reqauth.data()) == 0)
            {
                int id        = 0;
                size_t id_loc = rawmsg.find('$') + 1;
                try
                {
                    id = std::stoi(rawmsg.substr(id_loc));
                }
                catch (std::invalid_argument)
                {
                    std::cout << "ChIRC: Recieved invalid reqauth" << std::endl;
                    return;
                }
                static Timer last_req_auth{};
                if (last_req_auth.test_and_set(1000) && id == this_ChIRC->data.id)
                {
                    this_ChIRC->sendAuth();
                }
            }
        }
    }
}

void ChIRC::ChIRC::sendHeartbeat()
{
    std::string output = std::string(heartbeat) + '$' + std::to_string(data.id) + "$" + std::to_string(game_state.load().party_size);
    privmsg(output, true);
}

void ChIRC::ChIRC::sendAuth()
{
    std::string output = std::string(auth) + '$' + std::to_string(data.id) + '$' + std::to_string(data.is_bot);
    privmsg(output, true);
}

void ChIRC::ChIRC::IRCThread()
{
    if (!IRC.InitSocket() || !IRC.Connect(data.address.c_str(), data.port) || !IRC.Login(data.nick + '-' + std::to_string(data.id), data.user))
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
                data.is_commandandcontrol = true;
                sendraw("JOIN " + data.commandandcontrol_channel + " " + data.commandandcontrol_password);
                sendraw("MODE " + data.commandandcontrol_channel + " +k " + data.commandandcontrol_password);
                sendraw("MODE " + data.commandandcontrol_channel + " +s");
                sendraw("MODE " + data.commandandcontrol_channel + " +n");
            }
            else
                data.is_commandandcontrol = false;
        }
    });
    joinChannel.detach();
    while (IRC.Connected() && status == running)
    {
        IRC.ReceiveData();
    }
    status.store(joining);
}

void ChIRC::ChIRC::ChangeState(bool state)
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

void ChIRC::ChIRC::updateID()
{
    std::random_device rd;
    std::mt19937 e{ rd() };
    std::uniform_int_distribution<int> dist{ 1, 10000 };
    data.id = dist(e);
}
void ChIRC::ChIRC::UpdateData(std::string user, std::string nick, std::string comms_channel, std::string commandandcontrol_channel, std::string commandandcontrol_password, std::string address, int port, bool is_bot)
{
    updateID();

    // Fix spaces
    std::replace(nick.begin(), nick.end(), ' ', '_');
    std::replace(nick.begin(), nick.end(), ' ', '_');

    data.user                       = user;
    data.nick                       = nick;
    data.comms_channel              = comms_channel;
    data.commandandcontrol_channel  = commandandcontrol_channel;
    data.commandandcontrol_password = commandandcontrol_password;
    data.address                    = address;
    data.port                       = port;
    data.is_bot                     = is_bot;
}

bool ChIRC::ChIRC::sendraw(std::string msg)
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
bool ChIRC::ChIRC::privmsg(std::string msg, bool command)
{
    msg = ucccccp::encrypt(msg, 'B');
    if (command)
        return sendraw("PRIVMSG " + data.commandandcontrol_channel + " :" + msg);
    else
        return sendraw("PRIVMSG " + data.comms_channel + " :" + msg);
}
void ChIRC::ChIRC::Update()
{
    if (status == joining)
    {
        IRC.Disconnect();
        thread.join();
        status = off;
    }
    if (shouldrun && status == off)
    {
        updateID();
        ChangeState(true);
    }
    else if (!shouldrun && status == running)
    {
        ChangeState(false);
    }
    static Timer heartbeat{};
    if (data.is_commandandcontrol && heartbeat.test_and_set(5000))
        sendHeartbeat();

    int todelete = -1;
    for (auto &i : peers)
    {
        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - i.second.heartbeat).count() >= 10)
        {
            todelete = i.first;
        }
    }
    if (todelete != -1)
    {
        std::lock_guard<std::mutex> lock(peers_lock);
        peers.erase(todelete);
        std::cout << "ChIRC: Timed out peer " << todelete << std::endl;
    }
}
