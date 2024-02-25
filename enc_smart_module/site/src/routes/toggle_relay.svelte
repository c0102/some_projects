<script lang="ts">
    let isOn = false;
    async function onClick(on_off: boolean) {
      isOn = on_off;
      await fetch("/api/toggle-relay", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({"cmd":"toggle_relay", is_on: isOn }),
      });
    }
  </script>
  
  {#if isOn}
    <div on:click={() => onClick(false)} class="rounded-full border-4 border-black border-solid w-16 h-16 bg-green-600 cursor-pointer" />
  {:else}
    <div on:click={() => onClick(true)} class="rounded-full border-4 border-black border-solid w-16 h-16  cursor-pointer" />
  {/if}