#include "cgi/CgiHandler.hpp"

#include "connection/Connection.hpp"
#include "event/Reactor.hpp"
#include "log/Log.hpp"

namespace {
    const int CGI_TIMEOUT_SEC = 500;
}  // namespace

CgiHandler::CgiHandler(Reactor& reactor, RequestParser& req, const ServerConfig& config,
                       const std::string& interpreter, const std::string& script_path)
        : AEventHandler(reactor),
          cgi(req, config, interpreter, script_path),
          _owner(NULL),
          write_buffer(req.body),
          read_buffer("") {
    touch();
}

// Feed the request body to the child's stdin.
// Once it's all written (or there was none) close the write end so the child sees EOF.
void CgiHandler::on_writable() {
    if (finished) return;
    if (write_buffer.empty()) {
        finish_writing();
        return;
    }
    ssize_t n = write(cgi.in_fd[1], write_buffer.data(), write_buffer.size());
    // n <= 0: pipe full (EAGAIN) or the child is gone.
    // We can retry again next poll(), let the timeout kill it if it's really buggy
    if (n <= 0) return;
    touch();
    write_buffer.erase(0, n);
    if (write_buffer.empty()) finish_writing();
}

void CgiHandler::finish_writing() {
    if (cgi.in_fd[1] == -1) return;
    reactor.unregister_fd(cgi.in_fd[1]);  // closes the fd -> EOF to the child
    cgi.in_fd[1] = -1;
}

// Drain the child's stdout. read() == 0 is EOF: the script is done, so reap it,
// translate the exit status, and hand the buffered output to the Connection.
void CgiHandler::on_readable() {
    if (finished) return;

    char buf[409600];
    ssize_t n = read(cgi.out_fd[0], buf, sizeof(buf));
    if (n > 0) {
        touch();
        read_buffer.append(buf, n);
        return;
    }
    if (n == 0) {
        cgi.reap();  // EOF: waitpid + status -> exec_status
    } else {
        cgi.kill_child();       // pipe error: don't leave the child running
        cgi.exec_status = 502;  // Bad Gateway
    }
    Log::debug("cgi: " + read_buffer);
    complete();
}

void CgiHandler::on_tick(time_t now) {
    if (finished) return;
    if (now - _last_activity > CGI_TIMEOUT_SEC) {
        Log::warning("CGI timeout -> 504");
        cgi.kill_child();
        cgi.exec_status = 504;
        complete();
    }
}

void CgiHandler::complete() {
    // The reactor owns us: leave our pipes registered so its reap pass closes them
    // and deletes us. Just flip finished, pass the output on, and cut the link both ways.
    finished = true;
    if (_owner) {
        _owner->on_cgi_done(*this);
        _owner->cgi = NULL;
        _owner = NULL;
    }
}

CgiHandler::~CgiHandler() {
    // Drop the Connection's dangling pointer, and kill the child if it is still running
    // (aborted CGI). The reactor closes the pipes, so the fds are not touched here.
    if (_owner) _owner->cgi = NULL;
    cgi.kill_child();
}
