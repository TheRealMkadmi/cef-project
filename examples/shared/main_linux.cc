// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "examples/shared/main.h"

#include <X11/Xlib.h>
#include <filesystem>
#include <cstdlib>

#include "include/base/cef_logging.h"
#include "include/cef_cookie.h"

#include "examples/shared/app_factory.h"
#include "examples/shared/client_manager.h"
#include "examples/shared/main_util.h"

namespace shared {

namespace {

int XErrorHandlerImpl(Display* display, XErrorEvent* event) {
  LOG(WARNING) << "X error received: "
               << "type " << event->type << ", "
               << "serial " << event->serial << ", "
               << "error_code " << static_cast<int>(event->error_code) << ", "
               << "request_code " << static_cast<int>(event->request_code)
               << ", "
               << "minor_code " << static_cast<int>(event->minor_code);
  return 0;
}

int XIOErrorHandlerImpl(Display* display) {
  return 0;
}

}  // namespace

// Entry point function for all processes.
int main(int argc, char* argv[]) {
  // Provide CEF with command-line arguments.
  CefMainArgs main_args(argc, argv);

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
  int exit_code = CefExecuteProcess(main_args, app, nullptr);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return exit_code;
  }

  // Install xlib error handlers so that the application won't be terminated
  // on non-fatal errors.
  XSetErrorHandler(XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);

  // Create the singleton manager instance.
  ClientManager manager;
  // Specify CEF global settings here.
  CefSettings settings;

  // TODO: Implement cookie persistence for Linux
  // Set up cache directory for cookie persistence
  std::filesystem::path cache_dir;
  const char* home = getenv("HOME");
  if (home) {
    cache_dir = std::filesystem::path(home) / ".local" / "share" / "MyCefApp" / "Cache";
  } else {
    cache_dir = "/tmp/MyCefApp/Cache";
  }
  
  // Create cache directory if it doesn't exist
  try {
    std::filesystem::create_directories(cache_dir);
  } catch (const std::exception& e) {
    // If creation fails, fall back to temp directory
    cache_dir = "/tmp/MyCefApp/Cache";
    std::filesystem::create_directories(cache_dir);
  }
  
  // Configure CEF settings for cookie persistence
  CefString(&settings.cache_path).FromString(cache_dir.string());
  settings.persist_session_cookies = 1;
  settings.persist_user_preferences = 1;

  // Initialize CEF for the browser process. The first browser instance will be
  // created in CefBrowserProcessHandler::OnContextInitialized() after CEF has
  // been initialized.
  CefInitialize(main_args, settings, app, nullptr);
  // Run the CEF message loop. This will block until CefQuitMessageLoop() is
  // called.
  CefRunMessageLoop();

  // TODO: Flush cookie store to ensure all cookies are written to disk
  CefRefPtr<CefCookieManager> cookie_manager = CefCookieManager::GetGlobalManager(nullptr);
  if (cookie_manager) {
    cookie_manager->FlushStore(nullptr);
  }

  // Shut down CEF.
  CefShutdown();

  return 0;
}

}  // namespace shared
