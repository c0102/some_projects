<script lang="ts">
  import "./app.css";
  export const prerender = true;
  import {WebsocketBuilder} from 'websocket-ts';
  import MediaQuery from './MediaQuery.svelte';


  let name = 'world';
  let col = "red";

  let mes = {
            state:{
              "ch1":false,
              "ch2":false,
              "ch3":false,
              "ch4":false
            },
            data:
            {"temperature":0.0, 
             "humidity":0.0,
             "battery-voltage":0.0,
             "solar-power":0.0,
             "yield-today":0.0,
             "yield-yesterday":0.0,
             "load-current":0.0,
             "battery-current":0.0,
            },
            in_state:
            {"free_memory":0,
             "min_free_memory":0,
             "day":"day",
             "ap_ip":"0.0.0.0",
             "eth_ip":"0.0.0.0",
             "app_version":"0.0.0"
            }

          };

  let b_cam_1 = false;
  let b_cam_2 = false;
  let b_cam_3 = false;
  let b_wifi_router = false;

  let manual_override = false;
  let day = false;

  // async function getStates() {

  //         const response = await fetch("/api/get-state",{
  //                     method: "POST",
  //                     headers: { "Content-Type": "application/json" },
  //                     body: JSON.stringify({"cmd":"get-state" }),
  //                     });
          
  //         return response.json();
         
          
  // }

  // // window.onload = () =>{
  // //   getStates().then((data) => {
  // //     b_cam_1 = data['ch1'];
  // //     b_cam_2 = data['ch2'];
  // //     b_cam_3 = data['ch3'];
  // //     b_wifi_router = data['ch4'];
  // //   });
  // // }
  
  
  // async function getData() {

  //   const response = await fetch("/api/get-data",{
  //                     method: "POST",
  //                     headers: { "Content-Type": "application/json" },
  //                     body: JSON.stringify({"cmd":"get-data" }),
  //                     });
  //   console.log(response);
  //   return response.json();
    
  // }

  // setInterval(() =>{
  //           getData().then((data)=>{
  //               mes = data;
  //           })
  // } , 10000);

  // setInterval(() => { 
    
  //   b_cam_1 = mes["state"]["ch1"];
  //   b_cam_2 = mes["state"]["ch2"];
  //   b_cam_3 = mes["state"]["ch3"];
  //   b_wifi_router = mes["state"]["ch4"];
  //   //console.log(b_cam_1)
  // }, 500);

  //  'ws://'+window.location.href.replace('http://','').replace('/','')+ ':80/ws' ||| 'ws://fo_control_panel.local:80/ws'
  
  const ws = new WebsocketBuilder('/ws')
    .onOpen((i, ev) => { console.log("opened") })
    .onClose((i, ev) => { console.log("closed") })
    .onError((i, ev) => { console.log("error") })
    .onMessage((i, ev) => { console.log("message"); 
                            mes = JSON.parse(ev.data); 
                            b_cam_1 = mes["state"]["ch1"];
                            b_cam_2 = mes["state"]["ch2"];
                            b_cam_3 = mes["state"]["ch3"];
                            b_wifi_router = mes["state"]["ch4"];
                            console.log(mes); 
                            console.log('b_cam_1:', b_cam_1);
                            console.log('b_cam_1:', b_cam_2);
                            console.log('b_cam_1:', b_cam_3);
                            console.log('b_wifi_router:', b_wifi_router);
                          })
    .onRetry((i, ev) => { console.log("retry") })
    .build();

  async function onClick(ch:string ) {
      if(ch === "ch1"){
        mes["state"]["ch1"] = !b_cam_1;
        await fetch("/api/toggle-switch", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({"cmd":"toggle_ch1", is_on: !b_cam_1 }),
      });

      }else if(ch === "ch2"){
        mes["state"]["ch2"] = !b_cam_2;
        await fetch("/api/toggle-switch", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({"cmd":"toggle_ch2", is_on: !b_cam_2 }),
      });
      }else if(ch === "ch3"){

        mes["state"]["ch3"] = !b_cam_3;
        await fetch("/api/toggle-switch", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({"cmd":"toggle_ch3", is_on: !b_cam_3 }),
      });

      }else if(ch === "ch4"){

        mes["state"]["ch4"] = !b_wifi_router;
        await fetch("/api/toggle-switch", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({"cmd":"toggle_ch4", is_on: !b_wifi_router }),
      });
      }
    
    }

    function startUpload() {
				var input = document.getElementById("otafile") as HTMLInputElement | null;
        var otafile = input.files;
        console.log("filesize: ",otafile.length)
				if (otafile.length == 0) {
					alert("No file selected!");
				} else {
					document.getElementById("otafile").setAttribute('disabled','true') ;
					document.getElementById("upload").setAttribute('disabled','true');

					var file = otafile[0];
					var xhr = new XMLHttpRequest();
					xhr.onreadystatechange = function() {
						if (xhr.readyState == 4) {
							if (xhr.status == 200) {
								document.open();
								document.write(xhr.responseText);
								document.close();
							} else if (xhr.status == 0) {
								alert("Server closed the connection abruptly!");
								location.reload()
							} else {
								alert(xhr.status + " Error!\n" + xhr.responseText);
								location.reload()
							}
						}
					};

					xhr.upload.onprogress = function (e) {
						var progress = document.getElementById("progress");
						progress.textContent = "Progress: " + (e.loaded / e.total * 100).toFixed(0) + "%";
					};
					xhr.open("POST", "/api/update", true);
					xhr.send(file);
				}
			}

    async function manual_control() {

      await fetch("/api/manual-control", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({"cmd":"manual_control", "manual_override": manual_override, "day":day }),
      });

    }

  // function handleClick() {
  //   let sendState = `{"on":"settings","b_cam_1":${b_cam_1},"b_cam_2":${b_cam_2},"b_cam_3":${b_cam_3},"wifi_router":${b_wifi_router}}`;

  //   sendState = JSON.stringify(sendState);
  //   sendState = JSON.parse(sendState);
  //   console.log(sendState);    
  //   ws.send(sendState);
	// }

  // setInterval(()=>{
  //   // ws.send("Qa Bone bir");
	// 	// console.log(b_cam_1);

  // },1000);


</script>



<div class="flex justify-center">
<img src="./image/foreveroceans.png" alt="forever oceans logo">
</div>

<MediaQuery query="(min-width: 1281px)" let:matches>
  {#if matches}
<div class="flex flex-col w-full border-opacity-50">


	<div class="grid h-30 card bg-base-300 rounded-box place-items-center">
    <div class="text-blue-900 mb-3 text-xl font-semibold">
    <h1>SCC Status</h1>
    </div>
    <br>
    <div class=" mb-3 text-blue-900  stats stats-horizontal shadow">
  
      <div class="stat">
        <div class="stat-title">Battery Voltage</div>
        <div class="overflow-auto text-2xl stat-value">{mes["data"]['battery-voltage']} V</div>
        <!-- <div class="stat-desc">Jan 1st - Feb 1st</div> -->
      </div>
      
      <div class="stat">
        <div class="stat-title">Solar Power</div>
        <div class="overflow-auto text-2xl stat-value">{mes["data"]['solar-power']} W</div>
        <!-- <div class="stat-desc">↗︎ 400 (22%)</div> -->
      </div>
      <div class="w-40 stat">
        <div class="stat-title">Yield Today</div>
        <div class="overflow-auto text-2xl stat-value">{mes["data"]['yield-today']} kWh</div>
        <!-- <div class="stat-desc">Jan 1st - Feb 1st</div> -->
      </div>
      
      <div class="w-40 stat">
        <div class="stat-title">Yield Yesterday</div>
        <div class="overflow-auto text-2xl stat-value">{mes["data"]['yield-yesterday']} kWh</div>
        <!-- <div class="stat-desc">↗︎ 400 (22%)</div> -->
      </div>
      
    </div>

  
    <div class="text-blue-900 mb-3 stats stats-horizontal shadow">
  
      <div class="stat">
        <div class="stat-title">Load Current</div>
        <div class="overflow-auto text-2xl stat-value">{mes["data"]['load-current']}  A</div>
        <!-- <div class="stat-desc">Jan 1st - Feb 1st</div> -->
      </div>
      
      <div class="stat">
        <div class="stat-title">Battery Current</div>
        <div class="overflow-auto text-2xl stat-value">{mes["data"]['battery-current']}  A</div>
        <!-- <div class="stat-desc">↗︎ 400 (22%)</div> -->
      </div>

      <div class="stat">
        <div class="stat-title">Temperature</div>
        <div class="overflow-auto text-2xl stat-value">{mes["data"]['temperature']} &#8451 </div>
        <!-- <div class="stat-desc">Jan 1st - Feb 1st</div> -->
      </div>  
      
      <div class="w-40 stat">
        <div class="stat-title">Humidity</div>
        <div class="overflow-auto text-2xl stat-value">{mes["data"]['humidity']} %</div>
        <!-- <div class="stat-desc">↗︎ 400 (22%)</div> -->
      </div>
      
    </div>
	</div>
  <div class="divider"></div>


  <div class="grid h-300 card bg-base-300 rounded-box place-items-center">
    <div class="text-blue-900 mb-3 text-xl font-semibold">
      <h1>Power Status</h1>
    </div>
     <br>

     <div class="grid grid-cols-4 gap-4">

      <div> 
          
        {#if b_wifi_router}
        <input on:click={() => onClick("ch4")} name="wifi_router" type="checkbox" class="toggle bg-blue-900" bind:checked={b_wifi_router} />
        
        <label class="text-blue-900 font-bold" for="wifi_router"> <p> WiFi Router </p></label>
        {:else}
        <input on:click={() => onClick("ch4")} name="wifi_router" type="checkbox" class="toggle bg-grey" bind:checked={b_wifi_router} />

        <label class="text-grey font-bold" for="wifi_router"><p> WiFi Router </p></label>
        {/if}
        <br>
      </div>

      <div>
        {#if b_cam_1}
        <input on:click={() => onClick("ch1")} id="cam_1" name="cam_1" type="checkbox" class="toggle bg-blue-900" bind:checked={b_cam_1} />
        <label class="text-blue-900 font-bold" for="cam_1"> <p> CAM_1 </p></label>
        {:else}
        <input on:click={() => onClick("ch1")} id="cam_1" name="cam_1" type="checkbox" class="toggle bg-grey" bind:checked={b_cam_1} />
        <label class="text-grey font-bold" for="cam_1"> <p> CAM_1 </p></label>
        {/if}
        <br>
        </div>

        <div>
          {#if b_cam_2}
          <input on:click={() => onClick("ch2")} name="cam_2" type="checkbox" class="toggle bg-blue-900" bind:checked={b_cam_2} />
          <label class="text-blue-900 font-bold" for="cam_2"> <p> CAM_2 </p> </label>
          {:else}
          <input on:click={() => onClick("ch2")} name="cam_2" type="checkbox" class="toggle bg-grey" bind:checked={b_cam_2} />
          <label class="text-grey font-bold" for="cam_2"> <p> CAM_2 </p></label>
          {/if}
          <br>
        </div>

        <div>
          {#if b_cam_3}
          <input on:click={() => onClick("ch3")} name="cam_3" type="checkbox" class="toggle bg-blue-900" bind:checked={b_cam_3} />

          <label class="text-blue-900 font-bold" for="cam_3"> <p> CAM_3 </p> </label>
          {:else}
          <input on:click={() => onClick("ch3")} name="cam_3" type="checkbox" class="toggle bg-grey" bind:checked={b_cam_3} />

          <label class="text-grey font-bold" for="cam_3"> <p> CAM_3 </p> </label>
          {/if}
          <br>
        </div>
      
       

    </div>

  
    <!-- <button on:click={handleClick} class="mb-3 btn btn-primary bg-blue-900">SAVE</button> -->
  
    
  </div>
  <div class="divider"></div>
  <div class="place-items-center">
  <div class="text-blue-900 mb-3 text-xl font-semibold">
    <h1>ESP32 OTA Firmware Update</h1>
  </div>
		<div>
			<label for="otafile">Firmware file:</label>
			<input type="file" id="otafile" name="otafile" />
		</div>
		<div>
			<button id="upload" type="button" on:click = {()=> startUpload()} >Upload</button>
		</div>
		<div id="progress"></div>
  </div>

  <div class="divider"></div>
  <div class="grid h-300 card bg-base-300 rounded-box place-items-center">
    <div class="text-blue-900 mb-3 text-xl font-semibold">
      <h1>Day/Night manual control for testing and debug</h1>
    </div>
  <br>

  <div class="grid grid-cols-2 gap-10">

    <div> 
        
      {#if manual_override}
      <input on:click={()=> {manual_override = false; manual_control();}} name="manual_override" type="checkbox" class="toggle bg-blue-900" bind:checked={manual_override} />
      
      <label class="text-blue-900 font-bold place-content-center" for="manual_override"> <p class="place-content-center"> Manual override </p></label>
      {:else}
      <input on:click={()=> {manual_override = true; manual_control();}} name="manual_override" type="checkbox" class="toggle bg-grey" bind:checked={manual_override} />

      <label class="text-grey font-bold" for="manual_override"><p> Manual override </p></label>
      {/if}
      <br>
    </div>

    <div> 
        
      {#if day}
      <input on:click={()=> {day = false; manual_control();}} name="day" type="checkbox" class="toggle bg-blue-900" bind:checked={day} />
      
      <label class="text-blue-900 font-bold" for="day"> <p> Day </p></label>
      {:else}
      <input on:click={()=> {day = true; manual_control();}} name="day" type="checkbox" class="toggle bg-grey" bind:checked={day} />

      <label class="text-grey font-bold" for="day"><p> Night </p></label>
      {/if}
      <br>
    </div>
  </div>
  </div>
  <div class="divider"></div>
  <div class="overflow-x-auto ml-auto mr-auto object-center">
    <table class="table-compact w-fit justify-center">
      <!-- head -->
      <!-- <thead>
        <tr>
          <th></th>
          <th>Name</th>
          <th>Job</th>
          <th>Favorite Color</th>
        </tr>
      </thead> -->
      <tbody>
        <!-- row 1 -->
        <tr>
          <th>Free memory</th>
          <td>{mes["in_state"]["free_memory"]}kB</td>
        </tr>
        <!-- row 2 -->
        <tr>
          <th>Min Free memory</th>
          <td>{mes["in_state"]["min_free_memory"]}kB</td>
        </tr>
        <!-- row 3 -->
        <tr>
          <th>State</th>
          <td>{mes["in_state"]["day"]}</td>
        </tr>
        <!----row 4-->
        <tr>
          <th>Ap IP</th>
          <td>{mes["in_state"]["ap_ip"]}</td>
        </tr>
        <!----row 5-->
        <tr>
          <th>ETH IP</th>
          <td>{mes["in_state"]["eth_ip"]}</td>
        </tr>
        <!----row 6-->
        <tr>
          <th>app version</th>
          <td>{mes["in_state"]["app_version"]}</td>
        </tr>
      </tbody>
    </table>
  </div>
</div>
	{/if}
</MediaQuery>

<MediaQuery query="(min-width: 481px) and (max-width: 1280px)" let:matches>
	{#if matches}
<div class="flex flex-col w-full border-opacity-50">

	<div class="grid h-30 card bg-base-300 rounded-box place-items-center">
    <div class="text-blue-900 mb-3 text-xl font-semibold">
      <h1>SCC Status</h1>
      </div>
<!--When in mobile FORMAT -->

<div class="mb-3 stats shadow">
  
  <div class="w-40 text-blue-900 stat">
    <div class="stat-title">Battery Voltage</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['battery-voltage']}</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>

  <div  class="w-40 text-blue-900 stat">
    <div class="stat-title">Solar Power</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['solar-power']} W</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>
  
</div>
<div class="mb-3 stats shadow">

  <div class="w-40 text-blue-900 stat">
    <div class="stat-title">Yield Today</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['yield-today']} kWh</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>

  <div  class="w-40 text-blue-900 stat">
    <div class="stat-title">Yield Yesterday</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['yield-yesterday']} kWh</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>
  
</div>
<div class="mb-3 stats shadow">

  <div class="w-40 text-blue-900 stat">
    <div class="stat-title">Load Current</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['load-current']} A</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>

  <div  class="w-40 text-blue-900 stat">
    <div class="stat-title">Battery Current</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['battery-current']} A</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>
  
</div>
<div class="mb-3 stats shadow">

  <div class="w-40 text-blue-900 stat">
    <div class="stat-title">Temperature</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['temperature']} &#8451</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>
  
  <div  class="w-40 text-blue-900 stat">
    <div class="stat-title">Humidity</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['humidity']} %</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>
  
</div>
</div>

<div class="divider"></div>


  <div class="grid h-30 card bg-base-300 rounded-box place-items-center">
    <div class="text-blue-900 mb-3 text-xl font-semibold">
      <h1>Power Status</h1>
    </div>
    <div class="mt-3 grid grid-rows-2 grid-flow-col gap-11">
      
      <div> 
          
        {#if b_wifi_router}
        <input on:click={() => onClick("ch4")} name="wifi_router" type="checkbox" class="toggle bg-blue-900" bind:checked={b_wifi_router} />
        
        <label class="text-blue-900 font-bold" for="wifi_router"> <p> WiFi Router </p></label>
        {:else}
        <input on:click={() => onClick("ch4")} name="wifi_router" type="checkbox" class="toggle bg-grey" bind:checked={b_wifi_router} />

        <label class="text-grey font-bold" for="wifi_router"><p> WiFi Router </p></label>
        {/if}
        <br>
      </div>

      <div>
        {#if b_cam_2}
        <input on:click={() => onClick("ch2")} name="cam_2" type="checkbox" class="toggle bg-blue-900" bind:checked={b_cam_2} />
        <label class="text-blue-900 font-bold" for="cam_2"> <p> CAM_2 </p> </label>
        {:else}
        <input on:click={() => onClick("ch2")} name="cam_2" type="checkbox" class="toggle bg-grey" bind:checked={b_cam_2} />
        <label class="text-grey font-bold" for="cam_2"> <p> CAM_2 </p></label>
        {/if}
        <br>
      </div>
      
      <div>
        {#if b_cam_1}
        <input on:click={() => onClick("ch1")} id="cam_1" name="cam_1" type="checkbox" class="toggle bg-blue-900" bind:checked={b_cam_1} />
        <label class="text-blue-900 font-bold" for="cam_1"> <p> CAM_1 </p></label>
        {:else}
        <input on:click={() => onClick("ch1")} id="cam_1" name="cam_1" type="checkbox" class="toggle bg-grey" bind:checked={b_cam_1} />
        <label class="text-grey font-bold" for="cam_1"> <p> CAM_1 </p></label>
        {/if}
        <br>
        </div>

        <div>
          {#if b_cam_3}
          <input on:click={() => onClick("ch3")} name="cam_3" type="checkbox" class="toggle bg-blue-900" bind:checked={b_cam_3} />

          <label class="text-blue-900 font-bold" for="cam_3"> <p> CAM_3 </p> </label>
          {:else}
          <input on:click={() => onClick("ch3")} name="cam_3" type="checkbox" class="toggle bg-grey" bind:checked={b_cam_3} />

          <label class="text-grey font-bold" for="cam_3"> <p> CAM_3 </p> </label>
          {/if}
          <br>
        </div>

    </div>

    <!-- <button on:click={handleClick} class="mb-3 btn btn-primary bg-blue-900">SAVE</button> -->

  </div>
  <div class="divider"></div>
  <div class="grid h-300 card bg-base-300 rounded-box place-items-center">
    <div class="text-blue-900 mb-3 text-xl font-semibold">
      <h1>Day/Night manual control for testing and debug</h1>
    </div>
  <br>

  <div class="grid grid-cols-2 gap-10">

    <div> 
        
      {#if manual_override}
      <input on:click={()=> {manual_override = false; manual_control();}} name="manual_override" type="checkbox" class="toggle bg-blue-900" bind:checked={manual_override} />
      
      <label class="text-blue-900 font-bold place-content-center" for="manual_override"> <p class="place-content-center"> Manual override </p></label>
      {:else}
      <input on:click={()=> {manual_override = true; manual_control();}} name="manual_override" type="checkbox" class="toggle bg-grey" bind:checked={manual_override} />

      <label class="text-grey font-bold" for="manual_override"><p> Manual override </p></label>
      {/if}
      <br>
    </div>

    <div> 
        
      {#if day}
      <input on:click={()=> {day = false; manual_control();}} name="day" type="checkbox" class="toggle bg-blue-900" bind:checked={day} />
      
      <label class="text-blue-900 font-bold" for="day"> <p> Day </p></label>
      {:else}
      <input on:click={()=> {day = true; manual_control();}} name="day" type="checkbox" class="toggle bg-grey" bind:checked={day} />

      <label class="text-grey font-bold" for="day"><p> Night </p></label>
      {/if}
      <br>
    </div>
  </div>

  </div>
  <div class="divider"></div>
  <div class="overflow-x-auto ml-auto mr-auto object-center">
    <table class="table-compact w-fit justify-center">
      <!-- head -->
      <!-- <thead>
        <tr>
          <th></th>
          <th>Name</th>
          <th>Job</th>
          <th>Favorite Color</th>
        </tr>
      </thead> -->
      <tbody>
        <!-- row 1 -->
        <tr>
          <th>Free memory</th>
          <td>{mes["in_state"]["free_memory"]}kB</td>
        </tr>
        <!-- row 2 -->
        <tr>
          <th>Min Free memory</th>
          <td>{mes["in_state"]["min_free_memory"]}kB</td>
        </tr>
        <!-- row 3 -->
        <tr>
          <th>State</th>
          <td>{mes["in_state"]["day"]}</td>
        </tr>
        <!----row 4-->
        <tr>
          <th>Ap IP</th>
          <td>{mes["in_state"]["ap_ip"]}</td>
        </tr>
        <!----row 5-->
        <tr>
          <th>ETH IP</th>
          <td>{mes["in_state"]["eth_ip"]}</td>
        </tr>
        <!----row 6-->
        <tr>
          <th>APP version</th>
          <td>{mes["in_state"]["app_version"]}</td>
        </tr>
      </tbody>
    </table>
  </div>
<!--When in mobile FORMAT -->
	</div>
  
	{/if}
</MediaQuery>

<MediaQuery query="(max-width: 480px)" let:matches>
	{#if matches}
<div class="flex flex-col w-full border-opacity-50">

		<div class="grid h-30 card bg-base-300 rounded-box place-items-center">
    <div class="text-blue-900 mb-3 text-xl font-semibold">
      <h1>SCC Status</h1>
      </div>
<!--When in mobile FORMAT -->

<div class="mb-3 stats shadow">
  
  <div class="w-40 text-blue-900 stat">
    <div class="stat-title">Battery Voltage</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['battery-voltage']}</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>

  <div  class="w-40 text-blue-900 stat">
    <div class="stat-title">Solar Power</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['solar-power']} W</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>
  
</div>
<div class="mb-3 stats shadow">

  <div class="w-40 text-blue-900 stat">
    <div class="stat-title">Yield Today</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['yield-today']} kWh</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>

  <div  class="w-40 text-blue-900 stat">
    <div class="stat-title">Yield Yesterday</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['yield-yesterday']} kwh</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>
  
</div>
<div class="mb-3 stats shadow">

  <div class="w-40 text-blue-900 stat">
    <div class="stat-title">Load Current</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['load-current']} A</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>

  <div  class="w-40 text-blue-900 stat">
    <div class="stat-title">Battery Current</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['battery-current']}  A</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>
  
</div>
<div class="mb-3 stats shadow">

  <div class="w-40 text-blue-900 stat">
    <div class="stat-title">Temperature</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['temperature']} &#8451</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>
  
  <div  class="w-40 text-blue-900 stat">
    <div class="stat-title">Humidity</div>
    <div class="overflow-scroll text-2xl stat-value">{mes["data"]['humidity']} %</div>
    <!-- <div class="stat-desc">21% more than last month</div> -->
  </div>
  
</div>

<!--When in mobile FORMAT -->
	</div>

  <div class="divider"></div>


  <div class="grid h-30 card bg-base-300 rounded-box place-items-center">
    <div class="text-blue-900 mb-3 text-xl font-semibold">
      <h1>Power Status</h1>
    </div>
    <div class="mt-3 grid grid-rows-2 grid-flow-col gap-11">

      <div> 
          
        {#if b_wifi_router}
        <input on:click={() => onClick("ch4")} name="wifi_router" type="checkbox" class="toggle bg-blue-900" bind:checked={b_wifi_router} />
        
        <label class="text-blue-900 font-bold" for="wifi_router"> <p> WiFi Router </p></label>
        {:else}
        <input on:click={() => onClick("ch4")} name="wifi_router" type="checkbox" class="toggle bg-grey" bind:checked={b_wifi_router} />

        <label class="text-grey font-bold" for="wifi_router"><p> WiFi Router </p></label>
        {/if}
        <br>
      </div>

      <div>
        {#if b_cam_2}
        <input on:click={() => onClick("ch2")} name="cam_2" type="checkbox" class="toggle bg-blue-900" bind:checked={b_cam_2} />
        <label class="text-blue-900 font-bold" for="cam_2"> <p> CAM_2 </p> </label>
        {:else}
        <input on:click={() => onClick("ch2")} name="cam_2" type="checkbox" class="toggle bg-grey" bind:checked={b_cam_2} />
        <label class="text-grey font-bold" for="cam_2"> <p> CAM_2 </p></label>
        {/if}
        <br>
      </div>

      <div>
        {#if b_cam_1}
        <input on:click={() => onClick("ch1")} id="cam_1" name="cam_1" type="checkbox" class="toggle bg-blue-900" bind:checked={b_cam_1} />
        <label class="text-blue-900 font-bold" for="cam_1"> <p> CAM_1 </p></label>
        {:else}
        <input on:click={() => onClick("ch1")} id="cam_1" name="cam_1" type="checkbox" class="toggle bg-grey" bind:checked={b_cam_1} />
        <label class="text-grey font-bold" for="cam_1"> <p> CAM_1 </p></label>
        {/if}
        <br>
      </div>

        <div>
          {#if b_cam_3}
          <input on:click={() => onClick("ch3")} name="cam_3" type="checkbox" class="toggle bg-blue-900" bind:checked={b_cam_3} />

          <label class="text-blue-900 font-bold" for="cam_3"> <p> CAM_3 </p> </label>
          {:else}
          <input on:click={() => onClick("ch3")} name="cam_3" type="checkbox" class="toggle bg-grey" bind:checked={b_cam_3} />

          <label class="text-grey font-bold" for="cam_3"> <p> CAM_3 </p> </label>
          {/if}
          <br>
        </div>
      
        
        

    </div>

    <!-- <div class="flex-row justify-end">
      <div>
      <button on:click={handleClick} class="mb-3 btn btn-primary bg-blue-900">SAVE</button>
      </div>
    </div> -->

  </div>

  
	
  <div class="divider"></div>
  <div class="grid h-300 card bg-base-300 rounded-box place-items-center">
    <div class="text-blue-900 mb-3 text-l font-bold">
      <h1>Day/Night manual control for testing and debug</h1>
    </div>
  <br>

  <div class="grid grid-cols-2 gap-10">

    <div> 
        
      {#if manual_override}
      <input on:click={()=> {manual_override = false; manual_control();}} name="manual_override" type="checkbox" class="toggle bg-blue-900" bind:checked={manual_override} />
      
      <label class="text-blue-900 font-bold place-content-center" for="manual_override"> <p class="place-content-center"> Manual override </p></label>
      {:else}
      <input on:click={()=> {manual_override = true; manual_control();}} name="manual_override" type="checkbox" class="toggle bg-grey" bind:checked={manual_override} />

      <label class="text-grey font-bold" for="manual_override"><p> Manual override </p></label>
      {/if}
      <br>
    </div>

    <div> 
        
      {#if day}
      <input on:click={()=> {day = false; manual_control();}} name="day" type="checkbox" class="toggle bg-blue-900" bind:checked={day} />
      
      <label class="text-blue-900 font-bold" for="day"> <p> Day </p></label>
      {:else}
      <input on:click={()=> {day = true; manual_control();}} name="day" type="checkbox" class="toggle bg-grey" bind:checked={day} />

      <label class="text-grey font-bold" for="day"><p> Night </p></label>
      {/if}
      <br>
    </div>
  </div>
</div>
  <div class="divider"></div>
  <div class="overflow-x-auto ">
    <table class="table-normal w-full ">
      <!-- head -->
      <!-- <thead>
        <tr>
          <th></th>
          <th>Name</th>
          <th>Job</th>
          <th>Favorite Color</th>
        </tr>
      </thead> -->
      <tbody>
        <!-- row 1 -->
        <tr>
          <th>Free memory</th>
          <td>{mes["in_state"]["free_memory"]}kB</td>
        </tr>
        <!-- row 2 -->
        <tr>
          <th>Min Free memory</th>
          <td>{mes["in_state"]["min_free_memory"]}kB</td>
        </tr>
        <!-- row 3 -->
        <tr>
          <th>State</th>
          <td>{mes["in_state"]["day"]}</td>
        </tr>
        <!----row 4-->
        <tr>
          <th>Ap IP</th>
          <td>{mes["in_state"]["ap_ip"]}</td>
        </tr>
        <!----row 5-->
        <tr>
          <th>ETH IP</th>
          <td>{mes["in_state"]["eth_ip"]}</td>
        </tr>
      </tbody>
    </table>
  </div>

</div>

	{/if}
</MediaQuery>

<!-- <h1 style="color:{col};">Hello {name}!</h1> -->


<slot />