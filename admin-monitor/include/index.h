const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE HTML><html>
<head>
  <title>Audio Guestbook</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p { font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #50B8B4; color: white; font-size: 1rem; }
    .content { padding: 15px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 800px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
    .reading { font-size: 1.3rem; }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>Audio Guestbook</h1>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card">
        <p style="color:rgb(10, 66, 64);">FIRMWARE</p><p><span class="reading"><span id="status">%STATUS%</span></span></p>
      </div>
      <div class="card">
        <p style="color:rgb(10, 66, 64);">RECORDINGS</p><p><span class="reading"><span id="rec">%RECORDINGS%</span></span></p>
      </div>
      <div class="card">
        <p style="color:rgb(10, 66, 64);">DISK SPACE REMAINING</p><p><span class="reading"><span id="disk">%DISKSPACE%</span> Gb</span></p>
      </div>
      <div class="card">
        <p style="color:rgb(10, 66, 64);">RUN TIME</p>
        <p><span class="reading">
          <span id="rt">%RUNTIME%</span>
        </span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('status', function(e) {
  console.log("status", e.data);
  document.getElementById("stat").innerHTML = e.data;
 }, false);
 
 source.addEventListener('recordings', function(e) {
  console.log("recordings", e.data);
  document.getElementById("rec").innerHTML = e.data;
 }, false);
 
 source.addEventListener('diskspace', function(e) {
  console.log("diskspace", e.data);
  document.getElementById("disk").innerHTML = e.data;
 }, false);

 source.addEventListener('runtime', function(e) {
  console.log("runtime", e.data);
  document.getElementById("rt").innerHTML = e.data;
 }, false);

}
</script>
</body>
</html>
)=====";