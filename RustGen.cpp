/*
 * Copyright (C) 2023 The Android Open Source Project
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

#define LOG_TAG "sysprop_rust_gen"

#include "RustGen.h"

#include <android-base/file.h>

#include <regex>
#include <string>

#include "CodeWriter.h"
#include "Common.h"
#include "sysprop.pb.h"

using android::base::Result;

namespace {

constexpr const char* kDocs = R"(//! Autogenerated system property accessors.
//!
//! This is an autogenerated module. The module contains methods for typed access to
//! Android system properties.)";

constexpr const char* kRustFileImports = R"(use std::fmt;
use rustutils::system_properties::{self, error::SysPropError, parsers_formatters};)";

constexpr const char* kIndent = "    ";

constexpr const char* kDeprecated = "#[deprecated]";

std::string GetRustEnumType(const sysprop::Property& prop) {
  std::string result = ApiNameToIdentifier(prop.api_name());
  return SnakeCaseToCamelCase(result) + "Values";
}

std::string GetRustReturnType(const sysprop::Property& prop) {
  switch (prop.type()) {
    case sysprop::Boolean:
      return "bool";
    case sysprop::Integer:
      return "i32";
    case sysprop::UInt:
      return "u32";
    case sysprop::Long:
      return "i64";
    case sysprop::ULong:
      return "u64";
    case sysprop::Double:
      return "f64";
    case sysprop::String:
      return "String";
    case sysprop::Enum:
      return GetRustEnumType(prop);
    case sysprop::BooleanList:
      return "Vec<bool>";
    case sysprop::IntegerList:
      return "Vec<i32>";
    case sysprop::UIntList:
      return "Vec<u32>";
    case sysprop::LongList:
      return "Vec<i64>";
    case sysprop::ULongList:
      return "Vec<u64>";
    case sysprop::DoubleList:
      return "Vec<f64>";
    case sysprop::StringList:
      return "Vec<String>";
    case sysprop::EnumList:
      return "Vec<" + GetRustEnumType(prop) + ">";
    default:
      __builtin_unreachable();
  }
}

std::string GetRustAcceptType(const sysprop::Property& prop) {
  switch (prop.type()) {
    case sysprop::Boolean:
      return "bool";
    case sysprop::Integer:
      return "i32";
    case sysprop::UInt:
      return "u32";
    case sysprop::Long:
      return "i64";
    case sysprop::ULong:
      return "u64";
    case sysprop::Double:
      return "f64";
    case sysprop::String:
      return "&str";
    case sysprop::Enum:
      return GetRustEnumType(prop);
    case sysprop::BooleanList:
      return "&[bool]";
    case sysprop::IntegerList:
      return "&[i32]";
    case sysprop::UIntList:
      return "&[u32]";
    case sysprop::LongList:
      return "&[i64]";
    case sysprop::ULongList:
      return "&[u64]";
    case sysprop::DoubleList:
      return "&[f64]";
    case sysprop::StringList:
      return "&[String]";
    case sysprop::EnumList:
      return "&[" + GetRustEnumType(prop) + "]";
    default:
      __builtin_unreachable();
  }
}

std::string GetTypeParser(const sysprop::Property& prop) {
  switch (prop.type()) {
    case sysprop::Boolean:
      return "parsers_formatters::parse_bool";
    case sysprop::Integer:
    case sysprop::UInt:
    case sysprop::Long:
    case sysprop::ULong:
    case sysprop::Double:
    case sysprop::String:
    case sysprop::Enum:
      return "parsers_formatters::parse";
    case sysprop::BooleanList:
      return "parsers_formatters::parse_bool_list";
    case sysprop::IntegerList:
    case sysprop::UIntList:
    case sysprop::LongList:
    case sysprop::ULongList:
    case sysprop::DoubleList:
    case sysprop::StringList:
    case sysprop::EnumList:
      return "parsers_formatters::parse_list";
    default:
      __builtin_unreachable();
  }
}

std::string GetTypeFormatter(const sysprop::Property& prop) {
  switch (prop.type()) {
    case sysprop::Boolean:
      if (prop.integer_as_bool()) {
        return "parsers_formatters::format_bool_as_int";
      }
      return "parsers_formatters::format_bool";
    case sysprop::String:
    case sysprop::Integer:
    case sysprop::UInt:
    case sysprop::Long:
    case sysprop::ULong:
    case sysprop::Double:
    case sysprop::Enum:
      return "parsers_formatters::format";
    case sysprop::BooleanList:
      if (prop.integer_as_bool()) {
        return "parsers_formatters::format_bool_list_as_int";
      }
      return "parsers_formatters::format_bool_list";
    case sysprop::IntegerList:
    case sysprop::UIntList:
    case sysprop::LongList:
    case sysprop::ULongList:
    case sysprop::DoubleList:
    case sysprop::StringList:
    case sysprop::EnumList:
      return "parsers_formatters::format_list";
    default:
      __builtin_unreachable();
  }
}

std::string GenerateRustSource(sysprop::Properties props, sysprop::Scope scope) {
  CodeWriter writer(kIndent);
  writer.Write("%s\n\n", kDocs);
  writer.Write("%s", kGeneratedFileFooterComments);
  writer.Write("%s\n\n", kRustFileImports);

  for (int i = 0; i < props.prop_size(); ++i) {
    const sysprop::Property& prop = props.prop(i);
    if (prop.scope() > scope) continue;

    std::string prop_id =
        CamelCaseToSnakeCase(ApiNameToIdentifier(prop.api_name()));

    std::string prop_const = ToUpper(std::string(prop_id)) + "_PROP";

    // Create constant.
    writer.Write("/// The property name of the \"%s\" API.\n", prop_id.c_str());
    writer.Write("pub const %s: &str = \"%s\";\n\n", prop_const.c_str(),
                 prop.prop_name().c_str());

    // Create enum.
    if (prop.type() == sysprop::Enum || prop.type() == sysprop::EnumList) {
      auto enum_type = GetRustEnumType(prop);
      auto values = ParseEnumValues(prop.enum_values());

      writer.Write("#[allow(missing_docs)]\n");
      writer.Write(
          "#[derive(Copy, Clone, Debug, Eq, "
          "PartialEq, PartialOrd, Hash, Ord)]\n");
      writer.Write("pub enum %s {\n", enum_type.c_str());
      writer.Indent();
      for (const std::string& value : values) {
        writer.Write("%s,\n", SnakeCaseToCamelCase(value).c_str());
      }
      writer.Dedent();
      writer.Write("}\n\n");

      // Enum parser.
      writer.Write("impl std::str::FromStr for %s {\n", enum_type.c_str());
      writer.Indent();
      writer.Write("type Err = String;\n\n");
      writer.Write(
          "fn from_str(s: &str) -> std::result::Result<Self, Self::Err> {\n");
      writer.Indent();
      writer.Write("match s {\n");
      writer.Indent();
      for (const std::string& value : values) {
        writer.Write("\"%s\" => Ok(%s::%s),\n", value.c_str(),
                     enum_type.c_str(), SnakeCaseToCamelCase(value).c_str());
      }
      writer.Write("_ => Err(format!(\"'{}' cannot be parsed for %s\", s)),\n",
                   enum_type.c_str());
      writer.Dedent();
      writer.Write("}\n");
      writer.Dedent();
      writer.Write("}\n");
      writer.Dedent();
      writer.Write("}\n\n");

      // Enum formatter.
      writer.Write("impl fmt::Display for %s {\n", enum_type.c_str());
      writer.Indent();
      writer.Write(
          "fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {\n");
      writer.Indent();
      writer.Write("match self {\n");
      writer.Indent();
      for (const std::string& value : values) {
        writer.Write("%s::%s => write!(f, \"%s\"),\n", enum_type.c_str(),
                     SnakeCaseToCamelCase(value).c_str(), value.c_str());
      }
      writer.Dedent();
      writer.Write("}\n");
      writer.Dedent();
      writer.Write("}\n");
      writer.Dedent();
      writer.Write("}\n\n");
    }

    // Write getter.
    std::string prop_return_type = GetRustReturnType(prop);
    std::string parser = GetTypeParser(prop);
    writer.Write("/// Returns the value of the property '%s' if set.\n",
                 prop.prop_name().c_str());
    if (prop.deprecated()) writer.Write("%s\n", kDeprecated);
    // Escape prop id if it is similar to `type` keyword.
    std::string identifier = (prop_id == "type") ? "r#" + prop_id : prop_id;
    writer.Write(
        "pub fn %s() -> std::result::Result<Option<%s>, SysPropError> {\n",
        identifier.c_str(), prop_return_type.c_str());
    writer.Indent();
    // Try original property.
    writer.Write("let result = match system_properties::read(%s) {\n",
                 prop_const.c_str());
    writer.Indent();
    writer.Write("Err(e) => Err(SysPropError::FetchError(e)),\n");
    writer.Write(
        "Ok(Some(val)) => "
        "%s(val.as_str()).map_err(SysPropError::ParseError).map(Some),\n",
        parser.c_str());
    writer.Write("Ok(None) => Ok(None),\n");
    writer.Dedent();
    writer.Write("};\n");
    // Try legacy property
    if (!prop.legacy_prop_name().empty()) {
      writer.Write("if result.is_ok() { return result; }\n");
      // Avoid omitting the error when fallback to legacy.
      writer.Write(
          "log::debug!(\"Failed to fetch the original property '%s' ('{}'), "
          "falling back to the legacy one '%s'.\", result.unwrap_err());\n",
          prop.prop_name().c_str(), prop.legacy_prop_name().c_str());
      writer.Write("match system_properties::read(\"%s\") {\n",
                   prop.legacy_prop_name().c_str());
      writer.Indent();
      writer.Write("Err(e) => Err(SysPropError::FetchError(e)),\n");
      writer.Write(
          "Ok(Some(val)) => "
          "%s(val.as_str()).map_err(SysPropError::ParseError).map(Some),\n",
          parser.c_str());
      writer.Write("Ok(None) => Ok(None),\n");
      writer.Dedent();
      writer.Write("}\n");
    } else {
      writer.Write("result\n");
    }
    writer.Dedent();
    writer.Write("}\n\n");

    // Write setter.
    if (prop.access() == sysprop::Readonly) continue;
    std::string prop_accept_type = GetRustAcceptType(prop);
    std::string formatter = GetTypeFormatter(prop);
    writer.Write(
        "/// Sets the value of the property '%s', "
        "returns 'Ok' if successful.\n",
        prop.prop_name().c_str());
    if (prop.deprecated()) writer.Write("%s\n", kDeprecated);
    writer.Write(
        "pub fn set_%s(v: %s) -> std::result::Result<(), SysPropError> {\n",
        prop_id.c_str(), prop_accept_type.c_str());
    writer.Indent();
    std::string write_arg;
    if (prop.type() == sysprop::String) {
      write_arg = "v";
    } else {
      // We need to borrow single values.
      std::string format_arg = prop.type() >= 20 ? "v" : "&v";
      writer.Write("let value = %s(%s);\n", formatter.c_str(),
                   format_arg.c_str());
      write_arg = "value.as_str()";
    }
    writer.Write(
        "system_properties::write(%s, %s).map_err(SysPropError::SetError)\n",
        prop_const.c_str(), write_arg.c_str());
    writer.Dedent();
    writer.Write("}\n\n");
  }
  return writer.Code();
}

};  // namespace

Result<void> GenerateRustLibrary(const std::string& input_file_path,
                                 sysprop::Scope scope,
                                 const std::string& rust_output_dir) {
  sysprop::Properties props;

  if (auto res = ParseProps(input_file_path); res.ok()) {
    props = std::move(*res);
  } else {
    return res.error();
  }

  std::string lib_path = rust_output_dir + "/mod.rs";
  std::string lib_result = GenerateRustSource(props, scope);
  if (!android::base::WriteStringToFile(lib_result, lib_path)) {
    return ErrnoErrorf("Writing generated rust lib to {} failed", lib_path);
  }

  return {};
}
