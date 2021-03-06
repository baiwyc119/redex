/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "SimpleReflectionAnalysis.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>
#include <unordered_map>

#include "DexClass.h"
#include "DexLoader.h"
#include "DexStore.h"
#include "DexUtil.h"
#include "IRCode.h"
#include "IRInstruction.h"
#include "IROpcode.h"
#include "JarLoader.h"
#include "RedexContext.h"

using namespace sra;

std::unordered_map<std::string, std::string> expected = {
    {"f1", "\"foo1\""},
    {"f2", "\"foo2\""},
    {"m1", "\"moo1\""},
    {"m2", "\"moo2\""},
    {"f3", "OBJECT{Ljava/lang/String;}"},
    {"f4", "\"foo2\""},
    {"f5", "\"foo2\""},
    {"f6", "OBJECT{Ljava/lang/String;}"},
    {"m7", "OBJECT{Ljava/lang/String;}"},
    {"f8", "OBJECT{Ljava/lang/String;}"},
    {"f9", "\"foo1\""}};

void validate_arguments(IRInstruction* insn,
                        const SimpleReflectionAnalysis& analysis) {
  auto label = analysis.get_abstract_object(insn->src(0), insn);
  auto actual = analysis.get_abstract_object(insn->src(1), insn);
  EXPECT_TRUE(label);
  EXPECT_EQ(STRING, label->kind);
  std::string label_str = label->dex_string->str();
  std::string actual_str = "?";
  if (actual) {
    std::ostringstream out;
    out << *actual;
    actual_str = out.str();
  }
  auto it = expected.find(label_str);
  EXPECT_TRUE(it != expected.end());
  EXPECT_EQ(it->second, actual_str);
}

TEST(SimpleReflectionAnalysisTest, nominalCases) {
  g_redex = new RedexContext();

  std::vector<DexStore> stores;
  DexMetadata dm;
  dm.set_id("classes");
  DexStore root_store(dm);

  const char* dexfile = std::getenv("dexfile");
  ASSERT_NE(nullptr, dexfile);
  root_store.add_classes(load_classes_from_dex(dexfile));
  stores.emplace_back(std::move(root_store));

  const char* android_env_sdk = std::getenv("ANDROID_SDK");
  const char* android_config_sdk = std::getenv("sdk_path");

  const char* android_sdk = (strncmp(android_config_sdk, "None", 4) != 0)
                                ? android_config_sdk
                                : android_env_sdk;

  ASSERT_NE(nullptr, android_sdk);
  const char* android_target = std::getenv("android_target");
  ASSERT_NE(nullptr, android_target);
  std::string android_version(android_target);
  ASSERT_NE("NotFound", android_version);
  std::string sdk_jar = std::string(android_sdk) + "/platforms/" +
                        android_version + "/android.jar";
  ASSERT_TRUE(load_jar_file(sdk_jar.c_str()));

  DexStoreClassesIterator it(stores);
  Scope scope = build_class_scope(it);
  for (const auto& cls : scope) {
    if (cls->get_name()->str() ==
        "Lcom/facebook/redextest/SimpleReflectionAnalysis$Isolate;") {
      for (const auto& method : cls->get_dmethods()) {
        if (method->get_name()->str() == "main") {
          SimpleReflectionAnalysis analysis(method);
          for (auto& mie : InstructionIterable(method->get_code())) {
            IRInstruction* insn = mie.insn;
            if (insn->opcode() == OPCODE_INVOKE_STATIC &&
                insn->get_method()->get_name()->str() == "check") {
              validate_arguments(insn, analysis);
            }
          }
        }
      }
    }
  }

  delete g_redex;
}
