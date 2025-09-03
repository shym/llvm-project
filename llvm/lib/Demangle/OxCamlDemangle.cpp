//===--- OxCamlDemangle.cpp -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines a demangler for the new mangling scheme devised for OxCaml
//
//===----------------------------------------------------------------------===//

#include "llvm/Demangle/Demangle.h"
#include "llvm/Demangle/StringView.h"
#include "llvm/Demangle/Utility.h"
#include <cassert>

using llvm::itanium_demangle::OutputBuffer;
using llvm::itanium_demangle::StringView;

static unsigned ConsumeUnsignedDecimal(StringView& sv) {
  unsigned res = 0, i = 0;
  while(sv[i] >= '0' && sv[i] <= '9') {
    res = res * 10 + (sv[i] - '0');
    i++;
  }
  assert(i > 0);
  sv = sv.dropFront(i);
  return res;
}

static unsigned ConsumeUnsigned26(StringView& sv) {
  unsigned res = 0, i = 0;
  while(sv[i] >= 'A' && sv[i] <= 'Z') {
    res = res * 26 + (sv[i] - 'A');
    i++;
  }
  assert(i > 0);
  sv = sv.dropFront(i);
  return res;
}

static bool islowerhex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

static unsigned lowerhex(char c) {
  if(c >= '0' && c <= '9')
    return c - '0';
  else {
    assert(c >= 'a' && c <= 'f');
    return c - 'a' + 10;
  }
}

char *llvm::oxcamlDemangle(const char *MangledName) {
  StringView Mangled(MangledName);
  if(!Mangled.consumeFront("_O"))
    return nullptr;

  // Allocate the buffer at a reasonable size, as OutputBuffer allocates 992
  // bytes when starting from an empty buffer
  char *DemangledBuffer;
  DemangledBuffer = static_cast<char *>(std::malloc(Mangled.size()));
  if (DemangledBuffer == nullptr)
    std::terminate();
  OutputBuffer Demangled(DemangledBuffer, Mangled.size());

  if(Mangled.consumeFront('N')) {
    // Named symbol
    while(!Mangled.empty()) {
      if(Mangled.consumeFront('u')) {
        if(!Demangled.empty())
          Demangled << '.';
        unsigned len = ConsumeUnsignedDecimal(Mangled);
        assert(Mangled.size() >= len);
        size_t split = Mangled.find('_');
        assert(split < len);
        StringView coded = Mangled.substr(0, split);
        StringView raw = Mangled.substr(split+1, len-split-1);
        while(!coded.empty()) {
          unsigned chunklen = ConsumeUnsigned26(coded);
          assert(chunklen <= raw.size());
          Demangled << raw.substr(0,chunklen);
          raw = raw.dropFront(chunklen);
          unsigned i;
          for(i = 0; i+1 < coded.size() && islowerhex(coded[i]); i+=2) {
            assert(islowerhex(coded[i+1]));
            char c = (char)(lowerhex(coded[i]) << 4 | lowerhex(coded[i+1]));
            Demangled << c;
          }
          coded = coded.dropFront(i);
        }
        if(!raw.empty())
          Demangled << raw;
        Mangled = Mangled.dropFront(len);
      } else if(!Mangled.consumeFront('_')) {
        if(!Demangled.empty())
          Demangled << '.';
        unsigned len = ConsumeUnsignedDecimal(Mangled);
        assert(Mangled.size() >= len);
        Demangled << Mangled.substr(0, len);
        Mangled = Mangled.dropFront(len);
      } else {
        // we are on the _ that separates the symbol per se from its unique id,
        // so we have nothing left to do
        break;
      }
    }
  } else {
    // Anonymous symbol
    assert(Mangled.consumeFront('A'));
    char *demangled_name = static_cast<char *>(std::malloc(sizeof("anonymous")));
    std::memcpy(demangled_name, "anonymous", sizeof("anonymous"));
    return demangled_name;
  }

  Demangled << '\0';

  return Demangled.getBuffer();
}
