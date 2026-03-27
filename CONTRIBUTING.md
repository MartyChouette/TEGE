# Contributing to TEGE

Thank you for your interest in The Enjin Game Engine!

## How to Contribute

TEGE is currently in early open-source development. Here's how you can help:

### Bug Reports

Found a bug? Please [open an issue](https://github.com/MartyChouette/TEGE/issues/new?template=bug_report.yml) with:
- Steps to reproduce
- Expected vs actual behavior
- Crash report (`enjin_crash.txt`) if applicable
- System info (OS, GPU, driver version)

You can also use **Help > Report Bug** (Ctrl+Shift+B) inside the editor to submit bug reports directly with screenshots and system info attached automatically.

### Feature Requests

Have an idea? [Submit a feature request](https://github.com/MartyChouette/TEGE/issues/new?template=feature_request.yml). We especially welcome suggestions related to:
- Accessibility improvements
- New art style presets
- Editor workflow enhancements
- Scripting API additions

### Documentation

Improvements to documentation, tutorials, and examples are welcome. See the `docs/` directory for existing documentation.

## Development Setup

See [docs/BUILD.md](docs/BUILD.md) for build instructions.

```bash
git clone https://github.com/MartyChouette/TEGE.git
cd TEGE
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Code Style

- C++20, types: `u8, u16, u32, u64, f32, f64, usize`
- Namespaces: `Enjin::Core`, `Enjin::Renderer`, `Enjin::ECS`, etc.
- Logging: `ENJIN_LOG_INFO/WARN/ERROR/FATAL(Category, format, ...)`
- Member prefix: `m_`
- API export: `ENJIN_API` macro

## License

By contributing, you agree that your contributions will be licensed under the same [BSL 1.1](LICENSE) license that covers the project.

See [About](README.md#about) for the developer disclaimer.
