#include <CmdParser/cmdparser.hpp>
#include <cstdlib>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "WebRTCStreamer.hpp"

void configure_parser(cli::Parser &parser) {
//   parser.set_optional<bool>("d", "debug", false, "Debug mode");
  parser.set_optional<bool>("r", "replay", false, "Replay mode");
  parser.set_optional<std::string>("i", "host", "127.0.0.1", "Host address");
  parser.set_optional<int>("p", "port", 9000, "Port");
  parser.set_required<std::string>("d", "device", "Device path");
}

int main(int argc, char **argv) {

  cli::Parser parser(argc, argv);
  configure_parser(parser);
  parser.run_and_exit_if_error();

  auto path = parser.get<std::string>("d");
  // Get or create logger
  auto logger = spdlog::get("console");
  if (!logger) {
    logger = spdlog::stdout_color_mt("console");
  }

  auto host = parser.get<std::string>("i");
  int port = parser.get<int>("p");

  WebRTCStreamer streamer(path, logger, host, port);
  streamer.run();
  return EXIT_SUCCESS;
}