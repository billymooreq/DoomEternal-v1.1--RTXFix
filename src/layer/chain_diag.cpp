#include "layer/chain_diag.h"

#include <windows.h>

#include <string>

#include "common/log.h"
#include "common/util.h"

namespace doomfix {

void LogLayersBelow(const char* stage, const std::vector<const void*>& nextFunctions) {
  std::wstring chain;
  bool renderDocBelow = false;
  for (const void* fn : nextFunctions) {
    const std::wstring module = ModuleNameFromAddress(fn);
    renderDocBelow |= ContainsNoCase(module, L"renderdoc");
    if (!chain.empty()) chain += L" -> ";
    chain += module;
  }
  DF_LOG_INFO("%s: chain below doomfix: %s", stage, ToUtf8(chain).c_str());

  if (renderDocBelow) {
    DF_LOG_INFO("%s: RenderDoc is below doomfix: it will not see hidden extensions", stage);
  } else if (GetModuleHandleW(L"renderdoc.dll")) {
    DF_LOG_WARN("%s: renderdoc.dll is loaded but is NOT below doomfix (it is closer to the "
                "application and sees the original calls). Re-run scripts\\install.ps1 to set "
                "up layer ordering.",
                stage);
  }
}

}  // namespace doomfix
