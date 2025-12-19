//=======================================================================================//
// File             : sms/sms_helper.h
// Author           : Rahul Bera, SAFARI Research Group (write2bera@gmail.com)
// Date             : 19/AUG/2025
// Description      : Defines auxiliary structures to implement
//                    Spatial Memory Streaming prefetcher, ISCA'06
//=======================================================================================//

#ifndef __SMS_HELPER_H__
#define __SMS_HELPER_H__

#include <stdint.h>

#include "bitmap.h"

class FTEntry
{
public:
  uint64_t page;
  uint64_t pc;
  uint32_t trigger_offset;

public:
  void reset()
  {
    page = 0xdeadbeef;
    pc = 0xdeadbeef;
    trigger_offset = 0;
  }
  FTEntry() { reset(); }
  ~FTEntry() {}
};

class ATEntry
{
public:
  uint64_t page;
  uint64_t pc;
  uint32_t trigger_offset;
  Bitmap pattern;
  uint32_t age;

public:
  void reset()
  {
    page = pc = 0xdeadbeef;
    trigger_offset = 0;
    pattern.reset();
    age = 0;
  }
  ATEntry() { reset(); }
  ~ATEntry() {}
};

class PHTEntry
{
public:
  uint64_t signature;
  Bitmap pattern;
  uint32_t age;

public:
  void reset()
  {
    signature = 0xdeadbeef;
    pattern.reset();
    age = 0;
  }
  PHTEntry() { reset(); }
  ~PHTEntry() {}
};

#endif /* __SMS_HELPER_H__ */
