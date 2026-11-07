# Рекомендованная последовательность OpenSpec changes

Дата: 2026-11-05 (обновлено 2026-11-07)  
Статус: исследовательский roadmap, дополняемый по мере реализации.

## Происхождение документа

Этот список появился в результате высокоуровневого проектирования на этапе
`/opsx-explore`. В ходе исследования были рассмотрены:

- целевая архитектура моделирующего комплекса;
- nexigen-cpp, включая локальные модули `app`, `runner`, `hub`, `metrics`,
  `tracing`, `chrono` и `core::ThreadSpec`;
- Eclipse iceoryx2 как zero-copy IPC data plane;
- режимы HIL/SIL, аппаратное время FPGA и детерминированное воспроизведение;
- CAN/CAN FD/CAN XL, CANopen CiA 301 и XDD CiA 311;
- видеопотоки различных форматов и кодеков;
- C++ и Rust components, Qt и Tauri + Svelte panels;
- Blackboard, Composer, конфигурация и walking skeleton.

Изначально в проекте не было активных OpenSpec changes и durable specs. С момента
создания документа все foundational changes пройдены, заархивированы в
`openspec/changes/archive/` и покрыты durable specs в `openspec/specs/`. Этот
документ теперь служит дорожной картой: фиксирует пройденный путь и следующие
кандидаты в changes.

## Принципы декомпозиции

1. Не создавать один большой change на всю систему.
2. Каждый change должен давать проверяемый вертикальный результат.
3. Минимальный Composer нужен рано, чтобы быстро получить сквозной запуск.
4. Аппаратные и высокопроизводительные решения подтверждаются измерениями.
5. Fast-класс 100 мкс, external DMA buffers и полная поддержка всех кодеков не
   блокируют первый walking skeleton.
6. Требования hot path сохраняются во всех changes: ноль аллокаций после
   `Armed`, ноль блокировок и исключений, предсказуемые буферы.

## Пройденная последовательность (заархивировано)

| # | Change | Основной результат | Статус |
|---|---|---|---|
| 0 | `add-typed-port-transport` | Типизированные порты, `step_io`, zero-copy transport, headless backend, LET/wiring specs | Заархивирован |
| 1 | `add-contracts-and-component-sdk` | Контракты, C++/Rust types, component ABI, `nxd-host`, headless tests | Заархивирован |
| 2 | `add-runtime-chronos-and-minimal-composer` | Кадры, время, LET, ThreadSpec, минимальный запуск через `nexigen-run` | Заархивирован |
| 3 | `add-virtual-can-skeleton` | `CAN_A`, два CANopen-узла, XDD, heartbeat, ошибки, монитор | Заархивирован |
| 4 | `add-video-streaming-skeleton` | `Camera_Front_Left`, FullHD RGBA 50 FPS и Qt/OpenGL receiver | Заархивирован |
| 5 | `add-blackboard-rust-and-tauri-skeleton` | Rust `scenario_control`, headless Blackboard, Tauri + Svelte stub | Заархивирован |
| 6 | `add-recording-replay-and-validation` | MCAP, replay, полная validation и приёмочные сценарии | Заархивирован |
| — | `stabilize-walking-skeleton` | Все 7 CMake-вариантов (default, debug, release, clang, asan, tsan, ubsan) зелёные | Заархивирован |

Текущий walking skeleton собирается и проходит 42/42 теста в Release, включая
end-to-end видео. Активных OpenSpec changes нет.

## 1. `add-contracts-and-component-sdk`

### Объём

- Декларативный IDL и `component.yaml`.
- Типизированные агрегаты на логический порт.
- `SignalValue<T>`: value, quality, origin.
- `SampleHeader` для iceoryx2 `user_header`.
- Базовые `RawCanBatch` и `VideoFrame`, включая heartbeat, hardware timestamps,
  error flags и `VideoCodec::DeviceRaw`.
- C++23 POD и Rust `#[repr(C)]` + `ZeroCopySend` generation.
- Стабильные type names, semver и `layout_hash`.
- Golden cross-language binary tests.
- Component descriptor: ports, parameters, thread roles, resources.
- C ABI plugin и `nxd-host` с `initialize/arm/step/stop`.
- Интеграция с `nexigen::app::Infrastructure` и `core::ThreadSpec`.
- Headless unit-test harness для пошагового вызова component.

### Вне объёма

Chronos, реальные partitions, Composer, CANopen stack, виртуальные источники,
Qt/Tauri, Recorder и запуск всего комплекса.

### Критерий завершения

Один и тот же binary vector корректно передаётся между C++ и Rust в обоих
направлениях; простой C++ и простой Rust component проходят одинаковый
headless lifecycle без аллокаций после `Armed`.

## 2. `add-runtime-chronos-and-minimal-composer`

### Объём

- `TimePage`, release/ack, ожидание с принудительным пробуждением при stop.
- Pacer: RT, Scaled, Free, Step.
- Целочисленное `SimTime`; строгие `SimTime`, `WallTime`, `HwFpgaTime`.
- LET между partitions и `Direct` только внутри одной partition.
- Trigger abstraction и виртуальные trigger sources.
- Применение `core::ThreadSpec` к partition/gateway/control threads.
- Минимальные `dome.yaml`, profile, Dome Plan и `domectl plan/up`.
- Генерация `SuiteManifest` и per-instance nexigen settings.
- Запуск через `nexigen-run`, observability через `nexigen-hub`.
- Spike iceoryx2: C++ под `-fno-exceptions -fno-rtti`, startup allocations,
  polling versus event wake-up, `Local/Ipc`, user headers и latency tails.

### Критерий завершения

Сквозной пример «один component -> один monitor» запускается и останавливается
через runner; повторный Step-прогон даёт одинаковый результат; stop не зависает
на ожидании кадра.

## 3. `add-virtual-can-skeleton`

### Объём

- Логический источник `CAN_A`.
- `virtual_can`, raw batch каждую миллисекунду, включая пустые batches.
- Virtual arbitration и hardware-like virtual timestamps.
- Собственные кадры возвращаются отправителю, как на целевом железе.
- Два CANopen-узла: `sensor_a` (node 11) и `controller_b` (node 12).
- XDD CiA 311 загружается на cold path и формирует OD/COB-ID filters.
- SYNC period 5 мс; приёмочный response budget не более 5 мс.
- Heartbeat и error flags; пользовательские модели могут использовать
  аппаратный интервал молчания.
- `can_monitor`, generic panel и source-name subscription tests.
- Spike: адаптация CANopenNode под sans-IO против собственного минимального
  CiA 301 stack в стиле nexigen.

### Вне объёма

Полная реализация CAN FD/CAN XL semantics и HIL PCIe gateway. Их контракты
учитываются, но не реализуются в skeleton.

### Критерий завершения

PDO-путь между двумя узлами работает, SYNC response укладывается в 5 мс,
heartbeat/error scenarios воспроизводимы, `CAN_B` в специальном тесте не попадает
в подписчиков `CAN_A`.

## 4. `add-video-streaming-skeleton`

### Объём

- Логический источник `Camera_Front_Left`.
- `virtual_video`: RGBA8888, 1920x1080, 50 FPS, детерминированная динамическая
  картинка.
- Генерация прямо в iceoryx2 loan; bounded fixed storage для skeleton.
- Отдельный Qt/OpenGL receiver с RX и GUI threads.
- Счётчики `rx_samples`, `rx_data_frames`, `displayed_frames`, gaps и errors.
- Heartbeat `NO SIGNAL` и aggregate video error flags.
- Измерения generation/publish/RX/display latency и throughput.
- Один FullHD video stream; второй источник допускается только в маленьких
  source-binding tests.

### Вне объёма

H.264/H.265/AV1/H.266 decoding, все raw pixel backends, RTP reassembly,
DeviceRaw source и external DMA buffers. `raw_dma_source` специально не
добавляется.

### Критерий завершения

На настроенном хосте за 60 секунд генерируется 3000 кадров, при нормальных
условиях нет sequence gaps и steady-state memory growth. GUI может отображать
только последний кадр, но RX учитывает все принятые кадры.

## 5. `add-blackboard-rust-and-tauri-skeleton`

### Объём

- Rust `scenario_control` как `cdylib` для `nxd-host`.
- Tauri + Svelte panel; backend использует iceoryx2, frontend получает только
  небольшие decimated snapshots.
- Blackboard для медленного общего состояния: `dome/info`, `source/*`,
  `scenario/state`, `video0/config`.
- Один writer на entry, `revision`, `apply_frame_no`, versioned POD values.
- `BlackboardUpdate` events для последующей записи/replay.
- C++/Rust Blackboard layout tests и Rust allocation-after-Armed checks.

### Ограничения

Blackboard не используется для TimePage, CAN/video data, commands queue или
высокочастотных сигналов. FullHD video через Tauri IPC не передаётся.

### Критерий завершения

Tauri command изменяет сценарий на заданном кадре; C++ components реагируют на
Rust-written entry; закрытие панели не влияет на выполнение.

## 6. `add-recording-replay-and-validation`

### Объём

- MCAP recording и schemas из IDL.
- Boundary data, raw CAN, commands, Blackboard updates, heartbeat, metadata.
- По умолчанию видеопиксели не записываются: параметры генератора, frame index
  и content hash вместо примерно 1.5 ТБ/час для RGBA FullHD 50 FPS.
- `run_id/run_dir` как идентичность сессии; копии исходных конфигов и планов.
- Replay в Step/Free и per-frame state/output hashes.
- Полная Composer validation: types, bindings, rates, resources, threads,
  profile/mode, COB-ID ownership и latency SLO.
- Приёмочные performance/reliability suites на настроенном RT-хосте.

### Критерий завершения

RT-прогон воспроизводится в Step с совпадающими CAN/state/video-content hashes;
первое расхождение показывает кадр, component и поле.

## Сквозные требования ко всем changes

- Debug, Release, TSan, ASan и Clang; strict latency gates только на подходящем
  выделенном оборудовании.
- Проверка нулевых аллокаций после `Armed`.
- Нет скрытого перехода `Required` RT configuration в non-RT.
- Все samples, commands и external events имеют однозначную frame/time identity.
- Панели не влияют на детерминированный результат моделей.
- Каждый change отдельно указывает performance, latency и memory impact.

## Рекомендуемые следующие change

Walking skeleton стабилен, но остаются отложенные элементы из #5 и мелкие
долги инфраструктуры. Рекомендуется не смешивать их в одном большом change.

| Приоритет | Change | Зачем | Ключевой результат |
|---|---|---|---|
| 1 | `add-blackboard-iceoryx2-backend` | Панель и внешние процессы не могут разделять headless Blackboard; durable spec `runtime/blackboard` требует multi-process backend | Blackboard поверх iceoryx2, C API без изменений, fork-тесты, панель подключается из другого процесса |
| 2 | `connect-scenario-panel-to-blackboard` | `panels/scenario-panel/src-tauri/src/lib.rs` содержит TODO: команды не публикуются, состояние не читается | Tauri backend публикует `ScenarioCommand`, получает decimated `scenario/state`, frontend отображает актуальное состояние |
| 3 | `add-panel-launcher-to-nexigen-run` | `nexigen-run` пока не умеет запускать panel-процессы | Dome Plan может объявлять панели, runner стартует их после simulation processes |
| 4 | `make-blackboard-seqlock-tsan-compatible` | В `stabilize-walking-skeleton` TSan-предупреждение подавлено через `-Wno-tsan`; правильный фикс — seqlock без неинструментируемого fence | TSan build проходит без специальных флагов, производительность сохраняется |
| 5 | `remove-ci-nexigen-cpp-template-workaround` | `.github/workflows/ci.yml` содержит TODO про upstream `nexigen-cpp` cmake config template | Убрать патч шаблона, когда upstream выложит файл или исправит `.gitignore` |

### Следующий шаг

Запустить `openspec new change add-blackboard-iceoryx2-backend`, затем
`openspec propose` для формирования proposal/design/specs/tasks. Этот change
разблокирует панель и является логическим продолжением skeleton.
