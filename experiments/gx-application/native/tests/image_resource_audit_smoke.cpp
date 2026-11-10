// Isolated synthetic resource bookkeeping tests; fake handles are not SDK runs.
#include "image_resource_audit.h"
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
using namespace olex2::harmony;
namespace {
unsigned checks=0,releases=0;int releaseResult=0;
struct Fake {int value=7;};
int ReleaseFake(Fake*) {++releases;return releaseResult;}
void Require(bool yes,const char* message) {++checks;if(!yes)throw std::runtime_error(message);}
ImageResourceAudit SyntheticFileAudit() {
  ImageResourceAudit a;a.file.openAttempted=a.file.opened=a.file.identityKnown=true;
  a.file.closeAttempted=a.file.scanCompleted=true;return a;
}
}
int main(int argc,char** argv) {
  try {
    Require(argc==2,"Expected exclusive artifact directory");
    std::filesystem::create_directories(argv[1]);const std::string path=std::string(argv[1])+"/synthetic-owned-image-input.txt";
    {std::ofstream out(path);out<<"clearly labeled synthetic resource audit fixture\n";Require(bool(out),"Write own fixture");}
    Fake fake;
    {auto audit=SyntheticFileAudit();releases=0;releaseResult=0;
      {AuditedImageHandle<Fake,ReleaseFake> h(audit,"fake");h.value=&fake;h.Created(0);}
      Require(audit.Balanced()&&releases==1,"Success released exactly once");
      Require(audit.handles[0].creationObserved&&audit.handles[0].releaseAttempted,"Recorded acquisition and release");}
    {auto audit=SyntheticFileAudit();releases=0;
      try {AuditedImageHandle<Fake,ReleaseFake> h(audit,"fake");h.value=&fake;h.Created(0);throw std::runtime_error("synthetic early exit");}catch(const std::runtime_error&){}
      Require(audit.Balanced()&&releases==1,"Exception releases owner");}
    {auto audit=SyntheticFileAudit();releases=0;
      {AuditedImageHandle<Fake,ReleaseFake> h(audit,"fake");h.Created(17);}
      Require(audit.Balanced()&&releases==0,"Failed creation without object has no release");}
    {auto audit=SyntheticFileAudit();releases=0;
      {AuditedImageHandle<Fake,ReleaseFake> h(audit,"fake");h.value=&fake;h.Created(17);}
      Require(audit.Balanced()&&releases==1,"Error with returned object still releases");}
    {auto audit=SyntheticFileAudit();releases=0;releaseResult=91;
      {AuditedImageHandle<Fake,ReleaseFake> h(audit,"fake");h.value=&fake;h.Created(0);}
      Require(!audit.Balanced()&&releases==1&&audit.handles[0].releaseCode==91,"Release error is not ignored");releaseResult=0;}
    {auto audit=SyntheticFileAudit();
      {AuditedImageHandle<Fake,ReleaseFake> h(audit,"fake");h.value=&fake;}
      Require(!audit.Balanced(),"Unobserved acquisition fails");}
    {auto audit=SyntheticFileAudit();
      for(unsigned i=0;i<6;++i){AuditedImageHandle<Fake,ReleaseFake> h(audit,"fake");h.Created(17);}
      Require(!audit.Balanced()&&audit.overflow,"Bounded audit cannot silently overflow");}
    {ImageResourceAudit audit;
      {AuditedImageFile f(path,audit);Require(f.value>=0,"Open own fixture");}
      {std::ofstream out(std::string(argv[1])+"/actual-owned-file-audit.json");out<<ImageResourceAuditJson(audit)<<'\n';Require(bool(out),"Retain actual audit before qualification");}
      std::cerr<<"ACTUAL_OWNED_FILE "<<ImageResourceAuditJson(audit)<<'\n';
      Require(audit.file.identityKnown&&audit.file.closeAttempted&&audit.file.closeCode==0,"Owned input was identified and closed");
#if defined(__linux__) || defined(__OHOS__)
      Require(audit.file.scanCompleted&&audit.Balanced(),"No surviving descriptor of own inode");
#else
      Require(!audit.file.scanCompleted&&!audit.Balanced(),"Host lacks target proc scan; no invented qualification");
#endif
      std::ofstream out(std::string(argv[1])+"/actual-owned-file-audit.json");out<<ImageResourceAuditJson(audit)<<'\n';Require(bool(out),"Retain actual audit");}
    {ImageResourceAudit audit;
      {AuditedImageFile f(path+"-missing",audit);Require(f.value<0,"Missing fixture is rejected");}
      Require(audit.Balanced()&&!audit.file.opened&&audit.file.openError!=0,"Failed open has no live owner");}
#if defined(__linux__) || defined(__OHOS__)
    {ImageResourceAudit audit;int duplicate=-1;
      {AuditedImageFile f(path,audit);duplicate=dup(f.value);Require(duplicate>=0,"Duplicate only own synthetic input");}
      Require(audit.file.scanCompleted&&audit.file.remainingInputDescriptors==1&&!audit.Balanced(),"Surviving own duplicate must fail");
      Require(close(duplicate)==0,"Close only the test-owned duplicate");}
#endif
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"fake_handles_are_sdk_execution\":false}\n";return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
