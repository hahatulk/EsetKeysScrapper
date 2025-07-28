#pragma once

#include "Helpers/Paths.h"

class Proxy {
public:
  string protocol;
  string ip;
  int port;
  string username;
  string password;

  Proxy(string proxy);
  ~Proxy();

  void changeProxy(string proxy);
  bool isWorking();

  string toString();

private:
  string buffer;

  static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                              void *userp);
};
