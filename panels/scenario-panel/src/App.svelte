<script lang="ts">
  import { invoke } from "@tauri-apps/api/core";

  let status = "ready";
  let applyFrame = 10;
  let commandValue = 42;

  async function sendCommand() {
    try {
      const result = await invoke<string>("send_scenario_command", {
        commandId: 1,
        applyFrameNo: applyFrame,
        value: commandValue,
      });
      status = result;
    } catch (e) {
      status = `error: ${e}`;
    }
  }
</script>

<main>
  <h1>Scenario Panel</h1>
  <label>
    Apply frame
    <input type="number" bind:value={applyFrame} />
  </label>
  <label>
    Value
    <input type="number" bind:value={commandValue} />
  </label>
  <button on:click={sendCommand}>Send allowlisted command</button>
  <p>{status}</p>
</main>

<style>
  main {
    font-family: system-ui, sans-serif;
    padding: 1rem;
  }
  label {
    display: block;
    margin: 0.5rem 0;
  }
</style>
