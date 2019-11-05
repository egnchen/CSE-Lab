#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// RPC stubs for clients to talk to extent_server

extent_client::extent_client(std::string servdst)
{
  // connect with lock server
  // connect with rpc server
  sockaddr_in dstsock;
  make_sockaddr(servdst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    puts("extent_client: bind failed");
  } else {
    puts("extent_client: bind successful");
  }
}

// a demo to show how to use RPC
extent_protocol::status extent_client::create(
  const uint32_t type, 
  extent_protocol::extentid_t &eid)
{
  // Your lab2 part1 code goes here
  puts("extent client: calling create");
  int ret = cl->call(extent_protocol::create, type, eid);
  assert(ret == extent_protocol::OK);
  return extent_protocol::OK;
}

extent_protocol::status extent_client::get(
  const extent_protocol::extentid_t eid,
  std::string &buf)
{
  // Your lab2 part1 code goes here
  puts("extent client: calling get");
  int ret = cl->call(extent_protocol::get, eid, buf);
  assert(ret == extent_protocol::OK);
  return extent_protocol::OK;
}

extent_protocol::status extent_client::getattr(
  const extent_protocol::extentid_t eid, 
	extent_protocol::attr &attr)
{
  puts("extent client: calling getattr");
  int ret = cl->call(extent_protocol::getattr, eid, attr);
  assert(ret == extent_protocol::OK);
  return extent_protocol::OK;
}

extent_protocol::status extent_client::put(
  const extent_protocol::extentid_t eid,
  const std::string &buf)
{
  // Your lab2 part1 code goes here
  puts("extent client: calling put");
  int r;
  int ret = cl->call(extent_protocol::put, eid, buf, r);
  assert(ret == extent_protocol::OK);
  return r;
}

extent_protocol::status extent_client::remove(
  const extent_protocol::extentid_t eid)
{
  // Your lab2 part1 code goes here
  puts("extent client: calling remove");
  int r;
  int ret = cl->call(extent_protocol::remove, eid, r);
  assert(ret == extent_protocol::OK);
  return r;
}


