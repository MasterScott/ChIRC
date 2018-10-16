#include "ChIRC.hpp"
#include <algorithm>
#include <random>
#include "../ucccccp/ucccccp.hpp"
#include "timer.hpp"

void ChIRC::ChIRC::basicHandler(IRCMessage msg, IRCClient *irc, void *ptr)
{
    ChIRC *this_ChIRC = static_cast<ChIRC *>(ptr);
    if (!this_ChIRC)
        return;
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
        if (channel == this_ChIRC->data.commandandcontrol_channel &&
            this_ChIRC->data.is_commandandcontrol)
        {
            if (rawmsg.find("cc_signon") == 0)
            {
                bool reply = false;
                if (rawmsg.find("cc_signonrep") == 0)
                    reply = true;

                size_t id_loc     = rawmsg.find("$");
                size_t is_bot_loc = rawmsg.find('$', id_loc + 1);

                int id;
                try
                {
                    std::string string =
                        rawmsg.substr(id_loc + 1, is_bot_loc - id_loc);
                    id = std::stoi(string);
                }
                catch (std::invalid_argument)
                {
                    return;
                }
                bool is_bot;
                try
                {
                    std::string string = rawmsg.substr(is_bot_loc + 1);
                    is_bot             = std::stoi(string);
                }
                catch (std::invalid_argument)
                {
                    return;
                }

                if (id == this_ChIRC->data.id)
                {
                    this_ChIRC->IRC.Disconnect();
                    this_ChIRC->updateID();
                    return;
                }

                if (!reply)
                    this_ChIRC->sendSignon(true);

                PeerData peer{};
                peer.heartbeat = std::chrono::system_clock::now();
                peer.nickname  = msg.prefix.nick;
                peer.is_bot    = is_bot;
                std::lock_guard<std::mutex> lock(this_ChIRC->peers_lock);
                this_ChIRC->peers[id] = peer;
            }
            else if (rawmsg.find("cc_heartbeat") == 0)
            {
                std::cout << "Heartbeat: " << rawmsg << std::endl;
                size_t party_size = rawmsg.find('$');

                if (party_size == rawmsg.npos)
                    return;
                // size_t next = rawmsg.find('$', party_size + 1);

                int id;
                try
                {
                    std::string string_id = rawmsg.substr(12, party_size - 12);
                    id                    = std::stoi(string_id);
                }
                catch (std::invalid_argument)
                {
                    return;
                }
                int size;
                try
                {
                    size = std::stoi(rawmsg.substr(party_size + 1));
                }
                catch (std::invalid_argument)
                {
                    return;
                }
                if (id == this_ChIRC->data.id)
                {
                    this_ChIRC->IRC.Disconnect();
                    this_ChIRC->updateID();
                    // Will auto restart
                    return;
                }

                if (this_ChIRC->peers.find(id) != this_ChIRC->peers.end())
                {
                    std::lock_guard<std::mutex> lock(this_ChIRC->peers_lock);
                    this_ChIRC->peers[id].heartbeat =
                        std::chrono::system_clock::now();
                    this_ChIRC->peers[id].party_size = size;
                }
                else
                    std::cout << "Heartbeat from unknown peer recieved: " << id
                              << std::endl;
            }
        }
    }
}

void ChIRC::ChIRC::IRCThread()
{
    if (!IRC.InitSocket() || !IRC.Connect(data.address.c_str(), data.port) ||
        !IRC.Login(data.nick + '-' + std::to_string(data.id), data.user))
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
                sendraw("JOIN " + data.commandandcontrol_channel + " " +
                        data.commandandcontrol_password);
                sendraw("MODE " + data.commandandcontrol_channel + " +k " +
                        data.commandandcontrol_password);
                sendraw("MODE " + data.commandandcontrol_channel + " +s");
                this->sendSignon(false);
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
void ChIRC::ChIRC::UpdateData(std::string user, std::string nick,
                              std::string comms_channel,
                              std::string commandandcontrol_channel,
                              std::string commandandcontrol_password,
                              std::string address, int port, bool is_bot)
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
        return sendraw("PRIVMSG " + data.commandandcontrol_channel + " :" +
                       msg);
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
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now() - i.second.heartbeat)
                .count() >= 10)
        {
            todelete = i.first;
        }
    }
    if (todelete != -1)
    {
        peers.erase(todelete);
        std::cout << "Timed out peer " << todelete << std::endl;
    }
}
