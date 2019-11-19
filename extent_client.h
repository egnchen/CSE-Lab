// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"
#include "lock_client.h"
#include <map>

struct InodeBuf {
public:
  // we don't need the dirty bit since it'll always be flushed
  // attr will always be valid, the question is
  // whether data exists or not
  extent_protocol::attr attr;
  bool data_valid = false;
  std::string data_buf;     // i don't want to deal with the mess of memory allocation...
  InodeBuf(): attr(), data_valid(false), data_buf(){}
};

class extent_client {
 private:
  rpcc *cl;
  lock_client *lck;
  static int last_port;
  int rextent_port;
  std::string id;
  
 public:
  std::map<extent_protocol::extentid_t, InodeBuf> cache_table;
  extent_client(std::string dst);

  extent_protocol::status create  (uint32_t type, extent_protocol::extentid_t &eid);
  extent_protocol::status get     (extent_protocol::extentid_t eid, std::string &buf);
  extent_protocol::status getattr (extent_protocol::extentid_t eid, extent_protocol::attr &attr);
  extent_protocol::status put     (extent_protocol::extentid_t eid, const std::string &buf);
  extent_protocol::status remove  (extent_protocol::extentid_t eid);
  extent_protocol::status flush   (extent_protocol::extentid_t eid);
  void erase(extent_protocol::extentid_t eid);
};

#endif 

