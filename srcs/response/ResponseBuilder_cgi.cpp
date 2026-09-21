
#include <cstring>

#include "cgi/CgiProcess.hpp"
#include "request/RequestParser.hpp"
#include "response/ResponseBuilder.hpp"
#include "utils/Utils.hpp"

namespace {
// parse a single line in a http header
// "Content-Length: 50" -> header[content-length] = 50
bool parse_http_header_line(const std::string& line, std::string& key,
                            std::string& value) {

    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) return false;
    key = line.substr(0, colon_pos);
    value = line.substr(colon_pos + 1);
    // if theres a space in the key = ERROR
    if (key.find_first_of(" ") != std::string::npos) return false;

    key = utils::to_lower(key);
    value = utils::trim(value);
    return true;
}

}  // namespace

// Construct the response from a finished CGI handler.
ResponseBuilder::ResponseBuilder(CgiHandler& handler)
        : waiting_for_cgi(false),
          protocol("HTTP/1.0"),
          location(NULL),
          config(handler.cgi.config),
          req(NULL) {  // CGI path: no request to re-route, so error_pages are skipped
    header["connection"] = "close";
    // fork/pipe failed, the script crashed (502), or it timed out (504):
    // build() turns the >= 400 status into an error page.
    if (handler.cgi.exec_status >= 400) {
        status_code = handler.cgi.exec_status;
        return;
    }
    parse_cgi_response(handler.read_buffer);
}

// Parse a raw CGI response (header block + body) into status_code/header/body.
void ResponseBuilder::parse_cgi_response(std::string raw) {
    // find the header/body separator (tolerate bare "\n\n")
    size_t sep = raw.find("\r\n\r\n");
    std::string delim = "\r\n";
    if (sep == std::string::npos) {
        sep = raw.find("\n\n");
        delim = "\n";
    }
    // malformed: no blank line
    if (sep == std::string::npos) {
        status_code = 502;
        return;
    }

    std::vector<std::string> lines = utils::split(raw.substr(0, sep), delim);

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string k, v;
        if (parse_http_header_line(lines[i], k, v)) {
            if (k == "set-cookie")
                set_cookies.push_back(v);  // keep every cookie, not just the last
            else
                header[k] = v;
        }
    }

    body = raw.substr(sep + delim.size() * 2);
    header["content-length"] = utils::to_str(body.size());

    // honor the "Status:" header
    status_code = 200;
    if (header.count("status")) {
        // usually a cgi response is something like `Status: 404 Not Found`,
        // we ignore the reason phrase
        const std::string& s = header["status"];
        std::string code_str = s.substr(0, s.find(' '));  // "404 Not Found" -> "404"
        if (!utils::parse_int(code_str, status_code)) status_code = 502;
        header.erase("status");
    }
}

// Also sets up the variables for the CGI Handler,
// returns it to the Connection.
bool ResponseBuilder::is_cgi_request(const std::string& filepath) {
    // is there an interpreter?
    const std::string* interpreter = NULL;
    for (std::map<std::string, std::string>::const_iterator it = location->cgi.begin();
         it != location->cgi.end(); ++it) {
        const std::string& ext = it->first;
        if (filepath.size() >= ext.size() &&
            filepath.compare(filepath.size() - ext.size(), ext.size(), ext) == 0) {
            interpreter = &it->second;
            break;
        }
    }
    if (interpreter == NULL) return false;
    // note: the interpreter can be equal to the empty string

    // Not run here (it would block the reactor): Connection::queue_response()
    // forks the script and registers its pipes.
    waiting_for_cgi = true;
    cgi_interpreter = *interpreter;
    cgi_filepath = filepath;
    return true;
}

// Find the CGI script in the URI: the first path segment that is a regular file.
// The rest is PATH_INFO ("/cgi-bin/up.py/foo" -> script "/cgi-bin/up.py").
bool ResponseBuilder::dispatch_cgi_path_info(const std::string& root) {
    const std::string& uri = req->URI;
    // Only proper prefixes ending at a '/' boundary; the full URI is handled by
    // the caller and is already known not to be a plain file.
    for (size_t i = 1; i < uri.size(); ++i) {
        if (uri[i] != '/') continue;
        std::string fs = map_path(uri.substr(0, i), root);
        if (!is_cgi_request(fs)) continue;
        // first file wins; PATH_INFO is the remainder

        return true;
    }
    return false;
}
