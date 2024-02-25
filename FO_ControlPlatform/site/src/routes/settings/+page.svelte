<script lang='ts'>
    import { onMount } from 'svelte';

    var settings = {
        ch1_pin:"",
        ch2_pin:"",
        ch3_pin:"",
        ch4_pin:"",
        sda_sht_pin:"",
        scl_sht_pin:"",
        rx_solar_pin:"",
        beacon_pin:"",
        thingname:"",
        ap_name:"",
        ap_pass:"",
        mqtt_host:"",
        fast_blink_on_duration:""
    }

    const getSettings = async () => {
      try{
        const res = await fetch("/api/get-settings",{
          method:"POST",
          headers: { "Content-Type": "application/json" },
          body: "\"cmd\":\"get_settings\""
        });
        console.log(res);
        settings = await res.json();
        console.log(settings);
      }catch (error){
        var xhr = new XMLHttpRequest();
        //xhr.responseType = 'json';
        xhr.onreadystatechange = function() {
						if (xhr.readyState == 4) {
							if (xhr.status == 200) {
                 
								console.log(xhr.statusText);
                console.log(xhr.responseText);
                console.log(xhr.responseType);
                console.log(xhr.response);
                settings = JSON.parse(xhr.responseText.match(/{(.*?)}/)[0]);

							} else if (xhr.status == 0) {
								//alert("Server closed the connection abruptly!");
								//location.reload()
							} else {
								//alert(xhr.status + " Error!\n" + xhr.responseText);
								//location.reload()
							}
						}
					};
          
          xhr.open("POST", "/api/get-settings", true);
          xhr.setRequestHeader('Content-type', "application/json");
          xhr.send("\"cmd\":\"get_settings\"");
          
      }
      
    };

    onMount(() => {
        getSettings();
    })

    const changeSettings = async (icmd:string) =>{

      let toSend = {cmd:icmd, settings:{}}
      toSend["settings"] = settings;
      console.log(JSON.stringify(toSend));
      const res = await fetch("/api/change-settings",{
          method:"POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(toSend)
      });
    }
    
    function saveBtn(){
        let isExecuted = confirm("Are you sure you want to change and save these settings? \nThe device will restart and this page will refresh in 10s if yes.");
        if(isExecuted) changeSettings("change_settings");
    }
    function resetBtn(){
      let isExecuted = confirm("Are you sure you want to reset the device to factory default settings? \nThe device will restart and this page will refresh in 10s if yes.");
      if(isExecuted) changeSettings("reset_settings");
    }

    function updateSettings(event:Event, setting:string){}
</script>

<div class="sm:grid place-content-center">
<div class="flex flex-col justify-center ">

    <table class="table-auto overflow-x-scroll table-normal">
      <!-- head -->
      <thead>
        <tr>
          <th class="border">Setting</th>
          <th class="border">Value</th>
        </tr>
      </thead>
      <tbody>
       {#each Object.entries(settings) as [setting, value] }
       <tr>
        <td class="border text-blue-900 font-semibold">{setting}</td>
        <td class="border"><input on:input={(event) => {settings[setting] = event.target.value}} type="text" class="justify-end placeholder-opacity-50 focus:placeholder-opacity-0 placeholder-black" placeholder="Enter a new value" value={value}/></td> 
       </tr>
        {/each}
      </tbody>
    </table>

    <button on:click={saveBtn} class='btn w-full'>Save</button>
    <button on:click={resetBtn} class='btn btn-warning w-full'>Reset To Default Settings</button>
</div>
</div>