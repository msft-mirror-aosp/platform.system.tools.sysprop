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

#define LOG_TAG "sysprop_java_gen"

#include "JavaGen.h"

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

constexpr const char* kJavaFileImports =
    R"(import android.annotation.SystemApi;

import java.util.ArrayList;
import java.util.function.Function;
import java.util.List;
import java.util.Optional;
import java.util.StringJoiner;

)";

constexpr const char* kJavaParsersAndFormatters =
    R"(private static Boolean tryParseBoolean(String str) {
    switch (str.toLowerCase()) {
        case "1":
        case "y":
        case "yes":
        case "on":
        case "true":
            return Boolean.TRUE;
        case "0":
        case "n":
        case "no":
        case "off":
        case "false":
            return Boolean.FALSE;
        default:
            return null;
    }
}

private static Integer tryParseInteger(String str) {
    try {
        return Integer.valueOf(str);
    } catch (NumberFormatException e) {
        return null;
    }
}

private static Long tryParseLong(String str) {
    try {
        return Long.valueOf(str);
    } catch (NumberFormatException e) {
        return null;
    }
}

private static Double tryParseDouble(String str) {
    try {
        return Double.valueOf(str);
    } catch (NumberFormatException e) {
        return null;
    }
}

private static String tryParseString(String str) {
    return str;
}

private static <T extends Enum<T>> T tryParseEnum(Class<T> enumType, String str) {
    try {
        return Enum.valueOf(enumType, str);
    } catch (IllegalArgumentException e) {
        return null;
    }
}

private static <T> List<T> tryParseList(Function<String, T> elementParser, String str) {
    List<T> ret = new ArrayList<>();

    for (String element : str.split(",")) {
        T parsed = elementParser.apply(element);
        if (parsed == null) {
            return null;
        }
        ret.add(parsed);
    }

    return ret;
}

private static <T extends Enum<T>> List<T> tryParseEnumList(Class<T> enumType, String str) {
    List<T> ret = new ArrayList<>();

    for (String element : str.split(",")) {
        T parsed = tryParseEnum(enumType, element);
        if (parsed == null) {
            return null;
        }
        ret.add(parsed);
    }

    return ret;
}

private static <T> String formatList(List<T> list) {
    StringJoiner joiner = new StringJoiner(",");

    for (T element : list) {
        joiner.add(element.toString());
    }

    return joiner.toString();
}

)";

constexpr const char* kJniLibraryIncludes =
    R"(#include <cstdint>
#include <iterator>
#include <string>

#include <dlfcn.h>
#include <jni.h>

#include <android-base/logging.h>

)";

constexpr const char* kJniLibraryUtils =
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

jstring GetProp(JNIEnv* env, const char* key) {
    auto pi = system_property_find(key);
    if (pi == nullptr) return env->NewStringUTF("");
    std::string ret;
    system_property_read_callback(pi, [](void* cookie, const char*, const char* value, std::uint32_t) {
        *static_cast<std::string*>(cookie) = value;
    }, &ret);
    return env->NewStringUTF(ret.c_str());
}

}  // namespace libc

class ScopedUtfChars {
  public:
    ScopedUtfChars(JNIEnv* env, jstring s) : env_(env), string_(s) {
        utf_chars_ = env->GetStringUTFChars(s, nullptr);
    }

    ~ScopedUtfChars() {
        if (utf_chars_) {
            env_->ReleaseStringUTFChars(string_, utf_chars_);
        }
    }

    const char* c_str() const {
        return utf_chars_;
    }

  private:
    JNIEnv* env_;
    jstring string_;
    const char* utf_chars_;
};

)";

constexpr const char* kJniOnload =
    R"(jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env = nullptr;

    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOG(ERROR) << "GetEnv failed";
        return -1;
    }

    jclass clazz = env->FindClass(kClassName);
    if (clazz == nullptr) {
        LOG(ERROR) << "Cannot find class " << kClassName;
        return -1;
    }

    if (env->RegisterNatives(clazz, methods, std::size(methods)) < 0) {
        LOG(ERROR) << "RegisterNatives failed";
        return -1;
    }

    return JNI_VERSION_1_6;
}
)";

const std::regex kRegexDot{"\\."};
const std::regex kRegexUnderscore{"_"};

struct Arguments {
  std::string java_output_dir_;
  std::string jni_output_dir_;
  std::string input_file_path_;
};

std::string GetJavaTypeName(const sysprop::Property& prop);
std::string GetJavaEnumTypeName(const sysprop::Property& prop);
std::string GetJavaPackageName(const sysprop::Properties& props);
std::string GetJavaClassName(const sysprop::Properties& props);
bool IsListProp(const sysprop::Property& prop);
void WriteJavaAnnotation(CodeWriter& writer, const sysprop::Property& prop);
bool GenerateJavaClass(const sysprop::Properties& props,
                       std::string* java_result, std::string* err);
bool GenerateJniLibrary(const sysprop::Properties& props,
                        std::string* jni_result, std::string* err);

std::string GetJavaEnumTypeName(const sysprop::Property& prop) {
  return PropNameToIdentifier(prop.name()) + "_values";
}

std::string GetJavaTypeName(const sysprop::Property& prop) {
  switch (prop.type()) {
    case sysprop::Boolean:
      return "Boolean";
    case sysprop::Integer:
      return "Integer";
    case sysprop::Long:
      return "Long";
    case sysprop::Double:
      return "Double";
    case sysprop::String:
      return "String";
    case sysprop::Enum:
      return GetJavaEnumTypeName(prop);
    case sysprop::BooleanList:
      return "List<Boolean>";
    case sysprop::IntegerList:
      return "List<Integer>";
    case sysprop::LongList:
      return "List<Long>";
    case sysprop::DoubleList:
      return "List<Double>";
    case sysprop::StringList:
      return "List<String>";
    case sysprop::EnumList:
      return "List<" + GetJavaEnumTypeName(prop) + ">";
    default:
      __builtin_unreachable();
  }
}

std::string GetParsingExpression(const sysprop::Property& prop) {
  std::string native_method =
      "native_" + PropNameToIdentifier(prop.name()) + "_get";

  switch (prop.type()) {
    case sysprop::Boolean:
      return "tryParseBoolean(" + native_method + "())";
    case sysprop::Integer:
      return "tryParseInteger(" + native_method + "())";
    case sysprop::Long:
      return "tryParseLong(" + native_method + "())";
    case sysprop::Double:
      return "tryParseDouble(" + native_method + "())";
    case sysprop::String:
      return "tryParseString(" + native_method + "())";
    case sysprop::Enum:
      return "tryParseEnum(" + GetJavaEnumTypeName(prop) + ".class, " +
             native_method + "())";
    case sysprop::EnumList:
      return "tryParseEnumList(" + GetJavaEnumTypeName(prop) + ".class, " +
             native_method + "())";
    default:
      break;
  }

  // The remaining cases are lists for types other than Enum which share the
  // same parsing function "tryParseList"
  std::string element_parser;

  switch (prop.type()) {
    case sysprop::BooleanList:
      element_parser = "v -> tryParseBoolean(v)";
      break;
    case sysprop::IntegerList:
      element_parser = "v -> tryParseInteger(v)";
      break;
    case sysprop::LongList:
      element_parser = "v -> tryParseLong(v)";
      break;
    case sysprop::DoubleList:
      element_parser = "v -> tryParseDouble(v)";
      break;
    case sysprop::StringList:
      element_parser = "v -> tryParseString(v)";
      break;
    default:
      __builtin_unreachable();
  }

  return "tryParseList(" + element_parser + ", " + native_method + "())";
}

std::string GetJavaPackageName(const sysprop::Properties& props) {
  const std::string& module = props.module();
  return module.substr(0, module.rfind('.'));
}

std::string GetJavaClassName(const sysprop::Properties& props) {
  const std::string& module = props.module();
  return module.substr(module.rfind('.') + 1);
}

bool IsListProp(const sysprop::Property& prop) {
  switch (prop.type()) {
    case sysprop::BooleanList:
    case sysprop::IntegerList:
    case sysprop::LongList:
    case sysprop::DoubleList:
    case sysprop::StringList:
    case sysprop::EnumList:
      return true;
    default:
      return false;
  }
}

void WriteJavaAnnotation(CodeWriter& writer, const sysprop::Property& prop) {
  switch (prop.scope()) {
    case sysprop::System:
      writer.Write("@SystemApi\n");
      break;
    case sysprop::Internal:
      writer.Write("/** @hide */\n");
      break;
    default:
      break;
  }
}

bool GenerateJavaClass(const sysprop::Properties& props,
                       std::string* java_result,
                       [[maybe_unused]] std::string* err) {
  std::string package_name = GetJavaPackageName(props);
  std::string class_name = GetJavaClassName(props);

  CodeWriter writer(kIndent);
  writer.Write("%s", kGeneratedFileFooterComments);
  writer.Write("package %s;\n\n", package_name.c_str());
  writer.Write("%s", kJavaFileImports);
  writer.Write("public final class %s {\n", class_name.c_str());
  writer.Indent();
  writer.Write("private %s () {}\n\n", class_name.c_str());
  writer.Write("static {\n");
  writer.Indent();
  writer.Write("System.loadLibrary(\"%s_jni\");\n",
               GetModuleName(props).c_str());
  writer.Dedent();
  writer.Write("}\n\n");
  writer.Write("%s", kJavaParsersAndFormatters);

  for (int i = 0; i < props.prop_size(); ++i) {
    writer.Write("\n");

    const sysprop::Property& prop = props.prop(i);

    std::string prop_id = PropNameToIdentifier(prop.name()).c_str();
    std::string prop_type = GetJavaTypeName(prop);

    if (prop.type() == sysprop::Enum || prop.type() == sysprop::EnumList) {
      WriteJavaAnnotation(writer, prop);
      writer.Write("public static enum %s {\n",
                   GetJavaEnumTypeName(prop).c_str());
      writer.Indent();
      for (const std::string& name :
           android::base::Split(prop.enum_values(), "|")) {
        writer.Write("%s,\n", name.c_str());
      }
      writer.Dedent();
      writer.Write("}\n\n");
    }

    WriteJavaAnnotation(writer, prop);

    writer.Write("public static Optional<%s> %s() {\n", prop_type.c_str(),
                 prop_id.c_str());
    writer.Indent();
    writer.Write("return Optional.ofNullable(%s);\n",
                 GetParsingExpression(prop).c_str());
    writer.Dedent();
    writer.Write("}\n\n");

    writer.Write("private static native String native_%s_get();\n",
                 prop_id.c_str());

    if (!prop.readonly()) {
      writer.Write("\n");
      WriteJavaAnnotation(writer, prop);
      writer.Write("public static boolean %s(%s value) {\n", prop_id.c_str(),
                   prop_type.c_str());
      writer.Indent();
      writer.Write("return native_%s_set(%s);\n", prop_id.c_str(),
                   IsListProp(prop) ? "formatList(value)" : "value.toString()");
      writer.Dedent();
      writer.Write("}\n\n");
      writer.Write("private static native boolean native_%s_set(String str);\n",
                   prop_id.c_str());
    }
  }

  writer.Dedent();
  writer.Write("}\n");

  *java_result = writer.Code();
  return true;
}

bool GenerateJniLibrary(const sysprop::Properties& props,
                        std::string* jni_result,
                        [[maybe_unused]] std::string* err) {
  CodeWriter writer(kIndent);
  writer.Write("%s", kGeneratedFileFooterComments);
  writer.Write("#define LOG_TAG \"%s_jni\"\n\n", props.module().c_str());
  writer.Write("%s", kJniLibraryIncludes);
  writer.Write("namespace {\n\n");
  writer.Write("constexpr const char* kClassName = \"%s\";\n\n",
               std::regex_replace(props.module(), kRegexDot, "/").c_str());
  writer.Write("%s", kJniLibraryUtils);

  for (int i = 0; i < props.prop_size(); ++i) {
    const sysprop::Property& prop = props.prop(i);
    std::string prop_id = PropNameToIdentifier(prop.name());
    std::string prefix = (prop.readonly() ? "ro." : "") + props.prefix();
    if (!prefix.empty() && prefix.back() != '.') prefix.push_back('.');

    writer.Write("jstring JNICALL %s_get(JNIEnv* env, jclass) {\n",
                 prop_id.c_str());
    writer.Indent();
    writer.Write("return libc::GetProp(env, \"%s%s\");\n", prefix.c_str(),
                 prop.name().c_str());
    writer.Dedent();
    writer.Write("}\n\n");

    if (!prop.readonly()) {
      writer.Write(
          "jboolean JNICALL %s_set(JNIEnv* env, jclass, "
          "jstring str) {\n",
          prop_id.c_str());
      writer.Indent();
      writer.Write(
          "return libc::system_property_set(\"%s%s\", ScopedUtfChars(env, "
          "str).c_str()) == 0 ? JNI_TRUE : JNI_FALSE;\n",
          prefix.c_str(), prop.name().c_str());
      writer.Dedent();
      writer.Write("}\n\n");
    }
  }

  writer.Write("const JNINativeMethod methods[] = {\n");
  writer.Indent();

  for (int i = 0; i < props.prop_size(); ++i) {
    const sysprop::Property& prop = props.prop(i);
    std::string prop_id = PropNameToIdentifier(prop.name());

    writer.Write(
        "{\"native_%s_get\", \"()Ljava/lang/String;\", "
        "reinterpret_cast<void*>(%s_get)},\n",
        prop_id.c_str(), prop_id.c_str());
    if (!prop.readonly()) {
      writer.Write(
          "{\"native_%s_set\", \"(Ljava/lang/String;)Z\", "
          "reinterpret_cast<void*>(%s_set)},\n",
          prop_id.c_str(), prop_id.c_str());
    }
  }

  writer.Dedent();
  writer.Write("};\n\n");
  writer.Write("}  // namespace\n\n");
  writer.Write("%s", kJniOnload);

  *jni_result = writer.Code();
  return true;
}

}  // namespace

bool GenerateJavaLibrary(const std::string& input_file_path,
                         const std::string& java_output_dir,
                         const std::string& jni_output_dir, std::string* err) {
  sysprop::Properties props;

  if (!ParseProps(input_file_path, &props, err)) {
    return false;
  }

  std::string java_result, jni_result;

  if (!GenerateJavaClass(props, &java_result, err)) {
    return false;
  }

  if (!GenerateJniLibrary(props, &jni_result, err)) {
    return false;
  }

  std::string package_name = GetJavaPackageName(props);
  std::string java_package_dir =
      java_output_dir + "/" + std::regex_replace(package_name, kRegexDot, "/");

  if (!IsDirectory(java_package_dir) && !CreateDirectories(java_package_dir)) {
    *err = "Creating directory to " + java_package_dir +
           " failed: " + strerror(errno);
    return false;
  }

  if (!IsDirectory(jni_output_dir) && !CreateDirectories(jni_output_dir)) {
    *err = "Creating directory to " + jni_output_dir +
           " failed: " + strerror(errno);
    return false;
  }

  std::string class_name = GetJavaClassName(props);
  std::string java_output_file = java_package_dir + "/" + class_name + ".java";
  if (!android::base::WriteStringToFile(java_result, java_output_file)) {
    *err = "Writing generated java class to " + java_output_file +
           " failed: " + strerror(errno);
    return false;
  }

  std::string jni_output_file = jni_output_dir + "/" + class_name + "_jni.cpp";
  if (!android::base::WriteStringToFile(jni_result, jni_output_file)) {
    *err = "Writing generated jni library to " + jni_output_file +
           " failed: " + strerror(errno);
    return false;
  }

  return true;
}
