// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "examples/shared/connection_monitor.h"

#include <sstream>
#include <iomanip>
#include <chrono>

#if defined(OS_WIN)
#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#elif defined(OS_LINUX)
#include <curl/curl.h>
#elif defined(OS_MACOSX)
#include <curl/curl.h>
#endif

#include "include/base/cef_callback.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_closure_task.h" // For CreateCefClosureTask

namespace shared {

namespace {

ConnectionMonitor* g_monitor = nullptr;

#if defined(OS_LINUX) || defined(OS_MACOSX)
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
  return size * nmemb;
}
#endif

}  // namespace

ConnectionMonitor::ConnectionMonitor()
    : is_monitoring_(false),
      should_stop_(false),
      check_interval_seconds_(10) {
#if defined(OS_LINUX) || defined(OS_MACOSX)
  curl_global_init(CURL_GLOBAL_ALL);
#endif
}

ConnectionMonitor::~ConnectionMonitor() {
  StopMonitoring();
  g_monitor = nullptr;

#if defined(OS_LINUX) || defined(OS_MACOSX)
  curl_global_cleanup();
#endif
}

// static
ConnectionMonitor* ConnectionMonitor::GetInstance() {
  if (!g_monitor) {
    g_monitor = new ConnectionMonitor();
  }
  return g_monitor;
}

void ConnectionMonitor::StartMonitoring(const std::string& url, int check_interval_seconds) {

  if (is_monitoring_) {
    StopMonitoring();
  }

  target_url_ = url;
  check_interval_seconds_ = check_interval_seconds > 0 ? check_interval_seconds : 1;
  should_stop_ = false;
  is_monitoring_ = true;

  if (monitoring_thread_.joinable()) {
      monitoring_thread_.join();
  }
  monitoring_thread_ = std::thread(&ConnectionMonitor::MonitoringLoop, this);
}

void ConnectionMonitor::StopMonitoring() {

  if (is_monitoring_) {
    should_stop_ = true;  
    is_monitoring_ = false;

    if (monitoring_thread_.joinable()) {
      monitoring_thread_.join();  
    }
  }
}

ConnectionMonitor::Status ConnectionMonitor::GetStatus() const {
  base::AutoLock lock_scope(status_lock_);
  return current_status_;
}

std::string ConnectionMonitor::GetStatusString() const {
  Status current_s = GetStatus();  
  std::stringstream ss;
  if (current_s.is_online) {
    ss << " [ONLINE " << current_s.latency_ms << "ms]";  
  } else {
    ss << " [OFFLINE]";  
  }
  return ss.str();
}

void ConnectionMonitor::SetStatusChangeCallback(std::function<void(const Status&)> callback) {
  status_callback_ = callback;
}

void ConnectionMonitor::MonitoringLoop() {
  while (!should_stop_) {
    int latency_ms = -1;
    bool is_online_now = CheckConnectivity(latency_ms);

    bool status_actually_changed = false;
    {
      base::AutoLock lock_scope(status_lock_);
      if (current_status_.is_online != is_online_now || current_status_.latency_ms != latency_ms) {
        status_actually_changed = true;
      }
      current_status_.is_online = is_online_now;
      current_status_.latency_ms = latency_ms;
      current_status_.last_check = std::chrono::system_clock::now();
    }

    if (status_callback_) {
        CefPostTask(TID_UI, base::BindOnce([](std::function<void(const Status&)> cb, Status status_to_send){
            if (cb) {
                cb(status_to_send);
            }
        }, status_callback_, GetStatus())); // GetStatus() is thread-safe
    }

    for (int i = 0; i < check_interval_seconds_ * 10 && !should_stop_; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

bool ConnectionMonitor::CheckConnectivity(int& latency_ms) {
  auto start_time = std::chrono::high_resolution_clock::now();
  bool success = false;

#if defined(OS_WIN)
  HINTERNET hSession = nullptr;
  HINTERNET hConnect = nullptr;
  HINTERNET hRequest = nullptr;

  try {
    hSession = WinHttpOpen(L"CEF Connection Monitor/1.0",
                          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);    if (hSession) {
      WinHttpSetTimeouts(hSession, 5000, 5000, 10000, 10000);
      std::wstring w_target_url(target_url_.begin(), target_url_.end());
      std::wstring w_target_url(target_url_.begin(), target_url_.end());
      URL_COMPONENTS urlComp;
      wchar_t szHostName[256];
      wchar_t szUrlPath[1024];

      ZeroMemory(&urlComp, sizeof(urlComp));
      urlComp.dwStructSize = sizeof(urlComp);
      urlComp.lpszHostName = szHostName;
      urlComp.dwHostNameLength = sizeof(szHostName) / sizeof(szHostName[0]);
      urlComp.lpszUrlPath = szUrlPath;
      urlComp.dwUrlPathLength = sizeof(szUrlPath) / sizeof(szUrlPath[0]);
      urlComp.nScheme = 0;

      if (WinHttpCrackUrl(w_target_url.c_str(), static_cast<DWORD>(w_target_url.length()), 0, &urlComp)) {
        hConnect = WinHttpConnect(hSession, szHostName, urlComp.nPort, 0);

        if (hConnect) {
          hRequest = WinHttpOpenRequest(hConnect, L"HEAD", szUrlPath,
                                       nullptr, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       (urlComp.nPort == 443 ? WINHTTP_FLAG_SECURE : 0));
          if (hRequest) {
            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
              if (WinHttpReceiveResponse(hRequest, nullptr)) {
                DWORD dwStatusCode = 0;
                DWORD dwSize = sizeof(dwStatusCode);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
                if (dwStatusCode >= 200 && dwStatusCode < 400) {
                  success = true;
                }
              }
            }
          }
        }
      }
    }
  } catch (...) {
    success = false;
  }

  if (hRequest) WinHttpCloseHandle(hRequest);
  if (hConnect) WinHttpCloseHandle(hConnect);
  if (hSession) WinHttpCloseHandle(hSession);

#elif defined(OS_LINUX) || defined(OS_MACOSX)
  CURL* curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, target_url_.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
      long response_code;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
      if (response_code >= 200 && response_code < 400) {
        success = true;
      }
    }
    curl_easy_cleanup(curl);
  }
#endif

  auto end_time = std::chrono::high_resolution_clock::now();
  latency_ms = success ? std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() : -1;

  return success;
}

}  // namespace shared
