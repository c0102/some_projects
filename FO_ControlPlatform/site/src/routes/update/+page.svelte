<script lang="ts">

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
</script>


<div class="grid place-items-center">
    <div class="text-blue-900 mb-3 text-xl font-semibold">
      <h1>ESP32 OTA Firmware Update</h1>
    </div>
          <div>
              <label class="text-blue-900 font-bold mr-10"  for="otafile">Firmware file:  </label>
              <input type="file" id="otafile" name="otafile" class="file-input file-input-bordered"/>
          </div>
          <div class="justify-center">
              <button class="btn mt-10" id="upload" type="button" on:click = {()=> startUpload()} >Upload</button>
          </div>
          <div id="progress"></div>
    </div>