# Walking Skeleton `nxd-ws01`

Дата: 2026-11-05  
Статус: состав и критерии, полученные на `/opsx-explore`; не реализация.

## 1. Цель

Проверить сквозной путь от конфигурации до запуска, обмена данными, operator
controls, записи и replay, не включая PCIe и все промышленные форматы в первый
инкремент.

## 2. Состав

```text
 nxd-chronos
 virtual_can             источник CAN_A
 sensor_a                C++, CANopen node 11
 controller_b            C++, CANopen node 12
 can_monitor             подписан на CAN_A
 virtual_video           источник Camera_Front_Left
 video_receiver          Qt/OpenGL
 scenario_control        Rust cdylib
 scenario_panel          Tauri + Svelte
 generic_panel           Qt
 nxd-recorder
 nexigen-run
 nexigen-hub
 iceoryx2 Blackboard
```

```text
 CAN_A:
   sensor_a -> virtual_can -> controller_b
        \                         /
         +------ can_monitor -----+

 Camera_Front_Left:
   virtual_video -> video_receiver

 scenario_panel -> scenario_control -> Blackboard
                                      |- virtual_video config
                                      |- scenario state
                                      `- source catalog
```

Аппаратные PCIe gateways в skeleton не используются. Контракты virtual twins
совпадают с будущими HIL-контрактами.

## 3. CANopen-контур

- Base frame: 1 мс.
- Classical CAN/CANopen CiA 301.
- `sensor_a`: node 11, детерминированный сигнал, TPDO1 каждые 10 мс и по SYNC.
- `controller_b`: node 12, RPDO на TPDO1 sensor_a через XDD, простая функция и
  собственный TPDO1.
- `virtual_can`: SYNC каждые 5 мс, raw batch каждый кадр, включая пустой,
  arbitration и virtual hardware timestamps.
- Собственные кадры возвращаются отправителю, как на целевом модуле.
- XDD CiA 311 определяет OD, PDO и COB-ID filters на cold path.
- Приёмочный SYNC response budget: не более 5 мс.
- CANopen игнорирует heartbeat; activity-aware модели могут его получать.

## 4. Видеотракт

Параметры:

| Параметр | Значение |
|---|---|
| Source name | `Camera_Front_Left` |
| Coding | Raw |
| Pixel format | RGBA8888 |
| Resolution | 1920x1080 |
| Rate | 50 FPS / 20 мс |
| Frame size | 8 294 400 bytes |
| Throughput | 414 720 000 bytes/s, около 395.5 МиБ/с |

Источник генерирует детерминированный динамический pattern прямо в iceoryx2
loan. Изображение зависит от `content_frame_index`, а не от wall-clock.

RX thread принимает все samples и считает data/heartbeat/errors/gaps. GUI
отображает последний кадр и не обязан показывать каждый принятый. Отображаются
`rx_data_frames`, `displayed_frames`, sequence gaps и rates.

Heartbeat приводит к состоянию `NO SIGNAL` без вызова decoder. Video errors
представлены только aggregate flags; детальных lost-line regions нет.

По умолчанию пиксели не записываются: RGBA FullHD 50 FPS даёт примерно 1.5 ТБ/час.
Сохраняются generator config, frame index и content hash.

## 5. Rust, Tauri и Blackboard

`scenario_control` — небольшой Rust component, единственный writer
`scenario/state`. Tauri + Svelte panel отправляет allowlisted commands.
`virtual_video` владеет собственной config entry; C++ components читают
Rust-written entries на границе кадра.

Blackboard entries:

- `dome/info`;
- `source/CAN_A`;
- `source/Camera_Front_Left`;
- `scenario/state`;
- `video0/config` (конкретное имя может быть согласовано с source name в proposal).

Каждое изменение имеет revision и `apply_frame_no` и сопровождается
`BlackboardUpdate` для recording/replay.

## 6. Исходные конфиги

Минимальный пользовательский набор:

```text
 ws01/
   dome.yaml
   host.yaml                  # domectl doctor
   profiles/sil.yaml
   devices/sensor_a.xdd
   devices/controller_b.xdd
   scenarios/basic.yaml       # при использовании сценария
```

Для последующего HIL добавляется `profiles/hil.yaml`, не меняя component
bindings.

Библиотека поставляет `component.yaml` и contract YAML. Инженер не редактирует
per-process JSON вручную.

## 7. Generated configs

```text
 build/ws01/sil/
   dome-plan.json
   suite.manifest.json
   blackboard.seed.json
   schemas/*.json
   instances/<instance>/instance.json
   instances/<instance>/settings/{logging,metrics,tracing,chrono}.json
```

`dome-plan.json` содержит source IDs, endpoints, contracts, processes, threads,
LET edges, QoS/memory, recording и SLO. Он детерминирован и хешируется.

## 8. Run artifacts

```text
 runs/<run_id>/
   suite.json
   suite-state.json
   dome/
     dome-plan.json
     dome.yaml
     profile.yaml
     host.yaml
     blackboard.seed.json
     schemas/
     recording/
   <app_name>/<instance_id>/
     resource.json
     exit.json
     settings/
     app.jsonl
     metrics.jsonl
     traces.jsonl
     chrono.jsonl
     stdout.log
     stderr.log
```

## 9. Критерии приёмки

### Функциональные

- Два CANopen-узла обмениваются PDO; XDD реально определяет OD/mapping.
- SYNC response не превышает 5 мс по виртуальным меткам.
- Оператор управляет semantic fields через Model/Manual/File/Generator.
- FullHD dynamic video отображается со счётчиками RX/displayed.
- Rust component проходит общий lifecycle; C++ читает его Blackboard values.
- Tauri command применяется на ожидаемом кадре.
- Закрытие любой панели не меняет выходные hashes.

### Подписки и имена

- В специальном тесте `CAN_B` не попадает в subscribers `CAN_A`.
- `source_id` соответствует Blackboard source catalog.
- Panels, logs и recording показывают logical source names.
- Смена профиля не меняет logical bindings.

### Heartbeat и ошибки

- CAN heartbeat не вызывает CANopen activity, но доступен activity view.
- Video heartbeat даёт `NO SIGNAL` и не увеличивает content index.
- Пропавшие heartbeats переводят health в `Lost` после configured threshold.
- CAN CRC error виден monitor/recorder, но не обрабатывается как нормальный PDO.
- Video MissingLines/CRC представлены aggregate flags и вызывают configured
  display policy.
- Error/heartbeat scenarios повторяются в Step.

### Производительность и память

- На настроенном хосте за 60 секунд генерируется 3000 video frames.
- В нормальных условиях `sequence_gaps = 0` и нет steady-state memory growth.
- Нет аллокаций после `Armed`, включая Rust component (test allocator).
- Измеряются p50/p99/p99.99 generation/publish/RX/display и partition jitter.
- На ненастроенной development machine допускается BestEffort без RT-гарантий.

### Lifecycle и replay

- Runner запускает/останавливает suite без zombies и зависаний на frame wait.
- JSONL telemetry корректно сбрасывается; новый run не зависит от предыдущего.
- RT-прогон повторяется в Step с одинаковыми CAN/state/video-content hashes.
- Первое расхождение показывает frame, component и field.

## 10. Явно не входит в skeleton

- PCIe/FPGA hardware и реальный IRQ path.
- Обязательный 100 мкс fast class.
- Все video decoders и RTP reassembly.
- Отдельный `raw_dma_source`.
- External DMA buffer descriptors.
- Полная CAN FD/CAN XL protocol support.
- Второй постоянный FullHD source.
- Видео через Tauri IPC.
- Графический editor комплексов.

Соответствующие контракты и extension points закладываются, но не реализуются
без отдельных changes.
