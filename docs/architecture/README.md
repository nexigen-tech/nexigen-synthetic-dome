# NexDome: Архитектурная документация

## Концепция

**NexDome** — высокоточная среда синтетического окружения (Synthetic Environment) и аппаратно-цифрового моделирования (Hardware-in-the-Loop, HIL). Платформа обеспечивает нуль-копийный обмен данными между runtime-модулями и компонентами плагинов (C++ и Rust), исполняемыми в едином адресном пространстве.

## Стек технологий

| Технология | Назначение |
|---|---|
| C++23 (`-fno-exceptions -fno-rtti`) | Runtime-ядро, хост, модули |
| Rust (edition 2021, panic=abort) | Инструмент сборки контрактов, ABI, SDK |
| Eclipse iceoryx2 (v0.10.0) | Zero-copy IPC |
| cbindgen | Генерация C-заголовков из Rust |
| YAML → nxdc → C++/Rust | Конвейер генерации кода |

## Навигационная карта

| Путь | Содержание |
|---|---|
| `README.md` | Концепция, стек, навигация (текущий документ) |
| `01_High_Level_Design/system_overview.md` | Ландшафт системы, границы контекста, потоки данных |
| `01_High_Level_Design/component_interop.md` | Спецификация C++/Rust FFI, жизненный цикл плагина, ABI |
| `02_Subsystems/cpp_modules.md` | 6 runtime-модулей: chronos, composer, transport, blackboard, recording, validation |
| `02_Subsystems/rust_crates.md` | 4 Rust-крейта: nxdc, nxd-abi, nxd-sdk, nxd-runtime-chronos |
| `03_Cross_Cutting/memory_management.md` | RAII vs Borrow Checker, аллокация на границе FFI |
| `03_Cross_Cutting/error_handling.md` | Маппинг `Result<T,E>` ↔ коды возврата, обработка паник |

## Ключевые свойства архитектуры

1. **Единое адресное пространство**: хост и плагины загружаются в один процесс через `dlopen`. Все порты — разделяемая память iceoryx2.
2. **Два SDK, один ABI**: C++ и Rust SDK разделяют единую cbindgen-генерируемую C ABI.
3. **Zero-copy на всём пути**: iceoryx2-loaned sample передаётся от writer к reader без копирования.
4. **Холодный/горячий пути**: аллокация и блокировки разрешены только до `arm()`; после `arm()` — детерминированный, без аллокаций.
5. **Контракты как единый источник истины**: все wire-форматы описаны в YAML, из них nxdc генерирует C++ и Rust типы с идентичным layout.