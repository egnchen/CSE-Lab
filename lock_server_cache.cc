// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

lock_server_cache::lock_server_cache():
  nacquire(0)
{
  // handle object already spawned
  // don't need to do anything here
  tprintf("lock server cache starting...\n");
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  lock_protocol::status ret = lock_protocol::OK, ret2;
  lock_data &lock = locks[lid];
  rpcc *clt = handle(id).safebind();
  lock.m->lock();
  cltputs("lock acquired, begin acquire");
  if(lock.holder == nullptr) {
    // no one is holding the lock
    cltputs("no one is holding lock, give directly");
    lock.holder = clt;
  } else {
    if(lock.holder == clt) {
      cltputs("warning: trying to acquire an already acquired lock.");
    } else {
      bool is_empty = lock.waiting.empty();
      lock.waiting.push(clt);
      if(is_empty) {
        // sends a revoke
        lock.m->unlock();
        cltputs("revoking...");
        lock.waiting.front()->call(rlock_protocol::revoke, lid, ret2);
        if(ret2 == rlock_protocol::OK) {
          // revoked, grant it now
          cltputs("revokation successful.");
          rpcc *n = lock.waiting.front();
          lock.m->lock();
          lock.holder = nullptr;
          lock.waiting.pop();
          lock.m->unlock();
          // send retry now
          cltputs("sending retry...");
          n->call(rlock_protocol::retry, lid, ret2);
        } else if(ret2 == rlock_protocol::WAIT) {
          // wait until released, so send back a retry
          cltputs("client says wait, so tell current client RETRY");
          ret = lock_protocol::RETRY;
        }
      } else {
        // send back a retry
        cltputs("many waiting, sending RETRY");
        ret = lock_protocol::RETRY;
      }
    }    
  }
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK, ret2;
  lock_data &lock = locks[lid];
  rpcc *clt = handle(id).safebind();

  lock.m->lock();
  cltputs("lock acquired, begin release");
  if(lock.holder != clt) {
    cltputs("error: lock not claimed by client");
    ret = lock_protocol::RPCERR;
    lock.m->unlock();
  } else {
    // release the lock
    rpcc *nclt = lock.waiting.front();
    lock.holder = nullptr;
    lock.waiting.pop();
    lock.m->unlock();
    nclt->call(rlock_protocol::retry, lid, ret2);
  }
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

