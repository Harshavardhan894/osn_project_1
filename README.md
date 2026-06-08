# Unix-Like Custom Shell in C

A custom Unix-like shell built in C that supports command execution, piping, redirection, background processes, signal handling, and job control.

## Features

* Command execution using `fork()` and `execvp()`
* Input and output redirection
* Multiple command piping
* Background process execution
* Process group based signal handling
* `Ctrl-C` and `Ctrl-Z` support
* Job control and activity tracking
* Custom `ping` signal command
* Command history support
* Arrow-key navigation for history
* Child process lifecycle management using `waitpid()`

---

## Tech Stack

* **Language:** C
* **OS Concepts:** Process Management, Signals, Pipes, Job Control
* **System Calls:** `fork`, `execvp`, `pipe`, `dup2`, `waitpid`, `kill`

---

## Project Structure

```bash
.
├── main.c
├── headers/
├── source/
├── Makefile
├── README.md
└── Mini Project 1 _ Operating Systems and Networks.pdf
```

---

## Build and Run

### Compile

```bash
make
```

### Run the Shell

```bash
./shell.out
```

---

## Supported Commands

The implemented commands and shell functionalities are documented in:

```bash
Mini Project 1 _ Operating Systems and Networks.pdf
```

Features include:

* Foreground and background execution
* Piping
* Redirection
* Job management
* Signal handling
* History navigation

---

## Example Usage

```bash
ls -l
```

```bash
cat file.txt | grep hello
```

```bash
sleep 10 &
```

```bash
ping <pid> <signal_number>
```

---

## Concepts Used

* Operating System Fundamentals
* Process Scheduling
* Inter-Process Communication (IPC)
* Signal Handling
* File Descriptor Management
* Terminal Control

---

