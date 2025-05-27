// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_EXAMPLES_SHARED_CONNECTION_MONITOR_H_
#define CEF_EXAMPLES_SHARED_CONNECTION_MONITOR_H_

#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <functional>

#include "include/wrapper/cef_helpers.h"

namespace shared {

class ConnectionMonitor {
 public:
  struct Status {
    bool is_online;
    int latency_ms;
    std::chrono::system_clock::time_point last_check;
  };

  ConnectionMonitor();
  ~ConnectionMonitor();

  // Returns the singleton instance of this object.
  static ConnectionMonitor* GetInstance();

  // Start monitoring the specified URL. Check interval in seconds.
  void StartMonitoring(const std::string& url, int check_interval_seconds = 30);
  
  // Stop monitoring.
  void StopMonitoring();

  // Get current connection status.
  Status GetStatus() const;

  // Get a formatted status string for display in title bar.
  std::string GetStatusString() const;

  // Set a callback to be notified when status changes.
  void SetStatusChangeCallback(std::function<void(const Status&)> callback);

 private:
  void MonitoringLoop();
  bool CheckConnectivity(int& latency_ms);

  std::string target_url_;
  int check_interval_seconds_;
  std::atomic<bool> is_monitoring_;
  std::atomic<bool> should_stop_;
  std::thread monitoring_thread_;
  Status current_status_;
  std::function<void(const Status&)> status_callback_;

  mutable base::Lock status_lock_;
};

}  // namespace shared

#endif  // CEF_EXAMPLES_SHARED_CONNECTION_MONITOR_H_
