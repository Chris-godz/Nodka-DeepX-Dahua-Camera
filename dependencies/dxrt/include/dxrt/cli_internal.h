// Copyright (c) 2022 DEEPX Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "dxrt/common.h"
#include "dxrt/cli.h"
#include <string>

namespace dxrt {
#if 0
class DeviceDumpCommand : public CLICommand
{
 public:
    explicit DeviceDumpCommand(cxxopts::ParseResult &);
 private:
    void doCommand(DevicePtr devicePtr) override;
};

class FWConfigCommand : public CLICommand
{
 public:
    explicit FWConfigCommand(cxxopts::ParseResult &);
 private:
    void doCommand(DevicePtr devicePtr) override;
};

class FWLogCommand : public CLICommand
{
 public:
    explicit FWLogCommand(cxxopts::ParseResult &);
 private:
    void doCommand(DevicePtr devicePtr) override;
};
#endif
class DXRT_API LedCommand : public CLICommand
{
 public:
    explicit LedCommand(cxxopts::ParseResult &);
 private:
    void doCommand(DevicePtr devicePtr) override;
};

class DXRT_API ModelUploadCommand : public CLICommand
{
   public:
      explicit ModelUploadCommand(cxxopts::ParseResult &);
   private:
      void doCommand(DevicePtr devicePtr) override;
};

}  // namespace dxrt
