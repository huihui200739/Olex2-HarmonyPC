#include "utility_checks.h"
#include "ebase.h"
#include "estrlist.h"
#include "eutf8.h"
#include "efile.h"
#include "exception.h"

#include <clocale>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <unistd.h>
#include <vector>

namespace olex2::harmony {
UtilityReport RunUtilityChecks(const std::string& directory) {
  UtilityReport result;
  olxstr test_path;
  bool owns_file = false;
  std::string temp_directory;
  auto check = [&](bool condition, const char* name) {
    result.checks.push_back({name, condition, condition ? "通过" : "未通过"});
    if (!condition) throw std::runtime_error(name);
  };
  try {
    if (directory.empty() || directory.front() != '/') {
      throw std::invalid_argument("An absolute application-owned writable directory is required");
    }
    static std::once_flag locale_once;
    std::call_once(locale_once, [] {
      if (!std::setlocale(LC_CTYPE, "C.UTF-8")) std::setlocale(LC_CTYPE, "");
    });
    olxstr value("Olex2-HarmonyPC");
    check(value.Replace("HarmonyPC", "OHOS") == "Olex2-OHOS", "upstream string replacement");
    const char* samples[] = {"Olex2", u8"café", u8"鸿蒙 PC / Olex2", u8"PC 😀"};
    for (const char* sample : samples) {
      const olxwstr unicode = TUtf8::Decode(sample);
      const olxcstr encoded = TUtf8::Encode(unicode);
      check(encoded.Length() == std::strlen(sample) &&
            std::memcmp(encoded.c_str(), sample, std::strlen(sample)) == 0,
            "upstream UTF-8 round trip");
    }
    TStrList names;
    names.Add("Olex2"); names.Add("HarmonyPC");
    check(names.Count() == 2 && names[1] == "HarmonyPC", "upstream string container");
    std::string pattern = directory + "/olex2-native-check-XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end()); buffer.push_back('\0');
    char* created = mkdtemp(buffer.data());
    if (!created) throw std::runtime_error("Cannot create a private test directory");
    temp_directory = created;
    test_path = TUtf8::Decode(temp_directory.c_str());
    test_path << TUtf8::Decode(u8"/olex2-鸿蒙.txt");
    const char payload[] = "Olex2 utility-layer smoke test\n";
    {
      TEFile file(test_path, olxstr("wbx"));
      owns_file = true;
      file.Write(payload, sizeof(payload));
      check(file.GetPosition() == sizeof(payload), "upstream Chinese-path file write");
    }
    check(TEFile::Exists(test_path), "upstream Chinese-path file lookup");
    char contents[sizeof(payload)] = {};
    { TEFile file(test_path, olxstr("rb")); file.Read(contents, sizeof(contents)); }
    check(std::memcmp(contents, payload, sizeof(payload)) == 0, "upstream Chinese-path file read");
    bool deleted = TEFile::DelFile(test_path);
    if (deleted) owns_file = false;
    check(deleted, "upstream test-file cleanup");
    if (rmdir(temp_directory.c_str()) != 0) throw std::runtime_error("Test directory cleanup failed");
    temp_directory.clear();
    result.passed = true;
  }
  catch (const TBasicException& error) { result.error = TUtf8::Encode(error.GetFullMessage()).c_str(); }
  catch (const std::exception& error) { result.error = error.what(); }
  if (owns_file) {
    // Never delete anything except the exclusively created file from this run.
    try { TEFile::DelFile(test_path); } catch (...) {}
  }
  if (!temp_directory.empty()) rmdir(temp_directory.c_str());
  return result;
}
}
