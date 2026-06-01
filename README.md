# SimpleShell

SimpleShell is a compact Unix-style command interpreter written in C++. It reads commands, launches real Unix programs, supports pipelines, tracks command history, and prints an execution report when the shell exits.

This uses the classic operating-system building blocks: `fork`, `execvp`, `pipe`, `dup2`, and `waitpid`.

## What It Does

- shows an interactive prompt,
- parses commands and arguments separated by whitespace,
- executes external Unix commands through the system `PATH`,
- supports multi-command pipelines such as `cat file.txt | grep hello | wc -l`,
- keeps a session-local command history,
- supports built-ins for `history`, `cd`, and `exit`,
- handles Ctrl-C as shell termination,
- prints PID, start time, duration, and exit status for every command entered.

## Project Layout

```text
simpleshell_cpp/
|-- main.cpp      # Shell implementation
|-- Makefile      # Build/run/clean targets
`-- README.md     # Project overview
```

## Build

Use a Linux environment or WSL with a Linux distribution installed.

```sh
make
```

This produces:

```text
simple-shell
```

## Run

```sh
./simple-shell
```

You will see a prompt:

```text
simple-shell>
```

## Supported Command Styles

SimpleShell can run ordinary Unix commands available through `PATH`, including:

```sh
ls
ls /home
echo hello world
wc -l fib.c
wc -c fib.c
grep printf helloworld.c
ls -R
ls -l
./fib 40
./helloworld
sort fib.c
uniq file.txt
cat fib.c | wc -l
cat helloworld.c | grep print | wc -l
```

## Built-ins

| Command | Purpose |
| --- | --- |
| `history` | Show commands entered during the current shell session. |
| `cd [dir]` | Change the working directory of the shell process. |
| `exit` | Exit cleanly and print the execution summary. |

## Input Rules

The assignment keeps command syntax intentionally simple. This implementation follows that model:

- commands and arguments are separated by whitespace,
- pipeline stages are separated by `|`,
- quotes are not supported,
- backslashes are not supported,
- shell expansion is not performed.

## Known Limitations

SimpleShell intentionally leaves these advanced shell features:

- quoted strings and escaping,
- redirection with `>`, `<`, `2>`,
- background jobs with `&`,
- command substitution with `$(...)`,
- environment expansion such as `$HOME`,
- wildcard expansion such as `*.cpp`,
- aliases, shell functions, and job control.

## Clean Build Artifacts

```sh
make clean
```
