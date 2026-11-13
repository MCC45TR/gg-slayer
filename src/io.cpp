// SPDX-License-Identifier: MIT
#include "core.hpp"
#include "../kernel/model_contract.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <poll.h>
#include <spawn.h>
#include <signal.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

namespace slayer {
std::string trim(std::string s) {
    auto first = s.find_first_not_of(" \r\n\t\0", 0, 5);
    if (first == std::string::npos) return {};
    auto last = s.find_last_not_of(" \r\n\t\0", std::string::npos, 5);
    return s.substr(first, last - first + 1);
}
std::string displayText(std::string value) {
    for(char& c:value)if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)==127)c=' ';
    return value;
}
std::vector<std::string> words(const std::string &s) {
    std::istringstream in(s); std::vector<std::string> result; std::string item;
    while (in >> item) result.push_back(item);
    return result;
}
int integer(const std::string &s, int low, int high, const std::string &name) {
    int value = 0; const auto [ptr, ec] = std::from_chars(s.data(), s.data()+s.size(), value);
    if (ec != std::errc{} || ptr != s.data()+s.size() || value < low || value > high)
        throw Error(name + ": " + std::to_string(low) + ".." + std::to_string(high) + " aralığında tam sayı gerekli.");
    return value;
}
static bool beneath(const fs::path &p, const fs::path &root) {
    auto a = p.begin(), b = root.begin();
    for (; b != root.end(); ++a, ++b) if (a == p.end() || *a != *b) return false;
    return true;
}
Io::Io(Options opts) : options(std::move(opts)) {
    if (!options.root.is_absolute()) throw Error("Test kökü mutlak yol olmalı.");
    options.root = fs::canonical(options.root);
    if (fixture() && getuid() != geteuid()) throw Error("Yetki yükseltilmiş test kökü kabul edilmez.");
    if(fixture()){
        auto fault=trim(read("/.slayerctl-fail-write",16));
        if(!fault.empty())testFailure=integer(fault,1,100000,"Test yazma hatası");
        auto post=trim(read("/.slayerctl-fail-write-after",16));
        if(!post.empty())testPostFailure=integer(post,1,100000,"Test store hatası");
    }
}
bool Io::fixture() const { return options.root != fs::path("/"); }
std::string Io::compatibilityIssue() const {
    if(!compatibilityCache){
        std::string issue;
        if(trim(read("/sys/class/dmi/id/sys_vendor",128))!=SLAYER_VENDOR ||
           trim(read("/sys/class/dmi/id/product_name",128))!=SLAYER_PRODUCT ||
           trim(read("/sys/class/dmi/id/product_sku",128))!=SLAYER_SKU)
            issue="Model/SKU uyumsuz; gereken: " SLAYER_VENDOR " / " SLAYER_PRODUCT " / " SLAYER_SKU ".";
        const auto bios=trim(read("/sys/class/dmi/id/bios_version",128));
        if(issue.empty()&&bios!=SLAYER_BIOS)
            issue="BIOS uyumsuz; gereken: " SLAYER_BIOS ", mevcut: "+(bios.empty()?"okunamadı":bios)+".";
        compatibilityCache=issue;
    }
    return *compatibilityCache;
}
void Io::requireCompatible() const {
    auto issue=compatibilityIssue();
    if(!issue.empty())throw Error("Donanım yazması kilitli: "+issue+" Zorla atlama desteklenmez.",4);
}
fs::path Io::path(const std::string &s) const {
    const fs::path rel(s);
    if (!rel.is_absolute()) throw Error("İç yol mutlak olmalı.");
    for (const auto &part : rel) if (part == "..") throw Error("Üst dizine geçiş reddedildi.");
    fs::path result = fixture() ? options.root / rel.relative_path() : rel;
    if (fixture()) {
        std::error_code ec;
        const auto canonical = fs::weakly_canonical(result, ec);
        if (ec || !beneath(canonical, options.root)) throw Error("Test kökü dışına çıkan bağlantı reddedildi.");
    }
    return result;
}
bool Io::exists(const std::string &s) const { std::error_code ec; return fs::exists(path(s), ec); }
std::string Io::read(const std::string &s, std::size_t limit) const {
    std::ifstream input(path(s), std::ios::binary);
    if (!input) return {};
    std::string out(std::min(limit, std::size_t(65536)), '\0');
    input.read(out.data(), static_cast<std::streamsize>(out.size()));
    out.resize(static_cast<std::size_t>(input.gcount()));
    return out;
}
std::vector<std::string> Io::list(const std::string &s) const {
    std::vector<std::string> result; std::error_code ec;
    fs::directory_iterator it(path(s), fs::directory_options::skip_permission_denied, ec);
    if (ec) return result;
    for (const auto &entry : it) {
        if (result.size() == 4096) throw Error("Aygıt dizini beklenen sınırı aştı.");
        result.push_back(entry.path().filename().string());
    }
    std::sort(result.begin(), result.end()); return result;
}
void Io::write(const std::string &s, const std::string &value) const {
    requireCompatible();
    // The public API accepts no arbitrary file paths. Every caller discovers a
    // standard sysfs control and validates both its type and its value first.
    if (!s.starts_with("/sys/") || value.size() > 4096) throw Error("Yazma yolu veya veri uzunluğu reddedildi.");
    const auto target = path(s);
    std::error_code ec; auto canonical = fs::canonical(target, ec);
    if (ec || (!fixture() && !beneath(canonical, fs::path("/sys")))) throw Error("Geçersiz sysfs hedefi.");
    if (fixture() && !beneath(canonical, options.root)) throw Error("Test dışına yazma reddedildi.");
    if (options.dry) { std::cout << "Önizleme: " << s << " = " << trim(value) << '\n'; return; }
    if (!fixture() && geteuid() != 0) throw Error("Bu işlem yönetici yetkisi gerektiriyor. Aynı komutu sudo ile çalıştırın.", 3);
    if(fixture()){++testWrites;if(testFailure&&testWrites==testFailure)throw Error("Enjekte edilen test yazma hatası.");}
    int fd = ::open(canonical.c_str(), O_WRONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) throw Error("Yazılamadı: " + s + ": " + std::strerror(errno), 3);
    struct stat st{};
    if (fstat(fd, &st) || !S_ISREG(st.st_mode)) { close(fd); throw Error("Normal sysfs dosyası bekleniyordu."); }
    if (fixture() && ftruncate(fd, 0)) {close(fd);throw Error("Test dosyası sıfırlanamadı.");}
    // A sysfs store is one transaction. A partial write must never become a
    // second store with the remaining suffix of a hardware command.
    ssize_t n;
    do {n = ::write(fd, value.data(), value.size());} while(n < 0 && errno == EINTR);
    const int error=errno;close(fd);
    if(fixture()&&testPostFailure&&testWrites==testPostFailure)throw Error("Enjekte edilen store sonrası hata.");
    if (n != static_cast<ssize_t>(value.size())) throw Error("Yazma tamamlanamadı: " + s + ": " + (n<0?std::string(std::strerror(error)):"kısmi sysfs yazması"));
}
FileLock::FileLock(const Io&io,const std::string&name){
    if(io.fixture()||io.dry())return;
    if(geteuid()!=0)throw Error("Değişiklik için yönetici yetkisi gerekir.",3);
    if(name.empty()||name.find_first_not_of("abcdefghijklmnopqrstuvwxyz-")!=std::string::npos)throw Error("Geçersiz iç kilit adı.");
    fd=open(("/run/lock/slayerctl-"+name+".lock").c_str(),O_WRONLY|O_CREAT|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK,0600);
    struct stat st{};
    if(fd<0||fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=0||st.st_nlink!=1||flock(fd,LOCK_EX|LOCK_NB)){if(fd>=0)close(fd);fd=-1;throw Error("Başka bir slayerctl işlemi çalışıyor: "+name,4);}
}
FileLock::~FileLock(){if(fd>=0)close(fd);}

std::string capture(const std::vector<std::string> &args, int timeoutMs) {
    if (args.empty() || (!args[0].starts_with("/usr/bin/")&&args[0]!="/usr/libexec/slayerctl-desktop")) throw Error("Geçersiz yardımcı program.");
    int pipes[2][2];
    if(pipe2(pipes[0],O_CLOEXEC))throw Error("Boru oluşturulamadı.");
    if(pipe2(pipes[1],O_CLOEXEC)){close(pipes[0][0]);close(pipes[0][1]);throw Error("Boru oluşturulamadı.");}
    // File actions must not alias fd 0/1/2 if the caller closed a standard fd.
    for(auto& pipe:pipes)for(auto& fd:pipe)if(fd<3){
        const int moved=fcntl(fd,F_DUPFD_CLOEXEC,3);
        if(moved<0){for(auto& pair:pipes)for(int item:pair)close(item);throw Error("Boru tanıtıcısı taşınamadı.");}
        close(fd);fd=moved;
    }
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    int result=posix_spawn_file_actions_init(&actions);
    if(result){for(auto& pipe:pipes)for(int fd:pipe)close(fd);throw Error("Süreç eylemleri oluşturulamadı.");}
    result=posix_spawnattr_init(&attributes);
    if(result){posix_spawn_file_actions_destroy(&actions);for(auto& pipe:pipes)for(int fd:pipe)close(fd);throw Error("Süreç öznitelikleri oluşturulamadı.");}
    result=posix_spawn_file_actions_adddup2(&actions,pipes[0][1],STDOUT_FILENO);
    if(!result)result=posix_spawn_file_actions_adddup2(&actions,pipes[1][1],STDERR_FILENO);
    for(auto& pipe:pipes)for(int fd:pipe)if(!result)result=posix_spawn_file_actions_addclose(&actions,fd);
    if(!result)result=posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);
    if(!result)result=posix_spawnattr_setpgroup(&attributes,0);
    std::vector<char*> argv;
    argv.reserve(args.size()+1);
    for(const auto& arg:args)argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    pid_t child=-1;
    if(!result)result=posix_spawn(&child,argv[0],&actions,&attributes,argv.data(),environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    for(auto& pipe:pipes)close(pipe[1]);
    if(result){for(auto& pipe:pipes)close(pipe[0]);throw Error("Süreç başlatılamadı: "+std::string(std::strerror(result)));}
    for(auto& pipe:pipes)fcntl(pipe[0],F_SETFL,O_NONBLOCK);
    std::string output[2];bool overflow=false,finished=false;int exitStatus=0;
    std::array<char,4096> buffer{};
    auto drain=[&]{for(unsigned i=0;i<2;++i){
        if(pipes[i][0]<0)continue;
        for(;;){
            auto n=read(pipes[i][0],buffer.data(),buffer.size());
            if(n<0&&errno==EINTR)continue;
            if(n==0){close(pipes[i][0]);pipes[i][0]=-1;break;}
            if(n<0)break;
            if(output[i].size()+static_cast<std::size_t>(n)>(i?8192u:65536u)){overflow=true;break;}
            output[i].append(buffer.data(),static_cast<std::size_t>(n));
        }
    }};
    auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeoutMs);
    while(std::chrono::steady_clock::now()<deadline&&!overflow){
        pollfd pfd[2]={{pipes[0][0],POLLIN,0},{pipes[1][0],POLLIN,0}};poll(pfd,2,20);drain();
        siginfo_t info{};
        if(waitid(P_PID,static_cast<id_t>(child),&info,WEXITED|WNOHANG|WNOWAIT)==0&&info.si_pid==child){
            finished=true;drain();break;
        }
    }
    // Keep the leader unreaped until the group is signalled: its PID cannot
    // be reused while we clean up descendants holding pipes or device handles.
    kill(-child,SIGKILL);
    while(waitpid(child,&exitStatus,0)<0&&errno==EINTR){}
    if(finished)drain();
    for(auto&pipe:pipes)if(pipe[0]>=0)close(pipe[0]);
    if(overflow)throw Error("Yardımcı program çıktı sınırını aştı.");
    if(!finished)throw Error("Yardımcı program zaman aşımına uğradı.");
    if(!WIFEXITED(exitStatus)||WEXITSTATUS(exitStatus)!=0)throw Error("Yardımcı program başarısız: "+trim(output[1]).substr(0,1024));
    return trim(output[0]);
}

[[noreturn]] void unverified(const std::string &name) {
    throw Error(name + ": bu BIOS/EC için yazma protokolü doğrulanmadı. Donanıma hiçbir değer yazılmadı. 'slayerctl capabilities' ayrıntıları gösterir.", 4);
}
}
