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

#include <cassert>

#include "llvm/Demangle/Demangle.h"
#include "llvm/Demangle/Utility.h"

using llvm::itanium_demangle::OutputBuffer;

#define ERROR (~((unsigned)0))

static unsigned ConsumeUnsignedDecimal(std::string_view& sv) {
  unsigned res = 0, i = 0;
  while(i < sv.size() && sv[i] >= '0' && sv[i] <= '9') {
    res = res * 10 + (sv[i] - '0');
    i++;
  }
  sv.remove_prefix(i);
  if(i == 0)
    return ERROR;
  return res;
}

static unsigned ConsumeUnsigned26(std::string_view& sv) {
  unsigned res = 0, i = 0;
  while(i < sv.size() && sv[i] >= 'A' && sv[i] <= 'Z') {
    res = res * 26 + (sv[i] - 'A');
    i++;
  }
  sv.remove_prefix(i);
  if(i == 0)
    return ERROR;
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

bool consume_front(std::string_view& sv, char c) {
  if(sv.size() > 1 && sv[0] == c) {
    sv.remove_prefix(1);
    return true;
  }
  return false;
}

char *llvm::oxcamlDemangle(const char *MangledName) {
  std::string_view Mangled(MangledName);
  if(!(consume_front(Mangled, '_') && consume_front(Mangled, 'O')))
    return nullptr;

  // Allocate the buffer at a reasonable size, as OutputBuffer allocates 992
  // bytes when starting from an empty buffer
  char *DemangledBuffer;
  DemangledBuffer = static_cast<char *>(std::malloc(Mangled.size()));
  if (DemangledBuffer == nullptr)
    std::terminate();
  OutputBuffer Demangled(DemangledBuffer, Mangled.size());

#define ENDONERROR() do {     \
  std::free(DemangledBuffer); \
  return nullptr;             \
} while(0)

  if(consume_front(Mangled, 'N')) {
    // Named symbol
    while(!Mangled.empty()) {
      if(consume_front(Mangled, 'u')) {
        if(!Demangled.empty())
          Demangled << '.';
        unsigned len = ConsumeUnsignedDecimal(Mangled);
        if(len == ERROR || len <= 0 || len > Mangled.size())
          ENDONERROR();
        size_t split = Mangled.find('_');
        if(split >= len) ENDONERROR();
        std::string_view coded = Mangled.substr(0, split);
        std::string_view raw = Mangled.substr(split+1, len-split-1);
        while(!coded.empty()) {
          unsigned chunklen = ConsumeUnsigned26(coded);
          if(chunklen == ERROR || chunklen > raw.size())
            ENDONERROR();
          Demangled << raw.substr(0,chunklen);
          raw.remove_prefix(chunklen);
          unsigned i;
          for(i = 0; i+1 < coded.size() && islowerhex(coded[i]); i+=2) {
            if(!islowerhex(coded[i+1]))
              ENDONERROR();
            char c = (char)(lowerhex(coded[i]) << 4 | lowerhex(coded[i+1]));
            Demangled << c;
          }
          coded.remove_prefix(i);
        }
        if(!raw.empty())
          Demangled << raw;
        Mangled.remove_prefix(len);
      } else if(!consume_front(Mangled, '_')) {
        if(!Demangled.empty())
          Demangled << '.';
        unsigned len = ConsumeUnsignedDecimal(Mangled);
        if(len == ERROR || len <= 0 || len > Mangled.size())
          ENDONERROR();
        Demangled << Mangled.substr(0, len);
        Mangled.remove_prefix(len);
      } else {
        // we are on the _ that separates the symbol per se from its unique id,
        // so we have nothing left to do
        break;
      }
    }
  } else {
    // Anonymous symbol
    if(!consume_front(Mangled, 'A'))
      ENDONERROR();
    char *demangled_name = static_cast<char *>(std::malloc(sizeof("anonymous")));
    std::memcpy(demangled_name, "anonymous", sizeof("anonymous"));
    return demangled_name;
  }

  Demangled << '\0';

  return Demangled.getBuffer();
}
