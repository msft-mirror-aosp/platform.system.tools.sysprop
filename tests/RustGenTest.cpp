/* Copyright (C) 2023 The Android Open Source Project
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
#include <android-base/file.h>
#include <android-base/test_utils.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <string>

#include "RustGen.h"

namespace {

constexpr const char* kTestSyspropFile =
    R"(owner: Platform
module: "android.sysprop.PlatformProperties"
prop {
    api_name: "test_double"
    type: Double
    prop_name: "android.test_double"
    scope: Internal
    access: ReadWrite
}
prop {
    api_name: "test_int"
    type: Integer
    prop_name: "android.test_int"
    scope: Public
    access: ReadWrite
}
prop {
    api_name: "test_string"
    type: String
    prop_name: "android.test.string"
    scope: Public
    access: Readonly
    legacy_prop_name: "legacy.android.test.string"
}
prop {
    api_name: "test_enum"
    type: Enum
    prop_name: "android.test.enum"
    enum_values: "a|b|c|D|e|f|G"
    scope: Internal
    access: ReadWrite
}
prop {
    api_name: "test_BOOLeaN"
    type: Boolean
    prop_name: "ro.android.test.b"
    scope: Public
    access: Writeonce
}
prop {
    api_name: "android_os_test-long"
    type: Long
    scope: Public
    access: ReadWrite
}
prop {
    api_name: "test_double_list"
    type: DoubleList
    scope: Internal
    access: ReadWrite
}
prop {
    api_name: "test_list_int"
    type: IntegerList
    scope: Public
    access: ReadWrite
}
prop {
    api_name: "test_strlist"
    type: StringList
    scope: Public
    access: ReadWrite
    deprecated: true
}
prop {
    api_name: "el"
    type: EnumList
    enum_values: "enu|mva|lue"
    scope: Internal
    access: ReadWrite
    deprecated: true
}
)";

constexpr const char* kExpectedPublicOutput =
    R"(//! Autogenerated system properties.
//!
//! This is autogenerated crate. The crate contains methods for easy access to
//! the Android system properties.

// Generated by the sysprop generator. DO NOT EDIT!

use std::fmt;
use rustutils::system_properties;

mod gen_parsers_and_formatters;

/// Errors this crate could generate.
#[derive(Debug)]
pub enum SysPropError {
    /// Failed to fetch the system property.
    FetchError(system_properties::PropertyWatcherError),
    /// Failed to set the system property.
    SetError(system_properties::PropertyWatcherError),
    /// Failed to parse the system property value.
    ParseError(String),
}

impl fmt::Display for SysPropError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            SysPropError::FetchError(err) =>
                write!(f, "failed to fetch the system property: {}", err),
            SysPropError::SetError(err) =>
                write!(f, "failed to set the system property: {}", err),
            SysPropError::ParseError(err) =>
                write!(f, "failed to parse the system property value: {}", err),
        }
    }
}

/// Result type specific for this crate.
pub type Result<T> = std::result::Result<T, SysPropError>;

/// Returns the value of the property 'android.test_int' if set.
pub fn test_int() -> Result<Option<i32>> {
    let result = match system_properties::read("android.test_int") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'android.test_int', returns 'Ok' if successful.
pub fn set_test_int(v: i32) -> Result<()> {
    let value = gen_parsers_and_formatters::format(&v);
    system_properties::write("android.test_int", value.as_str()).map_err(SysPropError::SetError)
}

/// Returns the value of the property 'android.test.string' if set.
pub fn test_string() -> Result<Option<String>> {
    let result = match system_properties::read("android.test.string") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    if result.is_ok() { return result; }
    log::debug!("Failed to fetch the original property 'android.test.string' ('{}'), falling back to the legacy one 'legacy.android.test.string'.", result.unwrap_err());
    match system_properties::read("legacy.android.test.string") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    }
}

/// Returns the value of the property 'ro.android.test.b' if set.
pub fn test_boo_lea_n() -> Result<Option<bool>> {
    let result = match system_properties::read("ro.android.test.b") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse_bool(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'ro.android.test.b', returns 'Ok' if successful.
pub fn set_test_boo_lea_n(v: bool) -> Result<()> {
    let value = gen_parsers_and_formatters::format_bool(&v);
    system_properties::write("ro.android.test.b", value.as_str()).map_err(SysPropError::SetError)
}

/// Returns the value of the property 'android_os_test-long' if set.
pub fn android_os_test_long() -> Result<Option<i64>> {
    let result = match system_properties::read("android_os_test-long") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'android_os_test-long', returns 'Ok' if successful.
pub fn set_android_os_test_long(v: i64) -> Result<()> {
    let value = gen_parsers_and_formatters::format(&v);
    system_properties::write("android_os_test-long", value.as_str()).map_err(SysPropError::SetError)
}

/// Returns the value of the property 'test_list_int' if set.
pub fn test_list_int() -> Result<Option<Vec<i32>>> {
    let result = match system_properties::read("test_list_int") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse_list(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'test_list_int', returns 'Ok' if successful.
pub fn set_test_list_int(v: &[i32]) -> Result<()> {
    let value = gen_parsers_and_formatters::format_list(v);
    system_properties::write("test_list_int", value.as_str()).map_err(SysPropError::SetError)
}

/// Returns the value of the property 'test_strlist' if set.
#[deprecated]
pub fn test_strlist() -> Result<Option<Vec<String>>> {
    let result = match system_properties::read("test_strlist") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse_list(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'test_strlist', returns 'Ok' if successful.
#[deprecated]
pub fn set_test_strlist(v: &[String]) -> Result<()> {
    let value = gen_parsers_and_formatters::format_list(v);
    system_properties::write("test_strlist", value.as_str()).map_err(SysPropError::SetError)
}

)";

constexpr const char* kExpectedInternalOutput =
    R"(//! Autogenerated system properties.
//!
//! This is autogenerated crate. The crate contains methods for easy access to
//! the Android system properties.

// Generated by the sysprop generator. DO NOT EDIT!

use std::fmt;
use rustutils::system_properties;

mod gen_parsers_and_formatters;

/// Errors this crate could generate.
#[derive(Debug)]
pub enum SysPropError {
    /// Failed to fetch the system property.
    FetchError(system_properties::PropertyWatcherError),
    /// Failed to set the system property.
    SetError(system_properties::PropertyWatcherError),
    /// Failed to parse the system property value.
    ParseError(String),
}

impl fmt::Display for SysPropError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            SysPropError::FetchError(err) =>
                write!(f, "failed to fetch the system property: {}", err),
            SysPropError::SetError(err) =>
                write!(f, "failed to set the system property: {}", err),
            SysPropError::ParseError(err) =>
                write!(f, "failed to parse the system property value: {}", err),
        }
    }
}

/// Result type specific for this crate.
pub type Result<T> = std::result::Result<T, SysPropError>;

/// Returns the value of the property 'android.test_double' if set.
pub fn test_double() -> Result<Option<f64>> {
    let result = match system_properties::read("android.test_double") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'android.test_double', returns 'Ok' if successful.
pub fn set_test_double(v: f64) -> Result<()> {
    let value = gen_parsers_and_formatters::format(&v);
    system_properties::write("android.test_double", value.as_str()).map_err(SysPropError::SetError)
}

/// Returns the value of the property 'android.test_int' if set.
pub fn test_int() -> Result<Option<i32>> {
    let result = match system_properties::read("android.test_int") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'android.test_int', returns 'Ok' if successful.
pub fn set_test_int(v: i32) -> Result<()> {
    let value = gen_parsers_and_formatters::format(&v);
    system_properties::write("android.test_int", value.as_str()).map_err(SysPropError::SetError)
}

/// Returns the value of the property 'android.test.string' if set.
pub fn test_string() -> Result<Option<String>> {
    let result = match system_properties::read("android.test.string") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    if result.is_ok() { return result; }
    log::debug!("Failed to fetch the original property 'android.test.string' ('{}'), falling back to the legacy one 'legacy.android.test.string'.", result.unwrap_err());
    match system_properties::read("legacy.android.test.string") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq, PartialOrd, Hash, Ord)]
pub enum TestEnumValues {
    A,
    B,
    C,
    D,
    E,
    F,
    G,
}

impl std::str::FromStr for TestEnumValues {
    type Err = String;

    fn from_str(s: &str) -> std::result::Result<Self, Self::Err> {
        match s {
            "a" => Ok(TestEnumValues::A),
            "b" => Ok(TestEnumValues::B),
            "c" => Ok(TestEnumValues::C),
            "D" => Ok(TestEnumValues::D),
            "e" => Ok(TestEnumValues::E),
            "f" => Ok(TestEnumValues::F),
            "G" => Ok(TestEnumValues::G),
            _ => Err(format!("'{}' cannot be parsed for TestEnumValues", s)),
        }
    }
}

impl fmt::Display for TestEnumValues {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            TestEnumValues::A => write!(f, "a"),
            TestEnumValues::B => write!(f, "b"),
            TestEnumValues::C => write!(f, "c"),
            TestEnumValues::D => write!(f, "D"),
            TestEnumValues::E => write!(f, "e"),
            TestEnumValues::F => write!(f, "f"),
            TestEnumValues::G => write!(f, "G"),
            _ => Err(fmt::Error),
        }
    }
}

/// Returns the value of the property 'android.test.enum' if set.
pub fn test_enum() -> Result<Option<TestEnumValues>> {
    let result = match system_properties::read("android.test.enum") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'android.test.enum', returns 'Ok' if successful.
pub fn set_test_enum(v: TestEnumValues) -> Result<()> {
    let value = gen_parsers_and_formatters::format(&v);
    system_properties::write("android.test.enum", value.as_str()).map_err(SysPropError::SetError)
}

/// Returns the value of the property 'ro.android.test.b' if set.
pub fn test_boo_lea_n() -> Result<Option<bool>> {
    let result = match system_properties::read("ro.android.test.b") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse_bool(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'ro.android.test.b', returns 'Ok' if successful.
pub fn set_test_boo_lea_n(v: bool) -> Result<()> {
    let value = gen_parsers_and_formatters::format_bool(&v);
    system_properties::write("ro.android.test.b", value.as_str()).map_err(SysPropError::SetError)
}

/// Returns the value of the property 'android_os_test-long' if set.
pub fn android_os_test_long() -> Result<Option<i64>> {
    let result = match system_properties::read("android_os_test-long") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'android_os_test-long', returns 'Ok' if successful.
pub fn set_android_os_test_long(v: i64) -> Result<()> {
    let value = gen_parsers_and_formatters::format(&v);
    system_properties::write("android_os_test-long", value.as_str()).map_err(SysPropError::SetError)
}

/// Returns the value of the property 'test_double_list' if set.
pub fn test_double_list() -> Result<Option<Vec<f64>>> {
    let result = match system_properties::read("test_double_list") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse_list(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'test_double_list', returns 'Ok' if successful.
pub fn set_test_double_list(v: &[f64]) -> Result<()> {
    let value = gen_parsers_and_formatters::format_list(v);
    system_properties::write("test_double_list", value.as_str()).map_err(SysPropError::SetError)
}

/// Returns the value of the property 'test_list_int' if set.
pub fn test_list_int() -> Result<Option<Vec<i32>>> {
    let result = match system_properties::read("test_list_int") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse_list(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'test_list_int', returns 'Ok' if successful.
pub fn set_test_list_int(v: &[i32]) -> Result<()> {
    let value = gen_parsers_and_formatters::format_list(v);
    system_properties::write("test_list_int", value.as_str()).map_err(SysPropError::SetError)
}

/// Returns the value of the property 'test_strlist' if set.
#[deprecated]
pub fn test_strlist() -> Result<Option<Vec<String>>> {
    let result = match system_properties::read("test_strlist") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse_list(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'test_strlist', returns 'Ok' if successful.
#[deprecated]
pub fn set_test_strlist(v: &[String]) -> Result<()> {
    let value = gen_parsers_and_formatters::format_list(v);
    system_properties::write("test_strlist", value.as_str()).map_err(SysPropError::SetError)
}

#[derive(Copy, Clone, Debug, Eq, PartialEq, PartialOrd, Hash, Ord)]
pub enum ElValues {
    Enu,
    Mva,
    Lue,
}

impl std::str::FromStr for ElValues {
    type Err = String;

    fn from_str(s: &str) -> std::result::Result<Self, Self::Err> {
        match s {
            "enu" => Ok(ElValues::Enu),
            "mva" => Ok(ElValues::Mva),
            "lue" => Ok(ElValues::Lue),
            _ => Err(format!("'{}' cannot be parsed for ElValues", s)),
        }
    }
}

impl fmt::Display for ElValues {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ElValues::Enu => write!(f, "enu"),
            ElValues::Mva => write!(f, "mva"),
            ElValues::Lue => write!(f, "lue"),
            _ => Err(fmt::Error),
        }
    }
}

/// Returns the value of the property 'el' if set.
#[deprecated]
pub fn el() -> Result<Option<Vec<ElValues>>> {
    let result = match system_properties::read("el") {
        Err(e) => Err(SysPropError::FetchError(e)),
        Ok(Some(val)) => gen_parsers_and_formatters::parse_list(val.as_str()).map_err(SysPropError::ParseError).map(Some),
        Ok(None) => Ok(None),
    };
    result
}

/// Sets the value of the property 'el', returns 'Ok' if successful.
#[deprecated]
pub fn set_el(v: &[ElValues]) -> Result<()> {
    let value = gen_parsers_and_formatters::format_list(v);
    system_properties::write("el", value.as_str()).map_err(SysPropError::SetError)
}

)";

}  // namespace

using namespace std::string_literals;

TEST(SyspropTest, RustGenTest) {
  TemporaryFile temp_file;

  // strlen is optimized for constants, so don't worry about it.
  ASSERT_EQ(write(temp_file.fd, kTestSyspropFile, strlen(kTestSyspropFile)),
            strlen(kTestSyspropFile));
  close(temp_file.fd);
  temp_file.fd = -1;

  TemporaryDir temp_dir;

  std::pair<sysprop::Scope, const char*> tests[] = {
      {sysprop::Scope::Internal, kExpectedInternalOutput},
      {sysprop::Scope::Public, kExpectedPublicOutput},
  };

  for (auto [scope, expected_output] : tests) {
    std::string rust_output_path = temp_dir.path + "/lib.rs"s;

    ASSERT_RESULT_OK(GenerateRustLibrary(temp_file.path, scope, temp_dir.path));

    std::string rust_output;
    ASSERT_TRUE(
        android::base::ReadFileToString(rust_output_path, &rust_output, true));
    EXPECT_EQ(rust_output, expected_output);

    unlink(rust_output.c_str());
    rmdir((temp_dir.path + "/com/somecompany"s).c_str());
    rmdir((temp_dir.path + "/com"s).c_str());
  }
}