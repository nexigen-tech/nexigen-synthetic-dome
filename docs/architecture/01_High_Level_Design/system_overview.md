# System Overview: Общий ландшафт и контекст системы

## Контекст системы

```mermaid
C4Context
    Person(operator, "Оператор", "Пользователь, запускающий симуляции")
    System_Boundary(nxd, "NexDome HIL Platform") {
        System(host, "nxd-host", "Головной процесс: загрузка плагинов, управление временем")
        System(chronos, "Chronos", "Модуль синхронизации времени")
        System(composer, "Composer", "Модуль компоновки и валидации дескрипторов")
        System(transport, "Transport", "Модуль транспорта / iceoryx2")
        System(bb, "Blackboard", "Разделяемая шина данных")
        System(rec, "Recording", "Запись/воспроизведение (MCAP)")
        System(val, "Validation", "Валидация записанных данных")
        System(panel, "Scenario Panel", "Tauri + Svelte UI для сценариев")
    }
    System_Ext(sut, "SUT (System Under Test)", "Тестируемая система, подключённая через HIL")
    
    Rel(operator, host, "Запускает, останавливает")
    Rel(host, chronos, "Управляет временем")
    Rel(host, composer, "Загружает и валидирует схемы")
    Rel(host, transport, "Управляет портами")
    Rel(host, bb, "Обновляет и читает blackboard")
    Rel(host, rec, "Управляет записью")
    Rel(host, val, "Запускает валидацию")
    Rel(chronos, sut, "Аппаратное время (FPGA)")
    Rel(panel, host, "WebSocket / IPC")
```

## Поток данных

```mermaid
sequenceDiagram
    participant Op as Оператор
    participant Host as nxd-host
    participant Chronos as Chronos
    participant Composer as Composer
    participant BB as Blackboard
    participant Comp as Компоненты (C++/Rust)
    
    Op->>Host: Запуск сценария
    Host->>Composer: Загрузка и валидация контрактов
    Composer-->>Host: Дескрипторы портов
    Host->>Chronos: Инициализация времени
    Host->>Comp: Загрузка плагинов (dlopen)
    Comp->>BB: Подписка на порты (iceoryx2)
    
    Note over Host,Comp: Фаза инициализации завершена
    
    loop Каждый такт симуляции
        Chronos-->>Host: tick (frame_no, sim_time_ns)
        Host->>Comp: step_io() — чтение входов
        Comp->>BB: Чтение iceoryx2 samples
        Host->>Comp: step() — вычисления
        Comp->>BB: Запись iceoryx2 samples
        Host->>Chronos: Подтверждение такта
    end
    
    Op->>Host: Остановка
    Host->>Comp: stop() — выгрузка плагинов
```

## Границы и зоны ответственности

| Зона | C++ | Rust |
|------|-----|------|
| **Runtime** | chronos, composer, transport, blackboard, recording, validation | `nxd-runtime-chronos` (Rust-обёртка над C API) |
| **SDK** | `sdk/cpp/` — C++ wrapper над cbindgen ABI | `nxd-abi` + `nxd-sdk` — безопасная обёртка |
| **Инструменты** | `nexigen-run`, `video-viewer` | `nxdc` (компилятор контрактов) |
| **Плагины** | Примеры: counter, virtual_can, video_opengl | Примеры: counter, scenario_control |
| **UI** | — | scenario-panel (Tauri + Svelte) |

## Риски и ограничения

| Риск | Описание | Митигация |
|------|----------|-----------|
| ABI-несовместимость | Плагин собран с другой версией SDK | Версионирование ABI через `abi_major`/`abi_minor` |
| Паника в Rust-плагине | `panic=abort` убивает весь процесс | Все Rust-компоненты используют `panic=abort` |
| Iceoryx2 deadlock | Неправильный порядок loan/write | Все публикации — через dedicated publisher per port |
| Layout mismatch | C++ struct vs `#[repr(C)]` Rust struct | `nxd_contracts` проверяет `sizeof()` на этапе сборки |