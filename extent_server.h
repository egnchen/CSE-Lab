// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "inode_manager.h"

class extent_server {
 protected:
#if 0
  typedef struct extent {
    std::string data;
    struct extent_protocol::attr attr;
  } extent_t;
  std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
  inode_manager *im;

 public:
  extent_server();

  extent_protocol::status create  (uint32_t type, extent_protocol::extentid_t &id);
  extent_protocol::status put     (extent_protocol::extentid_t id, const std::string buf, int &ret);
  extent_protocol::status get     (extent_protocol::extentid_t id, std::string &buf);
  extent_protocol::status getattr (extent_protocol::extentid_t id, extent_protocol::attr &attr);
  extent_protocol::status remove  (extent_protocol::extentid_t id, int &ret);
};

#endif 







