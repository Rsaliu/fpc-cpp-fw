# file_handler

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`file_handler` provides a **RAII filesystem abstraction** on top of whatever filesystem is mounted (typically SPIFFS on this project). It exposes a pure `IFileSystem` interface so that higher-level components (like `config_manager`) can be tested without a real filesystem.

The concrete `FileHandler` class accepts injectable `init_fn` and `deinit_fn` callbacks — making it easy to mount different filesystems (SPIFFS, LittleFS, FAT) by swapping those callbacks.

---

## Public API

### `IFileSystem` — pure interface

```cpp
class IFileSystem {
public:
    virtual Result<void>        write(const std::string& path, const std::string& data) = 0;
    virtual Result<std::string> read(const std::string& path)                           = 0;
    virtual Result<void>        append(const std::string& path, const std::string& data)= 0;
    virtual Result<void>        rename(const std::string& src, const std::string& dst)  = 0;
    virtual Result<void>        remove(const std::string& path)                         = 0;
    virtual Result<std::size_t> get_size(const std::string& path)                       = 0;
};
```

---

### `FileHandlerConfig`

```cpp
using FileInitFn   = std::function<Result<void>()>;
using FileDeinitFn = std::function<Result<void>()>;

struct FileHandlerConfig {
    FileInitFn   init_fn{};    // called by FileHandler::init()   (e.g. esp_vfs_spiffs_register)
    FileDeinitFn deinit_fn{};  // called by FileHandler::deinit() (e.g. esp_vfs_spiffs_unregister)
};
```

---

### `FileHandler`

```cpp
class FileHandler final : public IFileSystem {
public:
    explicit FileHandler(FileHandlerConfig config);
    ~FileHandler() override;  // calls deinit() if initialized

    Result<void> init();
    Result<void> deinit();
    bool         is_initialized() const noexcept;

    // IFileSystem overrides:
    Result<void>        write(const std::string& path, const std::string& data) override;
    Result<std::string> read(const std::string& path)                           override;
    Result<void>        append(const std::string& path, const std::string& data)override;
    Result<void>        rename(const std::string& src, const std::string& dst)  override;
    Result<void>        remove(const std::string& path)                         override;
    Result<std::size_t> get_size(const std::string& path)                       override;
};
```

---

## Example — SPIFFS-backed file handler

```cpp
FileHandlerConfig cfg{
    .init_fn = []() -> Result<void> {
        esp_vfs_spiffs_conf_t conf{
            .base_path = "/spiffs",
            .partition_label = nullptr,
            .max_files = 5,
            .format_if_mount_failed = true,
        };
        return (esp_vfs_spiffs_register(&conf) == ESP_OK)
               ? Result<void>::ok()
               : Result<void>::err(SystemError::Failed);
    },
    .deinit_fn = []() -> Result<void> {
        esp_vfs_spiffs_unregister(nullptr);
        return Result<void>::ok();
    },
};

FileHandler fs{cfg};
fs.init();

auto r = fs.read("/spiffs/config.json");
if (r.is_ok()) { /* use r.value() */ }
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| Direct `fopen`/`fread`/`fwrite` calls in `config_manager.c` | `IFileSystem` interface — testable, swappable |
| Hard-coded SPIFFS mount in application code | Injectable `init_fn` / `deinit_fn` callbacks |
| No RAII — filesystem unmounted manually | Destructor calls `deinit()` automatically |
