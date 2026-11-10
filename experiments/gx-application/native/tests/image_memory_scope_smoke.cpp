// Ordinary file-to-owned-memory lifetime; fake handles explicitly aren't ImageKit.
#include "image_resource_audit.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace olex2::harmony;
namespace {unsigned checks=0,releases=0;struct Fake{};int Release(Fake*){++releases;return 0;}
void Require(bool yes,const char* text){++checks;if(!yes)throw std::runtime_error(text);}}
int main(int argc,char** argv) {
  try {
    Require(argc==2,"Expected artifact directory");std::filesystem::create_directories(argv[1]);
    const std::string path=std::string(argv[1])+"/synthetic-memory-input.txt",content="isolated synthetic encoded-byte lifetime fixture";
    {std::ofstream f(path);f<<content;Require(bool(f),"Write own synthetic input");}
    ImageResourceAudit audit;Fake fake;
    {
      AuditedImageBuffer bytes(path,audit);
      Require(std::string(bytes.bytes.begin(),bytes.bytes.end())==content,"Read exact owned bytes");
      Require(audit.file.closeAttempted&&audit.file.closeCode==0,"Input file closes before any decoder owner");
      Require(!audit.bufferReleasedAfterHandles,"Encoded byte owner is still alive");
      AuditedImageHandle<Fake,Release> source(audit,"source");source.value=&fake;source.Created(0);
    }
    Require(releases==1&&audit.bufferReleasedAfterHandles,"Encoded owner ends after public owner releases");
    Require(audit.OwnedScopeBalanced()&&!audit.sdkReceivedFileDescriptor,"Known owned scope balances without passing a descriptor");
    Require(audit.encodedBytes==content.size(),"Encoded byte count retained");
    {std::ofstream f(std::string(argv[1])+"/actual-owned-memory-audit.json");f<<ImageResourceAuditJson(audit)<<'\n';Require(bool(f),"Retain actual scope audit");}
    // Synthetic negative lifetime ordering: it must fail even if a release is
    // performed later. This never asks the SDK to use a dangling pointer.
    ImageResourceAudit bad;Fake other;
    {AuditedImageHandle<Fake,Release> source(bad,"source");
      {AuditedImageBuffer bytes(path,bad);source.value=&other;source.Created(0);}
      Require(!bad.bufferReleasedAfterHandles,"Early byte release is detected");}
    Require(!bad.OwnedScopeBalanced(),"Late public release cannot repair an invalid byte lifetime");
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"fake_handles_are_sdk_execution\":false,\"full_metadata_scan_passed\":"<<(audit.Balanced()?"true":"false")<<"}\n";return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
