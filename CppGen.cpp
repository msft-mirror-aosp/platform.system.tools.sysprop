/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "sysprop_cpp_gen"

#include "CppGen.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <cerrno>
#include <regex>
#include <string>

#include "CodeWriter.h"
#include "Common.h"
#include "sysprop.pb.h"

namespace {

constexpr const char* kIndent = "    ";

constexpr const char* kCppHeaderIncludes =
    R"(#include <cstdint>
#include <optional>
#include <string>
#include <vector>

)";

constexpr const char* kCppSourceIncludes =
    R"(#include <cstring>
#include <iterator>
#include <type_traits>
#include <utility>

#include <dlfcn.h>
#include <strings.h>

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

)";

constexpr const char* kCppParsersAndFormatters =
    R"(template <typename T> constexpr bool is_vector = false;

template <typename T> constexpr bool is_vector<std::vector<T>> = true;

template <> [[maybe_unused]] std::optional<bool> DoParse(const char* str) {
    static constexpr const char* kYes[] = {"1", "true"};
    static constexpr const char* kNo[] = {"0", "false"};

    for (const char* yes : kYes) {
        if (strcasecmp(yes, str) == 0) return std::make_optional(true);
    }

    for (const char* no : kNo) {
        if (strcasecmp(no, str) == 0) return std::make_optional(false);
    }

    return std::nullopt;
}

template <> [[maybe_unused]] std::optional<std::int32_t> DoParse(const char* str) {
    std::int32_t ret;
    bool success = android::base::ParseInt(str, &ret);
    return success ? std::make_optional(ret) : std::nullopt;
}

template <> [[maybe_unused]] std::optional<std::int64_t> DoParse(const char* str) {
    std::int64_t ret;
    bool success = android::base::ParseInt(str, &ret);
    return success ? std::make_optional(ret) : std::nullopt;
}

template <> [[maybe_unused]] std::optional<double> DoParse(const char* str) {
    int old_errno = errno;
    errno = 0;
    char* end;
    double ret = std::strtod(str, &end);
    if (errno != 0) {
        return std::nullopt;
    }
    if (str == end || *end != '\0') {
        errno = old_errno;
        return std::nullopt;
    }
    errno = old_errno;
    return std::make_optional(ret);
}

template <> [[maybe_unused]] std::optional<std::string> DoParse(const char* str) {
    return std::make_optional(str);
}

template <typename Vec> [[maybe_unused]] std::optional<Vec> DoParseList(const char* str) {
    Vec ret;
    for (auto&& element : android::base::Split(str, ",")) {
        auto parsed = DoParse<typename Vec::value_type>(element.c_str());
        if (!parsed) {
            return std::nullopt;
        }
        ret.emplace_back(std::move(*parsed));
    }
    return std::make_optional(std::move(ret));
}

template <typename T> inline std::optional<T> TryParse(const char* str) {
    if constexpr(is_vector<T>) {
        return DoParseList<T>(str);
    } else {
        return DoParse<T>(str);
    }
}

[[maybe_unused]] std::string FormatValue(std::int32_t value) {
    return std::to_string(value);
}

[[maybe_unused]] std::string FormatValue(std::int64_t value) {
    return std::to_string(value);
}

[[maybe_unused]] std::string FormatValue(double value) {
    return android::base::StringPrintf("%.*g", std::numeric_limits<double>::max_digits10, value);
}

[[maybe_unused]] std::string FormatValue(bool value) {
    return value ? "true" : "false";
}

template <typename T>
[[maybe_unused]] std::string FormatValue(const std::vector<T>& value) {
    if (value.empty()) return "";

    std::string ret;

    for (auto&& element : value) {
        if (ret.empty()) ret.push_back(',');
        if constexpr(std::is_same_v<T, std::string>) {
            ret += element;
        } else {
            ret += FormatValue(element);
        }
    }

    return ret;
}

)";

constexpr const char* kCppLibcUtil =
    R"(namespace libc {

struct prop_info;

const prop_info* (*system_property_find)(const char* name);

void (*system_property_read_callback)(
    const prop_info* pi,
    void (*callback)(void* cookie, const char* name, const char* value, std::uint32_t serial),
    void* cookie
);

int (*system_property_set)(const char* key, const char* value);

void* handle;

__attribute__((constructor)) void load_libc_functions() {
    handle = dlopen("libc.so", RTLD_LAZY | RTLD_NOLOAD);

    system_property_find = reinterpret_cast<decltype(system_property_find)>(dlsym(handle, "__system_property_find"));
    system_property_read_callback = reinterpret_cast<decltype(system_property_read_callback)>(dlsym(handle, "__system_property_read_callback"));
    system_property_set = reinterpret_cast<decltype(system_property_set)>(dlsym(handle, "__system_property_set"));
}

__attribute__((destructor)) void release_libc_functions() {
    dlclose(handle);
}

template <typename T>
std::optional<T> GetProp(const char* key) {
    auto pi = system_property_find(key);
    if (pi == nullptr) return std::nullopt;
    std::optional<T> ret;
    system_property_read_callback(pi, [](void* cookie, const char*, const char* value, std::uint32_t) {
        *static_cast<std::optional<T>*>(cookie) = TryParse<T>(value);
    }, &ret);
    return ret;
}

}  // namespace libc

)";

const std::regex kRegexDot{"\\."};
const std::regex kRegexUnderscore{"_"};

std::string GetHeaderIncludeGuardName(const sysprop::Properties& props);
std::string GetCppEnumName(const sysprop::Property& prop);
std::string GetCppPropTypeName(const sysprop::Property& prop);
std::string GetCppNamespace(const sysprop::Properties& props);

bool GenerateHeader(const sysprop::Properties& props,
                    std::string* header_result, std::string* err);
bool GenerateSource(const sysprop::Properties& props,
                    const std::string& include_name, std::string* source_result,
                    std::string* err);

std::string GetHeaderIncludeGuardName(const sysprop::Properties& props) {
  return "SYSPROPGEN_" + std::regex_replace(props.module(), kRegexDot, "_") +
         "_H_";
}

std::string GetCppEnumName(const sysprop::Property& prop) {
  return PropNameToIdentifier(prop.name()) + "_values";
}

std::string GetCppPropTypeName(const sysprop::Property& prop) {
  switch (prop.type()) {
    case sysprop::Boolean:
      return "bool";
    case sysprop::Integer:
      return "std::int32_t";
    case sysprop::Long:
      return "std::int64_t";
    case sysprop::Double:
      return "double";
    case sysprop::String:
      return "std::string";
    case sysprop::Enum:
      return GetCppEnumName(prop);
    case sysprop::BooleanList:
      return "std::vector<bool>";
    case sysprop::IntegerList:
      return "std::vector<std::int32_t>";
    case sysprop::LongList:
      return "std::vector<std::int64_t>";
    case sysprop::DoubleList:
      return "std::vector<double>";
    case sysprop::StringList:
      return "std::vector<std::string>";
    case sysprop::EnumList:
      return "std::vector<" + GetCppEnumName(prop) + ">";
    default:
      __builtin_unreachable();
  }
}

std::string GetCppNamespace(const sysprop::Properties& props) {
  return std::regex_replace(props.module(), kRegexDot, "::");
}

bool GenerateHeader(const sysprop::Properties& props, std::string* header_result,
                    [[maybe_unused]] std::string* err) {
  CodeWriter writer(kIndent);

  writer.Write("%s", kGeneratedFileFooterComments);

  std::string include_guard_name = GetHeaderIncludeGuardName(props);
  writer.Write("#ifndef %s\n#define %s\n\n", include_guard_name.c_str(),
               include_guard_name.c_str());
  writer.Write("%s", kCppHeaderIncludes);

  std::string cpp_namespace = GetCppNamespace(props);
  writer.Write("namespace %s {\n\n", cpp_namespace.c_str());

  for (int i = 0; i < props.prop_size(); ++i) {
    if (i > 0) writer.Write("\n");

    const sysprop::Property& prop = props.prop(i);
    std::string prop_id = PropNameToIdentifier(prop.name());
    std::string prop_type = GetCppPropTypeName(prop);

    if (prop.type() == sysprop::Enum || prop.type() == sysprop::EnumList) {
      writer.Write("enum class %s {\n", GetCppEnumName(prop).c_str());
      writer.Indent();
      for (const std::string& name :
           android::base::Split(prop.enum_values(), "|")) {
        writer.Write("%s,\n", name.c_str());
      }
      writer.Dedent();
      writer.Write("};\n\n");
    }

    writer.Write("std::optional<%s> %s();\n", prop_type.c_str(),
                 prop_id.c_str());
    if (!prop.readonly()) {
      writer.Write("bool %s(const %s& value);\n", prop_id.c_str(),
                   prop_type.c_str());
    }
  }

  writer.Write("\n}  // namespace %s\n\n", cpp_namespace.c_str());

  writer.Write("#endif  // %s\n", include_guard_name.c_str());

  *header_result = writer.Code();
  return true;
}

bool GenerateSource(const sysprop::Properties& props,
                    const std::string& include_name, std::string* source_result,
                    [[maybe_unused]] std::string* err) {
  CodeWriter writer(kIndent);
  writer.Write("%s", kGeneratedFileFooterComments);
  writer.Write("#include <%s>\n\n", include_name.c_str());
  writer.Write("%s", kCppSourceIncludes);

  std::string cpp_namespace = GetCppNamespace(props);

  writer.Write("namespace {\n\n");
  writer.Write("using namespace %s;\n\n", cpp_namespace.c_str());
  writer.Write(
      "template <typename T> std::optional<T> DoParse(const char* str);\n\n");

  for (int i = 0; i < props.prop_size(); ++i) {
    const sysprop::Property& prop = props.prop(i);
    if (prop.type() != sysprop::Enum && prop.type() != sysprop::EnumList) {
      continue;
    }

    std::string prop_id = PropNameToIdentifier(prop.name());
    std::string enum_name = GetCppEnumName(prop);

    writer.Write("constexpr const std::pair<const char*, %s> %s_list[] = {\n",
                 enum_name.c_str(), prop_id.c_str());
    writer.Indent();
    for (const std::string& name :
         android::base::Split(prop.enum_values(), "|")) {
      writer.Write("{\"%s\", %s::%s},\n", name.c_str(), enum_name.c_str(),
                   name.c_str());
    }
    writer.Dedent();
    writer.Write("};\n\n");

    writer.Write("template <>\n");
    writer.Write("std::optional<%s> DoParse(const char* str) {\n",
                 enum_name.c_str());
    writer.Indent();
    writer.Write("for (auto [name, val] : %s_list) {\n", prop_id.c_str());
    writer.Indent();
    writer.Write("if (strcmp(str, name) == 0) {\n");
    writer.Indent();
    writer.Write("return val;\n");
    writer.Dedent();
    writer.Write("}\n");
    writer.Dedent();
    writer.Write("}\n");
    writer.Write("return std::nullopt;\n");
    writer.Dedent();
    writer.Write("}\n\n");

    if (!prop.readonly()) {
      writer.Write("std::string FormatValue(%s value) {\n", enum_name.c_str());
      writer.Indent();
      writer.Write("for (auto [name, val] : %s_list) {\n", prop_id.c_str());
      writer.Indent();
      writer.Write("if (val == value) {\n");
      writer.Indent();
      writer.Write("return name;\n");
      writer.Dedent();
      writer.Write("}\n");
      writer.Dedent();
      writer.Write("}\n");

      std::string prefix = (prop.readonly() ? "ro." : "") + props.prefix();
      if (!prefix.empty() && prefix.back() != '.') prefix.push_back('.');

      writer.Write(
          "LOG(FATAL) << \"Invalid value \" << "
          "static_cast<std::int32_t>(value) << "
          "\" for property \" << \"%s%s\";\n",
          prefix.c_str(), prop_id.c_str());

      writer.Write("__builtin_unreachable();\n");
      writer.Dedent();
      writer.Write("}\n\n");
    }
  }
  writer.Write("%s", kCppParsersAndFormatters);
  writer.Write("%s", kCppLibcUtil);
  writer.Write("}  // namespace\n\n");

  writer.Write("namespace %s {\n\n", cpp_namespace.c_str());

  for (int i = 0; i < props.prop_size(); ++i) {
    if (i > 0) writer.Write("\n");

    const sysprop::Property& prop = props.prop(i);
    std::string prop_id = PropNameToIdentifier(prop.name());
    std::string prop_type = GetCppPropTypeName(prop);
    std::string prefix = (prop.readonly() ? "ro." : "") + props.prefix();
    if (!prefix.empty() && prefix.back() != '.') prefix.push_back('.');

    if (prop.type() == sysprop::Enum || prop.type() == sysprop::EnumList) {
      std::string enum_name = GetCppEnumName(prop);
    }

    writer.Write("std::optional<%s> %s() {\n", prop_type.c_str(),
                 prop_id.c_str());
    writer.Indent();
    writer.Write("return libc::GetProp<%s>(\"%s%s\");\n", prop_type.c_str(),
                 prefix.c_str(), prop.name().c_str());
    writer.Dedent();
    writer.Write("}\n");

    if (!prop.readonly()) {
      writer.Write("\nbool %s(const %s& value) {\n", prop_id.c_str(),
                   prop_type.c_str());
      writer.Indent();
      writer.Write(
          "return libc::system_property_set(\"%s%s\", "
          "%s.c_str()) == 0;\n",
          prefix.c_str(), prop.name().c_str(),
          prop.type() == sysprop::String ? "value" : "FormatValue(value)");
      writer.Dedent();
      writer.Write("}\n");
    }
  }

  writer.Write("\n}  // namespace %s\n", cpp_namespace.c_str());

  *source_result = writer.Code();

  return true;
}

}  // namespace

bool GenerateCppFiles(const std::string& input_file_path,
                      const std::string& header_output_dir,
                      const std::string& source_output_dir,
                      const std::string& include_name, std::string* err) {
  sysprop::Properties props;

  if (!ParseProps(input_file_path, &props, err)) {
    return false;
  }

  std::string header_result, source_result;

  if (!GenerateHeader(props, &header_result, err)) {
    return false;
  }

  if (!GenerateSource(props, include_name, &source_result, err)) {
    return false;
  }

  std::string output_basename = android::base::Basename(input_file_path);

  std::string header_path = header_output_dir + "/" + output_basename + ".h";
  std::string source_path = source_output_dir + "/" + output_basename + ".cpp";

  if (!IsDirectory(header_output_dir) && !CreateDirectories(header_output_dir)) {
    *err = "Creating directory to " + header_output_dir +
           " failed: " + strerror(errno);
    return false;
  }

  if (!IsDirectory(source_output_dir) && !CreateDirectories(source_output_dir)) {
    *err = "Creating directory to " + source_output_dir +
           " failed: " + strerror(errno);
    return false;
  }

  if (!android::base::WriteStringToFile(header_result, header_path)) {
    *err = "Writing generated header to " + header_path +
           " failed: " + strerror(errno);
    return false;
  }

  if (!android::base::WriteStringToFile(source_result, source_path)) {
    *err = "Writing generated source to " + source_path +
           " failed: " + strerror(errno);
    return false;
  }

  return true;
}
