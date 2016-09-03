#pragma once

#include <curlpp/Easy.hpp>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Types.hpp>
#include <curlpp/Form.hpp>

#include "transport/ThreadPool.h"

#include "json.hpp"

#include "Swiften/Swiften.h"

#include "transport/Config.h"
#include "transport/NetworkPlugin.h"

using json = nlohmann::json;


class Sinfar : public Transport::NetworkPlugin {
	public:
		Sinfar(Transport::Config *config, Swift::SimpleEventLoop *loop, const std::string &host, int port);
		virtual ~Sinfar();

		// NetworkPlugin uses this method to send the data to networkplugin server
		void sendData(const std::string &string);

		void handleLoginRequest(const std::string &user, const std::string &legacyName, const std::string &password);

		void handleLogoutRequest(const std::string &user, const std::string &legacyName);

        void handleVCardRequest(const std::string &user, const std::string &legacyName, unsigned int id);

		void handleMessageSendRequest(const std::string &user, const std::string &legacyName, const std::string &message, const std::string &xhtml = "", const std::string &id = "");

		void handleBuddyUpdatedRequest(const std::string &user, const std::string &buddyName, const std::string &alias, const std::vector<std::string> &groups);

		void handleBuddyUpdatedRequest(const std::string &user, const std::string &buddyName);

		void handleBuddyRemovedRequest(const std::string &user, const std::string &buddyName, const std::vector<std::string> &groups);

		void LoginPlayer(const std::string &buddyName);

		void handleRawXML(const std::string &xml);

		void LogoutPlayer(const std::string &buddyName);

        void handleBuddies(const pbnetwork::Buddies &);

        void addRoomList(const std::string &user);
		
		void UpdatePlayer();

        void PollMessage(const std::string &user);

        void PollMessageCallBack(const std::string &user,const std::string & messages);

        void HandleTell(const std::string &user,json payload);

        void HandleRoom(const std::string &user,json payload);

        void handleJoinRoomRequest(const std::string &user, const std::string &room, const std::string &nickname, const std::string &pasword);

        void handleLeaveRoomRequest(const std::string &user, const std::string &room); 

        struct BuddyData
        {
            std::string alias;
            std::vector<std::string> groups;
        };

        struct Room
        {
            std::string room;
            std::string prefix;
        };

	private:
		// This method has to call handleDataRead with all received data from network plugin server
		void _handleDataRead(boost::shared_ptr<Swift::SafeByteArray> data);

	private:
		struct UserData
		{   
            Swift::Timer::ref m_message_timer;
            std::string m_legacy;
			std::map<std::string,BuddyData> m_buddy_list;
			std::set<std::string> m_room_list;
		};
        Transport::ThreadPool* m_pool;
		Swift::BoostNetworkFactories *m_factories;
		Swift::BoostIOServiceThread m_boostIOServiceThread;
		boost::shared_ptr<Swift::Connection> m_conn;
		Transport::Config *config;
		Swift::Timer::ref m_update_player_timer;
		std::map<std::string,UserData> m_userdb;
		std::set<std::string> m_onlineUsers;
		std::set<std::string> m_online_player;
		std::map<std::string,std::string> m_player_unescape;
		boost::mutex m_userlock,m_userdblock,m_online_player_block;
        std::string m_temp_dir;
        std::list<Room> rooms;
        std::list<std::string> rooms2;
        std::list<std::string> names;
};

class MessagePooler : public Transport::Thread
{
    std::string m_user;
    std::string m_answer;
    std::string m_path;
    boost::function< void (std::string&, std::string &) > m_callBack;
    public:
        MessagePooler(const std::string &user,const std::string &path,boost::function< void (std::string&,std::string&) >  cb);
        void run();
        void finalize();
};
