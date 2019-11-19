/*
extent client with cache

Take advantage of our cached lock client/server,
flush cache whenever the lock is *actually* released.
*/
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// port assign
int extent_client::last_port = 0;

// RPC stubs for clients to talk to extent_server

extent_client::extent_client(std::string servdst)
{
  // set up a random port to receive rpc requests
  // srand(time(NULL)^last_port);
  // last_port = rextent_port = ((rand()%32000) | (0x1 << 10));
  // const char *hname = "127.0.0.1";
  // std::ostringstream host;
  // host << hname << ":" << rextent_port;  
  // id = host.str();;
  // rpcs *rlsrpc = new rpcs(rextent_port);
  // TODO assign RPCs
  
  // connect with lock server
  // connect with rpc server
  sockaddr_in dstsock;
  make_sockaddr(servdst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    puts("\textent_client: bind failed");
  } else {
    puts("\textent_client: bind successful");
  }
}

extent_protocol::status extent_client::create(
  const uint32_t type, 
  extent_protocol::extentid_t &eid)
{
  // Your lab2 part1 code goes here
  printf("\textent client: create type=%u\n", type);
  // call directly
  int ret = cl->call(extent_protocol::create, type, eid);
  assert(ret == extent_protocol::OK);
  return extent_protocol::OK;
}

extent_protocol::status extent_client::get(
  const extent_protocol::extentid_t eid,
  std::string &buf)
{
  printf("\textent client: get %llu\n", eid);
  auto cache_ite = cache_table.find(eid);
  if(cache_ite != cache_table.end()) {
    puts("\tcache hit");
    InodeBuf &cache = cache_ite->second;
    if(cache.data_valid == false) {
      puts("\tcache data invalid, fetching data...");
      int ret = cl->call(extent_protocol::get, eid, cache.data_buf);
      assert(ret == extent_protocol::OK);
      cache.data_valid = true;
    }
    buf = cache.data_buf;
  } else {
    // no cache yet, rpc & return one
    puts("\tcache not found, fetching attr & data...");
    InodeBuf &cache = cache_table[eid];
    int ret = cl->call(extent_protocol::get, eid, cache.data_buf);
    assert(ret == extent_protocol::OK);
    ret = cl->call(extent_protocol::getattr, eid, cache.attr);
    assert(ret == extent_protocol::OK);
    cache.data_valid = true;
    buf = cache.data_buf;
  }
  return extent_protocol::OK;
}

extent_protocol::status extent_client::getattr(
  const extent_protocol::extentid_t eid, 
	extent_protocol::attr &attr)
{
  printf("\textent client: getattr %llu\n", eid);
  // just like get, but do not fetch file content
  auto cache_ite = cache_table.find(eid);
  if(cache_ite != cache_table.end()) {
    puts("\tcache hit");
    attr = cache_ite->second.attr;
  } else {
    puts("\tcache not found, fetching attr...");
    InodeBuf &cache = cache_table[eid];
    // call getattr
    int ret = cl->call(extent_protocol::getattr, eid, cache.attr);
    assert(ret == extent_protocol::OK);
    // TODO add dirty bit here
    attr = cache.attr;
    cache.data_valid = false;
  }
  return extent_protocol::OK;
}

extent_protocol::status extent_client::put(
  const extent_protocol::extentid_t eid,
  const std::string &buf)
{
  printf("\textent client: put %llu\n", eid);
  // put in cache
  auto cache_ite = cache_table.find(eid);
  InodeBuf *cache;
  if(cache_ite == cache_table.end()) {
    puts("\tcache doesn't exist yet, fetching attr...");
    int ret;
    cache = &(cache_table[eid]);
    // call getattr too
    ret = cl->call(extent_protocol::getattr, eid, cache->attr);
    assert(ret == extent_protocol::OK);
  } else cache = &(cache_ite->second);
  // write to cache, dirty bit automatically set
  cache->data_buf = buf;
  cache->data_valid = true;
  cache->attr.ctime = cache->attr.mtime = time(nullptr);
  cache->attr.size = buf.size();
  return extent_protocol::OK;
}

extent_protocol::status extent_client::remove(
  const extent_protocol::extentid_t eid)
{
  // no bullshit, remove it directly
  printf("\textent client: remove %llu\n", eid);
  int r;
  int ret = cl->call(extent_protocol::remove, eid, r);
  assert(ret == extent_protocol::OK);
  // invalidate the cache for good
  erase(eid);
  return extent_protocol::OK;
}

void extent_client::erase(
  const extent_protocol::extentid_t eid)
{
  cache_table.erase(eid);
}

extent_protocol::status extent_client::flush(
  const extent_protocol::extentid_t eid)
{
  // flush the cache to remote
  puts("\textent_client: calling flush");
  auto cache_entry = cache_table.find(eid);
  if(cache_entry == cache_table.end()) {
    // no cache
    puts("\twarning: flushing an entry that doesn't exist, will not flush.");
  } else {
    InodeBuf &cache = cache_entry->second;
    if(cache.data_valid == false) {
      puts("\tonly getattr was called, will not flush.");
    } else {
      puts("\tflushing(putting) to remote...");
      int r;
      int ret = cl->call(extent_protocol::put, eid, cache.data_buf, r);
      assert(ret == extent_protocol::OK);
    }
    // remove local cache
    cache_table.erase(eid);
    puts("\tflushed & removed.");
  }
  return extent_protocol::OK;
}

