// RPC stubs for clients to talk to lock_server
#include "lock_client.h"
#include "rpc/rpc.h"
#include <arpa/inet.h>

#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>

lock_client::lock_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() < 0) {
    printf("lock_client: call bind\n");
  }
}

int lock_client::stat(lock_protocol::lockid_t lid)
{
  int r;
  printf("lock client: %d stat %llu\n", cl->id(), lid);
  lock_protocol::status ret = cl->call(lock_protocol::stat, cl->id(), lid, r);
  VERIFY (ret == lock_protocol::OK);
  return r;
}

lock_protocol::status lock_client::acquire(lock_protocol::lockid_t lid)
{
	// Your lab2 part2 code goes here
  int r = true;
  printf("lock client: %d trying to get %llu\n", cl->id(), lid);
  if(acquired[lid] == 0) {
    // not acquired
    int r;
    lock_protocol::status ret;
    ret = cl->call(lock_protocol::acquire, cl->id(), lid, r);
    VERIFY (ret == lock_protocol::OK);
    VERIFY (r == true);
  }
  acquired[lid]++;
  return r;
}

lock_protocol::status lock_client::release(lock_protocol::lockid_t lid)
{
	// Your lab2 part2 code goes here
  int r = true;
  printf("lock client: %d trying to release %llu\n", cl->id(), lid);  
  if(acquired[lid] == 0) {
    // directly return
    printf("Warning: Trying to release %llu which wasn't previously acquired.\n", lid);
    r = false;
  } else if(acquired[lid] > 1) {
    acquired[lid]--;
  } else {
    int ret = cl->call(lock_protocol::release, cl->id(), lid, r);
    VERIFY (ret == lock_protocol::OK);
    VERIFY (r == true);
    acquired[lid]--;
  }
  return r;
}

