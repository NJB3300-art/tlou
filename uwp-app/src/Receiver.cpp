#include "pch.h"
#include "Receiver.h"
#include "Stage.h"

#include <cctype>
#include <io.h>

#include <winrt/Windows.Networking.Connectivity.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Storage.Streams.h>

using namespace winrt;
using namespace winrt::Windows::Networking;
using namespace winrt::Windows::Networking::Connectivity;
using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Storage::Streams;
namespace fs = std::filesystem;

namespace tlou {

namespace {

constexpr char UploadPage[] = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Send The Last of Us to the Xbox</title>
<style>
body{margin:0;background:#0e1210;color:#ebf0e6;font:16px/1.5 system-ui,"Segoe UI",sans-serif}
main{max-width:760px;margin:0 auto;padding:28px 20px}h1{margin:0 0 4px;font-size:1.9rem}
.dim{color:#8c9a8e}.box{background:#18201b;border-radius:12px;padding:16px 18px;margin:14px 0}
label{display:block;font-weight:600;margin-bottom:6px}input[type=text]{font:inherit;padding:8px 10px;
border-radius:8px;border:1px solid #2c3a31;background:#0e1210;color:inherit;width:9em;letter-spacing:.2em}
button{font:inherit;font-weight:600;background:#549e61;color:#fff;border:0;border-radius:10px;padding:10px 22px;cursor:pointer}
button:disabled{opacity:.5;cursor:default}progress{width:100%;height:16px}code{background:#202924;padding:1px 6px;border-radius:6px}
#log{font:13px/1.4 ui-monospace,Consolas,monospace;white-space:pre-wrap;max-height:220px;overflow:auto;color:#8c9a8e}
</style></head><body><main>
<h1>Send the game to the Xbox</h1>
<p class="dim">Pick your folders. They are copied into the app's <code>TLOU</code> folder on the console.
An interrupted transfer continues where it stopped when you press Send again.</p>
<div class="box"><label for="code">Code shown on the TV</label><input id="code" type="text" inputmode="numeric" maxlength="6"></div>
<div class="box"><label>Game folder (CUSA00552)</label><input id="game" type="file" webkitdirectory multiple>
<p class="dim" id="game-n">Not chosen.</p></div>
<div class="box"><label>Update folder (CUSA00552-patch, version 1.11)</label><input id="update" type="file" webkitdirectory multiple>
<p class="dim" id="update-n">Not chosen.</p></div>
<div class="box"><label>Stage folder (uwp-stage, made by Stage-TLOU-Xbox.cmd on the PC)</label><input id="stage" type="file" webkitdirectory multiple>
<p class="dim" id="stage-n">Not chosen.</p></div>
<button id="send">Send</button>
<div class="box"><progress id="bar" value="0" max="1"></progress><p id="state" class="dim">Ready.</p><div id="log"></div></div>
</main><script>
const CHUNK=32*1024*1024, targets={game:"CUSA00552",update:"CUSA00552-patch",stage:"stage"};
const $=id=>document.getElementById(id), log=t=>{$("log").textContent=t+"\n"+$("log").textContent};
const gib=b=>(b/1073741824).toFixed(2)+" GiB";
for(const k in targets){$(k).onchange=()=>{const f=[...$(k).files],s=f.reduce((a,x)=>a+x.size,0);
 $(k+"-n").textContent=f.length?f.length+" files, "+gib(s):"Not chosen.";};}
async function api(method,name,params,body){const q=new URLSearchParams({code:$("code").value.trim(),...params});
 const r=await fetch("/api/"+name+"?"+q,{method,body});const t=await r.text();
 if(!r.ok)throw new Error(t||r.statusText);return t?JSON.parse(t):{};}
function relative(file,target){const parts=file.webkitRelativePath.split("/");parts[0]=target;return parts.join("/");}
$("send").onclick=async()=>{
 const jobs=[];for(const k in targets)for(const f of $(k).files)jobs.push([f,relative(f,targets[k])]);
 if(!jobs.length){$("state").textContent="Choose at least one folder.";return;}
 $("send").disabled=true;
 try{
  const st=await api("GET","status",{});const total=jobs.reduce((a,[f])=>a+f.size,0);
  if(st.free>=0&&st.free<total)log("Warning: the console reports "+gib(st.free)+" free for "+gib(total)+".");
  let done=0;$("bar").max=total||1;
  for(const [f,path] of jobs){
   let have=(await api("GET","info",{path})).size;
   if(have===f.size){done+=f.size;$("bar").value=done;continue;}
   let off=have>0&&have<f.size?have:0;done+=off;
   while(off<f.size||(f.size===0&&off===0)){
    const end=Math.min(off+CHUNK,f.size);let tries=0;
    for(;;){try{await api("PUT","file",{path,offset:off,total:f.size},f.slice(off,end));break;}
     catch(e){if(++tries>=4)throw e;log("retry "+path+": "+e.message);await new Promise(r=>setTimeout(r,1500*tries));}}
    done+=end-off;off=end;$("bar").value=done;
    $("state").textContent=gib(done)+" of "+gib(total)+"  ·  "+path;
    if(f.size===0)break;
   }
  }
  await api("POST","done",{});$("state").textContent="Done: "+gib(total)+". Check the launcher on the TV.";log("Finished.");
 }catch(e){$("state").textContent="Stopped: "+e.message+" (press Send to continue)";}
 $("send").disabled=false;};
</script></body></html>
)HTML";

std::string Json(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        if (uint8_t(c) < 0x20) continue;
        out += c;
    }
    return out + "\"";
}

std::string UrlDecode(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size() && isxdigit(uint8_t(text[i + 1])) && isxdigit(uint8_t(text[i + 2]))) {
            out += char(std::stoi(text.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            out += text[i] == '+' ? ' ' : text[i];
        }
    }
    return out;
}

std::map<std::string, std::string> Query(const std::string& target) {
    std::map<std::string, std::string> query;
    const size_t q = target.find('?');
    if (q == std::string::npos) return query;
    std::stringstream parts(target.substr(q + 1));
    std::string part;
    while (std::getline(parts, part, '&')) {
        const size_t eq = part.find('=');
        if (eq != std::string::npos) query[UrlDecode(part.substr(0, eq))] = UrlDecode(part.substr(eq + 1));
    }
    return query;
}

// A path inside the TLOU folder, or empty when it tries to leave it or names another folder.
fs::path SafePath(const fs::path& root, std::string relative) {
    std::replace(relative.begin(), relative.end(), '\\', '/');
    if (relative.empty() || relative.front() == '/' || relative.find(':') != std::string::npos) return {};
    std::stringstream parts(relative);
    std::string part;
    std::vector<std::string> pieces;
    while (std::getline(parts, part, '/')) {
        if (part.empty() || part == "." || part == "..") return {};
        pieces.push_back(part);
    }
    if (pieces.size() < 2) return {};
    if (pieces[0] != "CUSA00552" && pieces[0] != "CUSA00552-patch" && pieces[0] != "stage") return {};
    fs::path path = root;
    for (const auto& piece : pieces) path /= Wide(piece);
    return path;
}

IAsyncAction Write(const IOutputStream& out, const std::string& bytes) {
    DataWriter writer(out);
    writer.WriteBytes(array_view<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()),
                                                reinterpret_cast<const uint8_t*>(bytes.data()) + bytes.size()));
    co_await writer.StoreAsync();
    co_await writer.FlushAsync();
    writer.DetachStream();
}

IAsyncAction Respond(const IOutputStream& out, int code, const std::string& type, const std::string& body) {
    const char* reason = code == 200 ? "OK" : code == 400 ? "Bad Request" : code == 403 ? "Forbidden"
                       : code == 404 ? "Not Found" : code == 409 ? "Conflict" : "Internal Server Error";
    std::string head = "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\nContent-Type: " + type +
                       "\r\nContent-Length: " + std::to_string(body.size()) +
                       "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
    co_await Write(out, head + body);
}

std::string LocalAddress() {
    try {
        for (const auto& host : NetworkInformation::GetHostNames()) {
            if (host.Type() != HostNameType::Ipv4) continue;
            const std::string ip = to_string(host.CanonicalName());
            if (ip.rfind("169.254.", 0) == 0 || ip.rfind("127.", 0) == 0) continue;
            return ip;
        }
    } catch (...) {
    }
    return "";
}

} // namespace

Receiver::~Receiver() {
    Stop();
}

void Receiver::Set(std::function<void(Status&)> change) {
    std::scoped_lock lock{mutex};
    change(status);
}

Receiver::Status Receiver::Snapshot() {
    std::scoped_lock lock{mutex};
    return status;
}

void Receiver::Start() {
    if (listener) return;
    char code[8];
    sprintf_s(code, "%06u", unsigned((GetTickCount64() * 2654435761ull + GetCurrentProcessId()) % 1000000ull));
    Set([&](Status& s) {
        s = Status{};
        s.code = code;
        s.message = "starting";
    });
    std::error_code ignored;
    fs::create_directories(root, ignored);
    Listen();
}

void Receiver::Stop() {
    if (listener) {
        try {
            listener.Close();
        } catch (...) {
        }
        listener = nullptr;
    }
    Set([](Status& s) {
        s.running = false;
        s.message = "stopped";
    });
}

winrt::fire_and_forget Receiver::Listen() {
    try {
        listener = StreamSocketListener();
        listener.ConnectionReceived([this](const StreamSocketListener&, const StreamSocketListenerConnectionReceivedEventArgs& e) {
            Serve(e.Socket());
        });
        co_await listener.BindServiceNameAsync(to_hstring(std::to_string(Port)));
        const std::string ip = LocalAddress();
        Set([&](Status& s) {
            s.running = true;
            s.address = ip.empty() ? "no network address" : "http://" + ip + ":" + std::to_string(Port);
            s.message = "waiting for the PC";
        });
    } catch (const hresult_error& e) {
        listener = nullptr;
        Set([&](Status& s) {
            s.running = false;
            s.message = "could not start: " + to_string(e.message());
        });
    }
}

winrt::fire_and_forget Receiver::Serve(StreamSocket socket) {
    const auto in = socket.InputStream();
    const auto out = socket.OutputStream();
    try {
        // Request line and headers.
        Buffer buffer(1 << 20);
        std::string data;
        size_t header_end = std::string::npos;
        while ((header_end = data.find("\r\n\r\n")) == std::string::npos) {
            const IBuffer got = co_await in.ReadAsync(buffer, buffer.Capacity(), InputStreamOptions::Partial);
            if (got.Length() == 0 || data.size() > 64 * 1024) co_return;
            data.append(reinterpret_cast<const char*>(got.data()), got.Length());
        }
        std::stringstream head(data.substr(0, header_end));
        std::string method, target, line;
        head >> method >> target;
        std::getline(head, line);
        uint64_t length = 0;
        while (std::getline(head, line)) {
            std::string lower = line;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(tolower(c)); });
            if (lower.rfind("content-length:", 0) == 0) length = std::stoull(line.substr(15));
        }
        std::string body_start = data.substr(header_end + 4);
        const std::string route = target.substr(0, target.find('?'));
        auto query = Query(target);

        if (method == "GET" && (route == "/" || route == "/index.html")) {
            co_await Respond(out, 200, "text/html; charset=utf-8", UploadPage);
            co_return;
        }
        std::string code;
        {
            std::scoped_lock lock{mutex};
            code = status.code;
        }
        if (route.rfind("/api/", 0) != 0) {
            co_await Respond(out, 404, "text/plain", "not found");
            co_return;
        }
        if (query["code"] != code) {
            co_await Respond(out, 403, "text/plain", "wrong code: use the six digits shown on the TV");
            co_return;
        }
        if (method == "GET" && route == "/api/status") {
            std::error_code error;
            const auto space = fs::space(root, error);
            co_await Respond(out, 200, "application/json",
                             "{\"free\":" + std::to_string(error ? -1.0 : double(space.available)) +
                                 ",\"root\":" + Json(Utf8(root.wstring())) + "}");
            co_return;
        }
        if (method == "POST" && route == "/api/done") {
            changed = true;
            Set([](Status& s) {
                s.current.clear();
                s.message = "transfer finished";
            });
            co_await Respond(out, 200, "application/json", "{\"ok\":true}");
            co_return;
        }
        const fs::path path = SafePath(root, query["path"]);
        if (path.empty()) {
            co_await Respond(out, 400, "text/plain", "path must start with CUSA00552/, CUSA00552-patch/ or stage/");
            co_return;
        }
        if (method == "GET" && route == "/api/info") {
            std::error_code error;
            const auto size = fs::file_size(path, error);
            co_await Respond(out, 200, "application/json",
                             "{\"size\":" + (error ? std::string("-1") : std::to_string(size)) + "}");
            co_return;
        }
        if (method == "PUT" && route == "/api/file") {
            const uint64_t offset = std::stoull(query["offset"].empty() ? "0" : query["offset"]);
            const uint64_t total = std::stoull(query["total"].empty() ? "0" : query["total"]);
            std::error_code error;
            fs::create_directories(path.parent_path(), error);
            const uint64_t have = fs::exists(path, error) ? fs::file_size(path, error) : 0;
            if (offset > have) {
                co_await Respond(out, 409, "text/plain", "offset beyond the received part");
                co_return;
            }
            FILE* file = _wfopen(path.c_str(), offset == 0 ? L"wb" : L"r+b");
            if (!file || _fseeki64(file, int64_t(offset), SEEK_SET) != 0) {
                if (file) fclose(file);
                co_await Respond(out, 500, "text/plain", "cannot write " + Utf8(path.wstring()));
                co_return;
            }
            Set([&](Status& s) {
                s.current = query["path"];
                s.message = "receiving";
            });
            uint64_t written = 0;
            bool ok = true;
            auto put = [&](const char* bytes, size_t count) {
                if (fwrite(bytes, 1, count, file) != count) ok = false;
                written += count;
            };
            put(body_start.data(), size_t(std::min<uint64_t>(body_start.size(), length)));
            while (ok && written < length) {
                const uint32_t want = uint32_t(std::min<uint64_t>(buffer.Capacity(), length - written));
                const IBuffer got = co_await in.ReadAsync(buffer, want, InputStreamOptions::Partial);
                if (got.Length() == 0) break;
                put(reinterpret_cast<const char*>(got.data()), got.Length());
                Set([&](Status& s) { s.bytes += got.Length(); });
            }
            // The tail of a shorter rewrite of an existing file goes away.
            if (ok && offset + written >= total) _chsize_s(_fileno(file), int64_t(total));
            fclose(file);
            if (!ok || written < length) {
                Set([&](Status& s) { s.message = "write failed (disk full?) for " + query["path"]; });
                co_await Respond(out, 500, "text/plain", "write failed (is the console's storage full?)");
                co_return;
            }
            if (offset + written >= total) {
                changed = true;
                Set([](Status& s) { ++s.files_done; });
            }
            co_await Respond(out, 200, "application/json", "{\"ok\":true,\"size\":" + std::to_string(offset + written) + "}");
            co_return;
        }
        co_await Respond(out, 404, "text/plain", "not found");
    } catch (const hresult_error& e) {
        Set([&](Status& s) { s.message = "connection: " + to_string(e.message()); });
    } catch (const std::exception& e) {
        Set([&](Status& s) { s.message = std::string("request: ") + e.what(); });
    }
}

} // namespace tlou
