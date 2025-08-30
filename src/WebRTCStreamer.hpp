#pragma once
#include "httplib.h"
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <rtc/rtc.hpp>
#include <spdlog/spdlog.h>

struct Context {
  GstElement *appsrc = nullptr;
  std::shared_ptr<rtc::Track> track;
  std::atomic<bool> running{true};
  std::atomic<bool> paused{false};
};

class WebRTCStreamer {
public:
  WebRTCStreamer(const std::string &path,
                 std::shared_ptr<spdlog::logger> logger,
                 const std::string &host, const int port,
                 int width = 640, int height = 480, int fps = 30)
      : path(path), logger(std::move(logger)), host(host), port(port),
        WIDTH(width), HEIGHT(height), FPS(fps) {}

  void run();

private:
  const std::string &path;
  const std::string &host;
  const int port;
  int WIDTH, HEIGHT, FPS;
  Context ctx;
  std::shared_ptr<rtc::PeerConnection> pc;
  std::mutex offer_mtx;
  std::condition_variable offer_cv;
  std::string offer_json;
  GstElement *pipeline = nullptr;
  httplib::Server svr;
  std::shared_ptr<spdlog::logger> logger;

  static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer user_data);
  void initWebRTC();
  void initGStreamer();
  void initHTTPServer();
  void captureLoop();
  void cleanup();
};
