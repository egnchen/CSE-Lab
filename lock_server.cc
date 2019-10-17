// the lock server implementation
#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
// singleton lock table
std::map<lock_protocol::lockid_t, lock_server::LockEntry> lock_server::locks;
std::mutex lock_server::g_mutex;

lock_server::lock_server():
  nacquire(0)
{
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  printf("lock server: stat %llu request from clt %d\n", lid, clt);
  {
    std::unique_lock<std::mutex> lock(g_mutex);

    auto ref = locks.find(lid);
    if(ref == locks.end())
      r = 0;  // not locked
    else {
      r = ref->second.clt == 0;
    }
  }
  return lock_protocol::OK;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  printf("lock server: acquire %llu by %d\n", lid, clt);
  {
    std::unique_lock<std::mutex> lock(g_mutex);
    auto ref = locks.find(lid);
    if(ref == locks.end()) {
      locks[lid] = LockEntry(0, new std::condition_variable());
      ref = locks.find(lid);
    }
    if(ref->second.clt != 0) {
      printf("current cltid = %d, waiting for release\n", ref->second.clt);
      ref->second.v->wait(lock);
    }
    ref->second.clt = clt;
    r = true;
  }
  return lock_protocol::OK;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  printf("lock server: release %llu by %d ", lid, clt);
  {
    std::unique_lock<std::mutex> lock(g_mutex);
    auto ref = locks.find(lid);
    if(ref == locks.end() || ref->second.clt != clt) {
      puts("cannot release: wrong client id.");
      r = false;  // cannot release
    } else {
      puts("releasing...");
      ref->second.clt = 0;
      r = true;
      ref->second.v->notify_one();
    }
  }
  return lock_protocol::OK;
}
