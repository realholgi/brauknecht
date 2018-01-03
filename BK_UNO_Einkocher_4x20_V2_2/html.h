const char PAGE_Kochen[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="de">
<head>
  <title>BrauKnecht</title>
  <meta charset="utf-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <!-- The above 3 meta tags *must* come first in the head; any other head content must come *after* these tags -->
  <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css" integrity="sha384-BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/K68vbdEjh4u" crossorigin="anonymous">
  <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap-theme.min.css" integrity="sha384-rHyoN1iRsVXV4nD0JutlnGaslCJuC7uwjduW9SVrLvRYooPp2bWYgmgJQIXwl/Sp" crossorigin="anonymous">
  <!--[if lt IE 9]>
 <script src="https://oss.maxcdn.com/html5shiv/3.7.3/html5shiv.min.js"></script>
  <script src="https://oss.maxcdn.com/respond/1.4.2/respond.min.js"></script>
<![endif]-->
</head>

<body>
<div class="container">

  <div class="page-header">
      <h1>BrauKnecht</h1>
  </div>

  <div id="error"></div>

  <div class="alert alert-danger" role="alert" id="alarm" style="display:none;">RUFALARM</div>


  <div class="panel panel-primary">
    <div class="panel-heading">
    <h3 class="panel-title"><span id="title">unknown</span></h3>
    </div>
    <div class="panel-body">
      <ul>
        <li>Ist-Wert: <span id="temp_ist"></span>&deg;C</li>
        <li>Sollwert: <span id="temp_soll"></span>&deg;C</li>
        <li>Heizung: <span id="heizung"></span></li>
      </ul>
    </div>
  </div>

  <div class="panel panel-info" id="zweites" style="display: none;">
    <div class="panel-heading">
      <h3 class="panel-title"><span id="title2">unknown</span></h3>
    </div>
    <div class="panel-body">
      <ul id="data">
      </ul>
    </div>
  </div>

</div>
<!-- /container -->
<script src="https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js"></script>
<script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js" integrity="sha384-Tc5IQib027qvyjSMfHjOMaLkfuWVxZxUPnCJA7l2mCWNIpG9mGCD8wGNIcPD7Txa" crossorigin="anonymous"></script>
<script src="https://unpkg.com/axios/dist/axios.min.js"></script>

<script>
   Number.prototype.round = function(decimals) { return Number((Math.round(this + "e" + decimals)  + "e-" + decimals)); }
   var fn = function(){
     axios.get('data.json')
       .then(function (response) {   
          document.getElementById('title').innerHTML = response.data.title;
          document.getElementById('temp_ist').innerHTML = response.data.temp_ist.round(1);
          document.getElementById('temp_soll').innerHTML = response.data.temp_soll.round(0);
          document.getElementById('heizung').innerHTML = response.data.heizung;

          document.getElementById('zweites').style.display = 'none';
          
          if (response.data.data && response.data.data.length>0) {
            document.getElementById('title2').innerHTML = response.data.title2;       
            document.getElementById('data').innerHTML = response.data.data;
            document.getElementById('zweites').style.display = 'block';
          }

          document.getElementById('alarm').style.display = 'none';

          //console.log("MODUS: "+ response.data.modus);
          //console.log("RUFMODUS: "+ response.data.rufmodus);

          if (response.data.rufmodus == 28 || response.data.modus == 18) {
            document.getElementById('alarm').style.display = 'block';
          }

          document.getElementById('error').innerHTML = '';
        })
       .catch(function (err) {
         document.getElementById('error').innerHTML = '<div class="alert alert-danger" role="alert">' + err.message + '</div>';
       });
       };
       fn();
       var interval = setInterval(fn, 1000);
 </script>

</body></html>
)=====";



