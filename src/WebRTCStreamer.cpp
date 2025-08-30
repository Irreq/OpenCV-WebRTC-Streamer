#include "WebRTCStreamer.hpp"

GstFlowReturn WebRTCStreamer::on_new_sample(GstAppSink *sink,
                                            gpointer user_data) {
  auto *ctx = reinterpret_cast<Context *>(user_data);
  if (!ctx->track || !ctx->track->isOpen())
    return GST_FLOW_OK;

  GstSample *sample = gst_app_sink_try_pull_sample(sink, 0);
  if (!sample)
    return GST_FLOW_OK;

  GstBuffer *buffer = gst_sample_get_buffer(sample);
  if (buffer) {
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
      ctx->track->send(reinterpret_cast<const std::byte *>(map.data), map.size);
      gst_buffer_unmap(buffer, &map);
    }
  }
  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

void WebRTCStreamer::initWebRTC() {
  rtc::InitLogger(rtc::LogLevel::Info);
  pc = std::make_shared<rtc::PeerConnection>();

  pc->onStateChange([this](rtc::PeerConnection::State state) {
    this->logger->info("PC State: {}", "2389478237482374");
    std::cout << "PC State: " << state << std::endl;
  });

  pc->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) {
    // std::cout << "Gathering State: " << state << std::endl;
    this->logger->info("Gathering State");
    if (state == rtc::PeerConnection::GatheringState::Complete) {
      auto description = pc->localDescription();
      if (description) {
        nlohmann::json msg = {{"type", description->typeString()},
                              {"sdp", std::string(description.value())}};
        {
          std::lock_guard<std::mutex> lk(offer_mtx);
          offer_json = msg.dump();
        }
        offer_cv.notify_all();
        this->logger->info("Offer ready");
      }
    }
  });

  rtc::Description::Video media("video", rtc::Description::Direction::SendOnly);
  media.addH264Codec(96);
  media.addSSRC(42, "video-send");
  ctx.track = pc->addTrack(media);
  pc->setLocalDescription();
}

void WebRTCStreamer::initGStreamer() {
  gst_init(nullptr, nullptr);
  std::string pipeline_desc =
      "appsrc name=mysrc is-live=true format=time do-timestamp=true "
      "caps=video/x-raw,format=BGR,width=640,height=480,framerate=30/1 ! "
      "videoconvert ! "
      "x264enc tune=zerolatency bitrate=1000 key-int-max=30 ! "
      "video/x-h264,profile=constrained-baseline ! "
      "h264parse config-interval=-1 ! "
      "rtph264pay pt=96 mtu=1200 ssrc=42 ! "
      "appsink name=rtp_sink emit-signals=true sync=false max-buffers=8 "
      "drop=true";

  GError *err = nullptr;
  pipeline = gst_parse_launch(pipeline_desc.c_str(), &err);
  if (!pipeline) {
    logger->error("Failed to create pipeline {}",
                  (err ? err->message : "unknown"));
    if (err)
      g_error_free(err);
    return;
  }

  auto *appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "mysrc");
  auto *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "rtp_sink");
  ctx.appsrc = appsrc;

  g_object_set(G_OBJECT(appsink), "emit-signals", TRUE, nullptr);
  g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), &ctx);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

void WebRTCStreamer::initHTTPServer() {
  // Serve index.html
  svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
    std::ifstream ifs("static/index.html");
    if (!ifs) {
      res.status = 404;
      res.set_content("Not found", "text/plain");
      return;
    }
    std::string html((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
    res.set_content(html, "text/html");
  });

  // /offer
  svr.Get("/offer", [this](const httplib::Request &, httplib::Response &res) {
    std::unique_lock<std::mutex> lk(offer_mtx);
    if (offer_json.empty())
      offer_cv.wait_for(lk, std::chrono::seconds(5),
                        [this] { return !offer_json.empty(); });
    if (offer_json.empty()) {
      res.status = 503;
      res.set_content("{\"error\":\"offer-not-ready\"}", "application/json");
    } else {
      res.set_content(offer_json, "application/json");
    }
  });

  // /answer
  svr.Post("/answer",
           [this](const httplib::Request &req, httplib::Response &res) {
             try {
               auto j = nlohmann::json::parse(req.body);
               rtc::Description answer(j["sdp"].get<std::string>(),
                                       j["type"].get<std::string>());
               pc->setRemoteDescription(answer);
               res.set_content("{\"status\":\"ok\"}", "application/json");
               std::cout << "[server] Received answer." << std::endl;
             } catch (const std::exception &e) {
               res.status = 400;
               res.set_content(std::string("{\"error\":\"") + e.what() + "\"}",
                               "application/json");
             }
           });

  // /control
  svr.Post(
      "/control", [this](const httplib::Request &req, httplib::Response &res) {
        try {
          auto j = nlohmann::json::parse(req.body);
          std::string cmd = j.value("command", "");
          if (cmd == "start")
            ctx.running = true;
          else if (cmd == "pause")
            ctx.paused = true;
          else if (cmd == "stop")
            ctx.running = false;
          res.set_content("{\"status\":\"ok\"}", "application/json");
        } catch (...) {
          res.status = 400;
          res.set_content("{\"error\":\"invalid-json\"}", "application/json");
        }
      });

  // Run HTTP server in its own thread
  std::thread([this]() {
    this->logger->info("Server HTTP listening on http://{}:{}", this->host,
                       this->port);
    if (!svr.listen(this->host, this->port)) {
      this->logger->error("Server Failed to bind to port {}", this->port);
    }
  }).detach();
}

void WebRTCStreamer::captureLoop() {
  cv::VideoCapture cap(this->path);
  if (!cap.isOpened())
    return;
  cap.set(cv::CAP_PROP_FRAME_WIDTH, WIDTH);
  cap.set(cv::CAP_PROP_FRAME_HEIGHT, HEIGHT);
  cap.set(cv::CAP_PROP_FPS, FPS);

  cv::Mat frame;
  GstClockTime timestamp = 0;
  GstClockTime duration = gst_util_uint64_scale_int(1, GST_SECOND, FPS);

  while (ctx.running) {
    if (ctx.paused) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    cap >> frame;
    if (frame.empty())
      continue;

    size_t size = frame.total() * frame.elemSize();
    GstBuffer *buf = gst_buffer_new_allocate(nullptr, size, nullptr);
    GstMapInfo map;
    gst_buffer_map(buf, &map, GST_MAP_WRITE);
    memcpy(map.data, frame.data, size);
    gst_buffer_unmap(buf, &map);

    GST_BUFFER_PTS(buf) = timestamp;
    GST_BUFFER_DTS(buf) = timestamp;
    GST_BUFFER_DURATION(buf) = duration;
    timestamp += duration;

    gst_app_src_push_buffer(GST_APP_SRC(ctx.appsrc), buf);
  }

  gst_app_src_end_of_stream(GST_APP_SRC(ctx.appsrc));
}

void WebRTCStreamer::cleanup() {
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(ctx.appsrc);
  gst_object_unref(pipeline);
}

void WebRTCStreamer::run() {
  initWebRTC();
  initGStreamer();
  initHTTPServer();
  captureLoop();
  cleanup();
}
