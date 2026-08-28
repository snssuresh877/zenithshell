# 🛡️ Security Policy

The ZenithShell team takes the security and safety of our software and users seriously.

---

## 🔒 Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x:                |

---

## 🛡️ Built-in Security Mitigations

ZenithShell is compiled and linked with the following hardening protections:
- **Stack Smashing Protection (`-fstack-protector-strong`)**: Buffer overflow detection on call frames.
- **Position Independent Executable (`-fPIE` / `-pie`)**: Full Address Space Layout Randomization (ASLR).
- **Full RELRO & BIND_NOW (`-Wl,-z,relro,-z,now`)**: Protects the Global Offset Table (GOT) from overwrite exploits.
- **Non-Executable Stack (`-Wl,-z,noexecstack`)**: Prevents arbitrary shellcode execution in stack memory.
- **Source Fortification (`-D_FORTIFY_SOURCE=2`)**: Runtime checks on memory and string operations.
- **Zero Polling & Sandboxed IPC**: Hyprland communication occurs strictly over local UNIX domain sockets without exposed TCP/UDP network ports.

---

## 🐛 Reporting a Vulnerability

If you discover a potential security vulnerability within ZenithShell, please do **not** open a public issue.

Instead, please report security vulnerabilities via **GitHub Private Security Advisories**:
1. Navigate to the [Security Advisories tab](https://github.com/snssuresh877/zenithshell/security/advisories).
2. Click **"Report a vulnerability"** to submit your report confidentially.

We will review your submission and release a patch promptly.
