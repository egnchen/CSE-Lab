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
  int r;
  lock_data &lock = locks[lid];
  rpcc *clt = handle(id).safebind();
  cltputs("::acquire: trying to get lock");
  lock.m->lock();
  if(lock.holder == nullptr) {
    // no one is holding the lock
    cltputs("no one is holding lock or pending, give directly");
    lock.holder = clt;
  } else {
    if(lock.holder == clt) {
      cltputs("warning: trying to acquire an already acquired lock, responding with RPCERR");
      ret = lock_protocol::RPCERR;
    } else {
      bool is_empty = lock.waiting.empty();
      lock.waiting.push(clt);
      if(is_empty) {
        // sends a revoke
        lock.m->unlock();
        cltprintf("revoking from %u\n", lock.holder->id());
        ret2 = lock.holder->call(rlock_protocol::revoke, lid, r);
        if(ret2 == rlock_protocol::OK) {
          // revoked, grant it now
          cltputs("revokation successful.");
          lock.m->lock();
          // grant it to n directly
          rpcc *n = lock.waiting.front();
          lock.holder = n;
          lock.waiting.pop();
          // send retry now
          // cltputs("sending retry...");
          // n->call(rlock_protocol::retry, lid, ret2);
        } else if(ret2 == rlock_protocol::WAIT) {
          // wait until released, so send back RETRY
          cltputs("client says wait, so tell current client RETRY");
          ret = lock_protocol::RETRY;
          return ret;
        }
      } else {
        // send back RETRY
        cltputs("many waiting, sending RETRY");
        ret = lock_protocol::RETRY;
      }
    }    
  }
  lock.m->unlock();
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
  } else {
    // release the lock
    lock.holder = nullptr;
    if(!lock.waiting.empty()) {
      rpcc *nclt = lock.waiting.front();
      lock.holder = nclt;
      lock.waiting.pop();
      cltprintf("sending retry to %u\n", nclt->id());
      nclt->call(rlock_protocol::retry, lid, ret2);
    }
  }
  lock.m->unlock();
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

