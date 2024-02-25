<script lang='ts'>

    import {WebsocketBuilder} from 'websocket-ts';
    import { onMount } from 'svelte'; 
    
    setInterval
    let mes = {
                state:{
                  "ch1":false,
                  "ch2":false,
                  "ch3":false,
                  "ch4":false,
                  "ch1_latch":false,
                  "ch2_latch":false,
                  "ch3_latch":false,
                  "ch4_latch":false
                },
                data:
                {"temperature":0.0, 
                 "humidity":0.0,
                 "battery-voltage": 0.0,
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
                },
                "server_resetting":'false'
              };
    
    let state = mes.state;
    let data = mes.data;
    let in_state = mes.in_state;


    let b_cam_1 = false;
    let b_cam_2 = false;
    let b_cam_3 = false;
    let b_wifi_router = false;
    
    let manual_override = false;
    let day = false;
    let socket = null; // new WebsocketBuilder("ws://"+window.location.host+":80/ws");
    let reset = false; 
    
    onMount(async () => {
      connectWS();
    });

   function  connectWSbuilder(){

    const ws = new WebsocketBuilder('/ws')
    .onOpen((i, ev) => { console.log("opened") })
    .onClose((i, ev) => { console.log("closed") })
    .onError((i, ev) => { console.log("error") })
    .onMessage((i, ev) => { console.log("message");mes = JSON.parse(ev.data);
                            state = mes.state;
                            data = Object.keys(mes.data).length > 1 ? mes.data: data;
                            in_state = mes.in_state; })
    .onRetry((i, ev) => { console.log("retry") })
    .build();
   
  }
      
    function connectWS(){
        //alert(navigator.userAgent);
        if( socket == null){
          
          socket = new WebSocket("ws://"+window.location.host+":80/ws");
          socket.onopen = () => {console.log("opened");}
          socket.onmessage = (event) => {
                            mes = JSON.parse(event.data);
                            state = mes.state;
                            data = Object.keys(mes.data).length > 1 ? mes.data: data;
                            in_state = Object.keys(mes.in_state).length > 1 ? mes.in_state : in_state;
                            reset = mes.server_resetting == 'true';

                            b_cam_1 = "ch1" in state ? state["ch1"] : b_cam_1;
                            b_cam_2 = "ch2" in state ? state["ch2"] : b_cam_2;
                            b_cam_3 = "ch3" in state ? state["ch3"] : b_cam_3;
                            b_wifi_router = "ch4" in state ? state["ch4"] : b_wifi_router;
                            console.log(mes); 
                            console.log(reset);
                            console.log('b_cam_1:', b_cam_1);
                            console.log('b_cam_1:', b_cam_2);
                            console.log('b_cam_1:', b_cam_3);
                            console.log('b_wifi_router:', b_wifi_router);

                            if(reset){
                              socket.close();
                              socket = null;
                              console.log("connection closed, reconnecting...")
                              setTimeout(connectWS,500);
                              
                            }

                      }
          socket.addEventListener('error', (event) => {
              console.log('WebSocket error: ', event);
              });
          // socket.onclose = () =>{
            
          // }

        }else{

          

        }
    }
    
  async function onClick(ch:string) {
          if(ch === "ch1"){
            state["ch1"] = !b_cam_1;
            await fetch("/api/toggle-switch", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({"cmd":"toggle_ch1", is_on: !b_cam_1 }),
          });
    
          }else if(ch === "ch2"){
            state["ch2"] = !b_cam_2;
            await fetch("/api/toggle-switch", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({"cmd":"toggle_ch2", is_on: !b_cam_2 }),
          });
          }else if(ch === "ch3"){
    
            state["ch3"] = !b_cam_3;
            await fetch("/api/toggle-switch", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({"cmd":"toggle_ch3", is_on: !b_cam_3 }),
          });
    
          }else if(ch === "ch4"){
    
            state["ch4"] = !b_wifi_router;
            await fetch("/api/toggle-switch", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({"cmd":"toggle_ch4", is_on: !b_wifi_router }),
          });
          }
        
        }

  async function manual_control() {
      await fetch("/api/manual-control", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({"cmd":"manual_control", "manual_override": manual_override, "day":day }),
      });
  }
    
    
    
    </script>
    
    
    <!-- <div class="flex justify-center">
    <img src="/foreveroceans.png" alt="forever oceans logo">
    </div> -->
    
    <svelte:window on:load={connectWS()}/>

    <div  class="flex flex-row text-blue-900 mb-3 justify-center text-xl font-semibold">
        <h1>SCC Status</h1>
    </div>
    <div class="flex justify-center">
     <div class="flex items-start flex-row lg:flex-col justify-center  ">
      <div class="stats text-blue-900 justify-center mb-3 stats-vertical lg:stats-horizontal  shadow">  
          
          <div class="w-40 stat ">
            <div class="stat-title">Battery Voltage</div>
            <div class="overflow-auto text-2xl stat-value">{data['battery-voltage']} V</div>
            <!-- <div class="stat-desc">Jan 1st - Feb 1st</div> -->
          </div>
          
          <div class="w-40 stat ">
            <div class="stat-title">Solar Power</div>
            <div class="overflow-auto text-2xl stat-value">{data['solar-power']} W</div>
            <!-- <div class="stat-desc">↗︎ 400 (22%)</div> -->
          </div>
    
          <div class="w-40 stat flex-none ">
            <div class="stat-title">Yield Today</div>
            <div class="overflow-auto text-2xl stat-value">{data['yield-today']} Wh</div>
            <!-- <div class="stat-desc">Jan 1st - Feb 1st</div> -->
          </div>
          
          <div class="w-40 stat ">
            <div class="stat-title">Yield Yesterday</div>
            <div class="overflow-auto text-2xl stat-value">{data['yield-yesterday']} Wh</div>
            <!-- <div class="stat-desc">↗︎ 400 (22%)</div> -->
          </div> 
          </div>
    
    
          <div class="stats text-blue-900 justify-center mb-3 stats-vertical lg:stats-horizontal  shadow">   
          <div class="w-40 stat ">
            <div class="stat-title">Load Current</div>
            <div class="overflow-auto text-2xl stat-value">{data['load-current']}  A</div>
            <!-- <div class="stat-desc">Jan 1st - Feb 1st</div> -->
          </div>
          
          <div class="w-40 stat ">
            <div class="stat-title">Battery Current</div>
            <div class="overflow-auto text-2xl stat-value">{data['battery-current']}  A</div>
            <!-- <div class="stat-desc">↗︎ 400 (22%)</div> -->
          </div>
    
          <div class="w-40 stat ">
            <div class="stat-title">Temperature</div>
            <div class="overflow-auto text-2xl stat-value">{data['temperature']} &#8451 </div>
            <!-- <div class="stat-desc">Jan 1st - Feb 1st</div> -->
          </div>  
          
          <div class="w-40 stat ">
            <div class="stat-title">Humidity</div>
            <div class="overflow-auto text-2xl stat-value">{data['humidity']} %</div>
            <!-- <div class="stat-desc">↗︎ 400 (22%)</div> -->
          </div>
          
        </div>
        </div>
        </div>
    
    <div class="divider"></div>
      
    <div class="grid h-300 card bg-base-300 rounded-box place-items-center">
        <div class="text-blue-900 mb-3 text-xl font-semibold">
          <h1>Power Status</h1>
        </div>
         <br>
    
         <div class="grid grid-cols-2 lg:grid-cols-4 grid-rows-2 lg:grid-rows-1 gap-4">
    
          <div> 
              
            {#if b_wifi_router}
            <input on:click={() =>{ onClick("ch4"); connectWSbuilder();}} name="wifi_router" type="checkbox" class="toggle bg-blue-900" bind:checked={b_wifi_router} />
            {#if state["ch4_latch"]}&#128274;{/if}
            <label class="text-blue-900 font-bold" for="wifi_router"> <p> WiFi Router</p></label>
            {:else}
            <input on:click={() => onClick("ch4")} name="wifi_router" type="checkbox" class="toggle bg-grey" bind:checked={b_wifi_router} />
            {#if state["ch4_latch"]}&#128274;{/if}
            <label class="text-grey font-bold" for="wifi_router"><p> WiFi Router </p></label>
            {/if}
            <br>
          </div>
    
          <div>
            {#if b_cam_1}
            <input on:click={() => onClick("ch1")} id="cam_1" name="cam_1" type="checkbox" class="toggle bg-blue-900" bind:checked={b_cam_1} />
            {#if state["ch1_latch"]}&#128274;{/if}
            <label class="text-blue-900 font-bold" for="cam_1"> <p> CAM_1 </p></label>
            {:else}
            <input on:click={() => onClick("ch1")} id="cam_1" name="cam_1" type="checkbox" class="toggle bg-grey" bind:checked={b_cam_1} />
            {#if state["ch1_latch"]}&#128274;{/if}
            <label class="text-grey font-bold" for="cam_1"> <p> CAM_1 </p></label>
            {/if}
            <br>
            </div>
    
            <div>
              {#if b_cam_2}
              <input on:click={() => onClick("ch2")} name="cam_2" type="checkbox" class="toggle bg-blue-900" bind:checked={b_cam_2} />
              {#if state["ch2_latch"]}&#128274;{/if}
              <label class="text-blue-900 font-bold" for="cam_2"> <p> CAM_2 </p> </label>
              {:else}
              <input on:click={() => onClick("ch2")} name="cam_2" type="checkbox" class="toggle bg-grey" bind:checked={b_cam_2} />
              {#if state["ch2_latch"]}&#128274;{/if}
              <label class="text-grey font-bold" for="cam_2"> <p> CAM_2 </p></label>
              {/if}
              <br>
            </div>
    
            <div>
              {#if b_cam_3}
              <input on:click={() => onClick("ch3")} name="cam_3" type="checkbox" class="toggle bg-blue-900" bind:checked={b_cam_3} />
              {#if state["ch3_latch"]}&#128274;{/if}
              <label class="text-blue-900 font-bold" for="cam_3"> <p> CAM_3 </p> </label>
              {:else}
              <input on:click={() => onClick("ch3")} name="cam_3" type="checkbox" class="toggle bg-grey" bind:checked={b_cam_3} />
              {#if state["ch3_latch"]}&#128274;{/if}
              <label class="text-grey font-bold" for="cam_3"> <p> CAM_3 </p> </label>
              {/if}
              <br>
            </div>
        </div>
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
    <div class="flex justify-center">
      <div class="overflow-x-auto ml-auto mr-auto justify-center">
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
              <td>{in_state["free_memory"]}kB</td>
            </tr>
            <!-- row 2 -->
            <tr>
              <th>Min Free memory</th>
              <td>{in_state["min_free_memory"]}kB</td>
            </tr>
            <!-- row 3 -->
            <tr>
              <th>State</th>
              <td>{in_state["day"]}</td>
            </tr>
            <!----row 4-->
            <tr>
              <th>Ap IP</th>
              <td>{in_state["ap_ip"]}</td>
            </tr>
            <!----row 5-->
            <tr>
              <th>ETH IP</th>
              <td>{in_state["eth_ip"]}</td>
            </tr>
            <!----row 6-->
            <tr>
              <th>app version</th>
              <td>{in_state["app_version"]}</td>
            </tr>
          </tbody>
        </table>
        <p></p>
      </div>
    </div>