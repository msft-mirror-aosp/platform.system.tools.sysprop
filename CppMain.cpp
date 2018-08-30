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

#define LOG_TAG "sysprop_cpp"

#include <android-base/logging.h>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <getopt.h>

#include "CppGen.h"

namespace {

struct Arguments {
  std::string input_file_path_;
  std::string header_output_dir_;
  std::string source_output_dir_;
};

[[noreturn]] void PrintUsage(const char* exe_name) {
  std::printf(
      "Usage: %s [--header-output-dir dir] [--source-output-dir dir] "
      "sysprop_file \n",
      exe_name);
  std::exit(EXIT_FAILURE);
}

bool ParseArgs(int argc, char* argv[], Arguments* args, std::string* err) {
  for (;;) {
    static struct option long_options[] = {
        {"header-output-dir", required_argument, 0, 'h'},
        {"source-output-dir", required_argument, 0, 's'},
    };

    int opt = getopt_long_only(argc, argv, "", long_options, nullptr);
    if (opt == -1) break;

    switch (opt) {
      case 'h':
        args->header_output_dir_ = optarg;
        break;
      case 's':
        args->source_output_dir_ = optarg;
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
  if (args->header_output_dir_.empty()) args->header_output_dir_ = ".";
  if (args->source_output_dir_.empty()) args->source_output_dir_ = ".";

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

  if (!GenerateCppFiles(args.input_file_path_, args.header_output_dir_,
                        args.source_output_dir_, &err)) {
    LOG(FATAL) << "Error during generating cpp sysprop from "
               << args.input_file_path_ << ": " << err;
  }
}
