# 🤝 Contributing to ZenithShell

Thank you for your interest in contributing to **ZenithShell**! We welcome contributions, bug reports, theme submissions, and feature enhancements.

---

## 🛠️ Development Setup

1. **Fork and Clone the Repository**:
   ```bash
   git clone https://github.com/<your-username>/zenithshell.git
   cd zenithshell
   ```

2. **Install Build Dependencies**:
   Refer to [`require_package.md`](require_package.md) for your distribution's packages.

3. **Compile in Debug Mode**:
   ```bash
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
   ninja -C build
   ```

4. **Test Run**:
   ```bash
   ./build/zenithshell
   ```

---

## 🎨 Adding New Themes

New color themes can be added without modifying C++ code:
1. Create a new `.toml` file in `~/.config/zenithshell/themes/<theme-name>.toml`.
2. Follow the 4-pillar design guidelines in [`README.md`](README.md):
   - Keep surfaces dark and neutral (`#0B0C12` / `#11121A`).
   - Use high-contrast text ($L \ge 0.94$, WCAG AAA).
   - Only interaction elements (active workspace pill, sliders, selection rows) adopt the accent color.

---

## 📋 Pull Request Guidelines

- Ensure your code follows modern **C++20** standards.
- Run `ninja -C build` to verify there are 0 warnings or compiler errors.
- Keep commits descriptive and follow conventional commit formats (`feat:`, `fix:`, `docs:`, `chore:`).
