#include "sinfar.h"
// Transport includes

#include "transport/Config.h"
#include "transport/NetworkPlugin.h"
#include "transport/Logging.h"

#include <regex>


#include<ctime>



// Swiften
#include "Swiften/Swiften.h"

// Boost
#include <boost/algorithm/string.hpp>

#include <stdlib.h>

#include <experimental/filesystem>

using namespace boost::filesystem;
using namespace boost::program_options;
using namespace Transport;

DEFINE_LOGGER(logger, "Backend Sinfar");

Sinfar::Sinfar(Config *config, Swift::SimpleEventLoop *loop, const std::string &host, int port) : NetworkPlugin() {
    LOG4CXX_INFO(logger, "Create the temp Dir.");
    char path[]="/tmp/SinfarXXXXXXXXXXXXXXXXXXXX";
    m_temp_dir=std::string(mkdtemp(path));
    this->config = config;
    m_factories = new Swift::BoostNetworkFactories(loop);
    m_update_player_timer = m_factories->getTimerFactory()->createTimer( 10000);
    m_conn = m_factories->getConnectionFactory()->createConnection();
    m_conn->onDataRead.connect(boost::bind(&Sinfar::_handleDataRead, this, _1));
    m_conn->connect(Swift::HostAddressPort(Swift::HostAddress(host), port));
    m_update_player_timer->onTick.connect(boost::bind(&Sinfar::UpdatePlayer, this));
    m_update_player_timer->start();
    LOG4CXX_INFO(logger, "Starting the plugin.");
    m_pool=new Transport::ThreadPool(loop,20);
    Transport::NetworkPlugin::PluginConfig conf;
    conf.setSupportBuddiesOnLogin(true);
    conf.setSupportMUC(true);
    conf.disableJIDEscaping();
    sendConfig(conf);

    rooms.push_back(Room({"talk","/tk "}));
    rooms.push_back(Room({"shout","/s "}));
    rooms.push_back(Room({"whisper","/w "}));
    rooms.push_back(Room({"dm_shout",""}));
    rooms.push_back(Room({"dm_whisper",""}));
    rooms.push_back(Room({"dm_tell",""}));
    rooms.push_back(Room({"quiet","/quiet "}));
    rooms.push_back(Room({"silent","/silent "}));
    rooms.push_back(Room({"yell","/yell "}));
    rooms.push_back(Room({"tell","/tp "}));
    rooms.push_back(Room({"party","/p "}));
    rooms.push_back(Room({"ooc","/ooc "}));
    rooms.push_back(Room({"sex","/sex "}));
    rooms.push_back(Room({"pvp","/pvp "}));
    rooms.push_back(Room({"action","/action "}));
    rooms.push_back(Room({"event","/event "}));
    rooms.push_back(Room({"ffa","/ffa "}));
    rooms.push_back(Room({"build","/build "}));
    rooms.push_back(Room({"all",""}));


    rooms2.push_back("talk");
    rooms2.push_back("shout");
    rooms2.push_back("whisper");
    rooms2.push_back("dm_shout");
    rooms2.push_back("dm_whisper");
    rooms2.push_back("dm_tell");
    rooms2.push_back("quiet");
    rooms2.push_back("silent");
    rooms2.push_back("yell");
    rooms2.push_back("tell");
    rooms2.push_back("party");
    rooms2.push_back("ooc");
    rooms2.push_back("sex");
    rooms2.push_back("pvp");
    rooms2.push_back("action");
    rooms2.push_back("event");
    rooms2.push_back("ffa");
    rooms2.push_back("build");
    rooms2.push_back("all");


    names.push_back("Talk");
    names.push_back("Shout");
    names.push_back("Whisper");
    names.push_back("DM Shout");
    names.push_back("DM Whisper");
    names.push_back("DM Tell");
    names.push_back("Quiet");
    names.push_back("Silent");
    names.push_back("Yell");
    names.push_back("Tell");
    names.push_back("Party");
    names.push_back("OOC");
    names.push_back("Sex");
    names.push_back("PvP");
    names.push_back("Action");
    names.push_back("Event");
    names.push_back("FFA");
    names.push_back("Build");
    names.push_back("All");
}


Sinfar::~Sinfar() {
    std::experimental::filesystem::remove_all(std::experimental::filesystem::path(m_temp_dir));
}

// NetworkPlugin uses this method to send the data to networkplugin server
void Sinfar::sendData(const std::string &string) {
    m_conn->write(Swift::createSafeByteArray(string));
}


void Sinfar::handleRawXML(const std::string &xml)
{
    //    LOG4CXX_INFO(logger, "raw xml: "<<xml);
}

// This method has to call handleDataRead with all received data from network plugin server
void Sinfar::_handleDataRead(boost::shared_ptr<Swift::SafeByteArray> data) {
    std::string d(data->begin(), data->end());
    handleDataRead(d);
}

void Sinfar::handleLoginRequest(const std::string &user, const std::string &legacyName, const std::string &password) {
    try {
        LOG4CXX_INFO(logger, user << ": Try login "<<" "<<legacyName<<" "<<password);
        curlpp::Easy handle;
        std::string url=std::string("http://nwn.sinfar.net/login.php?nocache=")+std::to_string(std::time(0));
        handle.setOpt(curlpp::Options::Url(url));
        std::string path=m_temp_dir+std::string("/")+user;
        handle.setOpt(curlpp::Options::CookieJar(path.c_str()));
        curlpp::Forms form;
        form.push_back(new curlpp::FormParts::Content("player_name", legacyName));
        form.push_back(new curlpp::FormParts::Content("password", password));
        handle.setOpt(curlpp::Options::HttpPost(form));
        std::ostringstream os;
        os<<handle;
        std::string str=os.str();
        if(str.length()==0)
        {
            LOG4CXX_INFO(logger, user << ": Login OK: "<<" "<<legacyName<<" "<<password);
            handleConnected(user);
            {
                boost::mutex::scoped_lock lock(m_userlock);
                m_onlineUsers.insert(user);
            }
            {
                boost::mutex::scoped_lock lock(m_userdblock);
                m_userdb[user].m_message_timer = m_factories->getTimerFactory()->createTimer( 10);
                m_userdb[user].m_message_timer->onTick.connect(boost::bind(&Sinfar::PollMessage, this,user));
                m_userdb[user].m_message_timer->start();
                m_userdb[user].m_legacy=legacyName;
            }
            addRoomList(user);
            //handleConnected("pablo_s_2@sinfar.sinfar-bane.ch");
            //handleConnected("nabla64@sinfar.sinfar-bane.ch");
        }
        else
        {
            LOG4CXX_INFO(logger, user << ": Login failed on sinfar: " <<str<<" "<<legacyName<<" "<<password);
        }
    }
    catch( curlpp::RuntimeError &e )
    {
        LOG4CXX_INFO(logger,user << "Error in connection to sinfar:" << e.what());
    }

    catch( curlpp::LogicError &e )
    {
        LOG4CXX_INFO(logger,user << "Error in connection to sinfar:" << e.what());
    }
}


void Sinfar::UpdatePlayer()
{
    LOG4CXX_INFO(logger,"Online Player Update");
    try {
        LOG4CXX_INFO(logger,": in try of UpdatePlayer.");
        curlpp::Cleanup cleanup;
        curlpp::Easy handle;
        std::string url=std::string("http://nwn.sinfar.net/getonlineplayers.php?nocache=")+std::to_string(std::time(0));
        handle.setOpt(curlpp::Options::Url(url));
        std::ostringstream os;
        os<<handle;
        std::string str=os.str();
        json j=json::parse(str);
        std::set<std::string> online_player;
        boost::mutex::scoped_lock lock(m_online_player_block);
        std::set<std::string> online_player2=m_online_player;
        for (json::iterator it = j.begin(); it != j.end(); ++it) {
            json temp= *it;
            json str=temp["playerName"];
            if(str.is_null()) continue;
            //std::string str_str=Swift::JID::getEscapedNode(str);
            std::string str_str=str;
            boost::algorithm::to_lower(str_str);
            online_player.insert(str_str);
            online_player2.erase(str_str);
            if(m_online_player.count(str_str)!=0) continue;
            LOG4CXX_INFO(logger,": New Player login - "<<str_str<<".");
            m_online_player.insert(str_str);
            m_player_unescape[str_str]=str;
            LoginPlayer(str_str);
            //TODO Add group
            //std::vector<std::string> group_list;
            //group_list.push_back("sinfar");
            //handleBuddyChanged(user, str_str, "",group_list, pbnetwork::STATUS_ONLINE);
        }

        for (auto it = online_player2.begin(); it != online_player2.end(); ++it) {
            auto str=*it;
            LOG4CXX_INFO(logger,": Player logout - "<<str<<".");
            //std::vector<std::string> group_list;
            //group_list.push_back("sinfar");
            //handleBuddyChanged(user, str, "",group_list, pbnetwork::STATUS_NONE);
            m_online_player.erase(str);
            LogoutPlayer(str);
        }
    }
    catch( curlpp::RuntimeError &e )
    {
        LOG4CXX_INFO(logger,"Error in connection to sinfar:" << e.what());
    }

    catch( curlpp::LogicError &e )
    {
        LOG4CXX_INFO(logger,"Error in connection to sinfar:" << e.what());
    }
    m_update_player_timer->start();
}

void Sinfar::handleLogoutRequest(const std::string &user, const std::string &legacyName) {
    LOG4CXX_INFO(logger, "Logout of  " << user);
    boost::mutex::scoped_lock lock(m_userdblock);
    m_userdb.erase(user);
    boost::mutex::scoped_lock lock2(m_userlock);
    m_onlineUsers.erase(user);

    try {
        curlpp::Easy handle;
        std::string url=std::string("http://nwn.sinfar.net/logout.php?nocache=")+std::to_string(std::time(0));
        handle.setOpt(curlpp::Options::Url(url));
        std::string path=m_temp_dir+std::string("/")+user;
        handle.setOpt(curlpp::Options::CookieFile(path.c_str()));
        std::ostringstream os;
        os<<handle;
        std::string str=os.str();
        if(str.length()==0)
        {
        }
        else
        {
            LOG4CXX_INFO(logger,user << "Error in logout from sinfar.net " <<str);
        }
    }
    catch( curlpp::RuntimeError &e )
    {
        LOG4CXX_INFO(logger,user << "Error in connection to sinfar:" << e.what());
    }

    catch( curlpp::LogicError &e )
    {
        LOG4CXX_INFO(logger,user << "Error in connection to sinfar:" << e.what());
    }
}

void Sinfar::handleMessageSendRequest(const std::string &user, const std::string &legacyName_, const std::string &message, const std::string &xhtml, const std::string &id) {


    try {

        std::string prefix="/tp ";
        std::string legacyName=legacyName_;
        LOG4CXX_INFO(logger, "Sending message from " << user << " to " << legacyName << ".");
        for( auto it=rooms.begin();it!=rooms.end();++it)
        {
            Room room=*it;
            if(legacyName==room.room)
            {
                prefix=room.prefix;
            }
        }
        const size_t last_slash_idx = legacyName.rfind('/');
        if (std::string::npos != last_slash_idx)
        {
            legacyName=legacyName.substr(last_slash_idx);
            legacyName.erase(0,1);
            LOG4CXX_INFO(logger, "Changed legacyName: " << legacyName);
        }
        if(prefix==std::string("/tp "))
        {
            boost::mutex::scoped_lock lock2(m_online_player_block);
            legacyName=m_player_unescape[legacyName];
            prefix=std::string("/tp \"")+legacyName+std::string("\" ");
        }

        LOG4CXX_INFO(logger, "prefix used: " << prefix);

        curlpp::Easy handle;
        std::string url=std::string("http://nwn.sinfar.net/sendchat.php?nocache=")+std::to_string(std::time(0));
        handle.setOpt(curlpp::Options::Url(url));
        std::string path=m_temp_dir+std::string("/")+user;
        handle.setOpt(curlpp::Options::CookieFile(path.c_str()));
        curlpp::Forms form;
        std::string message_=prefix+message;
        form.push_back(new curlpp::FormParts::Content("chat-message",message_));
        handle.setOpt(curlpp::Options::HttpPost(form));
        std::ostringstream os;
        os<<handle;
        std::string str=os.str();
        if(str.length()==0)
        {
            handleMessageAck(user,legacyName,id);
        }
        else
        {
            handleMessage(user,"SERVER",str);
            LOG4CXX_INFO(logger, user << ": Send of message failed on sinfar: " <<user<<" "<<legacyName<<" "<<message<<" "<<str);
        }
    }
    catch( curlpp::RuntimeError &e )
    {
        LOG4CXX_INFO(logger,user << "Error in connection to sinfar:" << e.what());
    }

    catch( curlpp::LogicError &e )
    {
        LOG4CXX_INFO(logger,user << "Error in connection to sinfar:" << e.what());
    }
}


void Sinfar::addRoomList(const std::string &user)
{


    handleRoomList(user,rooms2,names);

}

void Sinfar::handleBuddyUpdatedRequest(const std::string &user, const std::string &buddyName, const std::string &alias, const std::vector<std::string> &groups) {

    //     std::string buddyName_=Swift::JID::getEscapedNode(buddyName);
    std::string buddyName_=buddyName;
    boost::algorithm::to_lower(buddyName_);

    LOG4CXX_INFO(logger, user << ": Begin HandleBuddyUpdate Request - "<<buddyName_<<".");
    {
        boost::mutex::scoped_lock lock(m_userdblock);
        BuddyData buddy;
        buddy.alias=alias;
        buddy.groups=groups;
        m_userdb[user].m_buddy_list[buddyName_]=buddy;
    }
    boost::mutex::scoped_lock lock2(m_online_player_block);
    if(m_online_player.count(buddyName_)==0)
    {
        LOG4CXX_INFO(logger, user << ": Add buddy offline REQUEST - "<<buddyName_<<".");
        handleBuddyChanged(user, buddyName_, alias, groups, pbnetwork::STATUS_NONE);
    }
    else
    {
        LOG4CXX_INFO(logger, user << ": Added buddy REQUEST - "<<buddyName_<<".");
        handleBuddyChanged(user, buddyName_, alias, groups, pbnetwork::STATUS_ONLINE);
    }
    LOG4CXX_INFO(logger, user << ": End HandleBuddyUpdate Request - "<<buddyName_<<".");
}


void Sinfar::handleBuddyUpdatedRequest(const std::string &user, const std::string &buddyName) {
    BuddyData buddy;
    {
        boost::mutex::scoped_lock lock(m_userdblock);
        buddy=m_userdb[user].m_buddy_list[buddyName];
    }
    if(m_online_player.count(buddyName)==0)
    {
        LOG4CXX_INFO(logger, user << ": Add buddy offline REQUEST 2 arg - "<<buddyName<<".");
        handleBuddyChanged(user, buddyName, buddy.alias, buddy.groups, pbnetwork::STATUS_NONE);
    }
    else
    {
        LOG4CXX_INFO(logger, user << ": Added buddy REQUEST 2arg - "<<buddyName<<".");
        handleBuddyChanged(user, buddyName, buddy.alias, buddy.groups, pbnetwork::STATUS_ONLINE);
    }
}

void Sinfar::handleBuddyRemovedRequest(const std::string &user, const std::string &buddyName, const std::vector<std::string> &groups) {

    LOG4CXX_INFO(logger, user << ": Removed buddy REQUEST2 - "<<buddyName<<".");
    boost::mutex::scoped_lock lock(m_userdblock);
    m_userdb[user].m_buddy_list.erase(buddyName);
    handleBuddyRemoved(user, buddyName);
}


void Sinfar::LoginPlayer(const std::string &buddyName)
{
    boost::mutex::scoped_lock lock(m_userlock);
    auto it=m_onlineUsers.begin();
    while(it != m_onlineUsers.end())
    {
        std::string user= *it;
        if(m_userdb[user].m_buddy_list.count(buddyName)>0)
        {
            handleBuddyUpdatedRequest(user,buddyName);
        }
        boost::mutex::scoped_lock lock(m_userdblock);
        auto it2=m_userdb[user].m_room_list.begin(); 
        while(it2 != m_userdb[user].m_room_list.end())
        {
            auto room=*it2;
            handleParticipantChanged(user,buddyName,room,0,pbnetwork::STATUS_ONLINE);
            it2++;
        }
        it++;
    }
}

void Sinfar::LogoutPlayer(const std::string &buddyName)
{
    boost::mutex::scoped_lock lock(m_userlock);
    auto it=m_onlineUsers.begin();
    while(it != m_onlineUsers.end())
    {
        std::string user= *it;
        if(m_userdb[user].m_buddy_list.count(buddyName)>0)
        {
            handleBuddyUpdatedRequest(user,buddyName);
        }
        boost::mutex::scoped_lock lock(m_userdblock);
        auto it2=m_userdb[user].m_room_list.begin(); 
        while(it2 != m_userdb[user].m_room_list.end())
        {
            auto room=*it2;
            handleParticipantChanged(user,buddyName,room,0,pbnetwork::STATUS_NONE);
            it2++;
        }
        it++;
    }
}

void Sinfar::handleBuddies(const pbnetwork::Buddies & Buddies)
{

    LOG4CXX_INFO(logger,"Handling Buddies");
    for(int i=0;i<Buddies.buddy_size();++i)
    {
        auto temp=Buddies.buddy(i);
        void handleBuddies(const pbnetwork::Buddies &);
        auto username=temp.username();
        if (temp.has_blocked())
        {
            handleBuddyBlockToggled(temp.username(), temp.buddyname(),temp.blocked());
        }
        else {
            std::vector<std::string> groups;
            for (int i = 0; i < temp.group_size(); i++) {
                groups.push_back(temp.group(i));
            }
            LOG4CXX_INFO(logger,"Buddies "<<temp.username()<<" ---- "<<temp.buddyname());
            handleBuddyUpdatedRequest(temp.username(), temp.buddyname(), temp.alias(), groups);
        }
    }
    LOG4CXX_INFO(logger,"End Buddies");
}

void Sinfar::PollMessage(const std::string &user)
{
    std::string path=m_temp_dir+std::string("/")+user;
    m_pool->runAsThread(new MessagePooler(user,path,boost::bind(&Sinfar::PollMessageCallBack, this, _1, _2)));
}


void Sinfar::PollMessageCallBack(const std::string &user,const std::string & messages)
{
    LOG4CXX_INFO(logger,"Json Received "<<messages);
    json j=json::parse(messages);
    json j2=j[0];
    for (json::iterator it = j2.begin(); it != j2.end(); ++it) {
        json temp= *it;
        json str=temp["channel"];
        std::string channel=temp["channel"];
        if(channel==std::string("4"))
        {
            HandleTell(user,temp);  
        }
        else
        {
            HandleRoom(user,temp);
        }
    }
    boost::mutex::scoped_lock lock(m_userdblock);
    m_userdb[user].m_message_timer->start();
}

void Sinfar::HandleTell(const std::string &user,json payload)
{

    LOG4CXX_INFO(logger,"Received tell "<<payload);
    std::string str=payload["fromPlayerName"];
    //std::string str_str=Swift::JID::getEscapedNode(str);
    std::string str_str=str;
    boost::algorithm::to_lower(str_str);
    boost::mutex::scoped_lock lock(m_userdblock);
    std::string legacy=m_userdb[user].m_legacy;
    if(str_str==legacy)
    {

        LOG4CXX_INFO(logger,"Its myself");
        str_str=payload["toPlayerName"];
        boost::algorithm::to_lower(str_str);
        std::string message=payload["message"];
        handleMessage(user,str_str,std::string("Message Send:") + message ,"","","",false,true);
    }
    else
    {
        LOG4CXX_INFO(logger,"Received tell to me: "<<user<<" "<<str_str<<" "<<payload["message"]<<" "<<legacy);
        handleMessage(user,str_str,payload["message"],"","","",false,true);
    }
    if(m_userdb[user].m_room_list.count("all")>0);
    {
        LOG4CXX_INFO(logger,"Received tell All: "<<payload);
        std::string str_message=payload["message"];
        std::string message=std::string("Tell: ")+str_message;
        handleMessage(user,"all",message,str_str);
    }
}


void Sinfar::HandleRoom(const std::string &user,json payload)
{

    std::string channel=payload["channel"];
    std::string str=payload["fromPlayerName"];
    //std::string str_str=Swift::JID::getEscapedNode(str);
    std::string str_str=str;
    boost::algorithm::to_lower(str_str);
    std::string room;
    std::string pref;
    if(channel==std::string("1"))
    {
        room=std::string("talk");
        pref=std::string("/tk ");
    }
    else if(channel==std::string("2"))
    {
        room=std::string("shout");
        pref=std::string("/s ");

    }
    else if(channel==std::string("3"))
    {
        room=std::string("whisper");
        pref=std::string("/w ");

    }
    else if(channel==std::string("18"))
    {
        room=std::string("dm_shout");
        pref=std::string("");

    }
    else if(channel==std::string("19"))
    {
        room=std::string("dm_whisper");
        pref=std::string("");

    }
    else if(channel==std::string("20"))
    {
        room=std::string("dm_tell");
        pref=std::string("");

    }
    else if(channel==std::string("31"))
    {
        room=std::string("quiet");
        pref=std::string("/quiet ");

    }
    else if(channel==std::string("32"))
    {
        room=std::string("silent");
        pref=std::string("/silent ");

    }
    else if(channel==std::string("30"))
    {
        room=std::string("yell");
        pref=std::string("/yell ");

    }
    else if(channel==std::string("6"))
    {
        room=std::string("party");
        pref=std::string("/p ");

    }
    else if(channel==std::string("164"))
    {
        room=std::string("ooc");
        pref=std::string("/ooc ");

    }
    else if(channel==std::string("108"))
    {
        room=std::string("sex");
        pref=std::string("/sex ");

    }
    else if(channel==std::string("228"))
    {
        room=std::string("pvp");
        pref=std::string("/pvp ");

    }
    else if(channel==std::string("116"))
    {
        room=std::string("action");
        pref=std::string("/action ");

    }
    else if(channel==std::string("104"))
    {
        room=std::string("event");
        pref=std::string("/event ");

    }
    else if(channel==std::string("132"))
    {
        room=std::string("ffa");
        pref=std::string("/ffa ");

    }
    else if(channel==std::string("102"))
    {
        room=std::string("build");
        pref=std::string("/build ");

    }
    else
    {
        room=std::string("all");
        pref=std::string("");

    }

    boost::mutex::scoped_lock lock(m_userdblock);
    if(m_userdb[user].m_room_list.count(room)>0);
    {
        std::string str_message=payload["message"];
        std::string message=str_message;
        handleMessage(user,room,message,str_str);
    }
    if(m_userdb[user].m_room_list.count("all")>0);
    {
        std::string str_message=payload["message"];
        std::string message=room+std::string(": ")+str_message;
        handleMessage(user,"all",message,str_str);
    }
}

MessagePooler::MessagePooler(const std::string &user,const std::string &path,boost::function< void (std::string&,std::string&) >  cb)
{
    m_user=user;
    m_callBack=cb;
    m_path=path;
}


void MessagePooler::run()
{
    LOG4CXX_INFO(logger,"PollMessage for "<<m_user);
    try {
        curlpp::Easy handle;
        std::string url=std::string("http://nwn.sinfar.net/getchat.php?nocache=")+std::to_string(std::time(0));
        handle.setOpt(curlpp::Options::Url(url));
        handle.setOpt(curlpp::Options::CookieFile(m_path.c_str()));
        std::ostringstream os;
        os<<handle;
        m_answer=os.str();
    }
    catch( curlpp::RuntimeError &e )
    {
        LOG4CXX_INFO(logger,m_user << "Error in connection to sinfar:" << e.what());
    }

    catch( curlpp::LogicError &e )
    {
        LOG4CXX_INFO(logger,m_user << "Error in connection to sinfar:" << e.what());
    }
}

void MessagePooler::finalize()
{
    m_callBack(m_user,m_answer);
}



void Sinfar::handleJoinRoomRequest(const std::string &user, const std::string &room, const std::string &nickname, const std::string &pasword)
{
    std::set<std::string> rooms;

    rooms.insert("talk");
    rooms.insert("shout");
    rooms.insert("whisper");
    rooms.insert("dm_shout");
    rooms.insert("dm_whisper");
    rooms.insert("dm_tell");
    rooms.insert("quiet");
    rooms.insert("silent");
    rooms.insert("yell");
    rooms.insert("tell");
    rooms.insert("party");
    rooms.insert("ooc");
    rooms.insert("sex");
    rooms.insert("pvp");
    rooms.insert("action");
    rooms.insert("event");
    rooms.insert("ffa");
    rooms.insert("build");
    rooms.insert("all");

    if(rooms.count(room)>0)
    {
        LOG4CXX_INFO(logger,user<<" Join Room "<<room);
        boost::mutex::scoped_lock lock(m_userdblock);
        m_userdb[user].m_room_list.insert(room);
        handleMessage(user,room,"Room Joined","SERVER MESSAGE:");

        auto it=m_online_player.begin();
        while(it != m_online_player.end())
        {
            std::string player= *it;
            LOG4CXX_INFO(logger,user<<" InLoop Join Room "<<room<<" "<<player);
            handleParticipantChanged(user,player,room,0,pbnetwork::STATUS_ONLINE);
            it++;
        }
        LOG4CXX_INFO(logger,user<<" AfterLoop Join Room "<<room);
        handleParticipantChanged(user,nickname,room,16,pbnetwork::STATUS_ONLINE);
        handleSubject(user,room,room,user);
        LOG4CXX_INFO(logger,user<<" Finish Join Room "<<room<<" "<<nickname);
    }
    else
    {
        handleMessage(user,room,"Room Not Available","SERVER MESSAGE:");
    }
}

void Sinfar::handleLeaveRoomRequest(const std::string &user, const std::string &room)
{
    LOG4CXX_INFO(logger,user<<" Leave Room "<<room);
    boost::mutex::scoped_lock lock(m_userdblock);
    m_userdb[user].m_room_list.erase(room);
    handleMessage(user,room,"Room Left:","SERVER MESSAGE:");
}



void Sinfar::handleVCardRequest(const std::string &user, const std::string &legacyName, unsigned int id)
{
    LOG4CXX_INFO(logger,user<<" Vcard requested "<<legacyName);
    handleVCard(user,id,legacyName,"",legacyName,"");

}
