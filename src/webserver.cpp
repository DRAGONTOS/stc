#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <spawn.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include "includes/webserver.hpp"
#include "includes/zip.hpp"

extern char **environ;

// what one job is doing, polled by the website
static std::mutex joblock;
static bool jobrunning = false;
static std::string jobmode;
static std::string jobtitle;
static std::string jobnote;
static std::vector<std::string> joblog;
static int jobtotal = 0;
static int jobfinished = 0;
static int jobtimedout = 0;
static int joberrored = 0;
// the folder the last web job installed into or updated, the zip download aims there when the page gives no folder
static std::string jobdir;
// the folders the last web job installed, so an export without a folder packs just those
static std::vector<std::string> jobfolders;

// escapes a string for a json response
static std::string jsonescape(const std::string& text) {
    std::string out;
    for (char now : text) {
        if (now == '"' || now == '\\') {
            out += '\\';
        }
        if (now == '\n') {
            out += "\\n";
            continue;
        }
        out += now;
    }
    return out;
}

// pulls one string value out of the json the website posts
static std::string jsonvalue(const std::string& body, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    std::size_t at = body.find(needle);
    if (at == std::string::npos) {
        return "";
    }
    at += needle.size();
    while (at < body.size() && (body[at] == ' ' || body[at] == '"')) {
        if (body[at] == '"') {
            at++;
            break;
        }
        at++;
    }
    std::string value;
    while (at < body.size() && body[at] != '"') {
        if (body[at] == '\\' && at + 1 < body.size()) {
            char next = body[at + 1];
            value += next == 'n' ? std::string("\n") : next == 't' ? std::string("\t") : std::string(1, next);
            at += 2;
        } else {
            value += body[at];
            at++;
        }
    }
    return value;
}

// %xx and + decoding for query values
static std::string urldecode(const std::string& text) {
    std::string out;
    for (std::size_t at = 0; at < text.size(); ++at) {
        if (text[at] == '%' && at + 2 < text.size() && std::isxdigit(static_cast<unsigned char>(text[at + 1])) && std::isxdigit(static_cast<unsigned char>(text[at + 2]))) {
            out += static_cast<char>(std::stoi(text.substr(at + 1, 2), nullptr, 16));
            at += 2;
        } else if (text[at] == '+') {
            out += ' ';
        } else {
            out += text[at];
        }
    }
    return out;
}

// the value of one key in a query string
static std::string queryvalue(const std::string& query, const std::string& key) {
    std::string needle = key + "=";
    std::size_t at = query.find(needle);
    if (at == std::string::npos) {
        return "";
    }
    at += needle.size();
    std::size_t end = query.find('&', at);
    return urldecode(query.substr(at, end == std::string::npos ? std::string::npos : end - at));
}

// runs stc as a subprocess with the prompt answered, feeding every output line to the job
static void runjob(std::string mode, std::vector<std::string> args) {
    std::string exe = std::filesystem::read_symlink("/proc/self/exe").string();

    std::vector<std::string> envstrings;
    for (char** now = environ; *now != nullptr; ++now) {
        if (std::strncmp(*now, "STC_YES=", 8) != 0) {
            envstrings.push_back(*now);
        }
    }
    envstrings.push_back("STC_YES=1");
    std::vector<char*> argv;
    argv.push_back(exe.data());
    for (const std::string& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    std::vector<char*> envp;
    for (const std::string& value : envstrings) {
        envp.push_back(const_cast<char*>(value.c_str()));
    }
    envp.push_back(nullptr);

    int outpipe[2];
    if (pipe(outpipe) != 0) {
        std::lock_guard<std::mutex> guard(joblock);
        joblog.push_back("Could not start stc.");
        jobrunning = false;
        return;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, outpipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, outpipe[0]);
    posix_spawn_file_actions_addclose(&actions, outpipe[1]);

    pid_t pid;
    int spawned = posix_spawnp(&pid, exe.c_str(), &actions, nullptr, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    close(outpipe[1]);
    if (spawned != 0) {
        close(outpipe[0]);
        std::lock_guard<std::mutex> guard(joblock);
        joblog.push_back("Could not start stc.");
        jobrunning = false;
        return;
    }

    // read the plain output line by line and turn it into the job status
    int outfd = outpipe[0];
    std::string piece;
    std::string rewritten;
    char chunk[4096];
    while (true) {
        struct pollfd watcher { outfd, POLLIN, 0 };
        if (poll(&watcher, 1, 500) <= 0) {
            // a finished child stops producing; notice it by trying to read
            ssize_t got = read(outfd, chunk, sizeof(chunk));
            if (got <= 0) {
                break;
            }
            piece.append(chunk, got);
            continue;
        }
        ssize_t got = read(outfd, chunk, sizeof(chunk));
        if (got <= 0) {
            break;
        }
        piece.append(chunk, got);

        std::size_t at = 0;
        while (at < piece.size()) {
            char now = piece[at];
            if (now == '\r') {
                rewritten = piece.substr(0, at);
                piece.erase(0, at + 1);
                at = 0;
                continue;
            }
            if (now != '\n') {
                at++;
                continue;
            }

            std::string line = rewritten + piece.substr(0, at);
            rewritten.clear();
            piece.erase(0, at + 1);
            at = 0;

            if (line.empty()) {
                continue;
            }
            std::lock_guard<std::mutex> guard(joblock);
            jobnote = line;
            if (line.find("Installing collection: ") == 0) {
                jobtitle = line.substr(23);
            } else if (line.find("Installing mod: ") == 0) {
                jobtitle = line.substr(16);
            } else if (line.find("Destination: ") == 0) {
                jobdir = line.substr(13);
            } else if (line.find("Installing:         ") == 0) {
                try {
                    jobtotal = std::stoi(line.substr(20));
                } catch (...) {
                }
            } else if (line.find("Updating: ") == 0) {
                try {
                    jobtotal = std::stoi(line.substr(10));
                } catch (...) {
                }
            } else if (line.find("Successfully downloaded") != std::string::npos || line.find("Error downloading") != std::string::npos) {
                joblog.push_back(line);
                jobfinished++;
                if (line.find("Error downloading") != std::string::npos) {
                    joberrored++;
                } else {
                    std::size_t nameat = line.find("downloaded: ");
                    if (nameat != std::string::npos) {
                        jobfolders.push_back(line.substr(nameat + 12));
                    }
                }
            } else if (line.find("Finished: ") == 0) {
                jobfinished = std::stoi(line.substr(10));
            } else if (line.find("Timed out: ") == 0) {
                jobtimedout = std::stoi(line.substr(11));
            } else if (line.find("Errored: ") == 0) {
                joberrored = std::stoi(line.substr(9));
            } else if (line.find("Total: ") == 0) {
                jobtotal = std::stoi(line.substr(7));
            }
        }
        if (rewritten.empty() && piece.find('\n') == std::string::npos && piece.size() > 4096) {
            piece.clear();
        }
    }
    close(outfd);

    int status;
    waitpid(pid, &status, 0);

    std::lock_guard<std::mutex> guard(joblock);
    if (!rewritten.empty() || !piece.empty()) {
        std::string line = rewritten + piece;
        if (line.find("Successfully downloaded") != std::string::npos || line.find("Error downloading") != std::string::npos) {
            joblog.push_back(line);
        }
    }
    jobrunning = false;
}

// one http response
static void sendresponse(int fd, const std::string& code, const std::string& contenttype, const std::string& body, const std::string& extra = "") {
    std::ostringstream head;
    head << "HTTP/1.1 " << code << "\r\n"
         << "Content-Type: " << contenttype << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n"
         << extra
         << "\r\n";
    std::string packet = head.str() + body;
    send(fd, packet.data(), packet.size(), 0);
}

// the website, one page with material you styling in the instance purple
static const std::string sitepage = R"sitehtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Steam Collector</title>
<style>
:root{
  --primary:#6750A3; --on-primary:#FFFFFF; --primary-container:#EADDFF; --on-primary-container:#21005D;
  --surface:#FEF7FF; --surface-container:#F3EDF7; --surface-high:#ECE6F0;
  --on-surface:#1D1B20; --on-surface-variant:#49454F; --outline:#CAC4D0; --error:#B3261E; --ok:#386A20;
}
@media (prefers-color-scheme: dark){
  :root{
    --primary:#D0BCFF; --on-primary:#381E72; --primary-container:#4F378B; --on-primary-container:#EADDFF;
    --surface:#141218; --surface-container:#211F26; --surface-high:#2B2930;
    --on-surface:#E6E0E9; --on-surface-variant:#CAC4D0; --outline:#49454F; --error:#F2B8B5; --ok:#A6D38A;
  }
}
body{margin:0;background:var(--surface);color:var(--on-surface);
  font-family:'Roboto Flex',Roboto,system-ui,sans-serif;font-size:14px;}
.topbar{position:sticky;top:0;background:var(--surface-container);padding:18px 24px;
  font-size:22px;font-weight:500;border-radius:0 0 16px 16px;z-index:1}
.card{background:var(--surface-container);border-radius:24px;padding:20px;margin:16px auto;max-width:560px}
h2{margin:0 0 14px;font-size:15px;font-weight:600;color:var(--primary);
  text-transform:uppercase;letter-spacing:.08em}
.row{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:12px}
.field{flex:1;min-width:140px;border:1px solid var(--outline);border-radius:12px;
  padding:12px 14px;background:transparent;color:var(--on-surface);font:inherit}
.field:focus{outline:2px solid var(--primary);border-color:transparent}
.btn{background:var(--primary);color:var(--on-primary);border:none;border-radius:999px;
  padding:11px 24px;font-size:14px;font-weight:600;cursor:pointer;font:inherit;font-weight:600}
.btn:hover{box-shadow:0 1px 6px rgba(0,0,0,.35)}
.btn:disabled{opacity:.4;cursor:default;box-shadow:none}
.seg{display:flex;border:1px solid var(--outline);border-radius:999px;overflow:hidden;margin-bottom:12px}
.seg button{flex:1;padding:10px;border:none;background:transparent;color:var(--on-surface);
  font:inherit;cursor:pointer}
.seg button.active{background:var(--primary-container);color:var(--on-primary-container);font-weight:600}
.progress{height:8px;border-radius:999px;background:var(--surface-high);overflow:hidden;margin-bottom:10px}
.progress>div{height:100%;background:var(--primary);width:0%;transition:width .4s}
.status{color:var(--on-surface-variant);margin-bottom:10px}
.log{font-family:ui-monospace,Menlo,monospace;font-size:12.5px;max-height:220px;overflow:auto;
  background:var(--surface-high);border-radius:12px;padding:10px;white-space:pre-wrap;word-break:break-word}
.log .ok{color:var(--ok)} .log .err{color:var(--error)}
.hint{color:var(--on-surface-variant);font-size:12px;margin-top:10px}
.note{color:var(--on-surface-variant);margin:14px auto 0;max-width:560px;padding:0 16px 24px;text-align:center;font-size:12px}
</style>
</head>
<body>
<div class="topbar">Steam Collector</div>

<div class="card">
  <h2>Install</h2>
  <div class="seg">
    <button id="mode-collection" class="active" onclick="setmode('collection')">Collection</button>
    <button id="mode-mod" onclick="setmode('mod')">Mod</button>
  </div>
  <div class="row"><input class="field" id="i-id" placeholder="Collection id"></div>
  <div class="row">
    <input class="field" id="i-user" placeholder="Username (optional)">
    <input class="field" id="i-pass" type="password" placeholder="Password (optional)">
  </div>
  <div class="row">
    <input class="field" id="i-game" placeholder="Game id">
    <input class="field" id="i-dir" placeholder="Folder (optional)">
  </div>
  <button class="btn" id="b-install" onclick="install()">Install</button>
</div>

<div class="card">
  <h2>Update</h2>
  <div class="row"><input class="field" id="u-dir" placeholder="Folder to update (empty = steamcmd only)"></div>
  <button class="btn" id="b-update" onclick="runupdate()">Update</button>
</div>

<div class="card">
  <h2>Zips</h2>
  <div class="row">
    <input class="field" id="z-dir" placeholder="Folder to pack">
    <button class="btn" id="b-export" onclick="window.location='/export?dir='+encodeURIComponent(document.getElementById('z-dir').value)">Download zip</button>
  </div>
  <div class="row">
    <input class="field" type="file" id="z-file">
    <input class="field" id="z-target" placeholder="Folder to import into">
    <button class="btn" id="b-import" onclick="doimport()">Upload zip</button>
  </div>
  <div class="hint">A downloaded zip carries the manifest and an install.sh, so it is a full backup; uploading one registers its mods for updates. After an install finishes, its zip downloads by itself.</div>
</div>

<div class="card" id="jobcard" style="display:none">
  <h2 id="jobmode">Job</h2>
  <div class="status" id="jobtitle"></div>
  <div class="progress"><div id="jobbar"></div></div>
  <div class="log" id="joblog"></div>
</div>

<div class="note">Steam Collector runs on this machine only. Steam credentials stay local and are only passed to steamcmd.</div>

<script>
let mode = 'collection';
let awaitingzip = false;
function setmode(m){
  mode = m;
  document.getElementById('mode-collection').className = m === 'collection' ? 'active' : '';
  document.getElementById('mode-mod').className = m === 'mod' ? 'active' : '';
  document.getElementById('i-id').placeholder = m === 'collection' ? 'Collection id' : 'Mod id';
}
async function post(url, body){
  const res = await fetch(url, {method:'POST', body});
  return res.json();
}
async function install(){
  const body = JSON.stringify({
    mode, id: document.getElementById('i-id').value,
    user: document.getElementById('i-user').value, pass: document.getElementById('i-pass').value,
    gameid: document.getElementById('i-game').value, dir: document.getElementById('i-dir').value
  });
  const res = await post('/install', body);
  if (res.error) { alert(res.error); } else { awaitingzip = true; }
}
async function runupdate(){
  const body = JSON.stringify({dir: document.getElementById('u-dir').value});
  const res = await post('/update', body);
  if (res.error) { alert(res.error); }
}
async function doimport(){
  const file = document.getElementById('z-file').files[0];
  if (!file) { alert('Pick a zip first.'); return; }
  const dir = encodeURIComponent(document.getElementById('z-target').value);
  const res = await fetch('/import?dir=' + dir + '&filename=' + encodeURIComponent(file.name),
    {method:'POST', body: file});
  const out = await res.json();
  if (out.error) { alert(out.error); } else { alert(out.message || 'Imported.'); }
}
async function poll(){
  try{
    const res = await fetch('/status');
    const job = await res.json();
    const card = document.getElementById('jobcard');
    card.style.display = job.running || job.log.length ? 'block' : 'none';
    document.getElementById('jobmode').textContent = job.running ? (job.mode === 'update' ? 'Updating' : 'Installing') : 'Last job';
    document.getElementById('jobtitle').textContent = job.title || '';
    const pct = job.total > 0 ? Math.round(job.finished * 100 / job.total) : (job.running ? 10 : 100);
    document.getElementById('jobbar').style.width = Math.min(pct, 100) + '%';
    const log = document.getElementById('joblog');
    log.innerHTML = job.log.map(l => {
      const cls = l.includes('Error downloading') ? 'err' : 'ok';
      return '<div class="' + cls + '">' + l.replace(/</g,'&lt;') + '</div>';
    }).join('') || '&nbsp;';
    for (const id of ['b-install','b-update','b-export','b-import']) {
      document.getElementById(id).disabled = job.running;
    }
    if (awaitingzip && !job.running && job.mode === 'install' && job.log.length > 0) {
      awaitingzip = false;
      window.location = '/export?dir=';
    }
  } catch(e) {}
}
setInterval(poll, 1200);
poll();
</script>
</body>
</html>
)sitehtml";

// handles one accepted connection
static void handle(int fd, const std::string& homedir) {
    std::string request;
    char buffer[4096];
    while (request.find("\r\n\r\n") == std::string::npos && request.size() < 65536) {
        ssize_t got = recv(fd, buffer, sizeof(buffer), 0);
        if (got <= 0) {
            return;
        }
        request.append(buffer, got);
    }

    std::size_t headend = request.find("\r\n\r\n");
    std::string head = request.substr(0, headend);
    std::string body = request.substr(headend + 4);

    std::size_t contentlength = 0;
    std::size_t at = 0;
    while ((at = head.find("Content-Length:", at)) != std::string::npos) {
        contentlength = std::strtoull(head.c_str() + at + 15, nullptr, 10);
        at += 15;
    }
    while (body.size() < contentlength) {
        ssize_t got = recv(fd, buffer, sizeof(buffer), 0);
        if (got <= 0) {
            return;
        }
        body.append(buffer, got);
    }

    std::istringstream firstline(head.substr(0, head.find("\r\n")));
    std::string method, target, version;
    firstline >> method >> target >> version;
    std::string path = target;
    std::string query;
    std::size_t mark = target.find('?');
    if (mark != std::string::npos) {
        path = target.substr(0, mark);
        query = target.substr(mark + 1);
    }

    if (method == "GET" && (path == "/" || path == "/index.html")) {
        sendresponse(fd, "200 OK", "text/html; charset=utf-8", sitepage);
        return;
    }
    if (method == "GET" && path == "/status") {
        std::ostringstream out;
        out << "{\"running\":";
        std::lock_guard<std::mutex> guard(joblock);
        out << (jobrunning ? "true" : "false")
            << ",\"mode\":\"" << jsonescape(jobmode) << "\""
            << ",\"title\":\"" << jsonescape(jobtitle) << "\""
            << ",\"total\":" << jobtotal
            << ",\"finished\":" << jobfinished
            << ",\"timedout\":" << jobtimedout
            << ",\"errored\":" << joberrored
            << ",\"log\":[";
        for (std::size_t index = 0; index < joblog.size(); ++index) {
            out << (index ? "," : "") << "\"" << jsonescape(joblog[index]) << "\"";
        }
        out << "]}";
        sendresponse(fd, "200 OK", "application/json", out.str());
        return;
    }
    if (method == "POST" && (path == "/install" || path == "/update")) {
        {
            std::lock_guard<std::mutex> guard(joblock);
            if (jobrunning) {
                sendresponse(fd, "409 Conflict", "application/json", "{\"error\":\"a job is already running\"}");
                return;
            }
        }

        std::string dir = jsonvalue(body, "dir");
        bool dirgiven = !dir.empty();
        if (!dirgiven) {
            dir = homedir + "/.local/share/stc/mods";
        }

        std::vector<std::string> args;
        std::string mode;
        if (path == "/update") {
            mode = "update";
            args = dirgiven ? std::vector<std::string>{"-u", dir} : std::vector<std::string>{"-u"};
        } else {
            mode = "install";
            std::string id = jsonvalue(body, "id");
            std::string gameid = jsonvalue(body, "gameid");
            std::string user = jsonvalue(body, "user");
            std::string pass = jsonvalue(body, "pass");
            std::string flag = jsonvalue(body, "mode") == "mod" ? "-m" : "-c";
            if (id.empty() || gameid.find_first_not_of("0123456789") != std::string::npos) {
                sendresponse(fd, "400 Bad Request", "application/json", "{\"error\":\"an id and a numeric gameid are required\"}");
                return;
            }
            if (!user.empty() && pass.empty()) {
                sendresponse(fd, "400 Bad Request", "application/json", "{\"error\":\"a password is needed with a username\"}");
                return;
            }
            if (!user.empty()) {
                args = {flag, id, user, pass, gameid, dir};
            } else {
                args = {flag, id, gameid, dir};
            }
        }

        {
            std::lock_guard<std::mutex> guard(joblock);
            jobrunning = true;
            jobmode = mode;
            jobtitle = "";
            jobnote = "";
            joblog.clear();
            jobtotal = 0;
            jobfinished = 0;
            jobtimedout = 0;
            joberrored = 0;
            jobfolders.clear();
            if (dirgiven || mode == "install") {
                jobdir = dir;
            }
        }
        std::thread(runjob, mode, args).detach();
        sendresponse(fd, "200 OK", "application/json", "{\"started\":true}");
        return;
    }
    if (method == "GET" && path == "/export") {
        std::string dir = queryvalue(query, "dir");
        if (dir.empty()) {
            std::lock_guard<std::mutex> guard(joblock);
            dir = jobdir;
        }
        if (dir.empty()) {
            dir = homedir + "/.local/share/stc/mods";
        }
        std::string temp = "/tmp/stc-web-" + std::to_string(getpid()) + "-" + std::to_string(time(nullptr)) + ".zip";
        std::string message;
        {
            std::lock_guard<std::mutex> guard(joblock);
            if (!jobfolders.empty() && std::filesystem::equivalent(std::filesystem::path(dir), std::filesystem::path(jobdir))) {
                message = exportzip(dir, temp, &jobfolders);
            } else {
                message = exportzip(dir, temp);
            }
        }
        if (message.empty()) {
            sendresponse(fd, "500 Internal Server Error", "application/json", "{\"error\":\"packing failed, see the server log\"}");
            return;
        }
        std::string name = std::filesystem::path(dir).lexically_normal().filename().string();
        if (name.empty()) {
            name = "stc";
        }
        std::string extra = "Content-Disposition: attachment; filename=\"" + name + ".zip\"\r\n";
        std::string head = "HTTP/1.1 200 OK\r\nContent-Type: application/zip\r\nContent-Length: "
            + std::to_string(std::filesystem::file_size(temp)) + "\r\nConnection: close\r\n" + extra + "\r\n";
        send(fd, head.data(), head.size(), 0);
        std::ifstream file{temp, std::ios::binary};
        char chunk[65536];
        while (file) {
            file.read(chunk, sizeof(chunk));
            if (file.gcount() > 0) {
                send(fd, chunk, file.gcount(), 0);
            }
        }
        std::filesystem::remove(temp);
        return;
    }
    if (method == "POST" && path == "/import") {
        std::string dir = queryvalue(query, "dir");
        if (dir.empty()) {
            dir = homedir + "/.local/share/stc/mods";
        }
        std::string temp = "/tmp/stc-web-" + std::to_string(getpid()) + "-" + std::to_string(time(nullptr)) + ".zip";
        std::ofstream file{temp, std::ios::binary};
        file << body;
        file.close();

        std::string message = importzip(temp, dir);
        std::filesystem::remove(temp);
        if (message.empty()) {
            sendresponse(fd, "500 Internal Server Error", "application/json", "{\"error\":\"import failed, see the server log\"}");
            return;
        }
        sendresponse(fd, "200 OK", "application/json", "{\"message\":\"" + jsonescape(message) + "\"}");
        return;
    }

    sendresponse(fd, "404 Not Found", "text/plain", "not here");
}

void webserver(const cmd *inputCmd, const std::string& port) {
    std::string homedir = inputCmd->userHome;

    int server = -1;
    int boundport = std::stoi(port);
    for (int attempt = 0; attempt < 21; ++attempt) {
        server = socket(AF_INET, SOCK_STREAM, 0);
        if (server < 0) {
            std::cerr << "Could not create a socket." << std::endl;
            return;
        }
        int reuse = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(boundport);
        if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
            break;
        }
        close(server);
        server = -1;
        boundport++;
    }
    if (server < 0) {
        std::cerr << "No free port near " << port << "." << std::endl;
        return;
    }
    if (listen(server, 8) != 0) {
        std::cerr << "Could not listen on the port." << std::endl;
        close(server);
        return;
    }

    std::cout << "opened website on localhost:" << boundport << " (Ctrl-C stops it)" << std::endl;

    // opens the site in the desktop's browser, quietly skipped when there is no desktop
    std::string url = "http://localhost:" + std::to_string(boundport);
    std::vector<std::string> openargs{"xdg-open", url};
    std::vector<char*> openargv;
    for (const std::string& arg : openargs) {
        openargv.push_back(const_cast<char*>(arg.c_str()));
    }
    openargv.push_back(nullptr);

    posix_spawn_file_actions_t openactions;
    posix_spawn_file_actions_init(&openactions);
    posix_spawn_file_actions_addopen(&openactions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&openactions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    pid_t opener;
    if (posix_spawnp(&opener, "xdg-open", &openactions, nullptr, openargv.data(), environ) == 0) {
        int status;
        waitpid(opener, &status, 0);
    }
    posix_spawn_file_actions_destroy(&openactions);

    while (true) {
        int client = accept(server, nullptr, nullptr);
        if (client < 0) {
            continue;
        }
        handle(client, homedir);
        close(client);
    }
}
