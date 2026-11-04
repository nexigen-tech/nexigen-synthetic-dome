# Реестр решений, ограничений и открытых вопросов

Дата: 2026-11-05  
Статус: протокол проектирования `/opsx-explore`; не durable OpenSpec specs.

## 1. Подтверждённые решения

| ID | Решение |
|---|---|
| D-01 | Целевая ОС — только Ubuntu 26.04+; QNX и Astra исключены. |
| D-02 | iceoryx2 — zero-copy IPC data plane между процессами. |
| D-03 | Поддерживаются RT/HIL, Scaled SIL, Free SIL и Step с детерминированным replay. |
| D-04 | Один execution engine; режимы различаются Pacer/trigger policy. |
| D-05 | Время симуляции целочисленное; есть строгие SimTime, WallTime, HwFpgaTime. |
| D-06 | HW_FPGA_TIME_BASE — общая аппаратная шкала в нс; ось HIL берётся из HW stamps, не IRQ wake time. |
| D-07 | Между partitions используется LET; Direct только внутри partition. |
| D-08 | v1 целится в base frame 1 мс; fast 100 мкс — архитектурная возможность, не обязательный объём. |
| D-09 | Сигналы группируются в typed aggregates на logical port. |
| D-10 | CANopen components используют raw CAN batches; semantic Signal ports также поддерживаются. |
| D-11 | Целевое железо возвращает собственные CAN-кадры; virtual CAN повторяет это поведение. |
| D-12 | Каждый CAN frame имеет HW timestamp, direction и reception error metadata. |
| D-13 | CAN contract учитывает Classical, FD и XL; skeleton реализует Classical/CANopen. |
| D-14 | CANopen config — XDD CiA 311; OD и filters загружаются на cold path. |
| D-15 | SYNC response budget — 5 мс; fast class для CANopen не требуется. |
| D-16 | Video contract учитывает Raw, JPEG, H264, H265, AV1, H266 и DeviceRaw. |
| D-17 | `VideoCodec::DeviceRaw` сохраняется: vendor driver знает, что opaque DMA bytes являются видеокадром. |
| D-18 | Детальные lost-line regions не нужны; достаточно aggregate error flags. |
| D-19 | Heartbeat имеет собственное HW formation time и доступен activity-aware моделям. |
| D-20 | Heartbeat отличается от Data и не увеличивает content/data index. |
| D-21 | Отдельный `raw_dma_source` в skeleton не добавляется. |
| D-22 | Компонент обычно является процессом с одной partition; multi-component partitions отложены. |
| D-23 | Ядро component и panel выполняются в разных процессах. |
| D-24 | Generic/operator panels — Qt; video receiver — Qt/OpenGL. |
| D-25 | Поддерживается разработка models на Rust через тот же component ABI. |
| D-26 | Поддерживаются Tauri panels; для skeleton выбран Svelte. |
| D-27 | FullHD video не передаётся через Tauri IPC. |
| D-28 | iceoryx2 Blackboard включается в первый walking skeleton для slow shared state. |
| D-29 | Источники имеют logical names: CAN_A, CAN_B, Camera_Front_Left и т.п. |
| D-30 | Receivers подписываются через logical source binding; physical mapping задаётся профилем. |
| D-31 | CPU affinities, policies и priorities задаются в config через общий ThreadSpec vocabulary. |
| D-32 | Composer генерирует per-process/per-thread configs; пользователь не дублирует process cpuset. |
| D-33 | `nexigen-run` — нижний уровень process/suite supervision; `run_id/run_dir` — session identity. |
| D-34 | nexigen Infrastructure, logging, metrics, tracing, chrono, hub и WorkerTuning повторно используются. |
| D-35 | Общий infrastructure worker в v1 не нужен; существующие workers получают configured placement. |
| D-36 | Высокоскоростная simulation recording — MCAP; nexigen JSONL telemetry сохраняется. |
| D-37 | FullHD pixels по умолчанию не записываются; сохраняются generator config/index/hash. |
| D-38 | Первая сборка комплексов — YAML/JSON + CLI; graphical editor отложен. |
| D-39 | Первая walking skeleton video configuration — RGBA8888 FullHD 50 FPS. |

## 2. Проектные defaults, которые нужно подтвердить в proposals

Эти решения не являются внешними фактами и должны быть оформлены как design
decisions в соответствующих changes:

- `SampleHeader` через iceoryx2 `user_header`.
- Входное acceptance window D = один base frame по умолчанию.
- Один writer на Signal и на Blackboard entry.
- `RawCanBatch` публикуется каждый base frame, включая пустой batch как watermark.
- Размерные классы CAN payloads 8/64/2048 и bounded VideoFrame storage.
- Один writer на protocol TX identifier; merge policy требуется при исключении.
- `nxd-host` загружает C++/Rust plugins через C ABI.
- Rust hot path: `panic=abort`, no allocation after Armed, no Mutex/RwLock.
- `run_dir/dome` хранит копии configs, schemas и recording.
- Основной recording format — MCAP (лицензию и API следует проверить в change).
- Video receiver отображает последний кадр, а RX учитывает все.
- External DMA descriptor mode отложен; inline payload является baseline.
- Portable FP build profile требуется для replay между разными CPU.

## 3. Открытые вопросы, не блокирующие первый change

1. **Heartbeat ActivitySemantics:** отсутствие valid data, любых RX frames или
   физической активности. Нужно согласовать с FPGA перед HIL gateway.
2. **Видео API модулей:** собранные кадры или RTP/DMA fragments. Contract
   допускает gateway reassembly, но Driver requirements нужно уточнить.
3. **Vendor API:** blocking/callback model, buffer ownership, thread safety,
   polling ring support и точные error codes.
4. **CAN XL metadata:** точный protocol-specific layout по используемому
   стандарту и возможностям модулей.
5. **Iceoryx2 Blackboard details:** конкретный value typing, startup creation,
   revision/update observation и worst-case memory.
6. **DeviceRaw timestamp semantics:** BlockStart, FirstSample, LastSample или
   BlockComplete для каждого vendor format.
7. **Механизм true external DMA zero-copy:** ownership и generation slots при
   нескольких subscribers. Не требуется для skeleton.
8. **Выбор CANopen stack:** CANopenNode adaptation versus собственный sans-IO
   subset определяется отдельным spike.

## 4. Риски и проверки

### Iceoryx2

- Версия 0.9.x и изменяющийся API; зафиксировать версию и прогонять sanitizers.
- Linux support в документации tier 2; QNX tier 3 больше не относится к проекту.
- Память и mappings рассчитываются Composer из subscribers/buffers/payload.
- Нужны собственные измерения p99.99 на целевом хосте; субмикросекундные
  marketing figures не принимаются как гарантия.

### nexigen-cpp verification

Сохранённые логи на 2026-11-04:

- TSan: 36/36;
- ASan: 36/36;
- Clang: 36/36;
- Release: 35/36, `nexigen_logging_tests` strict accepted median 129.94 ns при
  gate 20 ns.

Это не доказывает regression на общей машине, но требует повторного strict
прогона на выделенном RT-хосте либо явно non-strict CI configuration.

Небольшая diagnostics issue: runner BestEffort warning сообщает
`runner_control_cpus is invalid; suite supervisor is unpinned` также при ошибках
name/scheduler и возможном partial application. Для эксплуатации полезен вывод
ThreadApplyReport.

### Детерминизм

- Побитовый replay гарантируется только на том же binary и CPU class.
- `-march=native`/LTO могут менять FP contraction; нужен portable replay build.
- Любые nondeterministic components должны явно объявляться и исключать claim
  полного replay без записи соответствующих boundary inputs.

### High-bandwidth video

- RGBA FullHD 50 FPS: 414 720 000 bytes/s и примерно 1.5 ТБ/час при recording.
- GUI display rate и data RX rate разделяются.
- External DMA zero-copy не следует начинать до измерения baseline inline path.

## 5. Отложенные возможности

- Обязательный fast 100 мкс class.
- Multi-component partitions и Direct optimization между отдельными apps.
- Полная поддержка CAN FD/XL protocol semantics.
- Все video decoders/converters и RTP packetization.
- Tauri video surface.
- Remote operators и host-to-host transport.
- Dynamic iceoryx2 slices как обязательный baseline.
- External DMA buffers и custom memory service types.
- Графический editor комплексов.
- AUTOSAR ARXML/runtime APIs; из AUTOSAR заимствованы только модель и словарь.

## 6. Связь с OpenSpec

Этот файл не заменяет requirements и scenarios. При создании changes решения
должны быть преобразованы в конкретные proposal/design/spec/tasks, а открытые
вопросы — в исследования, spikes или явные assumptions.
