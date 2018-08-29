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

#define LOG_TAG "sysprop_java"

#include <android-base/logging.h>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <getopt.h>

#include "JavaGen.h"

namespace {

struct Arguments {
  std::string input_file_path_;
  std::string java_output_dir_;
  std::string jni_output_dir_;
};

[[noreturn]] void PrintUsage(const char* exe_name) {
  std::printf(
      "Usage: %s [--java-output-dir dir] [--jni-output-dir dir] sysprop_file\n",
      exe_name);
  std::exit(EXIT_FAILURE);
}

bool ParseArgs(int argc, char* argv[], Arguments* args, std::string* err) {
  for (;;) {
    static struct option long_options[] = {
        {"java-output-dir", required_argument, 0, 'j'},
        {"jni-output-dir", required_argument, 0, 'n'},
    };

    int opt = getopt_long_only(argc, argv, "", long_options, nullptr);
    if (opt == -1) break;

    switch (opt) {
      case 'j':
        args->java_output_dir_ = optarg;
        break;
      case 'n':
        args->jni_output_dir_ = optarg;
        break;
      default:
        PrintUsage(argv[0]);
    }
  }

  if (optind >= argc) {
    *err = "No input file specified";
    return false;
  }

  if (optind + 1 < argc) {
    *err = "More than one input file";
    return false;
  }

  args->input_file_path_ = argv[optind];
  if (args->java_output_dir_.empty()) args->java_output_dir_ = ".";
  if (args->jni_output_dir_.empty()) args->jni_output_dir_ = ".";

  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  Arguments args;
  std::string err;
  if (!ParseArgs(argc, argv, &args, &err)) {
    std::fprintf(stderr, "%s: %s\n", argv[0], err.c_str());
    PrintUsage(argv[0]);
  }

  if (!GenerateJavaLibrary(args.input_file_path_, args.java_output_dir_,
                           args.jni_output_dir_, &err)) {
    LOG(FATAL) << "Error during generating java sysprop from "
               << args.input_file_path_ << ": " << err;
  }
}
