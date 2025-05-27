// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "examples/minimal/client_minimal.h"
#include "examples/shared/app_factory.h"
#include "examples/shared/browser_util.h"
#include "examples/shared/client_util.h"
#include "examples/shared/connection_monitor.h"

namespace minimal {

namespace {

const char kStartupURL[] = "https://qr-menu.tn/";

}  // namespace

// Minimal implementation of CefApp for the browser process.
class BrowserApp : public CefApp, public CefBrowserProcessHandler {
 public:
  BrowserApp() {}

  // CefApp methods:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  void OnBeforeCommandLineProcessing(
      const CefString& process_type,
      CefRefPtr<CefCommandLine> command_line) override {
    // Command-line flags can be modified in this callback.
    // |process_type| is empty for the browser process.
    if (process_type.empty()) {
#if defined(OS_MACOSX)
      // Disable the macOS keychain prompt. Cookies will not be encrypted.
      command_line->AppendSwitch("use-mock-keychain");
#endif
    }
  }

  // CefBrowserProcessHandler methods:
  void OnContextInitialized() override {
    // Initialize connection monitoring
    shared::ConnectionMonitor* monitor =
        shared::ConnectionMonitor::GetInstance();
    // Set the callback for UI updates
    monitor->SetStatusChangeCallback(
        [](const shared::ConnectionMonitor::Status& status) {
          // This callback is now guaranteed to be called on the UI thread by
          // ConnectionMonitor
          shared::UpdateAllBrowserTitles();
        });
    monitor->StartMonitoring("https://qr-menu.tn/", 5);  // Check every 5 seconds

    // Initial title update might still be useful if StartMonitoring is async
    // or if the first check takes time.
    // shared::UpdateAllBrowserTitles(); // Consider if this is needed or if the first callback suffices

    // Create the browser window.
    shared::CreateBrowser(new Client(), kStartupURL, CefBrowserSettings());
  }

 private:
  IMPLEMENT_REFCOUNTING(BrowserApp);
  DISALLOW_COPY_AND_ASSIGN(BrowserApp);
};

}  // namespace minimal

namespace shared {

CefRefPtr<CefApp> CreateBrowserProcessApp() {
  return new minimal::BrowserApp();
}

}  // namespace shared
