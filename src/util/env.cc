#include "env.h"

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <cstring>
#include <sys/stat.h>
#include <sys/time.h>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <fstream>

#include "threadpool_imp.h"

namespace util {

Status PosixError(const std::string& context, int error_number) {
if (error_number == ENOENT) {
    return Status::NotFound(context, std::strerror(error_number));
} else {
    return Status::IOError(context, std::strerror(error_number));
}
}

Env* Env::Default() {
    // The following function call initializes the singletons of ThreadLocalPtr
    // right before the static default_env.  This guarantees default_env will
    // always being destructed before the ThreadLocalPtr singletons get
    // destructed as C++ guarantees that the destructions of static variables
    // is in the reverse order of their constructions.
    //
    // Since static members are destructed in the reverse order
    // of their construction, having this call here guarantees that
    // the destructor of static PosixEnv will go first, then the
    // the singletons of ThreadLocalPtr.
    static Env default_env;
    return &default_env;
}


Env::Env():
    thread_pools_(Priority::TOTAL),
    next_id_(1) {
    ThreadPoolImpl::PthreadCall("mutex_init", pthread_mutex_init(&mu_, nullptr));
    for (int pool_id = 0; pool_id < Env::Priority::TOTAL; ++pool_id) {
        thread_pools_[pool_id].SetThreadPriority(
            static_cast<Env::Priority>(pool_id));
        // This allows later initializing the thread-local-env of each thread.
        thread_pools_[pool_id].SetHostEnv(this);
    }
    // IncBackgroundThreadsIfNeeded(0 , Env::Priority::BOTTOM);
    // IncBackgroundThreadsIfNeeded(1 , Env::Priority::LOW);
    // IncBackgroundThreadsIfNeeded(2 , Env::Priority::HIGH);
    std::string logfile_name = "log" + std::to_string(NowMicros()/1000000) + ".log";
    NewLogger(logfile_name, &logger_);
}

Env::~Env() {
    for (const auto tid : threads_to_join_) {
    pthread_join(tid, nullptr);
    }
    for (int pool_id = 0; pool_id < Env::Priority::TOTAL; ++pool_id) {
    thread_pools_[pool_id].JoinAllThreads();
    }
    delete logger_;
}

std::string Env::Execute(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void Env::PinCore(int i) {
    // ------------------- pin current thread to core i -------------------
    // printf("Pin thread: %2d.\n", i);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(i, &cpuset);
    pthread_t thread;
    thread = pthread_self();
    int rc = pthread_setaffinity_np(thread,
                                    sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        fprintf(stderr,"Error calling pthread_setaffinity_np: %d \n", rc);
    }
}
bool Env::FileExists(const std::string& filename) {
    return ::access(filename.c_str(), F_OK) == 0;
}

bool Env::FileValid(int fd) {
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

Status Env::GetChildren(const std::string& directory_path,
                    std::vector<std::string>* result) {
    result->clear();
    ::DIR* dir = ::opendir(directory_path.c_str());
    if (dir == nullptr) {
        return PosixError(directory_path, errno);
    }
    struct ::dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        result->emplace_back(entry->d_name);
    }
    ::closedir(dir);
    return Status::OK();
}


Status Env::DeleteFile(const std::string& filename) {
    if (::unlink(filename.c_str()) != 0) {
        return PosixError(filename, errno);
    }
    return Status::OK();
}

Status Env::CreateDir(const std::string& dirname) {
    if (::mkdir(dirname.c_str(), 0755) != 0) {
        return PosixError(dirname, errno);
    }
    return Status::OK();
}


Status Env::CreateFile(const std::string& filename, size_t size) {
    int flag = O_RDWR | O_DIRECT | O_CREAT;
    int fd = open(filename.c_str(), flag  , 0666);
    if (fd < 0) {
        ERROR("Create %s fail. %s", filename.c_str(), strerror(-fd));
        return Status::IOError("Create file fail");
    }
    int res = ftruncate(fd, size);
    if (res < 0) {
        ERROR("Set size %ld fail", size);
        return Status::IOError("Cannot set size");
    }
    fdatasync(fd);
    close(fd);
    return Status::OK();
}


Status Env::DeleteDir(const std::string& dirname) {
    if (::rmdir(dirname.c_str()) != 0) {
        return PosixError(dirname, errno);
    }
    return Status::OK();
}

Status Env::GetFileSize(const std::string& filename, int64_t* size) {
    struct ::stat file_stat;
    if (::stat(filename.c_str(), &file_stat) != 0) {
        *size = 0;
        return PosixError(filename, errno);
    }
    *size = file_stat.st_size;
    return Status::OK();
}

Status Env::RenameFile(const std::string& from, const std::string& to) {
    if (std::rename(from.c_str(), to.c_str()) != 0) {
        return PosixError(from, errno);
    }
    return Status::OK();
}

// Returns the number of micro-seconds since some fixed point in time. Only
// useful for computing deltas of time.
// uint64_t Env::NowMicros() {
//     static constexpr uint64_t kUsecondsPerSecond = 1000000;
//     struct ::timeval tv;
//     ::gettimeofday(&tv, nullptr);
//     return static_cast<uint64_t>(tv.tv_sec) * kUsecondsPerSecond + tv.tv_usec;
// }

// Returns the number of nano-seconds since some fixed point in time. Only
// useful for computing deltas of time in one run.
// Default implementation simply relies on NowMicros.
// In platform-specific implementations, NowNanos() should return time points
// that are MONOTONIC.


// Converts seconds-since-Jan-01-1970 to a printable string
std::string Env::TimeToString(uint64_t secondsSince1970) {
    const time_t seconds = (time_t)secondsSince1970;
    struct tm t;
    int maxsize = 64;
    std::string dummy;
    dummy.reserve(maxsize);
    dummy.resize(maxsize);
    char* p = &dummy[0];
    localtime_r(&seconds, &t);
    snprintf(p, maxsize,
            "%04d/%02d/%02d-%02d:%02d:%02d ",
            t.tm_year + 1900,
            t.tm_mon + 1,
            t.tm_mday,
            t.tm_hour,
            t.tm_min,
            t.tm_sec);
    return dummy;
}

// Sleep/delay the thread for the prescribed number of micro-seconds.
void Env::SleepForMicroseconds(int micros) {
    ::usleep(micros);
}

uint64_t Env::GetThreadID() const {
std::hash<std::thread::id> hasher;
return hasher(std::this_thread::get_id());
}


void PthreadCall(const char* label, int result) {
if (result != 0) {
    fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
    abort();
}
}


struct StartThreadState {
void (*user_function)(void*);
void* arg;
};


static void* StartThreadWrapper(void* arg) {
StartThreadState* state = reinterpret_cast<StartThreadState*>(arg);
state->user_function(state->arg);
delete state;
return nullptr;
}


void Env::Schedule(void (*function)(void* arg1), void* arg, Priority pri,
                        void* tag, void (*unschedFunction)(void* arg)) {
assert(pri >= Priority::BOTTOM && pri <= Priority::HIGH);
thread_pools_[pri].Schedule(function, arg, tag, unschedFunction);
}


int Env::UnSchedule(void* arg, Priority pri) {
return thread_pools_[pri].UnSchedule(arg);
}

unsigned int Env::GetThreadPoolQueueLen(Priority pri) const {
assert(pri >= Priority::BOTTOM && pri <= Priority::HIGH);
return thread_pools_[pri].GetQueueLen();
}


void Env::WaitAllJobs() {
    while(GetThreadPoolQueueLen(Priority::HIGH) > 0) {
        SleepForMicroseconds(1000);
    }
    while(GetThreadPoolQueueLen(Priority::LOW) > 0) {
        SleepForMicroseconds(1000);
    }
    while(GetThreadPoolQueueLen(Priority::BOTTOM) > 0) {
        SleepForMicroseconds(1000);
    }
}

void Env::WaitAllJobs(Priority pri) {
    while(GetThreadPoolQueueLen(pri) > 0) {
        SleepForMicroseconds(1000);
    }
}

void Env::StartThread(void (*function)(void* arg), void* arg) {
pthread_t t;
StartThreadState* state = new StartThreadState;
state->user_function = function;
state->arg = arg;
PthreadCall(
    "start thread", pthread_create(&t, nullptr, &StartThreadWrapper, state));
PthreadCall("lock", pthread_mutex_lock(&mu_));
threads_to_join_.push_back(t);
PthreadCall("unlock", pthread_mutex_unlock(&mu_));
}

void Env::WaitForJoin() {
for (const auto tid : threads_to_join_) {
    pthread_join(tid, nullptr);
}
threads_to_join_.clear();
}


Status Env::NewLogger(const std::string& filename, PosixLogger** result) {
    std::FILE* fp = std::fopen(filename.c_str(), "w");
    if (fp == nullptr) {
        *result = nullptr;
        return PosixError(filename, errno);
    } else {
        *result = new PosixLogger(fp);
        return Status::OK();
    }
}


// Allow increasing the number of worker threads.
void Env::IncBackgroundThreadsIfNeeded(int num, Priority pri) {
    assert(pri >= Priority::BOTTOM && pri <= Priority::HIGH);
    thread_pools_[pri].IncBackgroundThreadsIfNeeded(num);
}

// Allow increasing the number of worker threads.
void Env::SetBackgroundThreads(int num, Priority pri) {
    assert(pri >= Priority::BOTTOM && pri <= Priority::HIGH);
    thread_pools_[pri].SetBackgroundThreads(num);
}

void Log(PosixLogger* info_log, const char* format, ...) {
    if (info_log != nullptr) {
        va_list ap;
        va_start(ap, format);
        info_log->Logv(format, ap);
        va_end(ap);
    }
}

// void INFO(const char* format, ...) {
//     char buffer[2048] = "[info] ";
//     va_list args;
//     va_start(args, format);
//     sprintf(buffer + strlen(buffer), "[%s] %s:%d ", __FILENAME__, __FUNCTION__, __LINE__);
//     sprintf(buffer + strlen(buffer), format, args);
//     util::Log(Env::Default()->LOG(), "%s", buffer);
//     va_end(args);
// }

// void DEBUG(const char* format, ...) {
//     #ifndef NDEBUG
//     char buffer[2048] = "[debug] ";
//     va_list args;
//     va_start(args, format);
//     sprintf(buffer + strlen(buffer), "[%s] %s:%d ", __FILENAME__, __FUNCTION__, __LINE__);
//     sprintf(buffer + strlen(buffer), format, args);
//     util::Log(Env::Default()->LOG(), "%s", buffer);
//     va_end(args);
//     #endif
// }

// void ERROR(const char* format, ...) {
//     char buffer[2048] = "[error] ";
//     va_list args;
//     va_start(args, format);
//     sprintf(buffer + strlen(buffer), "[%s] %s:%d ", __FILENAME__, __FUNCTION__, __LINE__);
//     sprintf(buffer + strlen(buffer), format, args);
//     util::Log(Env::Default()->LOG(), "%s", buffer);
//     va_end(args);
// }

std::string Env::PriorityToString(Env::Priority priority) {
switch (priority) {
    case Env::Priority::BOTTOM:
    return "Bottom";
    case Env::Priority::LOW:
    return "Low";
    case Env::Priority::HIGH:
    return "High";
    case Env::Priority::USER:
    return "User";
    case Env::Priority::TOTAL:
    assert(false);
}
return "Invalid";
}

}