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

/** @brief Converts an sio::message:ptr object into nlohmann::json
 *
 * @param sio
 * @return nlohmann::json
 */
nlohmann::json createJson(sio::message::ptr sio)
{
    // return json
    nlohmann::json json;

    try {
        // browse flags, we consider it can only be array/vector or object/map
        if (sio->get_flag() == sio::message::flag_array) {
            for (int i = 0; i < int(sio->get_vector().size()); ++i) {
                if (sio->get_vector()[i]->get_flag() == sio::message::flag_object ||
                    sio->get_vector()[i]->get_flag() == sio::message::flag_array) {
                    json[i] = createJson(sio->get_vector()[i]);
                } else if (sio->get_vector()[i]->get_flag() == sio::message::flag_integer) {
                    json[i] = sio->get_vector()[i]->get_int();
                } else if (sio->get_vector()[i]->get_flag() == sio::message::flag_double) {
                    json[i] = sio->get_vector()[i]->get_double();
                } else if (sio->get_vector()[i]->get_flag() == sio::message::flag_string) {
                    json[i] = sio->get_vector()[i]->get_string();
                } else if (sio->get_vector()[i]->get_flag() == sio::message::flag_boolean) {
                    json[i] = (sio->get_vector()[i]->get_bool() ? "true" : "false");
                } else if (sio->get_vector()[i]->get_flag() == sio::message::flag_null) {
                    // json[i] = "null"; // do not set json[i] so that it's set to json-null properly
                } else {
                    std::cout << "Unknown flag in vector: " << sio->get_flag() << ", i is " << i << std::endl;
                }
            }
        } else if (sio->get_flag() == sio::message::flag_object) {
            for (auto it = sio->get_map().cbegin(); it != sio->get_map().cend(); ++it) {
                if (it->second->get_flag() == sio::message::flag_object ||
                    it->second->get_flag() == sio::message::flag_array) {
                    json[it->first] = createJson(it->second);
                } else if (it->second->get_flag() == sio::message::flag_integer) {
                    json[it->first] = it->second->get_int();
                } else if (it->second->get_flag() == sio::message::flag_double) {
                    json[it->first] = it->second->get_double();
                } else if (it->second->get_flag() == sio::message::flag_string) {
                    json[it->first] = it->second->get_string();
                } else if (it->second->get_flag() == sio::message::flag_boolean) {
                    json[it->first] = it->second->get_bool();
                } else if (it->second->get_flag() == sio::message::flag_null) {
                    // json[it->first] = "null"; // do not set json[i] so that it's set to json-null properly
                } else {
                    std::cout << "Unknown flag in object: " << sio->get_flag() << ", it first is " << it->first << std::endl;
                }
            }
        } else {
            std::cout << "Unknown flag in createJson function: " << sio->get_flag() << std::endl;
        }
    } catch (nlohmann::json::exception &e) {
        std::cout << "JSON exception caught in " << __FUNCTION__ << " (message: " << e.what() << ")" << std::endl;
    }

    // return
    return json;
}

void bind_events()
{
	current_socket->on("data_RT", sio::socket::event_listener_aux([&](string const& name, message::ptr const& data, bool isAck,message::list &ack_resp)
           {
                std::cout << "data_RT" << std::endl;
                std::cout << "flag:" << data->get_flag()  <<std::endl;
                if (data->get_flag() == sio::message::flag_object ) {
                    nlohmann::json j_data;
                    j_data = createJson(data);
                    std::cout << "data:" << j_data << std::endl;
                }
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

    json j;
    j["clientID"] = "B03508EE8A334D81";

    sio::message::ptr object = createObject(j);
    current_socket->emit("ASK_JOIN_C", object);


    _lock.lock();
    _cond.wait(_lock);
    _lock.unlock();
    HIGHLIGHT("Closing...");
    h.sync_close();
    h.clear_con_listeners();
	return 0;
}

