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

#include "Common.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

#include <android-base/file.h>
#include <android-base/strings.h>
#include <google/protobuf/text_format.h>

#include "sysprop.pb.h"

namespace {

bool IsCorrectIdentifier(const std::string& name);
bool IsCorrectPropertyName(const std::string& name);
bool ValidateProp(const sysprop::Properties& props,
                  const sysprop::Property& prop, std::string* err);
bool ValidateProps(const sysprop::Properties& props, std::string* err);

bool IsCorrectIdentifier(const std::string& name) {
  if (name.empty()) return false;
  if (std::isalpha(name[0]) == 0 && name[0] != '_') return false;

  for (size_t i = 1; i < name.size(); ++i) {
    if (std::isalpha(name[i]) || name[i] == '_') continue;
    if (std::isdigit(name[i])) continue;

    return false;
  }

  return true;
}

bool IsCorrectPropertyName(const std::string& name) {
  if (name.empty()) return false;

  for (const std::string& token : android::base::Split(name, ".")) {
    if (!IsCorrectIdentifier(token)) return false;
  }

  return true;
}

bool ValidateProp(const sysprop::Properties& props,
                  const sysprop::Property& prop, std::string* err) {
  if (!IsCorrectPropertyName(prop.name())) {
    if (err) *err = "Invalid prop name \"" + prop.name() + "\"";
    return false;
  }

  if (prop.type() == sysprop::Enum || prop.type() == sysprop::EnumList) {
    std::vector<std::string> names =
        android::base::Split(prop.enum_values(), "|");
    if (names.empty()) {
      if (err) *err = "Enum values are empty for prop \"" + prop.name() + "\"";
      return false;
    }

    for (const std::string& name : names) {
      if (!IsCorrectIdentifier(name)) {
        if (err)
          *err = "Invalid enum value \"" + name + "\" for prop \"" +
                 prop.name() + "\"";
        return false;
      }
    }

    std::unordered_set<std::string> name_set;
    for (const std::string& name : names) {
      if (!name_set.insert(name).second) {
        if (err)
          *err = "Duplicated enum value \"" + name + "\" for prop \"" +
                 prop.name() + "\"";
        return false;
      }
    }
  }

  if (props.owner() == sysprop::Platform) {
    std::string full_name = props.prefix() + prop.name();
    if (android::base::StartsWith(full_name, "vendor.") ||
        android::base::StartsWith(full_name, "odm.")) {
      if (err)
        *err = "Prop \"" + prop.name() +
               "\" owned by platform cannot have vendor. or odm. namespace";
      return false;
    }
  }

  return true;
}

bool ValidateProps(const sysprop::Properties& props, std::string* err) {
  std::vector<std::string> names = android::base::Split(props.module(), ".");
  if (names.size() <= 1) {
    if (err) *err = "Invalid module name \"" + props.module() + "\"";
    return false;
  }

  for (const auto& name : names) {
    if (!IsCorrectIdentifier(name)) {
      if (err) *err = "Invalid name \"" + name + "\" in module";
      return false;
    }
  }

  if (!props.prefix().empty() && !IsCorrectPropertyName(props.prefix())) {
    if (err) *err = "Invalid prefix \"" + props.prefix() + "\"";
    return false;
  }

  if (props.prop_size() == 0) {
    if (err) *err = "There is no defined property";
    return false;
  }

  for (int i = 0; i < props.prop_size(); ++i) {
    const auto& prop = props.prop(i);
    if (!ValidateProp(props, prop, err)) return false;
  }

  std::unordered_set<std::string> prop_names;

  for (int i = 0; i < props.prop_size(); ++i) {
    const auto& prop = props.prop(i);
    auto res = prop_names.insert(PropNameToIdentifier(prop.name()));

    if (!res.second) {
      if (err) *err = "Duplicated prop name \"" + prop.name() + "\"";
      return false;
    }
  }

  if (props.owner() == sysprop::Platform) {
    if (props.module() != "android.os.PlatformProperties") {
      if (err)
        *err =
            "Platform-defined properties should have "
            "\"android.os.PlatformProperties\" as module name";
      return false;
    }
  } else {
    if (props.module() == "android.os.PlatformProperties") {
      if (err)
        *err =
            "Vendor or Odm cannot use \"android.os.PlatformProperties\" as "
            "module name";
      return false;
    }
  }

  return true;
}

}  // namespace

// For directory functions, we could use <filesystem> of C++17 if supported..
bool CreateDirectories(const std::string& path) {
  struct stat st;

  // If already exists..
  if (stat(path.c_str(), &st) == 0) {
    return false;
  }

  size_t last_slash = path.rfind('/');
  if (last_slash > 0 && last_slash != std::string::npos) {
    std::string parent = path.substr(0, last_slash);
    if (!IsDirectory(parent) && !CreateDirectories(parent)) return false;
  }

  // It's very unlikely, but if path contains ".." or any symbolic links, it
  // might already be created before this line.
  return mkdir(path.c_str(), 0755) == 0 || IsDirectory(path);
}

bool IsDirectory(const std::string& path) {
  struct stat st;

  if (stat(path.c_str(), &st) == -1) return false;
  return S_ISDIR(st.st_mode);
}

std::string GetModuleName(const sysprop::Properties& props) {
  const std::string& module = props.module();
  return module.substr(module.rfind('.') + 1);
}

bool ParseProps(const std::string& input_file_path, sysprop::Properties* props,
                std::string* err) {
  std::string file_contents;

  if (!android::base::ReadFileToString(input_file_path, &file_contents, true)) {
    *err = "Error reading file " + input_file_path + ": " + strerror(errno);
    return false;
  }

  if (!google::protobuf::TextFormat::ParseFromString(file_contents, props)) {
    *err = "Error parsing file " + input_file_path;
    return false;
  }

  if (!ValidateProps(*props, err)) {
    return false;
  }

  for (int i = 0; i < props->prop_size(); ++i) {
    // set each optional field to its default value
    sysprop::Property& prop = *props->mutable_prop(i);
    if (!prop.has_readonly()) prop.set_readonly(true);
  }

  return true;
}

std::string PropNameToIdentifier(const std::string& name) {
  static const std::regex kRegexDot{"\\."};
  return std::regex_replace(name, kRegexDot, "_");
}
