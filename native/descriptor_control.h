#pragma once
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <dirent.h>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#include <vector>
// Fixed-duration controls at explicitly named stages. No image API is called
// by this evaluation during the interval; asynchronous activity is still visible.
// Never wait for a favorable count or suppress a failed run.
inline std::string ObserveNoDecoderDescriptorControl(const char* stage="before-first-image-api") {
  std::ostringstream out;out<<"{\"stage\":\""<<stage<<"\",\"scope\":\"fixed descriptor sampling interval with no image API calls by this evaluation\",\"samples\":[";
  const auto start=std::chrono::steady_clock::now();
  for(unsigned sample=0;sample<=40;++sample) {
    if(sample)std::this_thread::sleep_until(start+std::chrono::milliseconds(25*sample));
    DIR* dir=opendir("/proc/self/fd");if(!dir)throw std::runtime_error("Own descriptor enumeration unavailable");
    const int own=dirfd(dir);std::vector<std::string> rows;
    while(auto* entry=readdir(dir)) {
      char* end=nullptr;long fd=std::strtol(entry->d_name,&end,10);
      if(!end||end==entry->d_name||*end||fd<0||fd==own)continue;
      struct stat state{};std::string row=std::to_string(fd)+":";
      if(fstat(static_cast<int>(fd),&state)==0)row+=std::to_string(state.st_dev)+":"+std::to_string(state.st_ino)+":"+std::to_string(state.st_mode);
      else row+="fstat-error:"+std::to_string(errno);
      rows.push_back(row);
    }
    closedir(dir);
    if(sample)out<<',';
    out<<"{\"sample\":"<<sample<<",\"elapsed_ms\":"<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()
       <<",\"count_excluding_scan_descriptor\":"<<rows.size()<<",\"identities\":[";
    for(size_t i=0;i<rows.size();++i) {if(i)out<<',';out<<'"'<<rows[i]<<'"';}
    out<<"]}";
  }
  out<<"],\"execution_completed\":true}";return out.str();
}
