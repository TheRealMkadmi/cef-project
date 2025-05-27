// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "examples/shared/client_util.h"

#include <sstream>
#include <string>
#include <map>

#include "examples/shared/client_manager.h"
#include "examples/shared/connection_monitor.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/wrapper/cef_helpers.h"

#if defined(OS_WIN) || defined(OS_LINUX)
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#endif

namespace shared {

namespace {

// Cache to store the original page titles (without connection status)
std::map<int, std::string> g_browser_titles;

}

void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  // Store the original title (without connection status) for later use
  std::string original_title = std::string(title);
  g_browser_titles[browser->GetIdentifier()] = original_title;

  // Get connection status and append to title
  std::string enhanced_title = original_title;
  ConnectionMonitor* monitor = ConnectionMonitor::GetInstance();
  if (monitor) {
    enhanced_title += monitor->GetStatusString();
  }

  CefString enhanced_title_cef = enhanced_title;

#if defined(OS_WIN) || defined(OS_LINUX)
  // The Views framework is currently only supported on Windows and Linux.
  CefRefPtr<CefBrowserView> browser_view =
      CefBrowserView::GetForBrowser(browser);
  if (browser_view) {
    // Set the title of the window using the Views framework.
    CefRefPtr<CefWindow> window = browser_view->GetWindow();
    if (window)
      window->SetTitle(enhanced_title_cef);
  } else
#endif
  {
    // Set the title of the window using platform APIs.
    PlatformTitleChange(browser, enhanced_title_cef);
  }
}

void OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Add to the list of existing browsers.
  ClientManager::GetInstance()->OnAfterCreated(browser);
}

bool DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Closing the main window requires special handling. See the DoClose()
  // documentation in the CEF header for a detailed destription of this
  // process.
  ClientManager::GetInstance()->DoClose(browser);

  // Allow the close. For windowed browsers this will result in the OS close
  // event being sent.
  return false;
}

void OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Remove the cached title for this browser
  g_browser_titles.erase(browser->GetIdentifier());

  // Remove from the list of existing browsers.
  ClientManager::GetInstance()->OnBeforeClose(browser);
}

std::string DumpRequestContents(CefRefPtr<CefRequest> request) {
  std::stringstream ss;

  ss << "URL: " << std::string(request->GetURL());
  ss << "\nMethod: " << std::string(request->GetMethod());

  CefRequest::HeaderMap headerMap;
  request->GetHeaderMap(headerMap);
  if (headerMap.size() > 0) {
    ss << "\nHeaders:";
    CefRequest::HeaderMap::const_iterator it = headerMap.begin();
    for (; it != headerMap.end(); ++it) {
      ss << "\n\t" << std::string((*it).first) << ": "
         << std::string((*it).second);
    }
  }

  CefRefPtr<CefPostData> postData = request->GetPostData();
  if (postData.get()) {
    CefPostData::ElementVector elements;
    postData->GetElements(elements);
    if (elements.size() > 0) {
      ss << "\nPost Data:";
      CefRefPtr<CefPostDataElement> element;
      CefPostData::ElementVector::const_iterator it = elements.begin();
      for (; it != elements.end(); ++it) {
        element = (*it);
        if (element->GetType() == PDE_TYPE_BYTES) {
          // the element is composed of bytes
          ss << "\n\tBytes: ";
          if (element->GetBytesCount() == 0) {
            ss << "(empty)";
          } else {
            // retrieve the data.
            size_t size = element->GetBytesCount();
            char* bytes = new char[size];
            element->GetBytes(size, bytes);
            ss << std::string(bytes, size);
            delete[] bytes;
          }
        } else if (element->GetType() == PDE_TYPE_FILE) {
          ss << "\n\tFile: " << std::string(element->GetFile());
        }
      }
    }
  }

  return ss.str();
}

void UpdateAllBrowserTitles() {
  // TODO: Ensure this is called on UI thread
  // CEF_REQUIRE_UI_THREAD();
  
  // Get all browsers from ClientManager and update their titles
  ClientManager* manager = ClientManager::GetInstance();
  if (manager) {
    manager->ForEachBrowser([](CefRefPtr<CefBrowser> browser) {
      // Get the cached original title for this browser
      int browser_id = browser->GetIdentifier();
      auto it = g_browser_titles.find(browser_id);
      
      std::string original_title;
      if (it != g_browser_titles.end()) {
        original_title = it->second;
      } else {
        // Fallback if no cached title exists
        original_title = "CEF Application";
      }
      
      // Update title with current connection status
      std::string enhanced_title = original_title;
      ConnectionMonitor* monitor = ConnectionMonitor::GetInstance();
      if (monitor) {
        enhanced_title += monitor->GetStatusString();
      }
      
      CefString enhanced_title_cef = enhanced_title;

#if defined(OS_WIN) || defined(OS_LINUX)
      // The Views framework is currently only supported on Windows and Linux.
      CefRefPtr<CefBrowserView> browser_view =
          CefBrowserView::GetForBrowser(browser);
      if (browser_view) {
        // Set the title of the window using the Views framework.
        CefRefPtr<CefWindow> window = browser_view->GetWindow();
        if (window)
          window->SetTitle(enhanced_title_cef);
      } else
#endif
      {
        // Set the title of the window using platform APIs.
        PlatformTitleChange(browser, enhanced_title_cef);
      }
    });
  }
}

}  // namespace shared
