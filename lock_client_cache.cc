// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include <sstream>
#include <iostream>
#include <stdio.h>
#include <chrono>

#include "lock_client_cache.h"
#include "rpc/rpc.h"
#include "handle.h"
#include "tprintf.h"
typedef std::chrono::duration<int,std::milli> millsecs;

int lock_client_cache::last_port = 0;
std::map<lock_protocol::lockid_t, lock_client_cache::lock_state>
  lock_client_cache::lock_cache;
std::mutex lock_client_cache::global_lock;

lock_client_cache::lock_client_cache(std::string xdst,
              lock_acquire_user *_lau, lock_release_user *_lru)
  : lock_client(xdst), lau(_lau), lru(_lru)
{
  // set up a random port to receive rpc requests
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname = "127.0.0.1";
  // VERIFY(gethostname(hname, 100) == 0);
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  // spawn the rpc client as usual
  sockaddr_in dstsock;
  make_sockaddr(xdst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if(cl->bind() < 0) {
    tprintf("[cid=%s]\tbind failed", id.c_str());
  }
  else tprintf("[id=%s %d]\tbind successful\n", id.c_str(), cl->id());
}

// will return true no matter what
lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  std::unique_lock<std::mutex> glock(global_lock, std::defer_lock);
  pthread_t thread_id = pthread_self();
  lock_protocol::status ret;
  int r = true, r2;
  lock_state &state = lock_cache[lid];
  bool flag;
  do {
    glock.lock();
    flag = false;
    switch(state.s) {
      case ACQUIRING:
        if(state.thread_id == 0)
          // should resend acquire rpc
          goto case_none;
        else {
          // this cannot happen
          assert(state.thread_id != pthread_self());
          // it is already being acquired
          // but rpc haven't returned yet,
          // so just wait
          goto case_locked;
        }
        break;
      case NONE:
      case_none:
        cltputs("NONE: trying to acquire lock");
        // acquire the lock from server
        // reenter the switch after state change
        state.acquire_cnt = 0;
        state.retry = state.revoke = false;
        state.thread_id = 0;
        do {
          state.s = ACQUIRING;
          glock.unlock();
          cltputs("sending acquire rpc");
          ret = cl->call(lock_protocol::acquire, lid, id, r2);
          glock.lock();
          if(ret == lock_protocol::OK) {
            // success
            cltputs("acquire rpc success");
            state.s = LOCKED;
            state.acquire_cnt = 1;
            state.thread_id = thread_id;
            if(lau) lau->doacquire(lid);
          } else if (ret == lock_protocol::RETRY) {
            // have to wait for retry
            // keep waiting until retry bit is set
            // state.s = NONE;
            cltputs("acquire rpc failed, waiting for retry...");
            while(!state.retry)
              state.retry_cv->wait_for(glock, millsecs(20));
            cltputs("retry received");
            assert(state.s == FREE);
            // grant the lock
            state.retry = false;  // turn off this bit
            flag = true;          // reenter the switch
          } else if (ret == lock_protocol::RPCERR) {
            cltputs("Warning: remote tells that lock is already acquired.");

          }
        } while (ret == lock_protocol::RPCERR);
        break;
      case FREE:
        // just acquire it
        cltputs("FREE: give lock directly");
        state.s = LOCKED;
        state.acquire_cnt = 1;
        state.thread_id = thread_id;
        break;
      case LOCKED:
      case_locked:
        if(state.thread_id != thread_id) {
          // not me, cond wait until released
          cltputs("LOCKED/ACQUIRING: waiting for release");
          flag = true;  // reenter the procedure
          state.release_cv->wait(glock);
        } else {
          if(state.s == LOCKED) {
            // reacquired by same thread, just add
            cltputs("LOCKED: reacquire by same thread, recursive");
            state.acquire_cnt++;
          } else {
            // being acquiring by same thread, this is abnormal
            cltputs("ACQUIRING: Error: reacquiring a lock which is being acquired.");
          }
        }
        break;
      case RELEASING:
        // shouldn't resend acquire rpc, could cause race
        cltputs("RELEASING: wait for releasing to complete");
        state.release_cv->wait_for(glock, millsecs(100));
        flag = true;
        break;
      default: break;
    }
    glock.unlock();
  } while(flag);
  return r;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  std::unique_lock<std::mutex> glock(global_lock, std::defer_lock);
  int r = true, r2;
  lock_protocol::status ret;
  lock_state &state = lock_cache[lid];

  glock.lock();
  switch(state.s) {
    case NONE:
    case FREE:
    case ACQUIRING:
      cltputs("error: trying to release a lock that hasn't been acquired yet");
      break;
    case RELEASING:
      cltputs("warning: trying to release a lock being released");
      break;
    case LOCKED:
      if(state.thread_id == pthread_self()) {
        if(--state.acquire_cnt == 0) {
          if(state.revoke) {
            cltputs("LOCKED: lock being revoked, so release rpc");
            // begin revoked, so set releasing
            state.s = RELEASING;
            // turn it off
            state.acquire_cnt = 0;
            state.thread_id = 0;
            state.revoke = false;
            state.retry = false;
            // do release handler
            if(lru) lru->dorelease(lid);
            // send rpc now
            glock.unlock();
            cltputs("sending release rpc");
            ret = cl->call(lock_protocol::release, lid, id, r2);
            glock.lock();
            cltputs("release rpc returned");
            if(ret == rlock_protocol::RPCERR) {
              // remote says this lock doesn't belong to you
              // cache inconsistency, release it anyway...
              cltputs("warning: remote says lock doesn't belong to oneself.");
            }
            if(state.s == RELEASING)
              state.s = NONE;
          } else {
            // simply free it
            cltputs("lock cached.");
            state.s = FREE;
          }
          // notify all friends to come
          state.release_cv->notify_all();
        } else cltputs("recursive release");
      } else
        cltprintf("warning: trying to release a lock that doesn't belong to itself.");
      break;
  }
  glock.unlock();
  return r;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  // handle revoke here
  std::unique_lock<std::mutex> glock(global_lock, std::defer_lock);
  int ret = rlock_protocol::OK;
  lock_state &state = lock_cache[lid];
  glock.lock();
  // set revoke to true first
  // revoke bit is already assumed - return OK
  if(state.revoke == true)
    ret = rlock_protocol::OK;
  else {
    state.revoke = true;
    if(state.retry == true) {
      // retry received before,
      // should be responeded by acquire instead
      ret = rlock_protocol::WAIT; 
    } else {
      switch(state.s) {
        case FREE:
        case RELEASING: // idempotent
          // revoke it right away
          cltputs("FREE/RELEASING: modify it right away and send OK");
          if(lru) lru->dorelease(lid);
          state.revoke = false;
          state.retry = false;
          state.acquire_cnt = 0;
          state.thread_id = 0;
          state.s = NONE;
          ret = rlock_protocol::OK;
          break;
        case NONE:
          cltputs("NONE: warning: a none lock being revoked. "
            "Will assume acquired and return WAIT");
          ret = rlock_protocol::WAIT;
          break;
        case LOCKED:
        case ACQUIRING:
          // cond wait for release...
          // revoke bit already set
          cltputs("LOCKED/ACQUIRING: return RETRY and send release later")
          // tell server to hold until a release rpc is sent
          ret = rlock_protocol::WAIT;
          break;
        default: break;
      }
    }
  }
  glock.unlock();
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  std::lock_guard<std::mutex> glock_guard(global_lock);
  int ret = rlock_protocol::OK;
  lock_state &state = lock_cache[lid];

  cltputs("retry received, acquiring lock & notifying others...");
  // set retry bit here
  // in case retry rpc arrive before cond variable wait started
  state.retry = true;
  if(state.s != NONE && state.s != ACQUIRING) {
    cltputs("warning: retry on an already cached lock, will assume previously revoked.");
    state.revoke = true;
  }
  state.s = FREE;
  state.acquire_cnt = 0;
  state.thread_id = 0;
  if(lau) lau->doacquire(lid);
  state.retry_cv->notify_all();
  return ret;
}
