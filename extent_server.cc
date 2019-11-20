/*
 * the extent server implementation
 */
#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "tprintf.h"

extent_server::extent_server() 
{
  im = new inode_manager();
}

int extent_server::create(uint32_t type, extent_protocol::extentid_t &id)
{
  // alloc a new inode and return inum
  tprintf("extent_server: create inode\n");
  id = im->alloc_inode(type);
  return extent_protocol::OK;
}

/*
 * int &ret is reserved here to avoid redundant (un)marshall
 */
int extent_server::put(extent_protocol::extentid_t id, const std::string buf, int &ret)
{
  tprintf("extent_server: put %llu, size=%lu\n", id, buf.size());
  id &= 0x7ffffffff;
  im->write_file(id, buf.c_str(), buf.size());
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  tprintf("extent_server: get %lld\n", id);

  id &= 0x7fffffff;
  int size = 0;
  char *cbuf = NULL;
  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }

  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &attr)
{
  tprintf("extent_server: getattr %lld\n", id);

  id &= 0x7fffffff;  
  memset(&attr, 0, sizeof(attr));
  im->getattr(id, attr);
  return extent_protocol::OK;
}


int extent_server::getall(extent_protocol::extentid_t id, extent_protocol::fullinfo &inf)
{
  tprintf("extent_server: get %lld\n", id);

  id &= 0x7fffffff;
  int size = 0;
  char *cbuf = NULL;
  im->read_file(id, &cbuf, &size);
  if (size == 0)
    inf.buf = "";
  else {
    inf.buf.assign(cbuf, size);
    free(cbuf);
  }

  memset(&(inf.attr), 0, sizeof(inf.attr));
  im->getattr(id, inf.attr);

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &ret)
{
  tprintf("extent_server: remove %lld\n", id);

  id &= 0x7fffffff;
  im->remove_file(id);
  ret = extent_protocol::OK;
  return extent_protocol::OK;
}

