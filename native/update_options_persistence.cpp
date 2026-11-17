#include "update_options_persistence.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
namespace olex2::harmony::ui {
namespace {
struct Observed {bool exists=false;struct stat data{};};
Observed Observe(const std::string& path){
  Observed result;
  if(lstat(path.c_str(),&result.data)==0){
    result.exists=true;
    if(!S_ISREG(result.data.st_mode))throw std::runtime_error("Update settings destination is not a regular file");
  }else if(errno!=ENOENT)throw std::runtime_error("Update settings destination cannot be inspected");
  return result;
}
bool Same(const Observed& a,const Observed& b){
  if(a.exists!=b.exists)return false;if(!a.exists)return true;
  const auto& x=a.data;const auto& y=b.data;
#if defined(__APPLE__)
  const auto xm=x.st_mtimespec,ym=y.st_mtimespec,xc=x.st_ctimespec,yc=y.st_ctimespec;
#else
  const auto xm=x.st_mtim,ym=y.st_mtim,xc=x.st_ctim,yc=y.st_ctim;
#endif
  return x.st_dev==y.st_dev&&x.st_ino==y.st_ino&&x.st_mode==y.st_mode&&x.st_size==y.st_size&&xm.tv_sec==ym.tv_sec&&xm.tv_nsec==ym.tv_nsec&&xc.tv_sec==yc.tv_sec&&xc.tv_nsec==yc.tv_nsec;
}
struct Temporary {
  std::string name;bool committed=false;
  ~Temporary(){if(!committed&&!name.empty())unlink(name.c_str());}
};
struct Descriptor {int fd=-1;explicit Descriptor(int value):fd(value){}~Descriptor(){if(fd>=0)close(fd);}};
}
void SaveUpdateOptionsAtomically(const std::string& destination,const std::function<bool(const std::string&)>& serialize){
  if(destination.empty()||destination.find('\0')!=std::string::npos||!serialize)throw std::invalid_argument("A settings destination and serializer are required");
  const auto before=Observe(destination);
  if(before.exists&&access(destination.c_str(),W_OK)!=0)throw std::runtime_error("Update settings are not writable");
  std::string pattern=destination+".harmony-XXXXXX";std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
  int created=mkstemp(name.data());if(created<0)throw std::runtime_error("Update settings temporary file could not be created");
  Temporary temporary{std::string(name.data())};
  if(close(created)!=0)throw std::runtime_error("Update settings temporary file could not be closed");
  if(!serialize(temporary.name))throw std::runtime_error("The original update settings serializer failed");
  Descriptor written(open(temporary.name.c_str(),O_RDWR|O_CLOEXEC|O_NOFOLLOW));
  if(written.fd<0)throw std::runtime_error("Serialized update settings could not be reopened");
  struct stat output{};
  if(fstat(written.fd,&output)!=0||!S_ISREG(output.st_mode))throw std::runtime_error("Serialized update settings are not a regular file");
  if(fchmod(written.fd,before.exists?(before.data.st_mode&0777):0600)!=0||fsync(written.fd)!=0)throw std::runtime_error("Serialized update settings could not be flushed");
  if(!Same(before,Observe(destination)))throw std::runtime_error("Update settings changed while saving; reload before retrying");
  if(std::rename(temporary.name.c_str(),destination.c_str())!=0)throw std::runtime_error("Update settings could not be replaced");
  temporary.committed=true;
}
}
