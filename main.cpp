#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
using pid_t = int;
#endif

using ll = long long;

namespace{
    volatile sig_atomic_t g_interrupted = 0;
    struct CommandRecord{
        int id = 0, exit_status = 0;
        std::string command, start_time;
        std::vector<pid_t> pids;
        ll duration_ms = 0;
        bool executed = false, builtin = false;
    };
    std::vector<CommandRecord> g_history;
    void on_sigint(int){
        g_interrupted = 1;
    }
    std::string trim(const std::string &text){
        const std::string whitespace = " \t\r\n";
        const std::size_t first = text.find_first_not_of(whitespace);
        if(first == std::string::npos) return "";
        const std::size_t last = text.find_last_not_of(whitespace);
        return text.substr(first, last - first + 1);
    }
    std::vector<std::string> split_whitespace(const std::string &text){
        std::istringstream input(text);
        std::vector<std::string> tokens;
        std::string token;
        while(input >> token) tokens.emplace_back(token);
        return tokens;
    }
    std::vector<std::string> split_pipeline(const std::string &command){
        std::vector<std::string> parts;
        std::string current;
        for(char ch : command){
            if(ch == '|') parts.emplace_back(trim(current)), current.clear();
            else current.push_back(ch);
        }
        parts.emplace_back(trim(current));
        return parts;
    }
    bool contains_unsupported_characters(const std::string &command){
        return command.find('\\') != std::string::npos || command.find('"') != std::string::npos ||
            command.find('\'') != std::string::npos;
    }
#ifdef _WIN32
    int change_directory(const char *path){
        return _chdir(path);
    }
#else
    int change_directory(const char *path){
        return chdir(path);
    }
#endif
    std::string current_time_string(){
        std::time_t now = std::time(nullptr);
        std::tm local_time{};
#ifdef _WIN32
        localtime_s(&local_time, &now);
#else
        localtime_r(&now, &local_time);
#endif
        std::ostringstream output;
        output << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
        return output.str();
    }
    void print_history(){
        for(const auto &record : g_history) std::cout << record.id << "  " << record.command << '\n';
    }
    std::string describe_status(int status){
#ifdef _WIN32
        return "exit=" + std::to_string(status);
#else
        if(WIFEXITED(status)) return "exit=" + std::to_string(WEXITSTATUS(status));
        if(WIFSIGNALED(status)) return "signal=" + std::to_string(WTERMSIG(status));
        return "status=" + std::to_string(status);
#endif
    }
#ifdef _WIN32
    int make_exit_status(int code){
        return code;
    }
#else
    int make_exit_status(int code){
        return (code & 0xff) << 8;
    }
#endif
    void print_exit_summary(){
        std::cout << "\nSimpleShell execution summary\n";
        std::cout << "-----------------------------\n";
        if(g_history.empty()){
            std::cout << "No commands were entered.\n";
            return;
        }
        for(const auto &record : g_history){
            std::cout << "[" << record.id << "] " << record.command << '\n';
            std::cout << "    started: " << record.start_time << '\n';
            if(record.builtin) std::cout << "    pid: shell-built-in\n";
            else if (record.pids.empty()) std::cout << "    pid: not launched\n";
            else{
                std::cout << "    pid:";
                for(pid_t pid : record.pids) std::cout << ' ' << pid;
                std::cout << '\n';
            }

            std::cout << "    duration: " << record.duration_ms << " ms\n";
            std::cout << "    result: " << describe_status(record.exit_status) << "\n";
        }
    }
    bool parse_command(const std::string &command, std::vector<std::vector<std::string>> &pipeline){
        pipeline.clear();

        if(contains_unsupported_characters(command)){
            std::cerr << "SimpleShell: quotes and backslashes are not supported\n";
            return false;
        }

        const std::vector<std::string> stages = split_pipeline(command);
        for(const std::string &stage : stages){
            if(stage.empty()){
                std::cerr << "SimpleShell: invalid empty command in pipeline\n";
                return false;
            }

            std::vector<std::string> args = split_whitespace(stage);
            if(args.empty()){
                std::cerr << "SimpleShell: invalid empty command\n";
                return false;
            }
            pipeline.emplace_back(args);
        }

        return true;
    }

    int wait_for_children(const std::vector<pid_t> &pids){
        int final_status = 0;

#ifdef _WIN32
        for(pid_t pid : pids){
            int status = 0;
            if(_cwait(&status, pid, 0) == -1){
                std::cerr << "SimpleShell: _cwait failed for " << pid << ": "
                        << std::strerror(errno) << '\n';
                break;
            }
            final_status = status;
        }
#else
        for(pid_t pid : pids){
            int status = 0;
            while(waitpid(pid, &status, 0) == -1){
                if(errno == EINTR) continue;
                std::cerr << "SimpleShell: waitpid failed for " << pid << ": "
                        << std::strerror(errno) << '\n';
                break;
            }
            final_status = status;
        }
#endif
        return final_status;
    }

    bool launch_pipeline(const std::vector<std::vector<std::string>> &pipeline, CommandRecord &record){
#ifdef _WIN32
        const std::size_t count = pipeline.size();
        if(count > 1)
        {
            std::cerr << "SimpleShell: pipeline execution is not supported on Windows\n";
            record.exit_status = make_exit_status(1);
            return false;
        }
        std::vector<char *> argv;
        argv.reserve(pipeline[0].size() + 1);
        for(const std::string &arg : pipeline[0]) argv.push_back(const_cast<char *>(arg.c_str()));
        argv.push_back(nullptr);
        pid_t pid = _spawnvp(_P_NOWAIT, argv[0], argv.data());
        if(pid == -1){
            std::perror("SimpleShell: spawn failed");
            record.exit_status = make_exit_status(1);
            return false;
        }
        record.pids.push_back(pid);
        record.exit_status = wait_for_children(record.pids);
        return true;
#else
        const std::size_t count = pipeline.size();
        std::vector<int> pipe_fds;
        pipe_fds.resize(count > 0 ? (count - 1) * 2 : 0, -1);
        for(std::size_t i = 0; i + 1 < count; i++){
            if(pipe(&pipe_fds[i * 2]) == -1){
                std::cerr << "SimpleShell: pipe failed: " << std::strerror(errno) << '\n';
                for(int fd : pipe_fds){
                    if(fd != -1) close(fd);
                }
                record.exit_status = make_exit_status(1);
                return false;
            }
        }
        for(std::size_t i = 0; i < count; i++){
            pid_t pid = fork();
            if(pid == -1){
                std::cerr << "SimpleShell: fork failed: " << std::strerror(errno) << '\n';
                for(int fd : pipe_fds){
                    if(fd != -1) close(fd);
                }
                record.exit_status = make_exit_status(1);
                return false;
            }
            if(pid == 0){
                std::signal(SIGINT, SIG_DFL);
                if(i > 0 && dup2(pipe_fds[(i - 1) * 2], STDIN_FILENO) == -1){
                    std::perror("SimpleShell: dup2 stdin");
                    _exit(126);
                }
                if(i + 1 < count && dup2(pipe_fds[i * 2 + 1], STDOUT_FILENO) == -1){
                    std::perror("SimpleShell: dup2 stdout");
                    _exit(126);
                }
                for(int fd : pipe_fds){
                    if(fd != -1) close(fd);
                }
                std::vector<char *> argv;
                argv.reserve(pipeline[i].size() + 1);
                for(const std::string &arg : pipeline[i]) argv.push_back(const_cast<char *>(arg.c_str()));
                argv.push_back(nullptr);

                execvp(argv[0], argv.data());
                std::perror("SimpleShell: execvp");
                _exit(127);
            }

            record.pids.push_back(pid);
        }

        for(int fd : pipe_fds){
            if(fd != -1) close(fd);
        }

        record.exit_status = wait_for_children(record.pids);
        return true;
#endif
    }
    CommandRecord &add_history_record(const std::string &command){
        CommandRecord record;
        record.id = static_cast<int>(g_history.size()) + 1, record.command = command;
        record.start_time = current_time_string(), g_history.push_back(record);
        return g_history.back();
    }
    bool run_builtin(const std::vector<std::vector<std::string>> &pipeline, CommandRecord &record){
        if(pipeline.size() != 1 || pipeline[0].empty()) return false;
        const std::string &command = pipeline[0][0];
        if(command == "history"){
            record.builtin = true, record.executed = true, record.exit_status = 0;
            print_history();
            return true;
        }
        if(command == "cd"){
            record.builtin = true, record.executed = true;

            const char *target = nullptr;
            target = (pipeline[0].size() > 1 ? pipeline[0][1].c_str() : target = std::getenv("HOME"));

            if(target == nullptr || change_directory(target) == -1){
                std::cerr << "SimpleShell: cd failed: " << std::strerror(errno) << '\n';
                record.exit_status = make_exit_status(1);
            }
            else record.exit_status = 0;
            return true;
        }
        if(command == "exit"){
            record.builtin = true, record.executed = true, record.exit_status = 0;
            g_interrupted = 1;
            return true;
        }
        return false;
    }
    void run_command(const std::string &raw_command){
        CommandRecord &record = add_history_record(raw_command);
        const auto start = std::chrono::steady_clock::now();
        std::vector<std::vector<std::string>> pipeline;
        if(!parse_command(raw_command, pipeline)) record.exit_status = make_exit_status(2);
        else if (!run_builtin(pipeline, record)) record.executed = launch_pipeline(pipeline, record);
        const auto end = std::chrono::steady_clock::now();
        record.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
}
int main()
{
#ifdef _WIN32
    if(std::signal(SIGINT, on_sigint) == SIG_ERR){
        std::cerr << "SimpleShell: failed to install SIGINT handler\n";
        return 1;
    }
#else
    struct sigaction action{};
    action.sa_handler = on_sigint;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if(sigaction(SIGINT, &action, nullptr) == -1){
        std::cerr << "SimpleShell: failed to install SIGINT handler: "
                << std::strerror(errno) << '\n';
        return 1;
    }
#endif

    while (!g_interrupted){
        std::cout << "simple-shell> " << std::flush;

        std::string input;
        if(!std::getline(std::cin, input)){
            if(g_interrupted) break;
            std::cout << '\n';
            break;
        }
        input = trim(input);
        if(input.empty()) continue;
        run_command(input);
    }
    print_exit_summary();
    return 0;
}