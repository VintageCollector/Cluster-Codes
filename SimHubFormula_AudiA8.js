// Recommended: select JavaScript mode in SimHub's formula editor.
// Audi A8 4H0 920 980 K; protocol v6 (31 fields). JavaScript mode.
// Set optional paths using real properties from SimHub Available Properties.
// A null optional path is intentionally unsupported and returns 0.
var diagnosticMode=false; // true returns readable telemetry; driving times out
var optional = {
 low:null, high:null, fog:null, brake:null, seatbelt:null, doors:null,
 abs:null, tc:null, tcOff:null, escOff:null, pcs:null, lda:null, cruise:null, mode:null, cel:null,
 left:null, right:null, hazard:null, park:null, flash:null
};
// Set true for a profile whose numeric forward gears should display M1..M8.
var manualForwardGears=false;
// Optional named selector for games without mode telemetry (e.g. ETS2/ATS).
// null=automatic; or 'Comfort', 'Sport', 'Sport+', 'ESC & TC OFF'.
var modeSelection=null;
function prop(path, fallback) {
 if(!path) return fallback;
 try { var x=$prop(path); return (x===null || x===undefined)?fallback:x; }
 catch(e) { return fallback; }
}
function number(path, lo, hi) {
 var x=Number(prop(path,0)); return isFinite(x)?Math.max(lo,Math.min(hi,Math.round(x))):0;
}
function flag(name) {
 var x=prop(optional[name],0);
 var text=String(x).toLowerCase();
 if(x===true || text==='true' || text==='on')return 1;
 if(x===false || text==='false' || text==='off')return 0;
 var n=Number(x);return isFinite(n)&&n>0?1:0;
}
function booleanValue(x) {
 if(x===null||x===undefined||x==='')return null;
 var text=String(x).toLowerCase();
 if(x===true||text==='true'||text==='on')return 1;
 if(x===false||text==='false'||text==='off')return 0;
 var n=Number(x);return isFinite(n)?(n>0?1:0):null;
}
function firstFlag(paths) {
 for(var i=0;i<paths.length;i++){var x=booleanValue(prop(paths[i],null));if(x!==null)return x;}
 return 0;
}
function integer(name,hi) { return number(optional[name],0,hi); }
// Serial mode: 0 Comfort,1 Sport,2 Sport+,3 unknown,4 ESC/TC OFF.
function decodeMode(value,allowNumeric) {
 if(value===null||value===undefined||value==='')return 3;
 if(allowNumeric && (typeof value==='number'||/^[0-4]$/.test(String(value))) && Number(value)>=0 && Number(value)<=4 && Number(value)===Math.floor(Number(value)))return Number(value);
 var name=String(value).toLowerCase().replace(/[^a-z0-9+]/g,'');
 if(name==='comfort'||name==='eco'||name==='normal')return 0;
 if(name==='sport')return 1;
 if(name==='sport+'||name==='sportplus')return 2;
 if(name==='esctcoff'||name==='escandtcoff'||name==='escoff'||name==='tcoff'||name==='off')return 4;
 return 3;
}
var running=prop('DataCorePlugin.GameRunning',false);
running=running===true || running===1 || running==='1' || String(running).toLowerCase()==='true';
var g=String(prop('DataCorePlugin.GameData.NewData.Gear','N')).toUpperCase().replace(/^\s+|\s+$/g,'');
var manualMatch=/^M([1-8])$/.exec(g);
var automaticMatch=/^[DS]([1-9]|1[0-8])$/.exec(g);
var gear=g==='P'?-2:(g==='R'?-1:(g==='N'?0:((g==='D'||g==='S')?100:(automaticMatch?Number(automaticMatch[1]):(manualMatch?100+Number(manualMatch[1]):Number(g))))));
if(!isFinite(gear)) gear=0; gear=Math.round(gear);
if(gear!==100 && !(gear>=101 && gear<=108)) gear=Math.max(-2,Math.min(18,gear));
if(manualForwardGears && gear>=1 && gear<=18) gear=100+Math.min(8,gear);
// Game adapters follow the property paths used by the user's Panamera project.
var game=String(prop('DataCorePlugin.CurrentGame','')).toLowerCase().replace(/[^a-z0-9]/g,'');
var beam=game.indexOf('beamng')>=0;
var truck=game==='ets2'||game==='ats'||game.indexOf('eurotrucksimulator')>=0||game.indexOf('americantrucksimulator')>=0;
var raw='DataCorePlugin.GameRawData.';
var dashLightText=[prop(raw+'ShowLights',''),prop('GameRawData.ShowLights',''),prop('DataCorePlugin.GameData.NewData.Light',''),prop('DataCorePlugin.GameData.NewData.DashLights','')].join(' ');

var userAbsPath=optional.abs;
function use(name,path) { if(!optional[name]) optional[name]=path; }
// ETS2/ATS reports active cruise as a nonzero cruise set speed.
// A valid zero wins over any fallback; never latch a remembered set speed.
var truckCruiseSource=null,truckCruiseSpeed=null;
if(truck && !optional.cruise) {
 var cruisePaths=[raw+'TruckValues.CurrentValues.DashboardValues.CruiseControlSpeed.Kph',
  'GameRawData.TruckValues.CurrentValues.DashboardValues.CruiseControlSpeed.Kph',
  raw+'TruckValues.CurrentValues.DashboardValues.CruiseControlSpeed.Value',
  'GameRawData.TruckValues.CurrentValues.DashboardValues.CruiseControlSpeed.Value'];
 for(var c=0;c<cruisePaths.length;c++) {
  var cruiseRaw=prop(cruisePaths[c],null);
  if(cruiseRaw===null||cruiseRaw===undefined||cruiseRaw==='')continue;
  var cruiseSpeed=Number(cruiseRaw);
  if(!isFinite(cruiseSpeed)||cruiseSpeed<0)continue;
  truckCruiseSource=cruisePaths[c];truckCruiseSpeed=cruiseSpeed;break;
 }
}
if(beam) {
 // Keep paths unset here: below we combine the available raw lighting fields
 // and fall back only when a primary parking-brake value is missing.
} else if(truck) {
 use('low',raw+'Lights.LowBeams');use('high',raw+'Lights.HighBeams');use('fog',raw+'Lights.FogFront');
 use('brake',raw+'TruckValues.CurrentValues.MotorValues.BrakeValues.ParkingBrake');
} else {
 use('brake','DataCorePlugin.GameData.NewData.Handbrake');
 use('abs','DataCorePlugin.GameData.NewData.ABSActive');
}
// Relay requests deliberately require steady command overrides, never lamp blink phase.
var values=[1,Date.now()%1000000,running?1:0,
 number('DataCorePlugin.GameData.NewData.Rpms',0,12000),
 number('DataCorePlugin.GameData.NewData.SpeedKmh',0,400),gear,
 flag('low'),flag('high'),flag('fog'),flag('brake'),flag('seatbelt'),integer('doors',255),
 flag('abs'),integer('tc',2),integer('pcs',3),flag('lda'),integer('cruise',2),3,
 flag('cel'),flag('left'),flag('right'),flag('hazard'),flag('park'),flag('flash')];
if(truck && !optional.cruise)values[16]=truckCruiseSpeed!==null&&truckCruiseSpeed>0?2:0;
if(beam) {
 var lightText=dashLightText;
 function hasToken(token){return (' '+lightText.replace(/[^A-Za-z0-9_]+/g,' ')+' ').indexOf(' '+token+' ')>=0?1:0;}
 if(!optional.high){var highRaw=booleanValue(prop(raw+'light_HighBeam',null));values[7]=highRaw===null?(firstFlag(['TelemetryData.HighBeamsOn'])||hasToken('DL_FULLBEAM')):highRaw;}
 if(!optional.low){
  var lowRaw=booleanValue(prop(raw+'light_LowBeam',null)),combinedRaw=booleanValue(prop(raw+'light_LowHighBeam',null));
  values[6]=lowRaw!==null?lowRaw:(combinedRaw!==null?combinedRaw:(firstFlag(['TelemetryData.HeadlightsOn'])||hasToken('DL_LOWBEAM')));
 }
 if(!optional.fog){var fogRaw=booleanValue(prop(raw+'light_Fog',null));values[8]=fogRaw===null?hasToken('DL_FOG'):fogRaw;}
 if(!optional.brake)values[9]=firstFlag([raw+'input_parkingBrake',raw+'light_Parkingbrake','DataCorePlugin.GameData.NewData.Handbrake','GameData.Handbrake']);
 if(!optional.abs)values[12]=firstFlag(['DataCorePlugin.GameData.NewData.ABSActive','GameData.ABSActive']);
 if(!optional.tc)values[13]=firstFlag(['DataCorePlugin.GameData.NewData.TCActive','GameData.TCActive']);
 // light_Abs is deliberately excluded: ABS configuration is not intervention.
}
// Reuse the exact v975 companion layout; ignore incompatible marker versions.
var tag=Number(prop(raw+'reserved_4',-1));
var companionVersion=isFinite(tag)&&tag>=0?Math.floor(tag)%1024:-1;
var companion=beam && (companionVersion===975||companionVersion===976||companionVersion===977||companionVersion===978||companionVersion===979||companionVersion===980);
if(running && companion) {
 var packed=Number(prop(raw+'reserved_3',-1));
 var absPacked=Number(prop(raw+'reserved_2',-1));
 if(!optional.abs && isFinite(absPacked) && absPacked>=0) values[12]=Math.floor(absPacked)%2;
 if((companionVersion>=976&&companionVersion<=980) && isFinite(absPacked) && absPacked>=0) {
  var lampMask=Math.floor(absPacked/65536)%16;
  if(!optional.high)values[7]=Math.floor(lampMask/2)%2;
  if(!optional.low)values[6]=lampMask%2;
  if(!optional.fog)values[8]=Math.floor(lampMask/4)%2;
  if(!optional.brake)values[9]=Math.floor(lampMask/8)%2;
  if(companionVersion>=979&&companionVersion<=980) {
   var relayMask=Math.floor(absPacked/1048576)%8;
   if(!optional.left)values[19]=relayMask%2;
   if(!optional.right)values[20]=Math.floor(relayMask/2)%2;
   if(!optional.hazard)values[21]=Math.floor(relayMask/4)%2;
  }
 }
 if(isFinite(packed) && packed>=0) {
  var status=Math.floor(packed)%65536;
  if(!optional.tc) values[13]=(status%4===2)?1:0;
  // BeamNG active cruise requests ON+SET. No separate armed flag.
  if(!optional.cruise) values[16]=(Math.floor(status/4)%4===2)?2:0;
  if(!optional.cel) values[18]=Math.floor(packed/262144)%2;
 }
 if(!optional.mode) { var modeCode=Math.floor(tag/2097152)%4; values[17]=modeCode===1?0:(modeCode>=2?modeCode-1:3); }
 // v975 reserved_1 low 3 bits encode disabled selector + 1.
 // 2=TC disabled, 3=ESC disabled; absence/unknown never means OFF.
 var environment=Number(prop(raw+'reserved_1',-1));
 var disabled=(isFinite(environment)&&environment>=0)?Math.floor(environment)%8-1:-1;
 var escDisabled=isFinite(packed)&&packed>=0&&Math.floor(packed/131072)%2===1;
 if(!optional.tc && (disabled===2||disabled===3||escDisabled))values[13]=2;
}
// The same named property adapters work for BeamNG and ETS2/ATS.
var modeSource='companion/unknown',modeRaw=null;
var modePaths=[raw+'ActiveDriveMode','GameRawData.ActiveDriveMode',raw+'electrics.values.driveMode',raw+'electrics.driveMode','DataCorePlugin.GameData.NewData.ActiveDriveMode'];
for(var m=0;m<modePaths.length;m++) {
 var named=prop(modePaths[m],null);
 if(named!==null && named!==''){modeRaw=named;modeSource=modePaths[m];values[17]=decodeMode(named,false);break;}
}
if(optional.mode){modeRaw=prop(optional.mode,null);modeSource=optional.mode;values[17]=decodeMode(modeRaw,true);}
if(modeSelection!==null){modeRaw=modeSelection;modeSource='modeSelection';values[17]=decodeMode(modeSelection,true);}
// Preserve the selected drive mode for S display; ESC disable is an independent state.
var driveModeForGear=values[17];
// A positive explicit OFF property takes priority over intervention activity.
if(running && (flag('tcOff')||flag('escOff')))values[13]=2;
if(values[17]===4)values[13]=2;
if(values[13]===2)values[17]=4;
if(!running){for(var i=3;i<values.length;i++)values[i]=0;values[17]=3;}

// Common dash-light tokens for games without the BeamNG companion.
if(!beam){
 var commonLights=dashLightText;
 function commonToken(t){return (' '+commonLights.replace(/[^A-Za-z0-9_]+/g,' ')+' ').indexOf(' '+t+' ')>=0?1:0;}
 if(!optional.low||prop(optional.low,null)===null)values[6]=commonToken('DL_LOWBEAM');
 if(!optional.high||prop(optional.high,null)===null)values[7]=commonToken('DL_FULLBEAM');
 if(!optional.fog||prop(optional.fog,null)===null)values[8]=commonToken('DL_FOG');
 if(!optional.tc)values[13]=firstFlag(['DataCorePlugin.GameData.NewData.TCActive']);
 if(!userAbsPath)values[12]=values[12]||commonToken('DL_ABS');
}
// Additional Audi controls. Bind buttons to real SimHub Control Mapper properties.
var audi={coolant:null,oil:null,rearFog:null,brakeFailure:null,brakeOverheat:null,
 brakeTemperature:null,cruiseSetSpeed:null,up:null,down:null,left:null,right:null,ok:null,distanceWarning:null,proximityDistanceAhead:null,engineDamage:null,boostBar:null,autoHighBeam:null,speedLimitKmh:null,fuelLitres:null,consumptionL100km:null};
// Celsius threshold is user-defined for the chosen game's reported brake temperature.
// 0 disables temperature-threshold inference; explicit failure/overheat still works.
var brakeHotThresholdC=0;
var benchTest=0; // 0 normal;1 low;2 high;3 front fog;4 rear fog;5 parking;
// 6 BRAKE warning;7 ABS;8 ESC off;9 ESC activity;10 CEL;11 cruise SET;
// 12 fixed60kmh/3000rpm/90C;13..17 menu UP/DOWN/LEFT/RIGHT/OK.
function audiFlag(name){var x=booleanValue(prop(audi[name],null));return x===null?0:x;}
var temperatureSources={coolant:null,oil:null};
function readTemp(path,paths,max,label){
 if(path)paths=[path];
 for(var t=0;t<paths.length;t++){
  var x=prop(paths[t],null);if(x===null||x===undefined||x===''||typeof x==='boolean')continue;
  x=Number(x);if(isFinite(x)&&x>=-48&&x<=max){temperatureSources[label]=paths[t];return Math.round(x);}
 }
 return -1000;
}
// Prefer known per-game temperature fields over generic properties that may remain0.
// A genuine0C from the selected raw sensor remains0; never replace it with normal90C.
var coolantPaths=[],oilPaths=[];
if(beam){coolantPaths=[raw+'waterTemp',raw+'engTemp','GameRawData.waterTemp','GameRawData.engTemp'];oilPaths=[raw+'oilTemp','GameRawData.oilTemp'];}
if(truck){coolantPaths=[raw+'TruckValues.CurrentValues.DashboardValues.WaterTemperature','GameRawData.TruckValues.CurrentValues.DashboardValues.WaterTemperature'];oilPaths=[raw+'TruckValues.CurrentValues.DashboardValues.OilTemperature','GameRawData.TruckValues.CurrentValues.DashboardValues.OilTemperature'];}
coolantPaths=coolantPaths.concat(['DataCorePlugin.GameData.NewData.WaterTemperature','DataCorePlugin.GameData.WaterTemperature']);
oilPaths=oilPaths.concat(['DataCorePlugin.GameData.NewData.OilTemperature','DataCorePlugin.GameData.OilTemperature']);
var coolant=readTemp(audi.coolant,coolantPaths,141,'coolant');
var oil=readTemp(audi.oil,oilPaths,190,'oil');
var brakeWarn=audiFlag('brakeFailure')||audiFlag('brakeOverheat');
var brakeTemp=prop(audi.brakeTemperature,null);
if(brakeHotThresholdC>0&&brakeTemp!==null&&brakeTemp!==''&&isFinite(Number(brakeTemp))&&Number(brakeTemp)>=brakeHotThresholdC)brakeWarn=1;
if(running&&companion){var statusForBrake=Number(prop(raw+'reserved_3',-1));if(isFinite(statusForBrake)&&statusForBrake>=0&&Math.floor(statusForBrake/65536)%2===1)brakeWarn=1;}
var setSpeed=0;
if(audi.cruiseSetSpeed)setSpeed=number(audi.cruiseSetSpeed,0,326);
else if(truck&&truckCruiseSpeed!==null&&/\.Kph$/.test(truckCruiseSource||''))setSpeed=Math.max(0,Math.min(326,Math.round(truckCruiseSpeed)));
else if(beam&&companion){
 var cruisePacked=Number(prop(raw+'reserved_3',-1));
 if(isFinite(cruisePacked)&&cruisePacked>=0)setSpeed=Math.round((Math.floor(cruisePacked/16)%4096)/10);
}
// Companion target speed is encoded in tenths of km/h; never substitute road speed.
var menuMask=audiFlag('up')+2*audiFlag('down')+4*audiFlag('left')+8*audiFlag('right')+16*audiFlag('ok');
// CEL follows ENGINE damage only, never ignition/checkengine bulb test or body damage.
var engineDamageSource=null,engineDamageRaw=null,engineDamaged=0;
var damagePaths=audi.engineDamage?[audi.engineDamage]:(truck?
 [raw+'TruckValues.CurrentValues.DamageValues.Engine','GameRawData.TruckValues.CurrentValues.DamageValues.Engine']:[]);
for(var q=0;q<damagePaths.length;q++){
 var damageValue=prop(damagePaths[q],null);if(damageValue===null||damageValue===undefined||damageValue==='')continue;
 var damageNumber=Number(damageValue);if(!isFinite(damageNumber)||damageNumber<0)continue;
 engineDamageSource=damagePaths[q];engineDamageRaw=damageValue;engineDamaged=damageNumber>0?1:0;break;
}
if(!audi.engineDamage&&beam&&companionVersion===980){
 var damagePacked=Number(prop(raw+'reserved_3',-1));
 if(isFinite(damagePacked)&&damagePacked>=0){engineDamageSource='companion980 engine damage';engineDamaged=Math.floor(damagePacked/524288)%2;engineDamageRaw=engineDamaged;}
}
if(beam||truck)values[18]=engineDamaged;
// Forward distance must be a real property in METRES, not an isnull(...,0) placeholder.
var distanceWarningSpeedKmh=20, distanceWarningThresholdM=15;
var distanceWarning=0,distanceSource='unavailable',distanceRaw=null;
if(audi.distanceWarning){
 distanceRaw=prop(audi.distanceWarning,null);distanceSource=audi.distanceWarning;
 var directWarning=booleanValue(distanceRaw);distanceWarning=directWarning===1?1:0;
}else if(audi.proximityDistanceAhead){
 distanceRaw=prop(audi.proximityDistanceAhead,null);distanceSource=audi.proximityDistanceAhead;
 if(distanceRaw!==null&&distanceRaw!==undefined&&distanceRaw!==''&&typeof distanceRaw!=='boolean'){
  var metres=Number(distanceRaw);
  if(isFinite(metres)&&metres>=0)distanceWarning=values[4]>distanceWarningSpeedKmh&&metres<distanceWarningThresholdM?1:0;
 }
}
// Boost override is BAR GAUGE by default; choose absolute input explicitly below.
// BeamNG companion turbo is bar gauge (boost PSI /14.504). No fake ETS2 source.
var boostInputIsAbsolute=false, boostAmbientBar=1.0;
var boostPaths=audi.boostBar?[audi.boostBar]:(beam?[raw+'turbo','GameRawData.turbo']:[]);
var boost=-1000,boostSource=null,boostInput=null;
for(var bi=0;bi<boostPaths.length;bi++){
 var bv=prop(boostPaths[bi],null);
 if(bv===null||bv===undefined||bv===''||typeof bv==='boolean')continue;
 bv=Number(bv);if(!isFinite(bv)||bv< (boostInputIsAbsolute?0:-1)||bv>5.1)continue;
 boostInput=bv;boostSource=boostPaths[bi];
 boost=Math.max(0,Math.min(510,Math.round((bv+(boostInputIsAbsolute?0:boostAmbientBar))*100)));break;
}
// Bind an ACTUAL automatic high-beam enabled/active property. Never infer from highbeam.
var autoHigh=audiFlag('autoHighBeam');
// Manual fallback is km/h;0 disables it. A valid game0 means no current limit.
var manualSpeedLimitKmh=0,limit=0,limitSource='unavailable',limitRaw=null;
var limitPaths=audi.speedLimitKmh?[audi.speedLimitKmh]:[];
if(!audi.speedLimitKmh&&truck)limitPaths=[raw+'NavigationValues.SpeedLimit.Kph','GameRawData.NavigationValues.SpeedLimit.Kph',raw+'NavigationValues.SpeedLimit.Value','GameRawData.NavigationValues.SpeedLimit.Value',raw+'NavigationValues.SpeedLimit','GameRawData.NavigationValues.SpeedLimit'];
var limitFound=false;
for(var li=0;li<limitPaths.length;li++){
 var lv=prop(limitPaths[li],null);if(lv===null||lv===undefined||lv===''||typeof lv==='boolean')continue;
 lv=Number(lv);if(!isFinite(lv)||lv<0)continue;
 // Raw SCS scalar is metres/second; explicitly Kph or user override is km/h.
 if(!audi.speedLimitKmh&&!/\.Kph$/.test(limitPaths[li]))lv*=3.6;
 if(lv>326)continue;limit=Math.round(lv);limitSource=limitPaths[li];limitRaw=lv;limitFound=true;break;
}
if(!audi.speedLimitKmh&&!limitFound&&beam&&companion){
 var lp=Number(prop(raw+'reserved_2',-1));
 if(isFinite(lp)&&lp>=0){var encoded=Math.floor(lp/2)%32768;if(encoded>0){limit=Math.round((encoded-1)/10);limitSource='BeamNG companion road edge';limitFound=true;}}
}
if(!limitFound&&manualSpeedLimitKmh>0){limit=Math.min(326,Math.round(manualSpeedLimitKmh));limitSource='manual fallback';}
var sportDisplay=(driveModeForGear===1||driveModeForGear===2)&&values[5]>0?1:0;
// Fuel volume is litres; average consumption fallback is L/100km.
var fuel=-1,economy=-1,fuelSource=null,economySource=null;
var fuelPaths=audi.fuelLitres?[audi.fuelLitres]:(beam?[raw+'fuelVolume','GameRawData.fuelVolume']:(truck?[raw+'TruckValues.CurrentValues.DashboardValues.FuelValue.Amount','GameRawData.TruckValues.CurrentValues.DashboardValues.FuelValue.Amount']:[]));
for(var fi=0;fi<fuelPaths.length;fi++){
 var fv=prop(fuelPaths[fi],null);if(fv===null||fv===undefined||fv===''||typeof fv==='boolean')continue;
 fv=Number(fv);if(isFinite(fv)&&fv>=0&&fv<=2000){fuel=Math.round(fv*100000);fuelSource=fuelPaths[fi];break;}
}
var economyPaths=audi.consumptionL100km?[audi.consumptionL100km]:(truck?[raw+'TruckValues.CurrentValues.DashboardValues.FuelValue.AverageConsumption','GameRawData.TruckValues.CurrentValues.DashboardValues.FuelValue.AverageConsumption']:[]);
for(var ei=0;ei<economyPaths.length;ei++){
 var ev=prop(economyPaths[ei],null);if(ev===null||ev===undefined||ev===''||typeof ev==='boolean')continue;
 ev=Number(ev);if(!audi.consumptionL100km)ev*=100; // SCS average is L/km.
 if(isFinite(ev)&&ev>=0&&ev<=1000){economy=Math.round(ev*1000);economySource=economyPaths[ei];break;}
}
var result=[6,values[1],running?1:0,values[3],values[4],coolant,oil,
 values[6],values[7],values[8],audiFlag('rearFog'),values[9],brakeWarn,
 values[12],values[13]===1?1:0,values[13]===2?1:0,values[18],values[16],setSpeed,
 values[19],values[20],values[21],menuMask,values[5],distanceWarning,boost,autoHigh,limit,sportDisplay,fuel,economy];
if(!running){for(var z=3;z<result.length;z++)result[z]=0;result[5]=result[6]=result[25]=-1000;result[29]=result[30]=-1;}
if(benchTest>=1&&benchTest<=26){
 for(var z=3;z<result.length;z++)result[z]=0;
 result[2]=1;result[5]=result[6]=result[25]=-1000;result[29]=result[30]=-1;
 var index={1:7,2:8,3:9,4:10,5:11,6:12,7:13,8:15,9:14,10:16};
 if(index[benchTest]!==undefined)result[index[benchTest]]=1;
 if(benchTest===11){result[17]=2;result[18]=80;}
 if(benchTest===18)result[24]=1;
 if(benchTest===25){result[3]=2000;result[4]=60;result[30]=10000;}
 if(benchTest===26){result[3]=2000;result[4]=60;result[17]=2;result[18]=60+Math.floor(Date.now()/5000)%3*20;}
 if(benchTest===22){result[3]=3000;result[27]=80;result[4]=Math.floor(Date.now()/3000)%2?90:70;}
 if(benchTest===23||benchTest===24){result[3]=3000;result[4]=40;result[23]=3;result[28]=benchTest===24?1:0;}
 if(benchTest===19){result[3]=3000;result[25]=100+Math.floor(Date.now()/3000)%3*50;}
 if(benchTest===20){result[3]=3000;result[5]=90;result[6]=110;}
 if(benchTest===21){result[7]=1;result[26]=Math.floor(Date.now()/3000)%2;}
 if(benchTest===12){result[3]=3000;result[4]=60;result[5]=90;result[6]=90;}
 if(benchTest>=13&&benchTest<=17)result[22]=Math.pow(2,benchTest-13);
}
if(diagnosticMode)return JSON.stringify({game:game,running:running,companionVersion:companionVersion,gameGear:g,driveModeForGear:driveModeForGear,
 engineDamageSource:engineDamageSource,engineDamageRaw:engineDamageRaw,distanceWarningSource:distanceSource,proximityDistanceAheadRaw:distanceRaw,temperatureSources:temperatureSources,truckCruiseSource:truckCruiseSource,truckCruiseSpeed:truckCruiseSpeed,
 fuelSource:fuelSource,fuelLitres:fuel>=0?fuel/100000:null,consumptionSource:economySource,consumptionL100km:economy>=0?economy/1000:null,
 speedLimitSource:limitSource,speedLimitKmh:result[27],sportGearRequest:result[28],boostSource:boostSource,boostGaugeInput:boostInput,autoHighBeamSource:audi.autoHighBeam,
 mapped:{boostAbsoluteCentibar:result[25],autoHighBeam:result[26],rpm:result[3],kmh:result[4],coolantC:result[5],oilC:result[6],low:result[7],high:result[8],frontFog:result[9],rearFog:result[10],parkingBrake:result[11],brakeWarning:result[12],abs:result[13],escActivity:result[14],escOff:result[15],cel:result[16],cruise:result[17],cruiseSetKmh:result[18],left:result[19],right:result[20],hazard:result[21],menuMask:result[22],distanceWarning:result[24]},
 missingMapping:{menuCAN:false,menuRightUnverified:true,frontFogIndicator:true},
 note:'CAN candidates require bench validation; menu payload candidates supplied; A8 physical verification still required'});
return result.join(';');

