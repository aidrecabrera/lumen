---
marp: true
theme: default
paginate: true
size: 16:9
title: Lumen
description: Local lighting and sensing platform on ESP32
style: |
  @import url('https://fonts.googleapis.com/css2?family=Stack+Sans+Text:wght@200..700&display=swap');
  @import url('https://fonts.googleapis.com/css2?family=Geist+Mono:wght@100..900&display=swap');

  :root {
    --base-50:   oklch(0.985 0 22.65);
    --base-100:  oklch(0.97  0 23.07);
    --base-200:  oklch(0.9224 0 23.19);
    --base-300:  oklch(0.8696 0 23.46);
    --base-400:  oklch(0.7084 0 23.42);
    --base-500:  oklch(0.5545 0 24.17);
    --base-600:  oklch(0.4411 0 24.82);
    --base-700:  oklch(0.3722 0 24.37);
    --base-800:  oklch(0.2686 0 23.43);
    --base-900:  oklch(0.2096 0 23.33);
    --base-950:  oklch(0.1458 0 22.58);
    --base-1000: oklch(0.1043 0 22.82);

    --background:         oklch(1 0 0);
    --foreground:         #1D1D1F;
    --black:              #1D1D1F;
    --card:               oklch(1 0 0);
    --card-foreground:    var(--base-800);
    --popover:            oklch(1 0 0);
    --popover-foreground: var(--base-800);
    --primary:            var(--base-950);
    --primary-foreground: oklch(1 0 0);
    --secondary:          var(--base-100);
    --secondary-foreground: var(--base-800);
    --muted:              var(--base-50);
    --muted-foreground:   var(--base-600);
    --accent:             var(--base-50);
    --accent-foreground:  var(--base-800);
    --destructive:        oklch(0.577 0.245 27.325);
    --border:             var(--base-200);
    --input:              var(--base-300);
    --ring:               var(--base-950);
    --radius:             0rem;

    --font-sans:    'Stack Sans Text', system-ui, -apple-system, sans-serif;
    --font-heading: var(--font-sans);
    --font-mono:    'Geist Mono', ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  }

  section.invert {
    --background:         var(--black);
    --foreground:         var(--base-200);
    --card:               var(--black);
    --card-foreground:    var(--base-200);
    --popover:            var(--black);
    --popover-foreground: var(--base-200);
    --primary:            var(--base-50);
    --primary-foreground: var(--base-900);
    --secondary:          var(--base-800);
    --secondary-foreground: var(--base-200);
    --muted:              var(--base-900);
    --muted-foreground:   var(--base-400);
    --accent:             var(--base-900);
    --accent-foreground:  var(--base-200);
    --destructive:        oklch(0.704 0.191 22.216);
    --border:             var(--base-800);
    --input:              var(--base-700);
    --ring:               var(--base-50);
  }

  section {
    font-family: var(--font-sans);
    background: var(--background);
    color: var(--foreground);
    padding: 42px 48px;
    font-size: 18px;
    line-height: 1.75;
  }

  h1 {
    font-family:     var(--font-heading);
    color:           var(--primary);
    font-size:       2.25rem;
    line-height:     2.5rem;
    font-weight:     800;
    letter-spacing:  -0.025em;
    text-wrap:       balance;
    margin:          0 0 0.4em 0;
  }

  h2 {
    font-family:       var(--font-heading);
    color:             var(--primary);
    font-size:         1.875rem;
    line-height:       2.25rem;
    font-weight:       600;
    letter-spacing:    -0.025em;
    margin:            0 0 0.75em 0;
    padding-bottom:    0.4em;
    border-bottom:     1px solid var(--border);
    transition-property: color, background-color, border-color;
    transition-timing-function: cubic-bezier(0.4, 0, 0.2, 1);
    transition-duration: 150ms;
  }

  h3 {
    font-family:    var(--font-heading);
    color:          var(--primary);
    font-size:      1.5rem;
    line-height:    2rem;
    font-weight:    600;
    letter-spacing: -0.025em;
    margin:         0 0 0.35em 0;
  }

  h4 {
    font-family:    var(--font-heading);
    color:          var(--primary);
    font-size:      1.25rem;
    line-height:    1.75rem;
    font-weight:    600;
    letter-spacing: -0.025em;
    margin:         0 0 0.3em 0;
  }

  p, li {
    color:       var(--foreground);
    line-height: 1.75rem;
  }

  p:not(:first-child) { margin-top: 1rem; }

  a { color: var(--primary); }

  strong {
    color:       var(--primary);
    font-weight: 650;
  }

  ul {
    margin:           0.4em 0 0.4em 1.5em;
    list-style-type:  disc;
  }

  li { margin: 0.1em 0; }

  blockquote {
    margin-top:    1.5rem;
    border-left:   2px solid var(--border);
    padding-left:  1.5rem;
    font-style:    italic;
    color:         var(--muted-foreground);
  }

  code, pre, kbd, samp {
    font-family: var(--font-mono) !important;
  }

  /* inline code */
  code {
    background:    var(--muted);
    color:         var(--primary);
    border:        1px solid var(--border);
    border-radius: 0.25rem;
    padding:       0.2rem 0.3rem;
    font-size:     0.875em;
    font-weight:   600;
  }

  /* fenced code block */
  pre {
    background:  var(--muted);
    color:       var(--card-foreground);
    border:      1px solid var(--border);
    padding:     14px 18px;
    font-size:   0.72em;
    line-height: 1.45;
    overflow:    hidden;
    border-radius: 0;
  }

  /* reset nested code inside pre */
  pre code {
    background:    transparent;
    color:         inherit;
    border:        none;
    border-radius: 0;
    padding:       0;
    font-size:     inherit;
    font-weight:   inherit;
  }

  table {
    border-collapse: collapse;
    width:      100%;
    font-size:  0.82em;
    margin-top: 1rem;
    overflow-y: auto;
  }

  th, td {
    border:  1px solid var(--border);
    padding: 0.5rem 1rem;
    text-align: left;
  }

  th {
    background:  var(--secondary);
    color:       var(--secondary-foreground);
    font-weight: 700;
  }

  tr:nth-child(even) { background-color: var(--muted); }

  img {
    border:     1px solid var(--border);
    background: var(--card);
  }

  .cols {
    display:               grid;
    grid-template-columns: 1fr 1fr;
    gap:                   24px;
    align-items:           start;
  }

  .cols-3 {
    display:               grid;
    grid-template-columns: 1fr 1fr 1fr;
    gap:                   18px;
    align-items:           start;
  }

  .box {
    border:     1px solid var(--border);
    background: var(--card);
    color:      var(--card-foreground);
    padding:    16px 18px;
  }

  .small  { font-size: 0.875rem; line-height: 1.4; }
  .tiny   { font-size: 0.75rem;  line-height: 1.3; }
  .muted  { color: var(--muted-foreground); }
  .lead   { color: var(--muted-foreground); font-size: 1.25rem; line-height: 1.75rem; }
  .large  { font-size: 1.125rem; line-height: 1.75rem; font-weight: 600; }

  .code-ref {
    margin-top: 14px;
    font-family: var(--font-mono);
    font-size:   0.72rem;
    color:       var(--muted-foreground);
  }

  .toggle {
    position: fixed;
    top:      12px;
    right:    16px;
    z-index:  9999;
    font-size: 14px;
  }

  .toggle button {
    font-family: var(--font-sans);
    border:      1px solid var(--border);
    background:  var(--card);
    color:       var(--foreground);
    padding:     8px 12px;
    cursor:      pointer;
  }
---

<div class="toggle">
  <button onclick="document.querySelectorAll('section').forEach((e) => e.classList.toggle('invert'))">Toggle appearance</button>
</div>

# Lumen

### Local lighting and sensing platform on ESP32

- local first
- MQTT today
- no cloud required
- built to grow without turning into a mess

---

## What this is

Lumen is not a cloud toy and it is not a one-off classroom demo.

It is a local node that:

- reads the environment
- controls LED output
- accepts commands
- reports what it is doing
- keeps enough structure that the firmware can grow without collapsing into one giant loop

That is the whole point.

<div class="code-ref">
Code: <code>src/lumen_main.cpp</code>, <code>src/lumen_task_manager.cpp</code>, <code>src/lumen_mqtt_client.cpp</code>
</div>

---

## Why this exists

A lot of systems like this get ruined by two bad choices:

1. add cloud dependence for no good reason
2. keep piling logic into one shared loop until nobody wants to touch it

Lumen avoids both.

Local control is simpler, faster, easier to debug, and more useful in places where internet is weak, expensive, or irrelevant. A farm does not need a SaaS dependency just to read a sensor or switch a light.

Food and lighting are physical problems. They should stay local unless there is a real reason not to.

---

## What users can do today

<div class="cols-3">
<div>

**Monitor**
- ambient light
- temperature
- humidity
- node state
- online / offline

</div>
<div>

**Control**
- LED brightness
- LED channel mix
- operating mode

</div>
<div>

**Receive**
- telemetry
- status updates
- acknowledgements
- energy data

</div>
</div>

This is not hypothetical. The current firmware already does it.

<div class="code-ref">
Code: <code>src/lumen_sensor_manager.cpp</code>, <code>src/lumen_led_control.cpp</code>, <code>src/lumen_energy_tracker.cpp</code>, <code>src/lumen_mqtt_client.cpp</code>
</div>

---

## Where it fits

<div class="cols-3">
<div class="box">

### Home growing

Shelves, tents, and small enclosures.

</div>
<div class="box">

### Farms and greenhouses

Local control where internet is weak or unnecessary.

</div>
<div class="box">

### Hobby and platform work

ESP32, sensors, LEDs, and a base you can keep extending.

</div>
</div>

One future direction is spectrum-aware plant lighting. That overlaps with [photomorphogenesis](https://en.wikipedia.org/wiki/Photomorphogenesis), where light intensity, timing, and composition all matter.

---

## Current implementation

No hand-waving. This is the current state.

<div class="cols">
<div class="box">

### Done

- BH1750 ambient light sensing
- DHT11 temperature and humidity sensing
- LED control
- autonomous brightness behavior
- manual control
- MQTT telemetry and commands
- energy tracking
- config persistence

</div>
<div class="box">

### Not done yet

- TCS34725 support
- RGB/Clear runtime data
- spectrum-aware control logic
- photoperiod scheduling
- direct transport mode
- multi-node coordination

</div>
</div>

<div class="code-ref">
Code: <code>src/lumen_sensor_manager.cpp</code>, <code>src/lumen_led_control.cpp</code>, <code>src/lumen_config_manager.cpp</code>
</div>

---

## Hardware profile

Current hardware is simple on purpose.

- controller:
  - ESP32 / ESP32-WROOM family
- sensors:
  - BH1750
  - DHT11
- actuation:
  - WS2812B / NeoPixel-compatible LEDs
- next addition:
  - TCS34725

![center w:520](../../assets/images/lumen-hardware-overview.png)

<div class="code-ref">
Config: <code>include/lumen_board_config.h</code> &nbsp; Wiring: <code>WIRING.md</code>
</div>

---

## Current architecture

This is the current production shape.

- one node
- brokered transport
- one or many clients through the broker
- clean topic namespace
- no cloud requirement

![bg right:35% contain](../../assets/diagrams/lumen-current-architecture-light.png)

<div class="code-ref">
Code: <code>src/lumen_mqtt_client.cpp</code>, <code>src/lumen_wifi_manager.cpp</code>
</div>

---

## Firmware design

The firmware is split by responsibility because stuffing everything into one loop is how projects become unmaintainable.

Core pieces:

- platform entry
- shared support
- connectivity
- sensing
- control
- coordination
- persistence

The split is deliberate. It keeps sensing, control, transport, and storage from tripping over each other.

![bg right:45% contain](../../assets/diagrams/lumen-firmware-design-light.png)

<div class="code-ref">
Code map: <code>src/lumen_main.cpp</code>, <code>src/lumen_task_manager.cpp</code>, <code>src/lumen_config_manager.cpp</code>, <code>src/lumen_system_utils.cpp</code>, <code>src/lumen_type_validation.cpp</code>
</div>

---

## Runtime flow

<div class="cols">
<div>

**Sensor path**
- sensing
- control
- state update
- telemetry

</div>
<div>

**Command path**
- MQTT inbound
- command queue
- control task
- acknowledgement

</div>
</div>

These parts have different timing and different failure modes. That separation matters.

![center w:860](../../assets/diagrams/lumen-runtime-flow-light.png)

<div class="code-ref">
Code: <code>void taskSensors(...)</code>, <code>void taskLed(...)</code>, <code>void taskMqtt(...)</code> in <code>src/lumen_task_manager.cpp</code>
</div>

---

## Code, not hand-waving

This is the actual shape of the runtime. The tasks are real.

```cpp
void taskSensors(void* parameter);
void taskLed(void* parameter);
void taskMqtt(void* parameter);
```

And the MQTT task does exactly what you would expect:

```cpp
const bool wifi_ready = WifiManager::connectOrPoll();
const bool mqtt_ready = MqttClient::connectOrPoll();
```

If both are up, it publishes telemetry and drains the outbound queues.

<div class="code-ref">
Source: <code>src/lumen_task_manager.cpp</code>
</div>

---

## Control path in code

Sensor reads and lighting control are also straightforward.

```cpp
bool readCurrent(SensorReading& out);
void controlTick(const SensorReading& latest_reading);
AckMessage applyCommand(const CommandEnvelope& command);
```

That gives you three clear control points:

- read the world
- update the light
- apply direct commands

Nothing mystical here.

<div class="code-ref">
Source: <code>src/lumen_sensor_manager.cpp</code> and <code>src/lumen_led_control.cpp</code>
</div>

---

## Transport path in code

The MQTT side is not magic either.

```cpp
bool connectOrPoll();
bool publishTelemetry(const SensorReading& reading);
bool publishStatus(const StatusMessage& status);
bool publishEnergy(const EnergyMessage& energy);
bool publishAck(const AckMessage& ack);
```

So when the deck says telemetry, status, energy, and acknowledgements are separate paths, that is not a marketing phrase. Those functions exist in the code.

<div class="code-ref">
Source: <code>src/lumen_mqtt_client.cpp</code>
</div>

---

## Technical decisions

<div class="cols">
<div>

### Why local first

- no cloud dependency
- lower operational friction
- works in remote setups
- easier to debug

### Why MQTT

- natural fit for telemetry plus commands
- multiple clients are easy
- keeps transport clean

</div>
<div>

### Why MsgPack

- smaller payloads
- less wire overhead
- better fit for constrained firmware

### Why tasks and queues

- cleaner timing boundaries
- less coupling
- easier to reason about than one big shared-state loop

</div>
</div>

<div class="code-ref">
Code: <code>src/lumen_mqtt_client.cpp</code>, <code>src/lumen_task_manager.cpp</code>
</div>

---

## Sensor choices

The current stack is not random.

- **BH1750 now**
  - enough for brightness-based control
  - simple and stable
- **DHT11 now**
  - cheap
  - common
  - enough for baseline environmental context
- **TCS34725 next**
  - adds RGB and clear data
  - moves the system toward spectrum awareness

Brightness is not the same thing as composition. That is why BH1750 and TCS34725 are different jobs.

---

## Wiring and deployment

The physical setup is straightforward.

- SDA: GPIO21
- SCL: GPIO22
- DHT11 DATA: GPIO4
- LED DATA: GPIO18
- sensors on 3.3 V
- LEDs on regulated 5 V
- common ground everywhere

And yes, it is local-only in the cloud sense.

```text
Lumen node <-> local Wi-Fi <-> local MQTT broker <-> local client/frontend
```

<div class="code-ref">
Config: <code>include/lumen_board_config.h</code> &nbsp; Wiring: <code>WIRING.md</code>
</div>

---

## What is missing

The current firmware is useful, but it is still narrow.

Current limits:

- single-node implementation
- MQTT-only transport
- no spectral sensing yet
- no direct mode yet
- RGB hardware only approximates far-red

That is fine. Better to be honest than pretend a prototype is a finished system.

---

## Roadmap

The platform is going in two directions.

### Deeper on one node

- TCS34725 integration
- richer sensor model
- spectrum-aware control
- photoperiod support
- better horticulture-focused behavior

### Broader across deployments

- more than one node
- direct mode
- hybrid mode
- cleaner client integration

That is the roadmap. Not magic. Just the next sensible steps.

---

## Target architecture

Current architecture is fine for now. This is where it goes next.

- many clients
- brokered and direct paths
- many nodes
- cleaner scaling model

![bg right:35% contain](../../assets/diagrams/lumen-target-architecture-light.png)

---

## Current vs target

This is the whole direction in one slide.

![center w:400](../../assets/diagrams/lumen-architecture-comparison-light.png)

---

## MQTT model

Lumen currently uses MQTT as the transport layer.

Published topics:

```text
lumen/<device_id>/telemetry
lumen/<device_id>/status
lumen/<device_id>/energy
lumen/<device_id>/availability
lumen/<device_id>/ack/<command_id>
```

Subscribed topics:

```text
lumen/<device_id>/command/led
lumen/<device_id>/command/config
lumen/<device_id>/command/mode
```

Payloads use MsgPack. Availability uses plain string values.

<div class="code-ref">
Source: <code>include/lumen_board_config.h</code>, <code>src/lumen_mqtt_client.cpp</code>
</div>

---

## Closing

Lumen already works as a local lighting and sensing node.

What matters now is not pretending it is finished. What matters is that the base is solid:

- local first
- structured firmware
- real sensing
- real control
- room to grow

That is a good place to be.