
#pragma once

#include <decent/encrypt/crypto_types.hpp>
#include <decent/encrypt/custodyutils.hpp>
#include <graphene/package/package.hpp>

#include <fc/optional.hpp>
#include <fc/signals.hpp>
#include <fc/time.hpp>
#include <fc/thread/thread.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/sha512.hpp>

#include <libtorrent/session.hpp>

#include <boost/filesystem.hpp>

#include <utility>
#include <vector>
#include <string>
#include <mutex>


namespace graphene { namespace package {


class torrent_transfer : public package_transfer_interface {
private:
    torrent_transfer(const torrent_transfer& orig);

public:
    torrent_transfer(torrent_transfer&&)                 = delete;
    torrent_transfer& operator=(const torrent_transfer&) = delete;
    torrent_transfer& operator=(torrent_transfer&&)      = delete;

    torrent_transfer();
    virtual ~torrent_transfer();

public:
    virtual void upload_package(transfer_id id, const package_object& package, transfer_listener* listener);
    virtual void download_package(transfer_id id, const std::string& url, transfer_listener* listener);

    virtual std::string get_transfer_url(transfer_id id);
    virtual void        print_status();

    virtual package_transfer_interface* clone() {
        return new torrent_transfer(*this);
    }

private:
    void handle_torrent_alerts();
    void update_torrent_status();
    package::package_object check_and_install_package();

private: // These will be shared by all clones (via clone()) of the initial instance, which in its turn is constructed only by the default c-tor.
    std::shared_ptr<fc::thread>           _thread;
    std::shared_ptr<fc::mutex>            _session_mutex;
    std::shared_ptr<libtorrent::session>  _session;

private: // These are used to maintain instance lifetime info, and will be shared by all async callbacks that access this instance.
    std::shared_ptr<fc::mutex>                 _lifetime_info_mutex;
    std::shared_ptr<std::atomic<bool>>         _instance_exists;

private:
    std::vector<std::pair<std::string, int> >  _default_dht_nodes;
    std::vector<std::string>                   _default_trackers;

    std::string                 _url;
    transfer_id                 _id;
    transfer_listener*          _listener;
    std::ofstream               _transfer_log;
    bool                        _is_upload;
    libtorrent::torrent_handle  _torrent_handle;
};


} } // graphene::package
