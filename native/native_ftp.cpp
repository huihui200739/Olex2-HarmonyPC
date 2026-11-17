// Real binary FTP filesystem used by the original updater and unirun launcher.
// Valid ordinary file transfer only; does not add TLS or invoke application code.
#include "wxftpfs.h"
#include "eutf8.h"
#include "ememstream.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
std::string Utf8(const olxstr& text) { olxcstr bytes=TUtf8::Encode(text); return std::string(bytes.c_str(),bytes.Length()); }
olxstr Text(const std::string& bytes) { return TUtf8::Decode(olxcstr(bytes.c_str(),bytes.size())); }
class Socket {
  int fd_=-1;
public:
  explicit Socket(int fd=-1):fd_(fd) {}
  ~Socket() { close(); }
  Socket(const Socket&)=delete;
  Socket& operator=(const Socket&)=delete;
  Socket(Socket&& other) noexcept:fd_(other.release()) {}
  Socket& operator=(Socket&& other) noexcept { if(this!=&other){close();fd_=other.release();}return *this; }
  int get() const {return fd_;}
  int release() {int result=fd_;fd_=-1;return result;}
  void close() {if(fd_>=0){::close(fd_);fd_=-1;}}
};
void ReadySocket(int fd) {
  timeval timeout{30,0};
  if(setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))!=0 || setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout))!=0) throw std::runtime_error("socket timeout configuration failed");
#ifdef SO_NOSIGPIPE
  int one=1;if(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one))!=0)throw std::runtime_error("socket signal configuration failed");
#endif
}
Socket ConnectAddress(const sockaddr* address,socklen_t length) {
  Socket socket(::socket(address->sa_family,SOCK_STREAM,0));
  if(socket.get()<0)throw std::runtime_error("socket creation failed");
  ReadySocket(socket.get());int flags=fcntl(socket.get(),F_GETFL,0);
  if(flags<0 || fcntl(socket.get(),F_SETFL,flags|O_NONBLOCK)<0)throw std::runtime_error("socket connection setup failed");
  if(::connect(socket.get(),address,length)!=0) {
    if(errno!=EINPROGRESS)throw std::runtime_error("connection failed");
    pollfd descriptor{socket.get(),POLLOUT,0};int result;
    do{result=poll(&descriptor,1,30000);}while(result<0&&errno==EINTR);
    int error=0;socklen_t size=sizeof(error);
    if(result<=0 || getsockopt(socket.get(),SOL_SOCKET,SO_ERROR,&error,&size)!=0 || error!=0)throw std::runtime_error("connection did not complete");
  }
  if(fcntl(socket.get(),F_SETFL,flags)<0)throw std::runtime_error("socket mode restoration failed");
  return socket;
}
void SendAll(int fd,const char* data,size_t size) {
  while(size) {
    int flags=0;
#ifdef MSG_NOSIGNAL
    flags=MSG_NOSIGNAL;
#endif
    ssize_t sent=send(fd,data,size,flags);
    if(sent<0&&errno==EINTR)continue;
    if(sent<=0)throw std::runtime_error("socket send failed");
    data+=sent;size-=static_cast<size_t>(sent);
  }
}
struct Reply {int code;std::string text;};
struct ProgressScope {
  TwxFtpFileSystem& fs;TOnProgress progress;bool entered=false;
  ProgressScope(TwxFtpFileSystem& owner,const olxstr& action):fs(owner) {
    progress.SetAction(action);progress.SetMax(0);progress.SetPos(0);
    fs.OnProgress.Enter(&fs,&progress);entered=true;
  }
  ~ProgressScope(){if(entered){try{fs.OnProgress.Exit(&fs,&progress);}catch(...){}}}
  void update(uint64_t position){progress.SetPos(position);fs.OnProgress.Execute(&fs,&progress);}
  void finish(const char* action){progress.SetAction(action);if(entered){entered=false;fs.OnProgress.Exit(&fs,&progress);}}
};
}

struct TwxFtpFileSystem::NativeState {
  TUrl url;Socket control;sockaddr_storage peer{};socklen_t peer_length=0;
  std::string pwd;std::recursive_mutex mutex;
  explicit NativeState(const TUrl& source):url(source) {}
  void disconnect(){control.close();}
  std::string line() {
    std::string result;
    for(;;){char byte;ssize_t count=recv(control.get(),&byte,1,0);if(count<0&&errno==EINTR)continue;if(count<=0)throw std::runtime_error("control connection ended");if(byte=='\n')break;if(byte!='\r')result+=byte;if(result.size()>65536)throw std::runtime_error("reply exceeds supported line size");}
    return result;
  }
  Reply reply() {
    std::string first=line();if(first.size()<3 || first[0]<'1'||first[0]>'5'||first[1]<'0'||first[1]>'9'||first[2]<'0'||first[2]>'9')throw std::runtime_error("invalid FTP reply");
    Reply result{(first[0]-'0')*100+(first[1]-'0')*10+first[2]-'0',first};
    if(first.size()>3 && first[3]=='-') {std::string end=first.substr(0,3)+" ";for(;;){std::string next=line();result.text+='\n'+next;if(next.compare(0,end.size(),end)==0)break;if(result.text.size()>262144)throw std::runtime_error("reply exceeds supported size");}}
    return result;
  }
  Reply command(const std::string& text) {
    if(text.find_first_of("\r\n")!=std::string::npos || text.find('\0')!=std::string::npos)throw std::runtime_error("unsupported FTP command text");
    std::string bytes=text+"\r\n";SendAll(control.get(),bytes.data(),bytes.size());return reply();
  }
  static bool success(const Reply& value) {return value.code/100==2;}
  std::string current_directory() {
    Reply result=command("PWD");if(result.code!=257)throw std::runtime_error("PWD did not succeed");
    size_t start=result.text.find('"');if(start==std::string::npos)throw std::runtime_error("PWD has no directory");std::string value;
    for(size_t i=start+1;i<result.text.size();++i){if(result.text[i]=='"'){if(i+1<result.text.size()&&result.text[i+1]=='"'){value+='"';++i;}else return value;}else value+=result.text[i];}
    throw std::runtime_error("PWD directory is incomplete");
  }
  bool directory_exists(const std::string& path) {
    std::string previous=pwd;if(!success(command("CWD "+path)))return false;
    if(!success(command("CWD "+previous)))throw std::runtime_error("could not restore current directory");return true;
  }
  bool directories(const std::string& path) {
    if(path.empty()||path=="/"||path==".")return true;
    std::string prefix;size_t begin=0;if(path[0]=='/'){prefix="/";begin=1;}
    while(begin<path.size()) {size_t end=path.find('/',begin);std::string part=path.substr(begin,end==std::string::npos?end:end-begin);if(!part.empty()){if(!prefix.empty()&&prefix.back()!='/')prefix+='/';prefix+=part;if(!directory_exists(prefix)&&!success(command("MKD "+prefix)))return false;}if(end==std::string::npos)break;begin=end+1;}
    return true;
  }
  void ensure() {
    if(control.get()>=0)return;
    const std::string desired_directory=pwd;
    const TUrl& endpoint=url.HasProxy()?url.GetProxy():url;addrinfo hints{},*addresses=nullptr;hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;
    const std::string host=Utf8(endpoint.GetHost()),port=std::to_string(endpoint.GetPort());
    if(getaddrinfo(host.c_str(),port.c_str(),&hints,&addresses)!=0)throw std::runtime_error("FTP name resolution failed");
    std::unique_ptr<addrinfo,decltype(&freeaddrinfo)> owner(addresses,&freeaddrinfo);
    for(addrinfo* item=addresses;item;item=item->ai_next){try{control=ConnectAddress(item->ai_addr,item->ai_addrlen);break;}catch(const std::exception&){}}
    if(control.get()<0)throw std::runtime_error("FTP endpoint connection failed");
    peer_length=sizeof(peer);if(getpeername(control.get(),reinterpret_cast<sockaddr*>(&peer),&peer_length)!=0)throw std::runtime_error("FTP peer lookup failed");
    Reply greeting=reply();if(greeting.code==120)greeting=reply();if(greeting.code!=220)throw std::runtime_error("FTP greeting failed");
    std::string user=Utf8(url.GetUser());if(user.empty())user="anonymous";Reply login=command("USER "+user);
    if(login.code==331){std::string password=Utf8(url.GetPassword());if(password.empty()&&user=="anonymous")password="anonymous@";login=command("PASS "+password);}
    if(login.code!=230)throw std::runtime_error("FTP login failed");
    if(!success(command("TYPE I")))throw std::runtime_error("FTP binary mode failed");
    command("OPTS UTF8 ON");pwd=current_directory();std::string base=desired_directory.empty()?Utf8(url.GetPath()):desired_directory;
    if(!base.empty()){if(!directories(base)||!success(command("CWD "+base)))throw std::runtime_error("FTP base directory failed");pwd=current_directory();}
  }
  Socket passive() {
    Reply response=command("EPSV");unsigned port=0;
    if(response.code==229) {
      size_t start=response.text.find('('),end=response.text.find(')',start);
      if(start==std::string::npos||end==std::string::npos)throw std::runtime_error("EPSV endpoint missing");
      std::string value=response.text.substr(start+1,end-start-1);if(value.size()<5 || value[0]!=value[1]||value[0]!=value[2]||value.back()!=value[0])throw std::runtime_error("EPSV endpoint unsupported");
      port=static_cast<unsigned>(std::stoul(value.substr(3,value.size()-4)));
    } else {
      response=command("PASV");if(response.code!=227||peer.ss_family!=AF_INET)throw std::runtime_error("passive mode unavailable");
      size_t start=response.text.find('('),end=response.text.find(')',start);if(start==std::string::npos||end==std::string::npos)throw std::runtime_error("PASV endpoint missing");
      std::string value=response.text.substr(start+1,end-start-1);std::replace(value.begin(),value.end(),',',' ');std::istringstream input(value);unsigned fields[6];for(unsigned& v:fields){if(!(input>>v)||v>255)throw std::runtime_error("PASV endpoint unsupported");}port=fields[4]*256+fields[5];
    }
    if(port==0||port>65535)throw std::runtime_error("passive port unavailable");
    // A data connection belongs to the authenticated control peer, not a
    // separately advertised machine. EPSV is preferred for IPv4 and IPv6.
    sockaddr_storage address=peer;
    if(address.ss_family==AF_INET)reinterpret_cast<sockaddr_in*>(&address)->sin_port=htons(port);
    else if(address.ss_family==AF_INET6)reinterpret_cast<sockaddr_in6*>(&address)->sin6_port=htons(port);
    else throw std::runtime_error("unsupported FTP address family");
    return ConnectAddress(reinterpret_cast<sockaddr*>(&address),peer_length);
  }
  bool receive(const std::string& request,const std::function<bool(const char*,size_t)>& consumer) {
    Socket data=passive();Reply start=command(request);if(start.code==550)return false;if(start.code/100!=1)throw std::runtime_error("FTP data request failed");
    char buffer[65536];for(;;){ssize_t count=recv(data.get(),buffer,sizeof(buffer),0);if(count<0&&errno==EINTR)continue;if(count<0)throw std::runtime_error("FTP data read failed");if(count==0)break;if(!consumer(buffer,static_cast<size_t>(count))){data.close();disconnect();return false;}}
    data.close();Reply done=reply();if(done.code!=226&&done.code!=250)throw std::runtime_error("FTP transfer did not complete");return true;
  }
};

TwxFtpFileSystem::TwxFtpFileSystem(const TUrl& url,TwxZipFileSystem* zip):state_(new NativeState(url)),zip_(zip) {
  try{state_->ensure();SetBase(url.GetPath());}
  catch(const std::exception& error){delete state_;delete zip_;state_=nullptr;zip_=nullptr;throw TFunctionFailedException(__OlxSourceInfo,olxstr("Native FTP: ")+error.what());}
}
TwxFtpFileSystem::~TwxFtpFileSystem(){delete state_;delete zip_;}
olxstr TwxFtpFileSystem::NormaliseName(const olxstr& name) const {
  std::string value=Utf8(TEFile::UnixPath(name));std::string pwd=state_->pwd;
  if(value==pwd)return EmptyString();if(!pwd.empty()&&pwd.back()!='/')pwd+='/';
  if(!pwd.empty()&&value.compare(0,pwd.size(),pwd)==0)value.erase(0,pwd.size());
  else {std::string base=Utf8(GetBase());if(!base.empty()&&base!="/"&&value.compare(0,base.size(),base)==0)value.erase(0,base.size());}
  return Text(value);
}
olx_object_ptr<IInputStream> TwxFtpFileSystem::_DoOpenFile(const olxstr& source) {
  std::lock_guard<std::recursive_mutex> lock(state_->mutex);Break=false;olxstr normal=NormaliseName(source);
  if(zip_!=nullptr&&zip_->Exists(normal))return zip_->OpenFile(normal);
  ProgressScope progress(*this,olxstr("Downloading ")+normal);
  try {
    if(Break){progress.finish("Download cancelled");return 0;}state_->ensure();Reply size=state_->command("SIZE "+Utf8(normal));if(size.code==213)progress.progress.SetMax(std::stoull(size.text.substr(4)));
    olx_object_ptr<TEMemoryStream> result=new TEMemoryStream();
    bool success=state_->receive("RETR "+Utf8(normal),[&](const char* bytes,size_t count){if(Break)return false;result->Write(bytes,count);progress.update(result->GetSize());return !Break;});
    if(!success||Break){progress.finish(Break?"Download cancelled":"Download failed");return 0;}
    result->SetPosition(0);progress.finish("Download complete");return result.release();
  } catch(const std::exception&){state_->disconnect();progress.finish("Download failed");return 0;}
}
bool TwxFtpFileSystem::_DoesExist(const olxstr& name,bool forced) {
  std::lock_guard<std::recursive_mutex> lock(state_->mutex);olxstr normal=NormaliseName(name);
  if(zip_!=nullptr&&zip_->Exists(normal))return true;
  if(Index!=nullptr&&!forced)return Index->GetRoot().FindByFullName(normal)!=nullptr;
  try{state_->ensure();std::string value=Utf8(normal);if(value.empty())return true;if(state_->command("SIZE "+value).code==213)return true;if(state_->command("MLST "+value).code==250)return true;if(state_->directory_exists(value))return true;
    bool found=false;bool success=state_->receive("NLST "+value,[&](const char*,size_t size){found=found||size!=0;return true;});return success&&found;
  }catch(const std::exception&){state_->disconnect();return false;}
}
bool TwxFtpFileSystem::_DoDelFile(const olxstr& name){std::lock_guard<std::recursive_mutex> lock(state_->mutex);try{state_->ensure();return NativeState::success(state_->command("DELE "+Utf8(NormaliseName(name))));}catch(const std::exception&){state_->disconnect();return false;}}
bool TwxFtpFileSystem::_DoDelDir(const olxstr& name){std::lock_guard<std::recursive_mutex> lock(state_->mutex);try{state_->ensure();return NativeState::success(state_->command("RMD "+Utf8(NormaliseName(name))));}catch(const std::exception&){state_->disconnect();return false;}}
bool TwxFtpFileSystem::_DoNewDir(const olxstr& name){std::lock_guard<std::recursive_mutex> lock(state_->mutex);try{state_->ensure();return state_->directories(Utf8(NormaliseName(name)));}catch(const std::exception&){state_->disconnect();return false;}}
bool TwxFtpFileSystem::_DoAdoptFile(const TFSItem& source){olx_object_ptr<IInputStream> input=source.GetIndexFS().OpenFile(source.GetIndexFS().GetBase()+source.GetFullName());return input.ok()&&AdoptStream(input,TEFile::UnixPath(source.GetFullName()));}
bool TwxFtpFileSystem::_DoAdoptStream(IInputStream& input,const olxstr& destination) {
  std::lock_guard<std::recursive_mutex> lock(state_->mutex);Break=false;olxstr normal=NormaliseName(destination);ProgressScope progress(*this,olxstr("Uploading ")+normal);progress.progress.SetMax(input.GetSize());
  std::string temporary;bool committed=false;
  auto cleanup=[&](){if(!temporary.empty()&&!committed){try{state_->ensure();state_->command("DELE "+temporary);}catch(const std::exception&){state_->disconnect();}}};
  try {
    if(Break){progress.finish("Upload cancelled");return false;}state_->ensure();std::string name=Utf8(normal);size_t slash=name.rfind('/');if(slash!=std::string::npos&&!state_->directories(name.substr(0,slash)))throw std::runtime_error("FTP upload directory unavailable");
    static std::atomic<unsigned long long> sequence{0};temporary=name+".olex2-upload-"+std::to_string(getpid())+"-"+std::to_string(++sequence);
    Socket data=state_->passive();Reply start=state_->command("STOR "+temporary);if(start.code/100!=1)throw std::runtime_error("FTP upload request failed");
    char buffer[65536];while(input.GetPosition()<input.GetSize()) {if(Break){data.close();state_->disconnect();cleanup();progress.finish("Upload cancelled");return false;}size_t size=static_cast<size_t>(std::min<uint64_t>(sizeof(buffer),input.GetSize()-input.GetPosition()));input.Read(buffer,size);SendAll(data.get(),buffer,size);progress.update(input.GetPosition());}
    if(Break){data.close();state_->disconnect();cleanup();progress.finish("Upload cancelled");return false;}
    shutdown(data.get(),SHUT_WR);data.close();Reply done=state_->reply();if(done.code!=226&&done.code!=250)throw std::runtime_error("FTP upload did not complete");
    if(state_->command("RNFR "+temporary).code!=350 || !NativeState::success(state_->command("RNTO "+name)))throw std::runtime_error("FTP upload commit failed");
    committed=true;progress.finish("Upload complete");return true;
  }catch(const std::exception&){state_->disconnect();cleanup();progress.finish("Upload failed");return false;}
}
olx_object_ptr<TEFile> TwxFtpFileSystem::SaveFile(const olxstr& name){olx_object_ptr<IInputStream> input=OpenFile(name);if(!input.ok())return 0;olx_object_ptr<TEFile> output=TEFile::TmpFile();*output<<*input;output->SetPosition(0);return output.release();}
void TwxFtpFileSystem::SetZipFS(TwxZipFileSystem* zip){std::lock_guard<std::recursive_mutex> lock(state_->mutex);if(zip_!=zip){delete zip_;zip_=zip;}}
bool TwxFtpFileSystem::ChangeDir(const olxstr& directory){std::lock_guard<std::recursive_mutex> lock(state_->mutex);try{state_->ensure();if(!NativeState::success(state_->command("CWD "+Utf8(directory))))return false;state_->pwd=state_->current_directory();return true;}catch(const std::exception&){state_->disconnect();return false;}}
