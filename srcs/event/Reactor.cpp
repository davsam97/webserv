#include "event/Reactor.hpp"

#include <cerrno>
#include <stdexcept>

#include "log/Log.hpp"
#include "signal/signal.hpp"

namespace {
    const int POLL_TIMEOUT_MS = 10000;
}  // namespace

Reactor::Reactor() {}

// Free every owned handler. A handler can span several fds (CgiHandler: two pipes),
// so unregister all fds first, then delete each handler once.
Reactor::~Reactor() {
    std::set<AEventHandler*> dead = _owned;
    for (size_t i = 0; i < _pollfds.size();) {
        if (_owned.count(fd_to_handler[_pollfds[i].fd]))
            unregister_fd(_pollfds[i].fd);
        else
            ++i;
    }
    for (std::set<AEventHandler*>::iterator it = dead.begin(); it != dead.end(); ++it)
        delete *it;
    _owned.clear();
}

// events = POLLIN or POLLOUT;
void Reactor::register_fd(int fd, int events, AEventHandler* handler, bool owned) {
    // Take ownership first: if a push_back below throws, ~Reactor still frees it.
    if (owned) _owned.insert(handler);
    pollfd polling_req;
    polling_req.fd = fd;
    polling_req.events = events;
    polling_req.revents = 0;
    _pollfds.push_back(polling_req);
    fd_to_handler[fd] = handler;
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

bool Reactor::has_registered_fd(AEventHandler* h) const {
    for (std::map<int, AEventHandler*>::const_iterator it = fd_to_handler.begin();
         it != fd_to_handler.end(); ++it)
        if (it->second == h) return true;
    return false;
}

void Reactor::unregister_fd(int fd) {
    fd_to_handler.erase(fd);
    close(fd);
    for (size_t i = 0; i < _pollfds.size(); i++) {
        if (_pollfds[i].fd == fd) {
            _pollfds.erase(_pollfds.begin() + i);
            break;
        }
    }
}

void Reactor::set_events(int fd, short events) {
    for (size_t i = 0; i < _pollfds.size(); i++) {
        if (_pollfds[i].fd == fd) {
            _pollfds[i].events = events;
            return;
        }
    }
}

// events = what we ask poll() for, revents = what happened.
void Reactor::run() {
    webserv::setup_signals();
    while (!webserv::g_stop) {
        // poll will set all the revents to 0, then
        // poll will BLOCK the process until an event triggers it
        int n = poll(&_pollfds[0], _pollfds.size(), POLL_TIMEOUT_MS);
        if (n < 0) {
            if (webserv::g_stop)  // signal interrupted
            {
                Log::error("SIGNAL INTERRUPT");
                continue;
            }
            Log::error("poll error");
            throw std::runtime_error(std::strerror(errno));
        }

        // goes through polls, handle events
        for (size_t i = 0; i < _pollfds.size(); i++) {
            if (_pollfds[i].revents == 0) continue;
            handle_event(_pollfds[i]);
        }

        // Timeout pass (a CgiHandler is ticked twice, the second is a no-op).
        time_t now = time(NULL);
        for (size_t i = 0; i < _pollfds.size(); ++i)
            fd_to_handler[_pollfds[i].fd]->on_tick(now);

        // Reap finished fds; delete an owned handler once its last fd is gone.
        // Safe only because no destructor unregisters fds.
        for (size_t i = 0; i < _pollfds.size();) {
            AEventHandler* h = fd_to_handler[_pollfds[i].fd];
            if (h->finished) {
                unregister_fd(_pollfds[i].fd);
                if (_owned.count(h) && !has_registered_fd(h)) {
                    _owned.erase(h);
                    delete h;
                }
            } else {
                ++i;
            }
        }
    }
}

void Reactor::handle_event(pollfd& pfd) {
    AEventHandler* h = fd_to_handler[pfd.fd];
    int revents = pfd.revents;
    // HUP/ERR go through on_readable(): read()==0 is EOF, read()<0 an abort.
    // POLLNVAL: the fd is not open.
    if (revents & (POLLIN | POLLHUP | POLLERR)) h->on_readable();
    if (revents & POLLOUT) h->on_writable();
    if (revents & POLLNVAL) h->finished = true;
}
