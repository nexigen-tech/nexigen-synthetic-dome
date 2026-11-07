# Tasks (status: implementation complete)

## 1. CAN_generator — C++ executable

- [x] 1.1 C++ executable `nxd_can_generator` using `IceoryxTransport` (CMake target)
- [x] 1.2 `--can-bus <name>` CLI argument, default `"CAN_DEMO"`
- [x] 1.3 1 ms publish loop with 6-frame rotating schedule (CAN IDs 0x100–0x500)
- [x] 1.4 SIGINT handler — graceful shutdown with exit code 0
- [x] 1.5 Build: `cmake --build build --target nxd_can_generator -j$(nproc)`
- [x] 1.6 Run: `./build/nxd_can_generator --can-bus "MY_BUS"`

## 2. CAN_monitor GUI — Rust с egui

- [x] 2.1 CAN_monitor Rust crate (egui, nxd-abi)
- [x] 2.2 Egui-окно: поле ввода bus name, индикатор статуса (цветной круг), таблица 10 фреймов
- [x] 2.3 Подписка на iceoryx2 через `nxd_can_subscriber_helper` (C++ child process, stdout pipe)
- [x] 2.4 10-элементный буфер + mpsc-канал для приёма фреймов
- [x] 2.5 Таблица: CAN ID hex, DLC, payload hex, direction, kind
- [x] 2.6 Bus name hot-swap: Enter → restart subscriber, clear buffer
- [x] 2.7 Статус: Receiving (зелёный), Subscribing... (серый), текст ошибки (красный)
- [x] 2.8 Crate в workspace `Cargo.toml`
- [x] 2.9 Build: `cargo build -p nxd-example-can-monitor-gui`

**Примечание:** GUI использует egui (не Slint) — Slint требует `libfontconfig` system dep, недоступную в этой среде.

## 3. Интеграционные тесты — все пройдены ✅

- [x] 3.1 **Custom bus name** — generator + helper на `CUSTOM_BUS`: 4,388,076 байт (11,194 фреймов) получено за 3 сек → ✅ PASS
- [x] 3.2 **Non-existent bus** — helper на `NONEXIST_BUS`: 0 байт получено → ✅ PASS
- [x] 3.3 **Isolated buses** — generator + helper на `CUSTOM_BUS`: данные получены → ✅ PASS
- [x] 3.4 **SIGINT graceful shutdown** — generator exit code 0, "total sent: 12000" → ✅ PASS
- [x] 3.5 **Restart after stop** — generator restarted на `TEST_RESTART`: 4,348,872 байт получено → ✅ PASS

## ⚡ Как запустить

```bash
# Терминал 1 — CAN генератор
./build/nxd_can_generator --can-bus CAN_DEMO

# Терминал 2 — CAN монитор (GUI)
cargo run -p nxd-example-can-monitor-gui
```

## Созданные файлы

```
📁 examples/components/
│   cpp/can_generator_main.cpp                — CAN_generator executable
│   cpp/can_subscriber_helper.cpp             — Subscriber helper (stdout pipe)
│   rust/can_monitor_gui/src/main.rs          — CAN_monitor Rust egui GUI
│   rust/can_monitor_gui/Cargo.toml           — Crate config
📁 runtime/demo_transport/
│   xdemo_shim.cpp / xdemo_shim.h             — C++ static lib for Rust FFI (резерв)
│   CMakeLists.txt                            — Build config
📁 CMakeLists.txt (root)                      — Добавлены nxd_can_generator,
│                                               nxd_can_subscriber_helper, xdemo_shim
```
