// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>

// acquire handler
void lock_acquire_handler::doacquire(lock_protocol::lockid_t lid) {
    if(clt->cache_table.find(lid) != clt->cache_table.end()) {
        printf("warning: cache not flushed before, lid=%llu\n", lid);
        clt->erase(lid);
    }
}

// revoke handler
void lock_release_handler::dorelease(lock_protocol::lockid_t lid) {
    clt->flush(lid);
}

yfs_client::yfs_client(std::string extend_dst, std::string lock_dst)
{
    ec = new extent_client(extend_dst);
    lc = new lock_client_cache(
        lock_dst, new lock_acquire_handler(ec), new lock_release_handler(ec));
    
    // init root dir
    extent_protocol::attr root_attr;
    lc->acquire(1);
    ec->getattr(1, root_attr);
    assert(root_attr.type == extent_protocol::T_DIR);
    if(root_attr.size == 0) {
        printf("yfs_client: initializing root dir...\n");
        // root directory not initailized yet
        std::string buf;
        assert(ec->get(1, buf) == extent_protocol::OK);
        buf.resize(sizeof(directory) + 2 * sizeof(directory_entry));
        yfs_client::directory *dir = (yfs_client::directory *)buf.data();
        addSubFile(dir, ".", 1);
        addSubFile(dir, "..", 1);
        dir->cnt = 2;
        assert(ec->put(1, buf) == extent_protocol::OK);
    } else {
        // already initialized
        printf("yfs_client: root dir already initalized.\n");
    }
    lc->release(1);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    // c++11 feature
    return std::stoull(n);
}

std::string
yfs_client::filename(inum inum)
{
    // c++11 feature
    return std::to_string(inum);
}

#define EXT_RPC_BOOL(_xx) do { \
    if ((_xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        return false; \
    } \
} while (0)

#define EXT_RPC(_xx) do { \
    if ((_xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        return IOERR; \
    } \
} while (0)

int
yfs_client::getattr(inum inum, extent_protocol::attr &ret)
{
    lc->acquire(inum);
    EXT_RPC(ec->getattr(inum, ret));
    if (ret.type == extent_protocol::T_FILE) {
        printf("%08lld is a file\n", inum);
    } else if (ret.type == extent_protocol::T_SYMLINK)
        printf("%08lld is a symlink\n", inum);
    else if (ret.type == extent_protocol::T_DIR)
        printf("%08lld is a dir\n", inum);
    else {
        printf("%08lld error: unknown file type %d", inum, ret.type);
        return 1;
    }
    lc->release(inum);
    return 0;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * */

// bool
// yfs_client::isdir(inum inum)
// {
//     extent_protocol::attr a;
//     EXT_RPC_BOOL(ec->getattr(inum, a));
//     return a.type == extent_protocol::T_DIR;
// }


// bool
// yfs_client::issymlink(inum inum)
// {
//     extent_protocol::attr a;
//     EXT_RPC_BOOL(ec->getattr(inum, a));
//     return a.type == extent_protocol::T_SYMLINK;
// }

int
yfs_client::getfile(inum inum, fileinfo &fin, extent_protocol::attr &attr)
{
    fin.atime = attr.atime;
    fin.mtime = attr.mtime;
    fin.ctime = attr.ctime;
    fin.size = attr.size;
    return OK;
}

int
yfs_client::getdir(inum inum, dirinfo &din, extent_protocol::attr &attr)
{
    printf("getdir %08llx\n", inum);

    din.atime = attr.atime;
    din.mtime = attr.mtime;
    din.ctime = attr.ctime;
    return OK;
}


// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    printf("setattr %08llx\n", ino);
    lc->acquire(ino);

    extent_protocol::attr a;
    EXT_RPC(ec->getattr(ino, a));

    if(a.size != size) {
        // expand or shrink
        std::string buf;
        EXT_RPC(ec->get(ino, buf));
        buf.resize(size);
        EXT_RPC(ec->put(ino, buf));
    }
    lc->release(ino);

    return OK;
}

void
yfs_client::addSubFile(yfs_client::directory *dir, const std::string filename, const inum ino)
{
    printf("adding subfile %s\n", filename.c_str());
    strcpy(dir->entries[dir->cnt].dirname, filename.c_str());
    dir->entries[dir->cnt].inum = ino;
    (dir->cnt)++;
}

void
yfs_client::removeSubFile(yfs_client::directory *dir, const std::string filename)
{
    printf("removing subfile %s\n", filename.c_str());
    for(unsigned int i = 0; i < dir->cnt; i++) {
        if(strcmp(filename.c_str(), dir->entries[i].dirname) == 0) {
            // found, remove it
            memmove(&(dir->entries[i]),
                &(dir->entries[i + 1]),
                (dir->cnt - i - 1) * sizeof(directory_entry));
            --(dir->cnt);
            return;
        }
    }
    // not found
    printf("subfile %s not found.\n", filename.c_str());
    return;
}

int
yfs_client::addFile(inum parent, const char *name, extent_protocol::types type, inum &ino_out)
{
    printf("create file under ino[%08llx]\n", parent);
    std::string buf;
    extent_protocol::extentid_t id;

    lc->acquire(parent);
    // look it up first
    bool found = false;
    lookup(parent, name, found, ino_out);
    if(found == true) {
        puts("Creation failed: file already exist.");
        lc->release(parent);
        return EXIST;
    }

    // create it right away
    EXT_RPC(ec->create(type, id));
    // add entry to parent directory
    EXT_RPC(ec->get(parent, buf));
    buf.resize(buf.length() + sizeof(directory_entry));
    addSubFile((directory *)(buf.data()), name, id);
    EXT_RPC(ec->put(parent, buf));

    lc->release(parent);
    // return them
    ino_out = id;
    return OK;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent information.
     */
    return addFile(parent, name, extent_protocol::T_FILE, ino_out);
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int ret = addFile(parent, name, extent_protocol::T_DIR, ino_out);
    if(ret != OK) {
        return ret;
    }
        
    puts("mkdir: writing directory.");
    // create content in memory
    std::string buf;
    directory *dir;
    buf.resize(sizeof(directory));
    dir = (directory *)(buf.data());
    dir->cnt = 0;
    // write it to disk

    lc->acquire(ino_out);
    EXT_RPC(ec->put(ino_out, buf));
    lc->release(ino_out);

    return OK;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    std::string buf;
    const directory *dir;
    found = false;
    printf("looking up file %s under parent %llu\n", name, parent);
    lc->acquire(parent);
    extent_protocol::attr attr;
    getattr(parent, attr);
    if(attr.type != extent_protocol::T_DIR) {
        lc->release(parent);
        return NOENT;
    }

    EXT_RPC(ec->get(parent, buf));
    lc->release(parent);
    dir = (const directory *)buf.c_str();
    // first two are . and ..
    for(unsigned int i = 0; i < dir->cnt; i++) {
        if(strcmp(name, dir->entries[i].dirname) == 0) {
            found = true;
            ino_out = dir->entries[i].inum;
            printf("\t[%llu]%s\n", dir->entries[i].inum, dir->entries[i].dirname);
            break;
        }
    }
    return found ? EXIST : NOENT;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    std::string buf;
    const directory *dirp;

    lc->acquire(dir);
    // check if the inode is a directory
    extent_protocol::attr attr;
    getattr(dir, attr);
    if(attr.type != extent_protocol::T_DIR) {
        lc->release(dir);
        return NOENT;
    }
    
    // read directory content
    list.clear();
    EXT_RPC(ec->get(dir, buf));
    lc->release(dir);

    // translate binary content and output
    dirp = (const directory *)(buf.c_str());
    printf("Reading dir, cnt = %d\n", dirp->cnt);
    for(unsigned int i = 0; i < dirp->cnt; i++) {
        dirent ent;
        ent.name = dirp->entries[i].dirname;
        ent.inum = dirp->entries[i].inum;
        list.push_back(ent);
        printf("\t[%llu]%s\n", dirp->entries[i].inum, dirp->entries[i].dirname);
    }
    return OK;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    /*
     * your code goes here.
     * note: read using ec->get().
     */
    std::string buf;
    lc->acquire(ino);
    EXT_RPC(ec->get(ino, buf));
    if(off + size > buf.length())
        data = buf.substr(off);
    else
        data = buf.substr(off, size);
    lc->release(ino);
    return OK;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    std::string buf;
    bytes_written = 0;
    
    lc->acquire(ino);
    EXT_RPC(ec->get(ino, buf));
    if(size + off > buf.size())
        buf.resize(off + size, '\0');
    bytes_written = size;
    memcpy((void *)(buf.data() + off), (void *)data, size);
    EXT_RPC(ec->put(ino, buf));
    lc->release(ino);
    
    return OK;
}

int yfs_client::unlink(inum parent, const char *name)
{
    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    bool found;
    inum ino;
    printf("unlinking %s from parent %08lld\n", name, parent);
    lc->acquire(parent);
    int ret = lookup(parent, name, found, ino);
    if(ret != EXIST) {
        lc->release(parent);
        return ret;
    }
    
    // remove entry in parent
    puts("lookup complete, try to unlink.");
    std::string buf;
    EXT_RPC(ec->get(parent, buf));
    directory *dir = (directory *)(buf.data());
    removeSubFile(dir, name);
    buf.resize(buf.length() - sizeof(directory_entry));
    EXT_RPC(ec->put(parent, buf));
    EXT_RPC(ec->remove(ino));
    lc->release(parent);
    return OK;
}

int yfs_client::ln(inum parent, const char *name, const char *link, inum &ino)
{
    /*
     * create symbolic link
     */
    printf("Creating symlink '%s' to '%s'\n", name, link);

    int ret = addFile(parent, name, extent_protocol::T_SYMLINK, ino);
    if(ret != OK)
        return ret;
    
    puts("Writing symlink content");
    std::string buf(link);
    lc->acquire(ino);
    EXT_RPC(ec->put(ino, buf));
    lc->release(ino);
    return OK;
}

int yfs_client::readlink(inum ino, std::string &buf) {
    /*
     * read symlink
     * the caller should handle allocated memory
     */
    printf("Reading link %08lld\n", ino);
    lc->acquire(ino);
    extent_protocol::attr attr;
    getattr(ino, attr);
    if(attr.type != extent_protocol::T_SYMLINK) {
        lc->release(ino);
        return IOERR;
    } else {
        EXT_RPC(ec->get(ino, buf));
        lc->release(ino);
        return OK;
    }
}