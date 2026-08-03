/**
 * @file webserver_utils.hpp
 * @brief Stateless HTTP helper functions used by the webserver handlers.
 *
 * Port of the reference C `webserver_utils.c`/`webserver_utils.h`. These are
 * free functions in `namespace fpc`; they translate to/from `esp_err_t` only at
 * the ESP-IDF `httpd` boundary.
 *
 * C++17 / fpc-cpp port.
 */

#pragma once

#include <cstddef>
#include <string_view>
#include "esp_err.h"
#include "esp_http_server.h"
#include "common.hpp"

namespace fpc {

/// Read the full request body into @p buffer (NUL-terminated). Sends a 500 and
/// returns an error if the body exceeds @p buffer_size.
Result<void> retrieve_request_body(httpd_req_t* req, char* buffer, std::size_t buffer_size);

/// Inject permissive CORS headers onto the response.
void inject_cors(httpd_req_t* req);

/// Extract cookie @p name from a Cookie header into @p out. Returns an error if
/// the named cookie is absent.
Result<void> parse_cookie(std::string_view cookie_hdr, std::string_view name,
                          char* out, std::size_t out_len);

/// Set the response Content-Type from a file path's extension.
esp_err_t set_content_type_from_file(httpd_req_t* req, std::string_view filepath);

/// Map a file path's extension to its SPIFFS subfolder (html/js/css/img).
void content_directory_name(std::string_view filepath, char* buff, std::size_t buff_size);

/// Format `{ "message": "<msg>" }` into @p buffer and return @p buffer.
const char* make_json_message(std::string_view message, char* buffer, std::size_t buffer_size);

} // namespace fpc
