// this is the lock server
// the lock client has a similar interface
#ifndef lock_server_h
#define lock_server_h
#include <string>
#include <map>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc/rpc.h"
#include <mutex>
#include <condition_variable>

class lock_server {

protected:
  int nacquire;
private:
  // singleton
  struct LockEntry {
    int clt = 0;
    std::condition_variable *v = nullptr;
    LockEntry(int clt = 0, std::condition_variable *v = nullptr)
      : clt(clt), v(v) {}
    // same impl as copy constructor
    // since we need to keep condition variable unique
    LockEntry(LockEntry &&le) {
      clt = le.clt;
      if(le.v) {
        if(v) delete v;
        v = le.v;
        le.v = nullptr;
      }
    }
    LockEntry &operator =(LockEntry &&le) {
      clt = le.clt;
      if(le.v) {
        if(v) delete v;
        v = le.v;
        le.v = nullptr;
      }
      return *this;
    }
    ~LockEntry() { delete(v); }
  };
  static std::map<lock_protocol::lockid_t, LockEntry> locks;
  static std::mutex g_mutex;
public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







