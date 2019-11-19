// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include <map>
#include <mutex>
#include <condition_variable>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include "tprintf.h"


// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 6.
class lock_release_user {
public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_acquire_user {
public:
  virtual void doacquire(lock_protocol::lockid_t) = 0;
  virtual ~lock_acquire_user() {};
};

class lock_client_cache : public lock_client {
public:
  enum state {
    NONE = 0,
    FREE,
    LOCKED,
    ACQUIRING,
    RELEASING
  };

struct lock_state {
    state s;
    bool revoke, retry;
    pthread_t thread_id;
    unsigned int acquire_cnt;
    std::condition_variable * release_cv, * retry_cv;
    lock_state():
      s(NONE), revoke(false), retry(false), thread_id(0), acquire_cnt(0),
      release_cv(new std::condition_variable()),
      retry_cv(new std::condition_variable()) {}
    lock_state(lock_state &&rr):
      s(rr.s), revoke(rr.revoke), retry(rr.retry), thread_id(rr.thread_id),
      acquire_cnt(rr.acquire_cnt), release_cv(rr.release_cv), retry_cv(rr.retry_cv) {
        rr.retry_cv = rr.release_cv = nullptr;
      }
    ~lock_state() {
      delete retry_cv;
      delete release_cv;
    }
  };

private:
  static std::map<lock_protocol::lockid_t, lock_state> lock_cache;
  static std::mutex global_lock;
  lock_release_user *lru;
  lock_acquire_user *lau;
  int rlock_port;
  std::string hostname;
  std::string id;

public:
  static int last_port;
  lock_client_cache(std::string xdst, lock_acquire_user* _lau = nullptr, lock_release_user *_lru = nullptr);
  virtual ~lock_client_cache() { delete lau; delete lru; }
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
};


#endif
