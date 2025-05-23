// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "examples/shared/main.h"

#include <windows.h>
#include <filesystem>
#include <shlobj.h>

#include "include/cef_sandbox_win.h"
#include "include/cef_cookie.h"

#include "examples/shared/app_factory.h"
#include "examples/shared/client_manager.h"
#include "examples/shared/main_util.h"

// When generating projects with CMake the CEF_USE_SANDBOX value will be defined
// automatically if using the required compiler version. Pass -DUSE_SANDBOX=OFF
// to the CMake command-line to disable use of the sandbox.

namespace shared {

// Create cache directory for persistent cookie storage
std::filesystem::path GetCacheDirectory() {
#if defined(OS_WIN)
  // Get %LOCALAPPDATA% directory
  wchar_t* localAppData = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
    std::filesystem::path cache_dir = std::filesystem::path(localAppData) / L"MyCefApp" / L"Cache";
    CoTaskMemFree(localAppData);
    
    // Create directory if it doesn't exist
    std::error_code ec;
    std::filesystem::create_directories(cache_dir, ec);
    
    return cache_dir;
  }
  
  // Fallback to current directory if LOCALAPPDATA is not available
  return std::filesystem::current_path() / "MyCefApp" / "Cache";
#else
  // TODO: Implement for Linux and macOS
  // Linux: ~/.config/MyCefApp/Cache
  // macOS: ~/Library/Application Support/MyCefApp/Cache
  return std::filesystem::current_path() / "MyCefApp" / "Cache";
#endif
}

// Entry point function for all processes.
int APIENTRY wWinMain(HINSTANCE hInstance) {
  // Enable High-DPI support on Windows 7 or newer.
  CefEnableHighDPISupport();

  void* sandbox_info = nullptr;

#if defined(CEF_USE_SANDBOX)
  // Manage the life span of the sandbox information object. This is necessary
  // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
  CefScopedSandboxInfo scoped_sandbox;
  sandbox_info = scoped_sandbox.sandbox_info();
#endif

  // Provide CEF with command-line arguments.
  CefMainArgs main_args(hInstance);

  // Create a temporary CommandLine object.
  CefRefPtr<CefCommandLine> command_line = CreateCommandLine(main_args);

  // Create a CefApp of the correct process type.
  CefRefPtr<CefApp> app;
  switch (GetProcessType(command_line)) {
    case PROCESS_TYPE_BROWSER:
      app = CreateBrowserProcessApp();
      break;
    case PROCESS_TYPE_RENDERER:
      app = CreateRendererProcessApp();
      break;
    case PROCESS_TYPE_OTHER:
      app = CreateOtherProcessApp();
      break;
  }

  // CEF applications have multiple sub-processes (render, plugin, GPU, etc)
  // that share the same executable. This function checks the command-line and,
  // if this is a sub-process, executes the appropriate logic.
  int exit_code = CefExecuteProcess(main_args, app, sandbox_info);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return exit_code;
  }
  // Create the singleton manager instance.
  ClientManager manager;

  // Specify CEF global settings here.
  CefSettings settings;

#if !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = true;
#endif

  // Configure cookie persistence settings
  std::filesystem::path cache_dir = GetCacheDirectory();
  
  // 1️⃣ Location for all Chromium data (cookies, cache, local-storage)
  CefString(&settings.cache_path) = cache_dir.u8string();
  
  // 2️⃣ Persist session cookies (those without an expiry date)
  settings.persist_session_cookies = 1;   // default is 0
  
  // 3️⃣ Persist user-prefs (JSON file stored next to Cookies SQLite DB)
  settings.persist_user_preferences = 1;  // default is 0

  // Initialize CEF for the browser process. The first browser instance will be
  // created in CefBrowserProcessHandler::OnContextInitialized() after CEF has
  // been initialized.
  CefInitialize(main_args, settings, app, sandbox_info);
  // Run the CEF message loop. This will block until CefQuitMessageLoop() is
  // called.
  CefRunMessageLoop();

  // Flush cookie store to ensure all cookies are written to disk
  CefRefPtr<CefCookieManager> cookie_manager = CefCookieManager::GetGlobalManager(nullptr);
  if (cookie_manager) {
    cookie_manager->FlushStore(nullptr);
  }

  // Shut down CEF.
  CefShutdown();

  return 0;
}

}  // namespace shared
