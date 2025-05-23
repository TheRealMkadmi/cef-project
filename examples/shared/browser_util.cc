// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "examples/shared/browser_util.h"

#include <filesystem>
#include "include/cef_command_line.h"
#include "include/cef_request_context.h"
#include "include/cef_request_context_handler.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shlobj.h>
#endif

namespace shared {

namespace {

// When using the Views framework this object provides the delegate
// implementation for the CefWindow that hosts the Views-based browser.
class WindowDelegate : public CefWindowDelegate {
 public:
  explicit WindowDelegate(CefRefPtr<CefBrowserView> browser_view)
      : browser_view_(browser_view) {}

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    // Add the browser view and show the window.
    window->AddChildView(browser_view_);
    window->Show();

    // Give keyboard focus to the browser view.
    browser_view_->RequestFocus();
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    browser_view_ = nullptr;
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    // Allow the window to close if the browser says it's OK.
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser)
      return browser->GetHost()->TryCloseBrowser();
    return true;
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    // Preferred window size.
    return CefSize(800, 600);
  }

  CefSize GetMinimumSize(CefRefPtr<CefView> view) override {
    // Minimum window size.
    return CefSize(200, 100);
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;

  IMPLEMENT_REFCOUNTING(WindowDelegate);
  DISALLOW_COPY_AND_ASSIGN(WindowDelegate);
};

#if defined(OS_WIN)
// Get cache directory for cookie persistence (same as main_win.cc)
std::filesystem::path GetCacheDirectory() {
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
}

// Get or create a shared request context for cookie persistence
CefRefPtr<CefRequestContext> GetSharedRequestContext() {
  static CefRefPtr<CefRequestContext> shared_context;
  
  if (!shared_context) {
    // Create a context with the same persistence settings as CefSettings
    CefRequestContextSettings context_settings;
    std::filesystem::path cache_dir = GetCacheDirectory();
      CefString(&context_settings.cache_path) = cache_dir.u8string();
    context_settings.persist_session_cookies = 1;
    context_settings.persist_user_preferences = 1;
    
    shared_context = CefRequestContext::CreateContext(context_settings, CefRefPtr<CefRequestContextHandler>());
  }
  
  return shared_context;
}
#else
// For non-Windows platforms, use the global context for now
CefRefPtr<CefRequestContext> GetSharedRequestContext() {
  // TODO: Implement for Linux and macOS with proper cache directory
  // For now, use global context which should inherit settings from CefSettings
  return CefRequestContext::GetGlobalContext();
}
#endif

}  // namespace

void CreateBrowser(CefRefPtr<CefClient> client,
                   const CefString& startup_url,
                   const CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();

  // Get the shared request context for cookie persistence
  // This ensures all browsers share the same cookie store
  CefRefPtr<CefRequestContext> request_context = GetSharedRequestContext();

#if defined(OS_WIN) || defined(OS_LINUX)
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  // Create the browser using the Views framework if "--use-views" is specified
  // via the command-line. Otherwise, create the browser using the native
  // platform framework. The Views framework is currently only supported on
  // Windows and Linux.
  const bool use_views = command_line->HasSwitch("use-views");
#else
  const bool use_views = false;
#endif

  if (use_views) {
    // Create the BrowserView with shared request context for cookie persistence.
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        client, startup_url, settings, nullptr, request_context, nullptr);

    // Create the Window. It will show itself after creation.
    CefWindow::CreateTopLevelWindow(new WindowDelegate(browser_view));
  } else {
    // Information used when creating the native window.
    CefWindowInfo window_info;

#if defined(OS_WIN)
    // On Windows we need to specify certain flags that will be passed to
    // CreateWindowEx().
    window_info.SetAsPopup(nullptr, "examples");
#endif    // Create the browser window with shared request context for cookie persistence.
    CefBrowserHost::CreateBrowser(window_info, client, startup_url, settings,
                                  nullptr, request_context);
  }
}

}  // namespace shared
