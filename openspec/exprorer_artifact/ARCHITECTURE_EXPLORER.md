# Архитектурное резюме Nexigen Synthetic Dome

Дата: 2026-11-05  
Статус: итог `/opsx-explore`, не утверждённая спецификация.

## 1. Назначение

Nexigen Synthetic Dome (NexDome) — конструктор высокопроизводительных
моделирующих комплексов HIL/SIL с повторным использованием компонентов,
контрактов, гейтвеев, сценариев и панелей.

Целевая платформа: **Ubuntu 26.04+**. QNX и Astra Linux исключены из текущих
требований. Базовый simulation cycle v1 — **1 мс**. Класс 100 мкс сохраняется
как архитектурная возможность, но не является обязательным объёмом v1.

## 2. Слои

```text
 +--------------------------------------------------------------+
 | Tooling: domectl | scenarios | record/replay | Qt/Tauri      |
 +--------------------------------------------------------------+
 | Composer: manifests -> validation -> Dome Plan -> nexigen-run|
 +--------------------------------------------------------------+
 | Library: models | CANopen | gateways | virtual twins | video |
 +--------------------------------------------------------------+
 | Contracts: IDL -> C++/Rust POD, descriptors, layout_hash     |
 +--------------------------------------------------------------+
 | NXD runtime: components | Chronos | LET | ports | Blackboard |
 +--------------------------------------------------------------+
 | nexigen-cpp: app | runner | hub | logging | metrics | tracing|
 |              chrono | diagnostics | core::ThreadSpec          |
 +--------------------------------------------------------------+
 | iceoryx2 data plane + Linux/Ubuntu + PCIe/FPGA gateways      |
 +--------------------------------------------------------------+
```

Граница: nexigen-cpp не знает о симуляции; NXD не дублирует process
supervision, telemetry settings и Infrastructure.

## 3. Компонентная модель

- Обычно один component = один процесс = один partition thread.
- Component поставляется как plugin для `nxd-host`; C ABI не допускает C++
  exceptions или Rust panics через границу.
- C++ components используют C++23 SDK; Rust components — `cdylib` и тот же ABI.
- `initialize/arm/step/stop`; аллокации только до `Armed`.
- Component видит typed inputs/outputs, `FrameContext`, RNG и telemetry handles.
- Component не видит физические каналы, CPU, iceoryx2 endpoints, Qt/Tauri,
  wall-clock или process supervisor.
- Optional `save_state/load_state` для replay checkpoints.

### Source mux

На semantic ports поддерживаются источники:

```text
 Model | Manual | File | Generator | Replay (и Upstream для input mux)
```

Каждое semantic field содержит `value + quality + origin`. Operator commands
получают `apply_frame_no` и записываются как внешние входы.

## 4. Время и исполнение

Строгие типы времени:

- `SimTime` — целочисленная ось симуляции;
- `WallTime` — `CLOCK_MONOTONIC_RAW` хоста;
- `HwFpgaTime` — общая аппаратная шкала **HW_FPGA_TIME_BASE**, нс.

В HIL `SimTime` совпадает со сдвинутой `HwFpgaTime`; в SIL ось виртуальная.
`nexigen::chrono` остаётся сервисом хостовых часов и не заменяет `nxd::chronos`.

| Режим | Release rule | Аппаратные gateways |
|---|---|---|
| RT | wall/HW trigger | разрешены |
| Scaled | wall time / speed | запрещены |
| Free | после ack предыдущего кадра | запрещены |
| Step | команда `step N` / `run-until` | запрещены |

Основные правила:

- Trigger — событие с timestamp: HW timer, digital input, video frame,
  wall/virtual timer или recorded trigger.
- Модель получает `frame_no`, `sim_time_ns`, `dt_ns` и seeded RNG.
- LET: данные кадра k видны потребителю с k+1; каждое sample имеет `frame_no`.
- `Direct` edges допускаются только внутри partition; алгебраические циклы
  требуют `Delayed`.
- Input acceptance window D (по умолчанию один base frame) отделяет HW event
  time от host delivery jitter; фактический frame assignment записывается.
- TX gateway сбрасывает outputs на следующем HW tick; при отсутствии результата
  применяется `repeat_last | skip | fault`.
- `tx_stamp` используется для фактического jitter measurement.
- Scheduled TX в контракте предусмотрен, но текущее железо его не требует.

Побитовый replay гарантируется только для того же binary и класса CPU. Для
кросс-машинного replay нужен portable FP profile без host-specific FMA changes.

## 5. Data plane и источники

iceoryx2 — единый IPC data plane. `Local` и `Ipc` рассматриваются как deployment
variants; SPSC nexigen используется для driver/runtime handoff и recording,
а не как замена портам.

Типы связей:

| NXD port | iceoryx2 |
|---|---|
| Signal, последнее значение | pub/sub history=1 или подходящая config |
| Event/Stream | bounded pub/sub queue + loan |
| Command | request/response |
| Wake/Tick | собственные TimePage/futex/spin primitives; event API по результатам spike |
| Slow shared state | Blackboard |

### Логические источники

Примеры: `CAN_A`, `CAN_B`, `Camera_Front_Left`, `Camera_Front_Right`.

- `dome.yaml` содержит logical source names и component bindings.
- `profiles/sil.yaml` и `profiles/hil.yaml` связывают имена с virtual/HW twins.
- В samples передаётся numeric `source_id`; строка остаётся в source catalog,
  Blackboard, recording, metrics и panels.
- Endpoint names не зависят от профиля.
- Один monitor не должен случайно получить данные другого logical source.

## 6. Контракты

Один IDL порождает C++ и Rust types, field descriptors, canonical schema,
`layout_hash`, cold JSON/`to_string`, panel metadata и MCAP schema.

Требования: fixed-width primitives, fixed arrays, explicit padding, trivially
copyable/destructible, no pointers/heap/STL strings на hot path. Поддерживаются
cross-language size/align/offset assertions и golden binary vectors.

### Semantic signals

Сигналы группируются в typed aggregates на logical port, а не публикуются как
отдельный service на каждый scalar.

### CAN

`RawCanFrame` содержит:

- обязательный `hw_stamp` на каждый принятый кадр;
- `dir = Rx | SelfRx`;
- `kind = Data | Remote | Error | Heartbeat`;
- `status = Valid | Degraded | Invalid`;
- Classical/FD/XL protocol, ID, raw DLC, payload length и protocol metadata;
- aggregate error flags и vendor error code.

Используются bounded batch variants для 8/64/2048-byte payloads. CANopen
использует только применимые protocol/frame kinds и по умолчанию игнорирует
heartbeat. Пользовательские activity models могут получать heartbeat.

### Video

Единый `VideoFrame` поддерживает Raw, JPEG, H.264, H.265, AV1, H.266/VVC,
`DeviceRaw` и расширения.

`VideoStreamDescriptor` описывает coding, pixel format, dimensions, rational
FPS, planes, codec config, max payload и vendor DeviceRaw metadata. Stream
revision/hash отделены от binary contract `layout_hash`.

`VideoFrame` содержит:

- `kind = Data | Heartbeat`;
- `status = Valid | Degraded | Invalid`;
- stream revision, content frame index, payload size;
- aggregate error flags и vendor error code;
- bounded payload и plane metadata для известных raw formats.

`DeviceRaw` остаётся частью `VideoCodec`: vendor driver гарантирует, что opaque
DMA bytes относятся к одному видеокадру. Детальные lost-line regions и отдельный
`RawDmaFrame` исключены по решению пользователя. Достаточно `MissingLines` и
других aggregate flags.

## 7. Heartbeat и health

Heartbeat имеет собственный `formed_hw_stamp`. Transport sequence растёт на
каждый sample, а content/data index — только на Data.

Модель может использовать:

```text
 [last_data.hw_stamp, heartbeat.hw_stamp) — интервал отсутствия активности
```

Heartbeat доступен через `port.activity()`; обычные protocol/data views его
фильтруют. `ReceiverHealth` различает `Starting | Active | NoSignal | Lost |
Fault`. Heartbeat сохраняется в recording. Точная `ActivitySemantics` остаётся
открытым вопросом: valid data, any RX frames или physical activity.

## 8. Инфраструктура и потоки

Используются готовые:

- `app::Infrastructure` и per-subsystem settings;
- `runner::Runner`, `SuiteManifest`, restart/impact policies;
- `RunIdentity/TelemetryPaths` и `run_id/run_dir`;
- `nexigen-hub`;
- metrics, tracing, diagnostics;
- `core::CpuSet`, `ThreadSpec`, `ThreadApplyReport`, `WorkerTuning`.

CPU pools: `os | irq_rt | rt | infra | panel`. Main thread закрепляется за infra
до запуска Infrastructure, поэтому SignalWatcher наследует non-RT placement.
Workers получают generated `worker_thread`; partition/RX/TX получают собственные
ThreadSpec. Общий infrastructure worker не нужен в v1.

Процессный cpuset вычисляется Composer как объединение назначенных CPU. Unknown
thread roles, priority errors и exclusive CPU conflicts отклоняются до запуска.
`Required` не допускает silent non-RT fallback.

## 9. Panels

- Generic operator panels: Qt, отдельный процесс.
- Video receiver: Qt/OpenGL; RX thread учитывает все кадры, GUI отображает
  последний доступный.
- Tauri + Svelte: панели сценариев и signals; Rust backend использует iceoryx2,
  frontend получает decimated snapshots.
- FullHD raw video через Tauri IPC не передаётся.
- Любая панель может быть закрыта без изменения результатов моделей.

## 10. Blackboard

Blackboard предназначен для `dome/info`, `source/*`, `scenario/state`,
`video*/config` и медленного health/config state.

Не используется для TimePage, high-rate signals, CAN/video data, command queues
и ordered frame release. Каждая entry имеет одного writer, revision, versioned
POD value и `apply_frame_no`. Изменения дополнительно публикуются как
`BlackboardUpdate` для recording/replay.

## 11. Запись и replay

- `run_id/run_dir` — идентичность сессии.
- JSONL telemetry остаётся в nexigen layout.
- MCAP — высокоскоростная simulation recording.
- Сохраняются boundary data, raw CAN, commands, Blackboard updates, heartbeat,
  parameters, XDD, plan, schemas, versions и HostCaps.
- По умолчанию FullHD pixels не записываются; сохраняются generator parameters,
  content frame index и content hash.
- Replay использует тот же manifest/seed/commands и сравнивает per-frame hashes.
