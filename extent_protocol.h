/*
 * extent wire protocol
 */
#ifndef extent_protocol_h
#define extent_protocol_h


#include "rpc.h"

class extent_protocol {
 public:
  typedef int status;
  typedef unsigned long long extentid_t;
  enum xxstatus { 
    OK = 0,
    RPCERR, NOENT, IOERR };
  enum rpc_numbers {
    put = 0x6001,
    get,
    getattr,
    getall,
    remove,
    create
  };

  enum types {
    T_DIR = 1,
    T_FILE,
    T_SYMLINK
  };

  struct attr {
    uint32_t type = 0;
    unsigned int atime = 0;
    unsigned int mtime = 0;
    unsigned int ctime = 0;
    unsigned int size = 0;
  };
  
  struct fullinfo {
    extent_protocol::attr attr;
    std::string buf;
  };
};

class rextent_protocol {
  public:
  typedef int status;
  typedef unsigned long long extentid_t;
  enum xxstatus { 
    OK = 0,
    RPCERR, NOENT, IOERR };
  enum rpc_numbers {
    put = 0x6001,
    get,
    getattr,
    remove,
    create
  };

  enum types {
    T_DIR = 1,
    T_FILE,
    T_SYMLINK
  };

  struct attr {
    uint32_t type;
    unsigned int atime;
    unsigned int mtime;
    unsigned int ctime;
    unsigned int size;
    attr(): type(0), atime(0), mtime(0), ctime(0), size(0) {}
  };
};

inline unmarshall &
operator>>(unmarshall &u, extent_protocol::attr &a)
{
  u >> a.type;
  u >> a.atime;
  u >> a.mtime;
  u >> a.ctime;
  u >> a.size;
  return u;
}

inline marshall &
operator<<(marshall &m, extent_protocol::attr a)
{
  m << a.type;
  m << a.atime;
  m << a.mtime;
  m << a.ctime;
  m << a.size;
  return m;
}


inline unmarshall &
operator>>(unmarshall &u, extent_protocol::fullinfo &inf)
{
  u >> inf.attr;
  u >> inf.buf;
  return u;
}

inline marshall &
operator<<(marshall &m, extent_protocol::fullinfo &inf)
{
  m << inf.attr;
  m << inf.buf;
  return m;
}

#endif 
