#ifndef yfs_client_h
#define yfs_client_h

#include <string>

#include "lock_protocol.h"
// #include "lock_client.h"
#include "lock_client_cache.h"

//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>
#include <mutex>

#define FILENAME_LENGTH 64

class yfs_client {
  extent_client *ec;
  lock_client *lc;
  std::mutex g_mutex;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
  
  struct directory_entry {
    char dirname[FILENAME_LENGTH];
    yfs_client::inum inum;
  };

  struct directory {
    unsigned int cnt; // min = 2
    struct directory_entry entries[0]; // placeholder for directory entries
  };

  void addSubFile(yfs_client::directory *dir, const std::string filename, const inum ino);
  void removeSubFile(yfs_client::directory *dir, const std::string filename);
  int addFile(inum, const char*, extent_protocol::types, inum &);
  
 public:
  yfs_client(std::string, std::string);

  int getattr(inum, extent_protocol::attr &);
  bool isdir(inum);
  bool issymlink(inum);

  int getfile(inum, fileinfo &, extent_protocol::attr &);
  int getdir(inum, dirinfo &, extent_protocol::attr &);

  int setattr(inum, size_t);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum,const char *);
  int mkdir(inum , const char *, mode_t , inum &);
  
  /** you may need to add symbolic link related methods here.*/
  int ln(inum, const char* name, const char *link, inum &);
  int readlink(inum, std::string &);
};

class lock_acquire_handler: public lock_acquire_user {
private:
  extent_client *clt;
public:
  lock_acquire_handler(extent_client *clt): clt(clt) {}
  void doacquire(lock_protocol::lockid_t lid);

  ~lock_acquire_handler() {}
};

class lock_release_handler: public lock_release_user {
private:
  extent_client *clt;
public:
  lock_release_handler(extent_client *clt): clt(clt) {}

  // declaration
  void dorelease(lock_protocol::lockid_t lid);

  ~lock_release_handler() {}
};

#endif 
