#include "../http.hpp"

#include <cerrno>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace piper_server {

void closeSocket(SocketHandle socketHandle) {
#ifdef _WIN32
  closesocket(socketHandle);
#else
  close(socketHandle);
#endif
}

bool clientDisconnected(SocketHandle socketHandle) {
  fd_set readSet;
  FD_ZERO(&readSet);
  FD_SET(socketHandle, &readSet);

  timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

#ifdef _WIN32
  int ready = select(0, &readSet, nullptr, nullptr, &timeout);
#else
  int ready = select(socketHandle + 1, &readSet, nullptr, nullptr, &timeout);
#endif

  if (ready <= 0 || !FD_ISSET(socketHandle, &readSet)) {
    return false;
  }

  char probe = 0;
#ifdef _WIN32
  int received = recv(socketHandle, &probe, 1, MSG_PEEK);
  if (received == 0) {
    return true;
  }
  if (received < 0) {
    int err = WSAGetLastError();
    return (err != WSAEWOULDBLOCK) && (err != WSAEINTR);
  }
#else
  ssize_t received = recv(socketHandle, &probe, 1, MSG_PEEK);
  if (received == 0) {
    return true;
  }
  if (received < 0) {
    return (errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINTR);
  }
#endif

  // The peer may have sent extra bytes on a keep-alive request. This server
  // closes every response, so extra data is ignored but does not mean closed.
  return false;
}

bool recvAppend(SocketHandle socketHandle, std::string &buffer) {
  char chunk[8192];
#ifdef _WIN32
  int received = recv(socketHandle, chunk, static_cast<int>(sizeof(chunk)), 0);
#else
  ssize_t received = recv(socketHandle, chunk, sizeof(chunk), 0);
#endif
  if (received <= 0) {
    return false;
  }

  buffer.append(chunk, static_cast<std::size_t>(received));
  return true;
}

void sendRaw(SocketHandle socketHandle, const std::string &data) {
  const char *ptr = data.data();
  std::size_t remaining = data.size();
  while (remaining > 0) {
#ifdef _WIN32
    int sent = send(socketHandle, ptr, static_cast<int>(remaining), 0);
#else
    ssize_t sent = send(socketHandle, ptr, remaining, 0);
#endif
    if (sent <= 0) {
      return;
    }
    ptr += sent;
    remaining -= static_cast<std::size_t>(sent);
  }
}

} // namespace piper_server
