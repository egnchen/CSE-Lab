#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>


#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include "handle.h"
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>

class lock_server_cache {
 private:
  int nacquire; // what's this for?
  // map to hold lock data
  struct lock_data {
    rpcc *holder;
    std::queue<rpcc *> waiting;
    std::mutex * const m;  // for performance, one mutex per lock
    std::condition_variable * const release_cv;
    lock_data():
      holder(nullptr), waiting(), m(new std::mutex()),
      release_cv(new std::condition_variable()) {}
  };
  std::map<lock_protocol::lockid_t, lock_data> locks;

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
