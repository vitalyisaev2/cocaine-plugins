/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include "elliptics.hpp"

using namespace cocaine;
using namespace cocaine::storage;

log_adapter_t::log_adapter_t(const boost::shared_ptr<logging::logger_t>& log, const int level):
    ioremap::elliptics::logger(level),
    m_log(log),
    m_level(level)
{ }

void log_adapter_t::log(const int level, const char * message) {
    size_t length = ::strlen(message) - 1;
    
    switch(level) {
        case DNET_LOG_NOTICE:
            m_log->info("%.*s", length, message);
            break;

        case DNET_LOG_INFO:
            m_log->info("%.*s", length, message);
            break;

        case DNET_LOG_DEBUG:
            m_log->debug("%.*s", length, message);
            break;

        case DNET_LOG_ERROR:
            m_log->error("%.*s", length, message);
            break;

        default:
            break;
    };
}

unsigned long log_adapter_t::clone() {
    return reinterpret_cast<unsigned long>(
        new log_adapter_t(m_log, m_level)
    );
}

namespace {
    struct digitizer {
        template<class T>
        int operator()(const T& value) {
            return value.asInt();
        }
    };
}

elliptics_storage_t::elliptics_storage_t(context_t& context, const std::string& name, const Json::Value& args):
    category_type(context, name, args),
    m_log(context.log(name)),
    m_log_adapter(m_log, args.get("verbosity", DNET_LOG_ERROR).asUInt()),
    m_node(m_log_adapter)
{
    Json::Value nodes(args["nodes"]);

    if(nodes.empty() || !nodes.isObject()) {
        throw storage_error_t("no nodes has been specified");
    }

    Json::Value::Members node_names(nodes.getMemberNames());

    for(Json::Value::Members::const_iterator it = node_names.begin();
        it != node_names.end();
        ++it)
    {
        try {
            m_node.add_remote(
                it->c_str(),
                nodes[*it].asInt()
            );
        } catch(const std::runtime_error& e) {
            // Do nothing. Yes. Really.
        }
    }

    Json::Value groups(args["groups"]);

    if(groups.empty() || !groups.isArray()) {
        throw storage_error_t("no groups has been specified");
    }

    std::vector<int> group_numbers;

    std::transform(
        groups.begin(),
        groups.end(),
        std::back_inserter(group_numbers),
        digitizer()
    );

    m_node.add_groups(group_numbers);
}

std::string
elliptics_storage_t::read(const std::string& collection,
                          const std::string& key)
{
    std::string blob;

    try {
        blob = m_node.read_data_wait(id(collection, key), 0, 0, 0, 0, 0);
    } catch(const std::runtime_error& e) {
        throw storage_error_t(e.what());
    }

    return blob;
}

void
elliptics_storage_t::write(const std::string& collection,
                           const std::string& key,
                           const std::string& blob)
{
    try {
        m_node.write_data_wait(id(collection, key), blob, 0, 0, 0, 0);
        
        std::vector<std::string> keylist(
            list(collection)
        );
        
        if(std::find(keylist.begin(), keylist.end(), key) == keylist.end()) {
            msgpack::sbuffer buffer;
            std::string object;
            
            keylist.push_back(key);
            msgpack::pack(&buffer, keylist);
            
            object.assign(
                buffer.data(),
                buffer.size()
            );

            m_node.write_data_wait(id("system", "list:" + collection), object, 0, 0, 0, 0);
        }
    } catch(const std::runtime_error& e) {
        throw storage_error_t(e.what());
    }
}

std::vector<std::string>
elliptics_storage_t::list(const std::string& collection) {
    std::vector<std::string> result;
    std::string blob;
    
    try {
        blob = m_node.read_data_wait(id("system", "list:" + collection), 0, 0, 0, 0, 0);
    } catch(const std::runtime_error& e) {
        return result;
    }

    msgpack::unpacked unpacked;

    try {
        msgpack::unpack(&unpacked, blob.data(), blob.size());
        unpacked.get().convert(&result);
    } catch(const msgpack::unpack_error& e) {
        throw storage_error_t("the collection metadata is corrupted");
    } catch(const msgpack::type_error& e) {
        throw storage_error_t("the collection metadata is corrupted");
    }

    return result;
}

void elliptics_storage_t::remove(const std::string& collection,
                                 const std::string& key)
{
    try {
        std::vector<std::string> keylist(list(collection)),
                                 updated;

        std::remove_copy(
            keylist.begin(),
            keylist.end(),
            std::back_inserter(updated),
            key
        );

        msgpack::sbuffer buffer;
        std::string object;

        msgpack::pack(&buffer, updated);
        object.assign(buffer.data(), buffer.size());

        m_node.write_data_wait(id("system", "list:" + collection), object, 0, 0, 0, 0);
        m_node.remove(id(collection, key));
    } catch(const std::runtime_error& e) {
        throw storage_error_t(e.what());
    }
}

extern "C" {
    void initialize(api::repository_t& repository) {
        repository.insert<elliptics_storage_t>("elliptics");
    }
}
