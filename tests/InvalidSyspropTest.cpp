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

#include <cstdio>
#include <string>

#include <android-base/file.h>
#include <android-base/test_utils.h>
#include <gtest/gtest.h>

#include "Common.h"
#include "sysprop.pb.h"

namespace {

constexpr const char* kDuplicatedField =
    R"(
owner: Vendor
module: "com.error.DuplicatedField"
prefix: "com.error"
prop {
    name: "dup"
    type: Integer
    scope: Internal
}
prop {
    name: "dup"
    type: Long
    scope: Public
}
)";

constexpr const char* kEmptyProp =
    R"(
owner: Vendor
module: "com.google.EmptyProp"
prefix: ""
)";

constexpr const char* kInvalidPropName =
    R"(
owner: Odm
module: "odm.invalid.prop.name"
prefix: "invalid"
prop {
    name: "!@#$"
    type: Integer
    scope: System
}
)";

constexpr const char* kEmptyEnumValues =
    R"(
owner: Odm
module: "test.manufacturer"
prefix: "test"
prop {
    name: "empty_enum_value"
    type: Enum
    scope: Internal
}
)";

constexpr const char* kDuplicatedEnumValue =
    R"(
owner: Vendor
module: "vendor.module.name"
prefix: ""
prop {
    name: "status"
    type: Enum
    enum_values: "on|off|intermediate|on"
    scope: Public
}
)";

constexpr const char* kInvalidModuleName =
    R"(
owner: Platform
module: ""
prefix: ""
prop {
    name: "integer"
    type: Integer
    scope: Public
}
)";

constexpr const char* kInvalidNamespaceForPlatform =
    R"(
owner: Platform
module: "android.os.PlatformProperties"
prefix: "vendor.buildprop"
prop {
    name: "utclong"
    type: Long
    scope: System
}
)";

constexpr const char* kInvalidModuleNameForPlatform =
    R"(
owner: Platform
module: "android.os.notPlatformProperties"
prefix: "android.os"
prop {
    name: "stringprop"
    type: String
    scope: Internal
}
)";

constexpr const char* kInvalidModuleNameForVendorOrOdm =
    R"(
owner: Vendor
module: "android.os.PlatformProperties"
prefix: "android.os"
prop {
    name: "init"
    type: Integer
    scope: System
}
)";

constexpr const char* kTestCasesAndExpectedErrors[][2] = {
    {kDuplicatedField, "Duplicated prop name \"dup\""},
    {kEmptyProp, "There is no defined property"},
    {kInvalidPropName, "Invalid prop name \"!@#$\""},
    {kEmptyEnumValues, "Invalid enum value \"\" for prop \"empty_enum_value\""},
    {kDuplicatedEnumValue, "Duplicated enum value \"on\" for prop \"status\""},
    {kInvalidModuleName, "Invalid module name \"\""},
    {kInvalidNamespaceForPlatform,
     "Prop \"utclong\" owned by platform cannot have vendor. or odm. "
     "namespace"},
    {kInvalidModuleNameForPlatform,
     "Platform-defined properties should have "
     "\"android.os.PlatformProperties\" as module name"},
    {kInvalidModuleNameForVendorOrOdm,
     "Vendor or Odm cannot use \"android.os.PlatformProperties\" as module "
     "name"},
};

}  // namespace

TEST(SyspropTest, InvalidSyspropTest) {
  TemporaryFile file;
  close(file.fd);
  file.fd = -1;

  for (auto [test_case, expected_error] : kTestCasesAndExpectedErrors) {
    ASSERT_TRUE(android::base::WriteStringToFile(test_case, file.path));
    std::string err;
    sysprop::Properties props;
    EXPECT_FALSE(ParseProps(file.path, &props, &err));
    EXPECT_EQ(err, expected_error);
  }
}
