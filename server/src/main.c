#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "types.h"
#include "utils.h"

#define DEFAULT_TCP_PORT 5556
#define DEFAULT_UDP_PORT 5555

static ServerState server_state;
static volatile int shutdown_requested = 0;

void signal_handler(int sig) {
  (void)sig;
  if (shutdown_requested) {
    printf("\nForce shutdown...\n");
    exit(1);
  }
  shutdown_requested = 1;
  printf("\nShutdown signal received...\n");
  stop_server(&server_state);
}

void print_usage(const char* program) {
  printf("Usage: %s [options]\n", program);
  printf("Options: --tcp <port>, -udp <port>, --help\n");
}

int main(int argc, char* argv[]) {
  int tcp_port = DEFAULT_TCP_PORT;
  int udp_port = DEFAULT_UDP_PORT;

  for (int i = 1; i < argc; i++)
    if (strcmp(argv[i], "--tcp") == 0) {
      if (i + 1 < argc) tcp_port = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--udp") == 0) {
      if (i + 1 < argc) udp_port = atoi(argv[++i]);
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    }

  printf("QuizNet\n\n");

  init_random();

#ifdef _WIN32
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
#else
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
#endif

  // Initialize server
  if (init_server(&server_state, tcp_port, udp_port) < 0) {
    printf("Failed to initialize server\n");
    return 1;
  }

  run_server(&server_state);
  cleanup_server(&server_state);

  printf("Server stopped.\n");
  return 0;
}
