// Recommended: select JavaScript mode in SimHub's formula editor.
// Safe baseline; choose actual properties from Available Properties for each game.
// A null optional path is intentionally unsupported and returns 0.
var diagnosticMode=false; // true returns readable telemetry; driving times out
var benchTest=0; // 0 game; 1..6 lamp tests; 7 all lights+42kmh; 8 speed42 only
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
var g=String(prop('DataCorePlugin.GameData.NewData.Gear','N')).toUpperCase();
var manualMatch=/^M([1-8])$/.exec(g);
var gear=g==='P'?-2:(g==='R'?-1:(g==='N'?0:(g==='D'?100:(manualMatch?100+Number(manualMatch[1]):Number(g)))));
if(!isFinite(gear)) gear=0; gear=Math.round(gear);
if(gear!==100 && !(gear>=101 && gear<=108)) gear=Math.max(-2,Math.min(18,gear));
if(manualForwardGears && gear>=1 && gear<=18) gear=100+Math.min(8,gear);
// Game adapters follow the property paths used by the user's Panamera project.
var game=String(prop('DataCorePlugin.CurrentGame','')).toLowerCase().replace(/[^a-z0-9]/g,'');
var beam=game.indexOf('beamng')>=0;
var truck=game==='ets2'||game==='ats'||game.indexOf('eurotrucksimulator')>=0||game.indexOf('americantrucksimulator')>=0;
var raw='DataCorePlugin.GameRawData.';
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
 var lightText=String(prop('DataCorePlugin.GameData.NewData.Light',prop('DataCorePlugin.GameData.NewData.DashLights','')));
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
var companion=beam && (companionVersion===975||companionVersion===976||companionVersion===977||companionVersion===978||companionVersion===979);
if(running && companion) {
 var packed=Number(prop(raw+'reserved_3',-1));
 var absPacked=Number(prop(raw+'reserved_2',-1));
 if(!optional.abs && isFinite(absPacked) && absPacked>=0) values[12]=Math.floor(absPacked)%2;
 if((companionVersion>=976&&companionVersion<=979) && isFinite(absPacked) && absPacked>=0) {
  var lampMask=Math.floor(absPacked/65536)%16;
  if(!optional.high)values[7]=Math.floor(lampMask/2)%2;
  if(!optional.low)values[6]=lampMask%2;
  if(!optional.fog)values[8]=Math.floor(lampMask/4)%2;
  if(!optional.brake)values[9]=Math.floor(lampMask/8)%2;
  if(companionVersion===979) {
   var relayMask=Math.floor(absPacked/1048576)%8;
   if(!optional.left)values[19]=relayMask%2;
   if(!optional.right)values[20]=Math.floor(relayMask/2)%2;
   if(!optional.hazard)values[21]=Math.floor(relayMask/4)%2;
  }
 }
 if(isFinite(packed) && packed>=0) {
  var status=Math.floor(packed)%65536;
  if(!optional.tc) values[13]=(status%4===2)?1:0;
  // User display rule: BeamNG active cruise requests ON+SET. No separate armed flag.
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
// A positive explicit OFF property takes priority over intervention activity.
if(running && (flag('tcOff')||flag('escOff')))values[13]=2;
if(values[17]===4)values[13]=2;
if(values[13]===2)values[17]=4;
if(!running){for(var i=3;i<values.length;i++)values[i]=0;values[17]=3;}
if(benchTest>=1&&benchTest<=8) {
 for(var i=3;i<values.length;i++)values[i]=0;values[2]=1;
 if(benchTest===1)values[6]=1;
 if(benchTest===2){values[6]=1;values[7]=1;}
 if(benchTest===3){values[6]=1;values[8]=1;}
 if(benchTest===4)values[9]=1;
 if(benchTest===5)values[12]=1;
 if(benchTest===6)values[13]=1;
 if(benchTest===7){values[6]=1;values[7]=1;values[8]=1;values[4]=42;}
 if(benchTest===8)values[4]=42;
}
function requestedLighting(low,high,fog,flash) {
 if(high && !flash)low=1;
 if(low&&high&&fog)return 0x68;
 else if(low&&high)return 0x60;
 else if(low&&fog)return 0x28;
 else if(high&&fog&&flash)return 0x48;
 else if(high&&fog)return 0x08;
 else if(low)return 0x20;
 else if(high)return 0x40;
 else if(fog)return 0x08;
 return 0;
}
if(diagnosticMode)return JSON.stringify({game:game,running:running,companion975:companion&&companionVersion===975,companionVersion:companionVersion,
 marker:tag,modeSource:modeSource,modeRaw:modeRaw,truckCruiseSource:truckCruiseSource,truckCruiseSpeed:truckCruiseSpeed,low:prop(raw+'light_LowBeam',null),combined:prop(raw+'light_LowHighBeam',null),
 high:prop(raw+'light_HighBeam',null),parkingInput:prop(raw+'input_parkingBrake',null),
 parkingLamp:prop(raw+'light_Parkingbrake',null),handbrake:prop('DataCorePlugin.GameData.NewData.Handbrake',null),
 absPacked:prop(raw+'reserved_2',null),statusPacked:prop(raw+'reserved_3',null),disabledPacked:prop(raw+'reserved_1',null),
 mapped:{kmh:values[4],low:values[6],high:values[7],fog:values[8],flash:values[23],brake:values[9],abs:values[12],tc:values[13],cruise:values[16],mode:values[17],tractionByteHex:(values[13]===2||values[17]===4?'18':(running&&values[13]===1?'02':'00')),ecoByteHex:(running&&values[17]===0&&values[13]!==2?'c0':'00'),sportByteHex:(running&&(values[17]===1||values[17]===2)&&values[13]!==2?'07':'00'),lightByteHex:requestedLighting(values[6],values[7],values[8],values[23]).toString(16),cruiseByteHex:(values[16]===2?18:(values[16]===1?16:0)).toString(16)}});
return values.join(';');
