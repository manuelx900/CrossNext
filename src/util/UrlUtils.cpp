#include "UrlUtils.h"

#include <cstdio>
#include <string_view>

namespace UrlUtils {
namespace {
bool isHexDigit(const char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }

bool shouldEncode(const unsigned char c) {
  if (c <= 0x20 || c >= 0x7f) return true;
  switch (c) {
    case '"':
    case '<':
    case '>':
    case '\\':
    case '^':
    case '`':
    case '{':
    case '|':
    case '}':
      return true;
    default:
      return false;
  }
}
}  // namespace

std::string ensureProtocol(const std::string& url) {
  if (url.find("://") == std::string::npos) {
    return "http://" + url;
  }
  return url;
}

std::string extractHost(const std::string& url) {
  const size_t protocolEnd = url.find("://");
  if (protocolEnd == std::string::npos) {
    // No protocol, find first slash
    const size_t firstSlash = url.find('/');
    return firstSlash == std::string::npos ? url : url.substr(0, firstSlash);
  }
  // Find the first slash after the protocol
  const size_t hostStart = protocolEnd + 3;
  const size_t pathStart = url.find('/', hostStart);
  return pathStart == std::string::npos ? url : url.substr(0, pathStart);
}

std::string encodeUnsafeUrlChars(const std::string& url) {
  std::string out;
  out.reserve(url.size());
  for (size_t i = 0; i < url.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(url[i]);
    if (c == '%' && i + 2 < url.size() && isHexDigit(url[i + 1]) && isHexDigit(url[i + 2])) {
      out += url[i];
      out += url[i + 1];
      out += url[i + 2];
      i += 2;
    } else if (c == '%' || shouldEncode(c)) {
      char encoded[4];
      snprintf(encoded, sizeof(encoded), "%%%02X", c);
      out += encoded;
    } else {
      out += static_cast<char>(c);
    }
  }
  return out;
}

std::string buildUrl(const std::string& serverUrl, const std::string& path) {
  // If path is already an absolute URL (has protocol), use it directly
  if (path.find("://") != std::string::npos) {
    return encodeUnsafeUrlChars(path);
  }
  const std::string urlWithProtocol = ensureProtocol(serverUrl);
  if (path.empty()) {
    return encodeUnsafeUrlChars(urlWithProtocol);
  }
  if (path[0] == '/') {
    // Absolute path - use just the host
    return encodeUnsafeUrlChars(extractHost(urlWithProtocol) + path);
  }
  // Relative path - strip query string from base before appending
  std::string base = urlWithProtocol;
  const size_t queryPos = base.find('?');
  if (queryPos != std::string::npos) {
    base.resize(queryPos);
  }
  if (base.back() == '/') {
    return encodeUnsafeUrlChars(base + path);
  }
  return encodeUnsafeUrlChars(base + "/" + path);
}

std::string resolveReference(const std::string& documentUrl, const std::string& reference) {
  if (reference.find("://") != std::string::npos) return encodeUnsafeUrlChars(reference);
  const std::string base = ensureProtocol(documentUrl);
  const size_t scheme = base.find("://");
  if (reference.starts_with("//")) return encodeUnsafeUrlChars(base.substr(0, scheme + 1) + reference);
  const size_t authorityEnd = base.find_first_of("/?#", scheme + 3);
  const std::string origin = base.substr(0, authorityEnd);
  const std::string withoutFragment = base.substr(0, base.find('#'));
  if (reference.empty() || reference[0] == '#') return encodeUnsafeUrlChars(withoutFragment + reference);
  const size_t basePathEnd = base.find_first_of("?#", scheme + 3);
  const std::string basePath = authorityEnd == std::string::npos || base[authorityEnd] != '/'
                                   ? "/"
                                   : base.substr(authorityEnd, basePathEnd - authorityEnd);
  if (reference[0] == '?') return encodeUnsafeUrlChars(origin + basePath + reference);
  const size_t pathEnd = reference.find_first_of("?#");
  std::string merged = reference.substr(0, pathEnd);
  if (merged[0] != '/') merged.insert(0, basePath.substr(0, basePath.rfind('/') + 1));
  std::string normalized;
  normalized.reserve(merged.size());
  std::string_view remaining = merged;
  while (!remaining.empty()) {
    if (remaining.starts_with("../")) {
      remaining.remove_prefix(3);
    } else if (remaining.starts_with("./")) {
      remaining.remove_prefix(2);
    } else if (remaining.starts_with("/./") || remaining == "/.") {
      remaining = remaining == "/." ? "/" : remaining.substr(2);
    } else if (remaining.starts_with("/../") || remaining == "/..") {
      remaining = remaining == "/.." ? "/" : remaining.substr(3);
      const size_t slash = normalized.rfind('/');
      normalized.resize(slash == std::string::npos ? 0 : slash);
    } else if (remaining == "." || remaining == "..") {
      remaining = {};
    } else {
      const size_t slash = remaining.find('/', remaining[0] == '/' ? 1 : 0);
      const size_t count = slash == std::string_view::npos ? remaining.size() : slash;
      normalized.append(remaining.data(), count);
      remaining.remove_prefix(count);
    }
  }
  return encodeUnsafeUrlChars(origin + normalized + (pathEnd == std::string::npos ? "" : reference.substr(pathEnd)));
}

bool resolveServerPath(std::string& path, const std::string& requestUrl) {
  if (path.empty() || path[0] != '/' || (path.size() > 1 && path[1] == '/')) return false;
  const size_t scheme = requestUrl.find("://");
  if (scheme == std::string::npos) return false;
  const size_t hostEnd = requestUrl.find_first_of("/?#", scheme + 3);
  path.insert(0, requestUrl, 0, hostEnd == std::string::npos ? requestUrl.size() : hostEnd);
  return true;
}

}  // namespace UrlUtils
