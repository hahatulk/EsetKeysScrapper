#include "Proxy.h"

Proxy::Proxy(string proxy) { this->changeProxy(proxy); }

Proxy::~Proxy() {}

void Proxy::changeProxy(string proxy) {
  // Initialize members to avoid undefined state
  this->protocol.clear();
  this->ip.clear();
  this->port = 0;
  this->username.clear();
  this->password.clear();
  this->buffer.clear();

  // Trim whitespace from proxy string
  proxy.erase(0, proxy.find_first_not_of(" \t\r\n"));
  proxy.erase(proxy.find_last_not_of(" \t\r\n") + 1);

  if (proxy.empty()) {
    cout << RED << "🛑 Error: Empty proxy string" << RESET << endl;
    return;
  }

  size_t protocol_end = proxy.find("://");
  if (protocol_end == string::npos) {
    cout << RED << "🛑 Error: Missing protocol in proxy: " << YELLOW << proxy
         << RESET << endl;
    return;
  }
  this->protocol = proxy.substr(0, protocol_end);
  if (this->protocol.empty()) {
    cout << RED << "🛑 Error: Empty protocol in proxy: " << YELLOW << proxy
         << RESET << endl;
    return;
  }

  size_t ip_start = protocol_end + 3;
  size_t at_pos = proxy.find('@', ip_start);
  if (at_pos != string::npos) {
    size_t cred_start = ip_start;
    size_t cred_end = proxy.find(':', cred_start);
    if (cred_end == string::npos || cred_end >= at_pos) {
      cout << RED << "🛑 Error: Invalid credentials format in proxy: " << YELLOW << proxy
           << RESET << endl;
      return;
    }
    this->username = proxy.substr(cred_start, cred_end - cred_start);
    this->password = proxy.substr(cred_end + 1, at_pos - cred_end - 1);
    ip_start = at_pos + 1;
  }

  size_t ip_end = proxy.find(':', ip_start);
  if (ip_end == string::npos) {
    cout << RED << "🛑 Error: Missing port in proxy: " << YELLOW << proxy
         << RESET << endl;
    return;
  }
  this->ip = proxy.substr(ip_start, ip_end - ip_start);
  if (this->ip.empty()) {
    cout << RED << "🛑 Error: Empty IP/hostname in proxy: " << YELLOW << proxy
         << RESET << endl;
    return;
  }

  size_t port_start = ip_end + 1;
  if (port_start >= proxy.length()) {
    cout << RED << "🛑 Error: Missing port number in proxy: " << YELLOW << proxy
         << RESET << endl;
    return;
  }
  try {
    this->port = stoi(proxy.substr(port_start));
    if (this->port <= 0 || this->port > 65535) {
      cout << RED << "🛑 Error: Invalid port number " << this->port
           << " in proxy: " << YELLOW << proxy << RESET << endl;
      this->port = 0;
    }
  } catch (const std::exception &e) {
    cout << RED << "🛑 Error: Invalid port format in proxy: " << YELLOW << proxy
         << RESET << endl;
    this->port = 0;
  }
}

bool Proxy::isWorking() {
  if (this->ip.empty() || this->port == 0) {
    cout << RED << "🛑 Error: Invalid proxy configuration: " << this->toString()
         << RESET << endl;
    return false;
  }

  CURL *ch = curl_easy_init();
  if (!ch) {
    cout << RED << "🛑 Error: Failed to initialize CURL for proxy "
         << this->toString() << RESET << endl;
    return false;
  }

  curl_easy_setopt(ch, CURLOPT_URL, "https://httpbin.org/get");
  curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, true);
  curl_easy_setopt(ch, CURLOPT_TIMEOUT, 15);
  curl_easy_setopt(ch, CURLOPT_USERAGENT,
                   "Mozilla/5.0 (compatible; MSIE 8.0; Windows NT 5.1; "
                   "InfoPath.2; SLCC1; .NET CLR 3.0.4506.2152; .NET CLR "
                   "3.5.30729; .NET CLR 2.0.50727)2011-09-08 13:55:49");

  // Set proxy type based on protocol
  if (this->protocol == "http" || this->protocol == "https") {
    curl_easy_setopt(ch, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
  } else if (this->protocol == "socks5") {
    curl_easy_setopt(ch, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
  } else if (this->protocol == "socks4") {
    curl_easy_setopt(ch, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
  } else if (this->protocol == "socks4a") {
    curl_easy_setopt(ch, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4A);
  } else if (this->protocol == "socks5h") {
    curl_easy_setopt(ch, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5_HOSTNAME);
  } else {
    // Fallback for unknown protocols, let libcurl handle it
    curl_easy_setopt(ch, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
    cout << YELLOW << "⚠ Warning: Unknown protocol '" << this->protocol
         << "', defaulting to HTTP proxy for " << this->toString() << RESET << endl;
  }

  curl_easy_setopt(ch, CURLOPT_PROXY, ip.c_str());
  curl_easy_setopt(ch, CURLOPT_PROXYPORT, port);

  if (!this->username.empty() && !this->password.empty()) {
    curl_easy_setopt(ch, CURLOPT_PROXYUSERNAME, this->username.c_str());
    curl_easy_setopt(ch, CURLOPT_PROXYPASSWORD, this->password.c_str());
    curl_easy_setopt(ch, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
  }

  this->buffer.clear();
  curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(ch, CURLOPT_WRITEDATA, &buffer);

  CURLcode res = curl_easy_perform(ch);
  if (res != CURLE_OK) {
    cout << RED << "🛑 CURL Error: " << curl_easy_strerror(res)
         << " for proxy " << this->toString() << RESET << endl;
    curl_easy_cleanup(ch);
    return false;
  }

  long response_code;
  curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &response_code);
  curl_easy_cleanup(ch);

  if (response_code == 200 && buffer.size() > 0) {
    // Verify the response is valid JSON to catch downstream issues
    try {
      nlohmann::json::parse(buffer);
      cout << "✅ Proxy working: " << this->toString() << RESET << endl;
      return true;
    } catch (const nlohmann::json::exception &e) {
      cout << RED << "🛑 Proxy failed: Invalid JSON response: " << e.what()
           << ", Buffer: '" << (buffer.size() > 100 ? buffer.substr(0, 100) + "..." : buffer)
           << "' for proxy " << this->toString() << RESET << endl;
      return false;
    }
  } else {
    cout << RED << "🛑 Proxy failed: HTTP " << response_code
         << ", Buffer size: " << buffer.size() << ", Response: '"
         << (buffer.size() > 100 ? buffer.substr(0, 100) + "..." : buffer)
         << "' for proxy " << this->toString() << RESET << endl;
    return false;
  }
}

string Proxy::toString() {
  if (this->ip.empty() || this->port == 0) {
    return "";
  }
  string result;
  if (!this->username.empty() && !this->password.empty()) {
    result = this->protocol + "://" + this->username + ":" + this->password + "@" +
             this->ip + ":" + to_string(this->port);
  } else {
    result = this->protocol + "://" + this->ip + ":" + to_string(this->port);
  }
  return result;
}

size_t Proxy::WriteCallback(void *contents, size_t size, size_t nmemb,
                            void *userp) {
  ((string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}
