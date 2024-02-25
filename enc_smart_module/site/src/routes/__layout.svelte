<script>
  import "../app.css";
</script>


<div class="drawer drawer-mobile">
  <input id="my-drawer-2" type="checkbox" class="drawer-toggle" />
  <div class="drawer-content flex flex-col items-center justify-center">
    <slot />
    <label for="my-drawer-2" class="btn btn-primary drawer-button lg:hidden">Menu</label>
  
  </div> 
  <div class="drawer-side">
    <label for="my-drawer-2" class="drawer-overlay"></label> 
    <ul class="menu p-4 overflow-y-auto w-80 bg-base-100 text-base-content">
      <!-- Sidebar content here -->
      <li><a href="/">Home</a></li>
      <li><a href="./toggle_relay">Relay</a></li>
      <li><a href="./switch_ap">Get APs</a></li>
      <li><a href="./settings">Settings</a></li>

      
    </ul>
  
  </div>
</div>