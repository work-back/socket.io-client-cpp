//
//  sio_test_sample.cpp
//
//  Created by Melo Yao on 3/24/15.
//

#include "sio_client.h"

#include <functional>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#ifdef WIN32
#define HIGHLIGHT(__O__) std::cout<<__O__<<std::endl
#define EM(__O__) std::cout<<__O__<<std::endl

#include <stdio.h>
#include <tchar.h>
#define MAIN_FUNC int _tmain(int argc, _TCHAR* argv[])
#else
#define HIGHLIGHT(__O__) std::cout<<"\e[1;31m"<<__O__<<"\e[0m"<<std::endl
#define EM(__O__) std::cout<<"\e[1;30;1m"<<__O__<<"\e[0m"<<std::endl

#define MAIN_FUNC int main(int argc ,const char* args[])
#endif

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace sio;
using namespace std;
std::mutex _lock;

std::condition_variable_any _cond;
bool connect_finish = false;

class connection_listener
{
    sio::client &handler;

public:
    
    connection_listener(sio::client& h):
    handler(h)
    {
    }
    

    void on_connected()
    {
        _lock.lock();
        _cond.notify_all();
        connect_finish = true;
        _lock.unlock();
    }
    void on_close(client::close_reason const& reason)
    {
        std::cout<<"sio closed "<<std::endl;
        exit(0);
    }
    
    void on_fail()
    {
        std::cout<<"sio failed "<<std::endl;
        exit(0);
    }
};

int participants = -1;

socket::ptr current_socket;

void bind_events()
{
	current_socket->on("data_RT", sio::socket::event_listener_aux([&](string const& name, message::ptr const& data, bool isAck,message::list &ack_resp)
                       {
                            std::cout<<"data_RT"<<std::endl;
                       }));

}

sio::message::ptr createObject(json o);

sio::message::ptr createArray(json o)
{
    sio::message::ptr array = array_message::create();

    for (json::iterator it = o.begin(); it != o.end(); ++it) {

        auto v = it.value();
        if (v.is_boolean())
        {
            array->get_vector().push_back(bool_message::create(v.get<bool>()));
        }
        else if (v.is_number_integer())
        {
            array->get_vector().push_back(int_message::create(v.get<int>()));
        }
        else if (v.is_string())
        {
            array->get_vector().push_back(string_message::create(v.get<string>()));
        }
        else if (v.is_array())
        {
            array->get_vector().push_back(createArray(v));
        }
        else if (v.is_object())
        {
            array->get_vector().push_back(createObject(v));
        }
    }
    return array;
}

sio::message::ptr createObject(json o)
{
    sio::message::ptr object = object_message::create();

    for (json::iterator it = o.begin(); it != o.end(); ++it)
    {
        auto key = it.key();
        auto v = it.value();

        if (v.is_boolean())
        {
            object->get_map()[key] = bool_message::create(v.get<bool>());
        }
        else if (v.is_number_integer())
        {
            object->get_map()[key] = int_message::create(v.get<int>());
        }
        else if (v.is_string())
        {
            object->get_map()[key] = string_message::create(v.get<std::string>());
        }
        else if (v.is_array())
        {
            json childObject = v;
            object->get_map()[key] = createArray(v);
        }
        else if (v.is_object())
        {
            json childObject = v;
            object->get_map()[key] = createObject(childObject);
        }
    }
    return object;
}

MAIN_FUNC
{

    sio::client h;
    connection_listener l(h);
    
    h.set_open_listener(std::bind(&connection_listener::on_connected, &l));
    h.set_close_listener(std::bind(&connection_listener::on_close, &l,std::placeholders::_1));
    h.set_fail_listener(std::bind(&connection_listener::on_fail, &l));

    h.set_reconnect_attempts(1000000);
    h.set_reconnect_delay(2000);
    h.set_reconnect_delay_max(10000);

	current_socket = h.socket();
    bind_events();

    std::cout<<"Do Try Connect ..."<<std::endl;

    h.connect("ws://openapi.sleepthing.com");
    // h.connect("ws://127.0.0.1:8080");
    _lock.lock();
    if(!connect_finish) {
        _cond.wait(_lock);
    }
    _lock.unlock();

    std::cout<<"Connect Success !!!"<<std::endl;

    //message::list li( "{\"clientID\":\"B03508EE8A334D81\"}");
    //li.push(string_message::create("economics"));
    //current_socket->emit("categories", li);

    // string cid = "[{\'clientID\':\'B03508EE8A334D81\'}]";
    // string cid = "{\'clientID\':\'B03508EE8A334D81\'}";
    // current_socket->emit("ASK_JOIN_C", cid);


    // auto binary_msg = sio::binary_message::create(binary_data);


    json j;
    j["clientID"] = "B03508EE8A334D81";

    sio::message::ptr object = createObject(j);
    current_socket->emit("ASK_JOIN_C", object);


    _lock.lock();
    _cond.wait(_lock);
    _lock.unlock();
#if 0
Login:
    string nickname;
    while (nickname.length() == 0) {
        HIGHLIGHT("Type your nickname:");
        
        getline(cin, nickname);
    }
	current_socket->on("login", sio::socket::event_listener_aux([&](string const& name, message::ptr const& data, bool isAck,message::list &ack_resp){
        _lock.lock();
        participants = data->get_map()["numUsers"]->get_int();
        bool plural = participants !=1;
        HIGHLIGHT("Welcome to Socket.IO Chat-\nthere"<<(plural?" are ":"'s ")<< participants<<(plural?" participants":" participant"));
        _cond.notify_all();
        _lock.unlock();
        current_socket->off("login");
    }));
    current_socket->emit("add user", nickname);
    _lock.lock();
    if (participants<0) {
        _cond.wait(_lock);
    }
    _lock.unlock();
    bind_events();
    
    HIGHLIGHT("Start to chat,commands:\n'$exit' : exit chat\n'$nsp <namespace>' : change namespace");
    for (std::string line; std::getline(std::cin, line);) {
        if(line.length()>0)
        {
            if(line == "$exit")
            {
                break;
            }
            else if(line.length() > 5&&line.substr(0,5) == "$nsp ")
            {
                string new_nsp = line.substr(5);
                if(new_nsp == current_socket->get_namespace())
                {
                    continue;
                }
                current_socket->off_all();
                current_socket->off_error();
                //per socket.io, default nsp should never been closed.
                if(current_socket->get_namespace() != "/")
                {
                    current_socket->close();
                }
                current_socket = h.socket(new_nsp);
                bind_events();
                //if change to default nsp, we do not need to login again (since it is not closed).
                if(current_socket->get_namespace() == "/")
                {
                    continue;
                }
                goto Login;
            }
            current_socket->emit("new message", line);
            _lock.lock();
            EM("\t\t\t"<<line<<":"<<"You");
            _lock.unlock();
        }
    }
    #endif
    HIGHLIGHT("Closing...");
    h.sync_close();
    h.clear_con_listeners();
	return 0;
}

