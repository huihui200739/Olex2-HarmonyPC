/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(__WXWIDGETS__) || defined(OLEX2_MINIZIP)
#include "wxzipfs.h"
#include "efile.h"
#include "bapp.h"
#include "log.h"
#include "ememstream.h"

#ifdef __WXWIDGETS__
//..............................................................................
TMemoryBlock* TZipWrapper::GetMemoryBlock(const olxstr &EM) {
  olxstr entryName = TEFile::UnixPath(EM);
  TMemoryBlock * mb = 0;
  if (UseCache) {
    mb = FMemoryBlocks.Find(entryName, 0);
  }
  if (mb == 0) {
    wxZipEntry *entry = FEntries.Find(TEFile::UnixPath(entryName), 0);
    if (entry == 0) {
      return 0;
    }
    if (!FInputStream->OpenEntry(*entry)) {
      return 0;
    }
    //if( FInputStream->GetLength() <=0 )  {  FInputStream->CloseEntry();  return NULL;  }

    mb = new TMemoryBlock;
    mb->Length = FInputStream->GetLength();
    mb->Buffer = new char [mb->Length + 1];
    mb->DateTime = entry->GetDateTime().GetTicks();
    FInputStream->Read( mb->Buffer, FInputStream->GetLength());
    FInputStream->CloseEntry();
    if (UseCache) {
      FMemoryBlocks.Add(entryName, mb);
    }
  }
  return mb;
}
//..............................................................................
TZipWrapper::TZipWrapper(const olxstr& zipName, bool useCache)
  : zip_name(zipName),
  OnProgress(Actions.New("ON_PROGRESS")),
  Break(false)
{
  FInputStream = 0;
  wxfile = 0;
  UseCache = useCache;
  if (!TEFile::Exists(zipName)) {
    return;
  }
  FInputStream = new wxZipInputStream(new wxFileInputStream(zipName.u_str()));
  wxZipEntry* entry;
  while ((entry = FInputStream->GetNextEntry()) != 0) {
    FEntries.Add(TEFile::UnixPath(entry->GetName()), entry);
  }
}
//..............................................................................
TZipWrapper::TZipWrapper(TEFile* file, bool useCache)
  : OnProgress(Actions.New("ON_PROGRESS")), Break(false)
{
  FInputStream = 0;
  wxfile = 0;
  UseCache = useCache;
  if (file == 0) {
    return;
  }
  wxfile = new wxFile(file->FileNo());
  FInputStream = new wxZipInputStream(new wxFileInputStream(*wxfile));
  wxZipEntry* entry;
  while ((entry = FInputStream->GetNextEntry()) != 0) {
    FEntries.Add(TEFile::UnixPath(entry->GetName()), entry);
  }
}
//..............................................................................
TZipWrapper::~TZipWrapper() {
  for (size_t i=0; i < FMemoryBlocks.Count(); i++) {
    TMemoryBlock *mb = FMemoryBlocks.GetValue(i);
    delete [] mb->Buffer;
    delete mb;
  }
  for (size_t i = 0; i < FEntries.Count(); i++) {
    delete FEntries.GetValue(i);
  }
  olx_del_obj(FInputStream);
  
  if (wxfile != 0) {
    wxfile->Detach();
    delete wxfile;
  }
}
//..............................................................................
IDataInputStream* TZipWrapper::OpenEntry(const olxstr &EN) {
  TMemoryBlock * mb = GetMemoryBlock(EN);
  if (mb == 0) {
    return 0;
  }
  TEMemoryStream *ms = new TEMemoryStream();
  ms->Write(mb->Buffer, mb->Length);
  ms->SetPosition(0);
  if (!UseCache) {
    delete [] mb->Buffer;
    delete mb;
  }
  return ms;
}
//..............................................................................
wxInputStream* TZipWrapper::OpenWxEntry(const olxstr &EN) {
  TMemoryBlock * mb = GetMemoryBlock(EN);
  if (mb != 0) {
    wxMemoryInputStream* rv = new wxMemoryInputStream(mb->Buffer, mb->Length);
    if (!UseCache) {
      delete mb->Buffer;
      delete mb;
    }
    return rv;
  }
  return 0;
}
//..............................................................................
bool TZipWrapper::ExtractAll(const olxstr& dest) {
  olxstr extractPath(TEFile::AddPathDelimeter(dest));
  TOnProgress Progress;
  Progress.SetMax(FEntries.Count());
  Progress.SetAction(olxstr("Unpacking ") << zip_name << "...");
  OnProgress.Enter(0, &Progress);
  const size_t bf_sz = 1024*64;
  olx_array_ptr<char> bf = new char[bf_sz];
  bool res = true;
  for (size_t i=0; i < FEntries.Count(); i++) {
    if( this->Break )  {
      res = false;
      break;
    }
    if (FEntries.GetKey(i).EndsWith('/')) {
      continue;
    }
    if (!FInputStream->OpenEntry(*FEntries.GetValue(i))) {
      continue;
    }
    Progress.SetPos(i+1);
    Progress.SetAction(FEntries.GetKey(i));
    OnProgress.Execute(0, &Progress);

    olxstr dest_file = extractPath + FEntries.GetKey(i);
    olxstr dst_dir = TEFile::ExtractFilePath(dest_file);
    if (!TEFile::Exists(dst_dir))
      if (!TEFile::MakeDirs(dst_dir)) {
        TBasicApp::NewLogEntry(logError) << "Failed to create folder: " <<
          dst_dir;
        res = false;
        break;
      }
    if (TEFile::Exists(dest_file)) {
      if (!TEFile::DelFile(dest_file)) {
        TBasicApp::NewLogEntry(logError) << "Failed to remove/update file: " <<
          dest_file;
        res = false;
        break;
      }
    }
    wxFileOutputStream fos(dest_file.u_str());
    while (FInputStream->Read(bf, bf_sz).LastRead() != 0) {
      fos.Write(bf, FInputStream->LastRead());
    }
  }
  Progress.SetPos(FEntries.Count());
  Progress.SetAction("Done");
  OnProgress.Exit(0, &Progress);
  return res;
}
//..............................................................................
#elif defined(OLEX2_MINIZIP)
// The platform backend uses only the pinned MiniZip/zlib source. C callbacks
// operate on an owned descriptor with pread: a borrowed TEFile is never closed,
// repositioned, or used as an implicit owner by the archive reader.
#include "eutf8.h"
#include "unzip.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
class ZipFd {
  int value;
public:
  explicit ZipFd(int fd = -1) : value(fd) {}
  ~ZipFd() { if (value >= 0) close(value); }
  ZipFd(const ZipFd&) = delete;
  ZipFd& operator=(const ZipFd&) = delete;
  ZipFd(ZipFd&& other) noexcept : value(other.Release()) {}
  ZipFd& operator=(ZipFd&& other) noexcept {
    Reset(other.Release());
    return *this;
  }
  int Get() const { return value; }
  int Release() { int result = value; value = -1; return result; }
  void Reset(int fd = -1) {
    if (value >= 0) close(value);
    value = fd;
  }
};

std::string ZipUtf8(const olxstr& value) {
  const olxcstr encoded = TUtf8::Encode(value);
  return std::string(encoded.c_str(), encoded.Length());
}

olxstr ZipEntryText(const char* value, size_t length, bool utf8) {
  if (utf8) return TUtf8::Decode(value, length);
  // ZIP's legacy encoding is CP437, not the process locale. Keep ASCII and
  // UTF-8 archives interoperable without depending on the installed locales.
  static const wchar_t cp437[128] = {
    0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,
    0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x00ec,0x00c4,0x00c5,
    0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00f2,0x00fb,0x00f9,
    0x00ff,0x00d6,0x00dc,0x00a2,0x00a3,0x00a5,0x20a7,0x0192,
    0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,
    0x00bf,0x2310,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,
    0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567,
    0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256b,
    0x256a,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,
    0x03b1,0x00df,0x0393,0x03c0,0x03a3,0x03c3,0x00b5,0x03c4,
    0x03a6,0x0398,0x03a9,0x03b4,0x221e,0x03c6,0x03b5,0x2229,
    0x2261,0x00b1,0x2265,0x2264,0x2320,0x2321,0x00f7,0x2248,
    0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x25a0,0x00a0
  };
  olxwstr decoded;
  for (size_t i = 0; i < length; ++i) {
    unsigned char ch = static_cast<unsigned char>(value[i]);
    decoded << (ch < 128 ? static_cast<wchar_t>(ch) : cp437[ch - 128]);
  }
  return decoded;
}

time_t ZipTimestamp(const tm_unz& source) {
  struct tm stamp = {};
  stamp.tm_sec = source.tm_sec; stamp.tm_min = source.tm_min;
  stamp.tm_hour = source.tm_hour; stamp.tm_mday = source.tm_mday;
  stamp.tm_mon = source.tm_mon; stamp.tm_year = source.tm_year - 1900;
  stamp.tm_isdst = -1;
  time_t result = mktime(&stamp);
  return result == static_cast<time_t>(-1) ? 0 : result;
}

// Only entry components are untrusted. The user-selected destination is opened
// once and all archive descendants are relative to that descriptor, never via
// archive-provided absolute paths or links.
bool ZipComponents(const olxstr& name, std::vector<std::string>& components) {
  const std::string path = ZipUtf8(TEFile::UnixPath(name));
  if (path.empty() || path[0] == '/' || path.find('\0') != std::string::npos ||
      path.find(':') != std::string::npos) return false;
  size_t start = 0;
  while (start < path.size()) {
    size_t end = path.find('/', start);
    if (end == std::string::npos) end = path.size();
    const std::string part = path.substr(start, end - start);
    if (part.empty() || part == "." || part == "..") return false;
    components.push_back(part);
    start = end + 1;
  }
  return !components.empty();
}

ZipFd ZipDestination(std::string path) {
  while (path.size() > 1 && path.back() == '/') path.pop_back();
  if (path.empty() || path.find('\0') != std::string::npos) return ZipFd();
  ZipFd fd(open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (fd.Get() >= 0 || errno != ENOENT) return fd;
  const size_t separator = path.find_last_of('/');
  const std::string parent = separator == std::string::npos ? "." :
    (separator == 0 ? "/" : path.substr(0, separator));
  const std::string leaf = separator == std::string::npos ? path :
    path.substr(separator + 1);
  if (leaf.empty() || leaf == "." || leaf == "..") return ZipFd();
  ZipFd parentFd = ZipDestination(parent);
  if (parentFd.Get() < 0 ||
      (mkdirat(parentFd.Get(), leaf.c_str(), 0755) != 0 && errno != EEXIST))
    return ZipFd();
  return ZipFd(openat(parentFd.Get(), leaf.c_str(),
    O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
}

ZipFd ZipParent(int root, const std::vector<std::string>& parts,
                bool directory) {
  ZipFd current(fcntl(root, F_DUPFD_CLOEXEC, 0));
  const size_t count = directory ? parts.size() : parts.size() - 1;
  for (size_t i = 0; current.Get() >= 0 && i < count; ++i) {
    if (mkdirat(current.Get(), parts[i].c_str(), 0755) != 0 && errno != EEXIST)
      return ZipFd();
    ZipFd next(openat(current.Get(), parts[i].c_str(),
      O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    current = std::move(next);
  }
  return current;
}

class ZipOutput {
  int parent;
  std::string temporary;
  ZipFd fd;
public:
  explicit ZipOutput(int directory) : parent(directory) {
    static std::atomic<unsigned long long> sequence(0);
    for (unsigned attempt = 0; attempt < 128; ++attempt) {
      temporary = ".olex2-unpack-" + std::to_string(getpid()) + "-" +
        std::to_string(sequence.fetch_add(1));
      fd.Reset(openat(parent, temporary.c_str(),
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
      if (fd.Get() >= 0) return;
      if (errno != EEXIST) break;
    }
    // Never unlink a name that this operation did not create.
    temporary.clear();
  }
  ~ZipOutput() {
    if (!temporary.empty()) unlinkat(parent, temporary.c_str(), 0);
  }
  bool IsOpen() const { return fd.Get() >= 0; }
  bool Write(const void* buffer, size_t size) {
    const char* data = static_cast<const char*>(buffer);
    while (size != 0) {
      const ssize_t written = write(fd.Get(), data, size);
      if (written < 0 && errno == EINTR) continue;
      if (written <= 0) return false;
      data += written; size -= static_cast<size_t>(written);
    }
    return true;
  }
  bool Commit(const std::string& name, time_t timestamp) {
    struct stat previous;
    if (fstatat(parent, name.c_str(), &previous, AT_SYMLINK_NOFOLLOW) == 0) {
      if (!S_ISREG(previous.st_mode)) return false;
    }
    else if (errno != ENOENT) return false;
    const struct timespec times[2] = {{timestamp, 0}, {timestamp, 0}};
    if (futimens(fd.Get(), times) != 0 || fsync(fd.Get()) != 0) return false;
    const int completedFd = fd.Release();
    if (close(completedFd) != 0) return false;
    if (renameat(parent, temporary.c_str(), parent, name.c_str()) != 0)
      return false;
    temporary.clear();
    return true;
  }
};
}

struct TZipWrapper::NativeState {
  struct Input {
    ZipFd Descriptor;
    uint64_t Position = 0;
    uint64_t Length = 0;
    int Error = 0;
  } InputFile;
  struct Entry {
    olxstr Name;
    unz64_file_pos Position;
    unz_file_info64 Info;
    time_t Timestamp;
    bool Directory;
    bool Ordinary;
  };
  unzFile Archive = 0;
  bool Ready = false;
  std::vector<Entry> Entries;
  olxstr_dict<size_t, false> Lookup;

  ~NativeState() { if (Archive != 0) unzClose(Archive); }

  static voidpf ZCALLBACK Open(voidpf opaque, const void*, int mode) noexcept {
    Input* input = static_cast<Input*>(opaque);
    if (input->Descriptor.Get() < 0 || (mode & ZLIB_FILEFUNC_MODE_WRITE) != 0)
      return 0;
    input->Position = 0; input->Error = 0;
    return input;
  }
  static uLong ZCALLBACK Read(voidpf, voidpf stream, void* buffer,
                             uLong size) noexcept {
    Input* input = static_cast<Input*>(stream);
    size_t total = 0;
    while (total < size) {
      const uint64_t maximum = static_cast<uint64_t>(
        std::numeric_limits<off_t>::max());
      if (input->Position > maximum) { input->Error = EOVERFLOW; break; }
      const size_t available = static_cast<size_t>(std::min<uint64_t>(
        maximum - input->Position, static_cast<uint64_t>(SSIZE_MAX)));
      const size_t count = std::min<size_t>(size - total, available);
      if (count == 0) { input->Error = EOVERFLOW; break; }
      const ssize_t received = pread(input->Descriptor.Get(),
        static_cast<char*>(buffer) + total, count,
        static_cast<off_t>(input->Position));
      if (received < 0 && errno == EINTR) continue;
      if (received < 0) { input->Error = errno; break; }
      if (received == 0) break;
      input->Position += static_cast<uint64_t>(received);
      total += static_cast<size_t>(received);
    }
    return static_cast<uLong>(total);
  }
  static uLong ZCALLBACK Write(voidpf, voidpf stream, const void*,
                              uLong) noexcept {
    static_cast<Input*>(stream)->Error = EBADF;
    return 0;
  }
  static ZPOS64_T ZCALLBACK Tell(voidpf, voidpf stream) noexcept {
    return static_cast<Input*>(stream)->Position;
  }
  static long ZCALLBACK Seek(voidpf, voidpf stream, ZPOS64_T offset,
                            int origin) noexcept {
    Input* input = static_cast<Input*>(stream);
    const uint64_t maximum = static_cast<uint64_t>(
      std::numeric_limits<off_t>::max());
    uint64_t base = 0, result = 0;
    if (origin == ZLIB_FILEFUNC_SEEK_CUR) base = input->Position;
    else if (origin == ZLIB_FILEFUNC_SEEK_END) base = input->Length;
    else if (origin != ZLIB_FILEFUNC_SEEK_SET) {
      input->Error = EINVAL; return -1;
    }
    if (origin != ZLIB_FILEFUNC_SEEK_SET && offset > maximum) {
      const uint64_t distance = (~static_cast<uint64_t>(offset)) + 1;
      if (distance > base) { input->Error = EINVAL; return -1; }
      result = base - distance;
    }
    else {
      if (offset > maximum || base > maximum - offset) {
        input->Error = EOVERFLOW; return -1;
      }
      result = base + offset;
    }
    input->Position = result;
    return 0;
  }
  static int ZCALLBACK Close(voidpf, voidpf) noexcept { return 0; }
  static int ZCALLBACK Error(voidpf, voidpf stream) noexcept {
    return static_cast<Input*>(stream)->Error;
  }

  bool Initialize(int descriptor) {
    InputFile.Descriptor.Reset(descriptor);
    struct stat details;
    if (descriptor < 0 || fstat(descriptor, &details) != 0 ||
        !S_ISREG(details.st_mode) || details.st_size < 0) return false;
    InputFile.Length = static_cast<uint64_t>(details.st_size);
    zlib_filefunc64_def functions = {};
    functions.zopen64_file = Open; functions.zread_file = Read;
    functions.zwrite_file = Write; functions.ztell64_file = Tell;
    functions.zseek64_file = Seek; functions.zclose_file = Close;
    functions.zerror_file = Error; functions.opaque = &InputFile;
    Archive = unzOpen2_64(this, &functions);
    if (Archive == 0) return false;
    unz_global_info64 info = {};
    if (unzGetGlobalInfo64(Archive, &info) != UNZ_OK) return false;
    if (info.number_entry != 0 && unzGoToFirstFile(Archive) != UNZ_OK)
      return false;
    for (ZPOS64_T i = 0; i < info.number_entry; ++i) {
      Entry entry = {};
      if (unzGetCurrentFileInfo64(Archive, &entry.Info, 0, 0, 0, 0, 0, 0)
          != UNZ_OK) return false;
      std::vector<char> name(static_cast<size_t>(entry.Info.size_filename) + 1);
      if (unzGetCurrentFileInfo64(Archive, &entry.Info, name.data(),
          static_cast<uLong>(name.size()), 0, 0, 0, 0) != UNZ_OK ||
          std::memchr(name.data(), 0, entry.Info.size_filename) != 0 ||
          unzGetFilePos64(Archive, &entry.Position) != UNZ_OK) return false;
      entry.Name = TEFile::UnixPath(ZipEntryText(name.data(),
        entry.Info.size_filename, (entry.Info.flag & 0x800) != 0));
      const unsigned type = (entry.Info.external_fa >> 16) & S_IFMT;
      entry.Directory = entry.Name.EndsWith('/') || type == S_IFDIR ||
        (entry.Info.external_fa & 0x10) != 0;
      entry.Ordinary = type == 0 || type == S_IFREG || type == S_IFDIR;
      entry.Timestamp = ZipTimestamp(entry.Info.tmu_date);
      const size_t found = Lookup.IndexOf(entry.Name);
      if (found == InvalidIndex) {
        Lookup.Add(entry.Name, Entries.size());
        Entries.push_back(entry);
      }
      else Entries[Lookup.GetValue(found)] = entry;
      if (i + 1 < info.number_entry && unzGoToNextFile(Archive) != UNZ_OK)
        return false;
    }
    Ready = InputFile.Error == 0;
    return Ready;
  }

  const Entry* Find(const olxstr& name) const {
    if (!Ready) return 0;
    const size_t index = Lookup.IndexOf(TEFile::UnixPath(name));
    return index == InvalidIndex ? 0 : &Entries[Lookup.GetValue(index)];
  }

  struct Reader {
    unzFile Archive;
    bool Opened;
    Reader(NativeState& state, const Entry& entry) : Archive(state.Archive),
      Opened(false)
    {
      Opened = unzGoToFilePos64(Archive, &entry.Position) == UNZ_OK &&
        unzOpenCurrentFile(Archive) == UNZ_OK;
    }
    ~Reader() { if (Opened) unzCloseCurrentFile(Archive); }
    int Read(void* buffer, unsigned size) {
      return Opened ? unzReadCurrentFile(Archive, buffer, size) : UNZ_PARAMERROR;
    }
    bool Finish() {
      if (!Opened) return false;
      Opened = false;
      return unzCloseCurrentFile(Archive) == UNZ_OK;
    }
  };
};

TZipWrapper::TZipWrapper(const olxstr& zipName, bool useCache)
  : FNative(0), zip_name(zipName), UseCache(useCache), Break(false),
  OnProgress(Actions.New("ON_PROGRESS"))
{
  std::unique_ptr<NativeState> state(new NativeState);
  const std::string filename = ZipUtf8(zipName);
  if (filename.find('\0') == std::string::npos)
    state->Initialize(open(filename.c_str(), O_RDONLY | O_CLOEXEC));
  FNative = state.release();
}

TZipWrapper::TZipWrapper(TEFile* file, bool useCache)
  : FNative(0), zip_name(file != 0 ? file->GetName() : EmptyString()),
  UseCache(useCache), Break(false), OnProgress(Actions.New("ON_PROGRESS"))
{
  std::unique_ptr<NativeState> state(new NativeState);
  if (file != 0 && file->Handle() != 0)
    state->Initialize(fcntl(file->FileNo(), F_DUPFD_CLOEXEC, 0));
  FNative = state.release();
}

TZipWrapper::~TZipWrapper() {
  for (size_t i = 0; i < FMemoryBlocks.Count(); ++i) {
    delete[] FMemoryBlocks.GetValue(i)->Buffer;
    delete FMemoryBlocks.GetValue(i);
  }
  delete FNative;
}

size_t TZipWrapper::Count() const {
  return FNative->Ready ? FNative->Entries.size() : 0;
}
const olxstr& TZipWrapper::Name(size_t i) const {
  return FNative->Entries.at(i).Name;
}
time_t TZipWrapper::Timestamp(size_t i) const {
  return FNative->Entries.at(i).Timestamp;
}
size_t TZipWrapper::Size(size_t i) const {
  return OlxIStream::CheckSize<size_t>(FNative->Entries.at(i).Info.uncompressed_size);
}
bool TZipWrapper::FileExists(const olxstr& name) const {
  return FNative->Find(name) != 0;
}

TMemoryBlock* TZipWrapper::GetMemoryBlock(const olxstr& name) {
  const olxstr normalized = TEFile::UnixPath(name);
  TMemoryBlock* cached = UseCache ? FMemoryBlocks.Find(normalized, 0) : 0;
  if (cached != 0) return cached;
  const NativeState::Entry* entry = FNative->Find(normalized);
  if (entry == 0 || entry->Directory || !entry->Ordinary) return 0;
  const uint64_t length = entry->Info.uncompressed_size;
  if (length > std::numeric_limits<uint32_t>::max() ||
      length >= std::numeric_limits<size_t>::max()) {
    throw TFunctionFailedException(__OlxSourceInfo,
      "ZIP entry exceeds the in-memory stream size limit");
  }
  NativeState::Reader reader(*FNative, *entry);
  if (!reader.Opened) return 0;
  std::unique_ptr<TMemoryBlock> block(new TMemoryBlock);
  std::unique_ptr<char[]> bytes(new char[static_cast<size_t>(length) + 1]);
  size_t total = 0;
  while (total < length) {
    const unsigned count = static_cast<unsigned>(
      std::min<uint64_t>(length - total, 65536));
    const int received = reader.Read(bytes.get() + total, count);
    if (received <= 0) return 0;
    total += static_cast<size_t>(received);
  }
  char extra;
  if (reader.Read(&extra, 1) != 0 || !reader.Finish()) return 0;
  bytes[total] = 0;
  block->Length = static_cast<uint32_t>(length);
  block->DateTime = static_cast<uint64_t>(entry->Timestamp);
  block->Buffer = bytes.get();
  if (UseCache) FMemoryBlocks.Add(normalized, block.get());
  bytes.release();
  return block.release();
}

IDataInputStream* TZipWrapper::OpenEntry(const olxstr& name) {
  TMemoryBlock* block = GetMemoryBlock(name);
  if (block == 0) return 0;
  struct TemporaryBlock {
    TMemoryBlock* Block;
    bool Owned;
    ~TemporaryBlock() {
      if (Owned) { delete[] Block->Buffer; delete Block; }
    }
  } owner = {block, !UseCache};
  std::unique_ptr<TEMemoryStream> stream(new TEMemoryStream);
  stream->Write(block->Buffer, block->Length);
  stream->SetPosition(0);
  return stream.release();
}

bool TZipWrapper::ExtractAll(const olxstr& destination) {
  if (!FNative->Ready) return false;
  Break = false;
  TOnProgress progress;
  progress.SetMax(Count());
  progress.SetAction(olxstr("Unpacking ") << zip_name << "...");
  OnProgress.Enter(0, &progress);
  bool result = true;
  size_t completed = 0;
  try {
    ZipFd root = ZipDestination(ZipUtf8(TEFile::UnixPath(destination)));
    result = root.Get() >= 0;
    char buffer[65536];
    for (size_t i = 0; result && i < Count(); ++i) {
      if (Break) { result = false; break; }
      const NativeState::Entry& entry = FNative->Entries[i];
      std::vector<std::string> parts;
      if (!entry.Ordinary || !ZipComponents(entry.Name, parts)) {
        result = false; break;
      }
      progress.SetPos(i); progress.SetAction(entry.Name);
      OnProgress.Execute(0, &progress);
      if (Break) { result = false; break; }
      ZipFd parent = ZipParent(root.Get(), parts, entry.Directory);
      if (parent.Get() < 0) { result = false; break; }
      if (!entry.Directory) {
        NativeState::Reader reader(*FNative, entry);
        ZipOutput output(parent.Get());
        if (!reader.Opened || !output.IsOpen()) { result = false; break; }
        uint64_t total = 0;
        while (!Break) {
          const int received = reader.Read(buffer, sizeof(buffer));
          if (received < 0) { result = false; break; }
          if (received == 0) break;
          if (!output.Write(buffer, static_cast<size_t>(received))) {
            result = false; break;
          }
          total += static_cast<uint64_t>(received);
        }
        if (Break || !result || total != entry.Info.uncompressed_size ||
            !reader.Finish()) { result = false; break; }
        if (!output.Commit(parts.back(), entry.Timestamp)) {
          result = false; break;
        }
      }
      completed = i + 1;
    }
  }
  catch (...) {
    progress.SetPos(completed); progress.SetAction("Failed");
    OnProgress.Exit(0, &progress);
    throw;
  }
  progress.SetPos(completed);
  progress.SetAction(result ? "Done" : (Break ? "Cancelled" : "Failed"));
  OnProgress.Exit(0, &progress);
  return result;
}
#endif

bool TZipWrapper::IsValidFileName(const olxstr& FN) {
  return TZipUrl::IsValidFileName(FN);
}
bool TZipWrapper::IsZipFile(const olxstr& FN) {
  return TZipUrl::IsZipFile(FN);
}
olxstr TZipWrapper::ExtractZipName(const olxstr& FN) {
  return TZipUrl::ExtractZipName(FN);
}
olxstr TZipWrapper::ExtractZipEntryName(const olxstr& FN) {
  return TZipUrl::ExtractZipEntryName(FN);
}
bool TZipWrapper::SplitZipUrl(const olxstr& fullName, TZipEntry& ZE) {
  return TZipUrl::SplitZipUrl(fullName, ZE);
}
olxstr TZipWrapper::ComposeFileName(const olxstr& zip, const olxstr& entry) {
  return TZipUrl::ComposeFileName(zip, entry);
}
/*____________________________________________________________________________*/


TwxZipFileSystem::TwxZipFileSystem(const olxstr& zip_name, bool UseCache)
: zip(zip_name, UseCache)
{
  Access = afs_ReadOnlyAccess;
  AActionHandler::SetToDelete(false);
  zip.OnProgress.Add(this);
}
//..............................................................................
TwxZipFileSystem::TwxZipFileSystem(TEFile* zip_fh, bool UseCache)
: zip(zip_fh, UseCache)
{
  Access = afs_ReadOnlyAccess;
  AActionHandler::SetToDelete(false);
  zip.OnProgress.Add(this);
}
//..............................................................................
TwxZipFileSystem::~TwxZipFileSystem() {
  zip.OnProgress.Remove(this);
}
//..............................................................................
olx_object_ptr<IInputStream> TwxZipFileSystem::_DoOpenFile(const olxstr& Source) {
  TOnProgress Progress;
  olxstr fn( TEFile::UnixPath(Source) );
  Progress.SetAction( olxstr("Extracting ") << fn );
  Progress.SetPos(0);
  Progress.SetMax(1);
  OnProgress.Enter(this, &Progress);

  IDataInputStream* rv = zip.OpenEntry(Source);
  if (rv == 0) {
    Progress.SetAction("Failed");
    OnProgress.Exit(this, &Progress);
    throw TFunctionFailedException(__OlxSourceInfo,
      olxstr("Could not locate zip entry: ") << fn);
  }

  Progress.SetAction("Done");
  Progress.SetPos(1);
  OnProgress.Exit(this, &Progress);
  return rv;
}
//..............................................................................
bool TwxZipFileSystem::ExtractAll(const olxstr& dest) {
  return zip.ExtractAll(dest);
}
//..............................................................................
#endif // wx or native MiniZip archive backend
