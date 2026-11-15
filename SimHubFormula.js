// v6 normal formula: fresh scoped evaluation and direct BeamNG lamp reads.
return (function(){
// Taycan + SimHub lighting / ESC / TCS formula for BeamNG, ETS2, ATS, AC.
// Enable "Use JavaScript" in SimHub; paste the entire file as the formula.
// Only INSERT the exact SimHub property names shown in your installation.
// 47 fields: retain the v9 field layout and append v10 cluster telemetry.
var override={
    key:"",high:"",fog:"",left:"",right:"",handbrake:"",charge:"",
    outside:"",brightness:"",door:"",checkEngine:"",cruise:"",low:"",parking:"",
    // Optional single 0/1/2/3/4 property: off/parking/low/high/rear-fog.
    // BeamNG's stock OutGauge DOES NOT expose this property: a custom telemetry
    // feed/plugin is required for individual parking/low/fog states.
    lightMode:"",
    // Each accepts an EXACT SimHub Insert Property path (0/1, true/false).
    // OFF is not the same as intervention. Don't map TCActive to TCOff!
    escOff:"",tcOff:"",escActive:"",tcActive:"",
    // Alternatively use a known game's ESC/TC enabled bit, inverted here.
    escEnabled:"",tcEnabled:"",
    // Assetto Corsa: optional TCLevel property when the selected car HAS TC.
    // A missing TC level or unsupported car must not produce a false OFF lamp.
    tcLevel:"",
    // Additional exact property names from SimHub's Available properties.
    cruiseActive:"", cruiseSetKmh:"", fuelPercent:"", fullRangeKm:"",
    speedLimitKmh:"", clockSeconds:"", oilTempC:"", oilPressureKpa:"",
    tpmsFL:"", tpmsFR:"", tpmsRL:"", tpmsRR:"", tpmsWarning:"",
    laneKeep:"", laneChange:"", doorMask:"", psmSport:"", psmOff:"",
    // OPTIONAL: signed actual motor power (-100=maximum recup to +100=power)
    // when a game's telemetry exposes a trustworthy normalized channel.
    powerSignedPercent:"",
    autoHigh:"", autoHold:"", absActive:"", absWarning:"", frontFog:"",
    rangeKm:"", batterySoc:"", batteryTempC:"", charging:""
};
// A configurable maximum estimate at 100% fuel/charge; never an ECU reading.
var configuredFullRangeKm=550;
// Gauge fallback without actual motor power telemetry: throttle => positive
// power, brake => negative (simulated) recuperation. This represents PEDAL
// INPUT, not true motor kW. Turn off for conventional trucks/cars if desired.
var approximateBrakeRecup=true;
var brakeRecupStrength=0.70;  // Full braking => -70% of recup meter
var gaugePedalDeadzone=1.5;  // Ignore tiny noisy pedal readings (percent)
// Each CAN donor option is controlled separately in SHCustomProtocol.h.

// Manual isolated bench test: -1=normal game, 0=all off, 1=parking,
// 2=low beam, 3=high beam, 4=rear fog. Restoring -1 is essential!
// Can be tested while BeamNG is NOT running; automatically sets key ON.
var lightTestMode=-1;
// Independent ESC/TC bench test: -1=game, 0=normal (no warnings),
// 1=ESC OFF, 2=TC OFF, 3=ESC intervening, 4=TC intervening.
// Set to -1 before actual gameplay. Works without a running game.
var escTestMode=-1;
// Bench-only cruise set speed (km/h). -1=telemetry; 0=off; 30..260=active.
var cruiseTestKmh=-1;
// Donor features bench demo (requires enabling each Arduino feature toggle).
// -1=normal; 1=80 km/h sign, both lanes green, PSM Sport, driver door and TPMS warning.
var donorTestMode=-1;
function get(name,fallback) {
    if(!name) return fallback;
    try {var x=$prop(name); return x===null||typeof x==="undefined"?fallback:x;}
    catch(error) {return fallback;}
}
function num(x,fallback) {var n=Number(x); return isFinite(n)?Math.round(n):fallback;}
function flag(x) {return x===true||x===1||x==="1"||String(x).toLowerCase()==="true"?1:0;}
function mask(name) {
    var value=get(name,null);
    if(value===null||value==="")return null;
    var v=Number(value); return isFinite(v)?v:null;
}
var p="DataCorePlugin.GameData.NewData.";
var lightTesting=lightTestMode>=0&&lightTestMode<=4;
var escTesting=escTestMode>=0&&escTestMode<=4;
var testing=lightTesting||escTesting||cruiseTestKmh>=0||donorTestMode===1;
var running=flag(get("DataCorePlugin.GameRunning",0))||testing;
// Loopback bridge is only valid while reporting fresh BeamNG data.
var bridge=flag(get("TaycanBeamNGPlugin.Connected",0));
function bridgeValue(name){return bridge?get("TaycanBeamNGPlugin."+name,null):null;}
function prefer(name,normal) {
    if(override[name]){var chosen=get(override[name],null);if(chosen!==null&&chosen!=="")return chosen;}
    var bridgeName=name.indexOf("tpms")===0?("Tpms"+name.substring(4)):
                   name.substring(0,1).toUpperCase()+name.substring(1);
    var b=bridgeValue(bridgeName);return b!==null&&b!==""?b:normal;
}
function maybeInt(value,other) {
    if(value===null||value===""||typeof value==="undefined")return other;
    var v=Number(value);return isFinite(v)?Math.round(v):other;
}

// Standard BeamNG OutGauge reports HIGH beam as bit 1 (value 2) and
// HANDBRAKE as bit 2 (value 4). Standard OutGauge has no low/parking/fog bits.
var dashMask=mask("GameRawData.ShowLights");
if(dashMask===null)dashMask=mask("DataCorePlugin.GameRawData.ShowLights");
var lights=String(get(p+"Light",get(p+"DashLights","")));
function lamp(name,token,beamBit) {
    if(!running)return 0;
    if(testing) return (name==="parking"&&lightTestMode===1)||
        (name==="low"&&lightTestMode===2)||
        (name==="high"&&lightTestMode===3)||
        (name==="fog"&&lightTestMode===4)?1:0;
    if(override[name])return flag(get(override[name],0));
    var b=bridgeValue(name==="low"?"Low":name==="high"?"High":name==="fog"?"Fog":name==="parking"?"Parking":"Unknown");
    if(b!==null)return flag(b);
    if(dashMask!==null&&beamBit)return (dashMask&beamBit)?1:0;
    return token&&lights.indexOf(token)>=0?1:0;
}
// Custom game lightMode, where configured, supersedes automatic lamp detection.
var mode=override.lightMode?num(get(override.lightMode,-1),-1):-1;
function beam(name,token,maskBit,modeValue) {
    if(testing)return lamp(name,token,maskBit);
    if(!running)return 0;
    if(override[name])return flag(get(override[name],0));
    var bv=bridgeValue(name==="low"?"Low":name==="high"?"High":name==="fog"?"Fog":name==="parking"?"Parking":"Unknown");
    if(bv!==null)return flag(bv);
    if(mode>=0&&mode<=4)return mode===modeValue?1:0;
    var extra=extraLamp(name);if(extra!==null)return extra;
    return lamp(name,token,maskBit);
}
// The bundled simhubextras.lua exports these flat light fields.
// Keep exact overrides, bridge and explicit lightMode ahead of automatic inputs.
// Fresh, literal property reads on EVERY invocation. No cached lamp values in root.
var beamLightLow=null,beamLightCombined=null,beamLightHigh=null,beamLightFog=null;
try{beamLightLow=$prop("DataCorePlugin.GameRawData.light_LowBeam");}catch(e){}
try{beamLightCombined=$prop("DataCorePlugin.GameRawData.light_LowHighBeam");}catch(e){}
try{beamLightHigh=$prop("DataCorePlugin.GameRawData.light_HighBeam");}catch(e){}
try{beamLightFog=$prop("DataCorePlugin.GameRawData.light_Fog");}catch(e){}
var lampGame=String(get("DataCorePlugin.CurrentGame","")).toLowerCase().replace(/[^a-z0-9]/g,"");
var beamExtraLights=lampGame.indexOf("beamng")>=0;
function extraLamp(name){
    if(!beamExtraLights)return null;
    var fields={low:[beamLightLow,beamLightCombined],high:[beamLightHigh],fog:[beamLightFog]};
    var names=fields[name];if(!names)return null;
    for(var i=0;i<names.length;i++){
        var v=names[i];
        if(v===null || typeof v==="undefined" || v==="")continue;
        if(v===true)return 1;if(v===false)return 0;
        var n=Number(v);if(isFinite(n))return n>0?1:0;
    }
    return null;
}
var high=beam("high","DL_FULLBEAM",2,3);
var low=beam("low","DL_LOWBEAM",0,2);
var handbrake=lamp("handbrake","DL_HANDBRAKE",4);
if(running && !testing && !override.handbrake){
    if(beamExtraLights){
        var hb=get("DataCorePlugin.GameRawData.input_parkingBrake",null);
        if(hb===null || hb==="")hb=get("DataCorePlugin.GameRawData.light_Parkingbrake",null);
        if(hb!==null && hb!=="" && isFinite(Number(hb)))handbrake=Number(hb)>=0.5?1:0;
    }else{
        var truckBrake=firstAvailable(["DataCorePlugin.GameRawData.TruckValues.CurrentValues.MotorValues.BrakeValues.ParkingBrake","DataCorePlugin.GameRawData.Drivetrain.ParkingBrake"]);
        if(truckBrake!==null)handbrake=flag(truckBrake);
    }
}
var parking=beam("parking","DL_SIDELIGHTS",0,1);
var fog=beam("fog","DL_FOG",0,4);
root.taycanLampInputs="low="+low+" high="+high+" fog="+fog;
var leftRaw=get(p+"TurnIndicatorLeft",get("TurnIndicatorLeft",null));
var rightRaw=get(p+"TurnIndicatorRight",get("TurnIndicatorRight",null));
var left=running?(override.left?lamp("left",""):
    (leftRaw!==null?flag(leftRaw):lamp("left","DL_SIGNAL_L",32))):0;
var right=running?(override.right?lamp("right",""):
    (rightRaw!==null?flag(rightRaw):lamp("right","DL_SIGNAL_R",64))):0;
// Generic normalized indicator inputs. SimHub property names differ by game
// and plugin version; only use available values rather than treating a missing
// "enabled" property as false (which would wrongly illuminate ESC OFF).
function firstAvailable(paths) {
    for(var i=0;i<paths.length;i++) {
        var v=get(paths[i],null);
        if(v!==null && typeof v!=="undefined" && v!=="") return v;
    }
    return null;
}
function optionalBool(path) {
    if(!path)return null;
    var v=get(path,null);return v===null||v===""?null:flag(v);
}
function resolveIndicator(kind,onPaths,enabledOverride) {
    var selected=optionalBool(override[kind]);
    if(selected!==null)return selected;
    if(enabledOverride){
        var enabled=optionalBool(override[enabledOverride]);
        if(enabled!==null)return enabled?0:1;
    }
    var b=bridgeValue(kind==="escOff"?"EscOff":kind==="tcOff"?"TcOff":
        kind==="escActive"?"EscActive":"TcActive");
    if(b!==null)return flag(b);
    var direct=firstAvailable(onPaths);
    return direct===null?null:flag(direct);
}
var escOff=resolveIndicator("escOff",[
    p+"ESCOff",p+"EscOff",p+"StabilityControlOff",
    "ESCOff","EscOff","GameRawData.escOff"
],"escEnabled");
var tcOff=resolveIndicator("tcOff",[
    p+"TCOff",p+"TCSOff",p+"TractionControlOff",
    "TCOff","TCSOff"
],"tcEnabled");
// The optional TCLevel inversion is deliberately opt-in: AC cars without TC
// can also report 0, which is not proof that TC was switched OFF.
if(tcOff===null && override.tcLevel) {
    var level=get(override.tcLevel,null);
    if(level!==null && level!=="" && isFinite(Number(level)))
        tcOff=Number(level)===0?1:0;
}
var escActive=resolveIndicator("escActive",[
    p+"ESCActive",p+"EscActive",p+"StabilityControlActive",
    "ESCActive","EscActive","GameRawData.escActive"
],"");
var tcActive=resolveIndicator("tcActive",[
    p+"TCActive",p+"TCSActive",p+"TractionControlActive",
    "TCActive","TCSActive","GameRawData.tcsActive"
],"");
// Decode only recognized companion layouts, exactly as exported by the bundled Lua.
var safetyTag=beamExtraLights?maybeInt(get("DataCorePlugin.GameRawData.reserved_4",null),-1):-1;
var safetyVersion=safetyTag>=1024?safetyTag%1024:safetyTag;
var safetyPacked=safetyVersion>=971 && safetyVersion<=978;
var packedActivity=maybeInt(get("DataCorePlugin.GameRawData.reserved_3",null),-1);
var driveModeCode=safetyVersion>=975 && safetyPacked?Math.floor(safetyTag/2097152):0;
if(running && !testing && safetyPacked){
    var packedEnvironment=maybeInt(get("DataCorePlugin.GameRawData.reserved_1",null),-1);
    var disabledSelector=safetyVersion>=973?(packedEnvironment>=0?packedEnvironment%8-1:-1):packedEnvironment;
    var activityCode=packedActivity>=0?packedActivity%4:0;
    var actualYawOff=packedActivity>=0 && Math.floor(packedActivity/131072)%2===1;
    if(!override.escOff && !override.escEnabled && (safetyVersion>=977 || bridgeValue("EscOff")===null) && (disabledSelector>=0 || actualYawOff))
        escOff=(actualYawOff || disabledSelector===3)?1:0;
    if(!override.tcOff && !override.tcEnabled && !override.tcLevel && (safetyVersion>=977 || bridgeValue("TcOff")===null) && disabledSelector>=0)
        tcOff=disabledSelector===2?1:0;
    // Export combines ESC/TC intervention. Preserve it as a combined activity lamp;
    // do not pretend it identifies which controller acted.
    if(safetyVersion>=977 && packedActivity>=0){
        if(!override.escActive)
            escActive=Math.floor(packedActivity/524288)%2;
        if(!override.tcActive)
            tcActive=Math.floor(packedActivity/1048576)%2;
    }else if(!override.tcActive && bridgeValue("TcActive")===null && (activityCode===1 || activityCode===2)){
        tcActive=activityCode===2?1:0;
    }
}
// BeamNG stock OutGauge: DL_TC is bit 4 (0x10), described as
// "TC active OR switched off". It cannot reliably distinguish the two.
// Treat it as a warning/intervention INDICATOR only, never as proof of TC OFF.
if(tcActive===null && dashMask!==null && !tcOff && !escOff)tcActive=(dashMask&0x10)?1:0;
if(escOff===null)escOff=0;
if(tcOff===null)tcOff=0;
if(escActive===null)escActive=0;
if(tcActive===null)tcActive=0;
if(escTesting){
    escOff=escTestMode===1?1:0;
    tcOff=escTestMode===2?1:0;
    escActive=escTestMode===3?1:0;
    tcActive=escTestMode===4?1:0;
}
if(!running){escOff=0;tcOff=0;escActive=0;tcActive=0;}
root.taycanHeartbeat=((Number(root.taycanHeartbeat)||0)+1)%1000000;
root.taycanFormulaVersion=10;
// ETS2 & ATS use Drivetrain's real cruise set target in km/h.
// The BeamNG extension/plugin publishes the same normalized property.
var scsCruise=get("DataCorePlugin.GameRawData.Drivetrain.CruiseControl",null);
var scsSet=get("DataCorePlugin.GameRawData.Drivetrain.CruiseControlSpeedKmh",null);
if(safetyPacked && packedActivity>=0){
    var cruiseCode=Math.floor(packedActivity/4)%4;
    if(cruiseCode===1 || cruiseCode===2){
        scsCruise=cruiseCode===2?1:0;
        scsSet=(Math.floor(packedActivity/16)%4096)/10;
    }
}
if(scsCruise===null)scsCruise=firstAvailable([p+"CruiseControlActive",p+"CruiseControl"]);
if(scsSet===null)scsSet=firstAvailable([p+"CruiseControlSpeedKmh",p+"CruiseControlSetSpeedKmh"]);
var cruiseInput=override.cruiseActive||override.cruise;
var cruiseActive=running?flag(get(cruiseInput,scsCruise===null?bridgeValue("CruiseActive"):scsCruise)):0;
var cruiseSet=running&&cruiseActive?maybeInt(get(override.cruiseSetKmh,scsSet===null?bridgeValue("CruiseSetKmh"):scsSet),0):0;
if(cruiseTestKmh>=0){cruiseActive=cruiseTestKmh>0?1:0;cruiseSet=cruiseTestKmh>0?cruiseTestKmh:0;}
// Use generic normalized properties where available; an unavailable sensor is
// encoded -1 rather than a fabricated tyre pressure or oil pressure.
var fuelDefault=get(p+"FuelPercent",null);
if(fuelDefault===null) {
 var fuelLitres=get(p+"Fuel",null),fuelMax=get(p+"MaxFuel",null);
 fuelDefault=(fuelLitres!==null&&fuelMax!==null&&Number(fuelMax)>0)?
   100*Number(fuelLitres)/Number(fuelMax):null;
}
var fuelPct=maybeInt(prefer("fuelPercent",fuelDefault),-1);
if(fuelPct<0)fuelPct=maybeInt(get(override.charge,0),0);
fuelPct=Math.max(0,Math.min(100,fuelPct));
var fullRange=Math.max(0,Math.min(2000,maybeInt(prefer("fullRangeKm",configuredFullRangeKm),configuredFullRangeKm)));
var speedLimit=maybeInt(prefer("speedLimitKmh",-1),-1);
var oilTemp=maybeInt(prefer("oilTempC",get(p+"OilTemperature",-99)),-99);
var oilPressure=maybeInt(prefer("oilPressureKpa",-1),-1);
var tpms=["FL","FR","RL","RR"].map(function(wheel){
    var key="tpms"+wheel;
    var packed=safetyVersion===978?maybeInt(get("DataCorePlugin.GameRawData.wheel_isDeflated_"+wheel.toLowerCase(),null),-1):-1;
    var pressure=Math.floor(packed/2)-1;
    if(!running || testing)return -1;
    if(override[key])return maybeInt(get(override[key],-1),-1);
    if(pressure>=0 && pressure<=1000)return pressure;
    return maybeInt(bridgeValue("Tpms"+wheel),-1);
});
root.taycanTyrePressureKpa=tpms;
var tyreWarning=flag(prefer("tpmsWarning",0));
var laneStatus=firstAvailable([p+"LaneKeepAssistStatus"]);
var laneKeep=maybeInt(prefer("laneKeep",laneStatus),0), laneChange=maybeInt(prefer("laneChange",0),0);
var doors=maybeInt(prefer("doorMask",0),0);
var psmSport=flag(get(override.psmSport,driveModeCode>0?(driveModeCode===2||driveModeCode===3):bridgeValue("PsmSport")));
var psmOff=(flag(get(override.psmOff,driveModeCode>0?driveModeCode===4:bridgeValue("PsmOff")))||
    (!override.psmOff&&escOff&&tcOff))?1:0;
if(donorTestMode===1){speedLimit=80;laneKeep=2;laneChange=1;
    tyreWarning=1;doors=1;psmSport=1;psmOff=0;}
if(!running){cruiseActive=0;cruiseSet=0;speedLimit=-1;laneKeep=0;laneChange=0;tyreWarning=0;psmSport=0;psmOff=0;}
// Smoothness is applied on the Arduino at CAN-frame rate, NOT here. Do not
// round throttle/brake before calculating: preserve 0.1% command resolution.
function pedalPercent(path,alternative) {
    var v=get(path,null);
    if(v===null || v==="")v=get(alternative,0);
    var n=Number(v);return isFinite(n)?Math.max(0,Math.min(100,n)):0;
}
var throttlePct=running?pedalPercent(p+"Throttle","DataCorePlugin.GameData.Throttle"):0;
var brakePct=running?pedalPercent(p+"Brake","DataCorePlugin.GameData.Brake"):0;
var explicitPower=override.powerSignedPercent?get(override.powerSignedPercent,null):null;
var signedPower;
if(running && explicitPower!==null && explicitPower!=="" && isFinite(Number(explicitPower))) {
    signedPower=Math.max(-100,Math.min(100,Number(explicitPower)));
} else if(throttlePct>gaugePedalDeadzone) {
    signedPower=throttlePct;
} else if(approximateBrakeRecup && brakePct>gaugePedalDeadzone &&
          Number(get(p+"SpeedKmh",0))>3) {
    signedPower=-brakePct*brakeRecupStrength;
} else signedPower=0;
// 41st field is +/-1000, preserving ten times finer input resolution while
// leaving indices 0..39 untouched for all previous SimHub dashboard fields.
var signedPowerPermille=Math.max(-1000,Math.min(1000,Math.round(signedPower*10)));

// Panamera 970 telemetry readers; existing speed, pedals and power calculations stay intact.
var clockFallback="none"; // "host" opts into PC local time if game time is absent.
function numeric(v){if(v===null || typeof v==="undefined" || typeof v==="boolean" || String(v).trim()==="")return null;var n=Number(v);return isFinite(n)?n:null;}
function firstNumber(names){for(var i=0;i<names.length;i++){var n=numeric(get(names[i],null));if(n!==null)return n;}return null;}
var game=String(get("DataCorePlugin.CurrentGame","")).toLowerCase().replace(/[^a-z0-9]/g,"");
var isBeam=game.indexOf("beamng")>=0;
var isTruck=game==="ets2" || game==="ats" || game.indexOf("eurotrucksimulator2")>=0 || game.indexOf("americantrucksimulator")>=0;
var raw="DataCorePlugin.GameRawData.";
var tag=isBeam?numeric(get(raw+"reserved_4",null)):null;
var version=tag!==null && tag>=1024?tag%1024:tag;
var packedRoad=isBeam?numeric(get(raw+"reserved_2",null)):null;
if(running && !override.speedLimitKmh && speedLimit<=0){
    var detected=null;
    if(isTruck)detected=firstNumber([raw+"NavigationValues.SpeedLimit.Kph",raw+"Job.SpeedLimitKmh"]);
    if(isBeam && version>=972 && version<=978){
        var packed=packedRoad;
        if(packed!==null && version>=976)packed=packed%16384;
        if(packed!==null && packed>=2)detected=(Math.floor(packed/2)-1)/10;
    }
    if(detected===null)detected=firstNumber([p+"SpeedLimitKmh"]);
    speedLimit=detected!==null?Math.round(detected):-1;
}
if(speedLimit<=0 || speedLimit>255)speedLimit=-1;
var clockSeconds=running && override.clockSeconds?numeric(get(override.clockSeconds,null)):null;
if(running && !override.clockSeconds){
    if(isTruck){var minutes=firstNumber([raw+"CommonValues.GameTime.Minutes",raw+"Time"]);if(minutes!==null && minutes>=0)clockSeconds=Math.floor(minutes%1440)*60;}
    if(isBeam && version>=974 && version<=978){var code=Math.floor(tag/1024)%2048;if(code>=1 && code<=1440)clockSeconds=(code-1)*60;}
}
if(running && clockSeconds===null && clockFallback==="host"){var d=new Date();clockSeconds=d.getHours()*3600+d.getMinutes()*60+d.getSeconds();}
clockSeconds=clockSeconds!==null && clockSeconds>=0 && clockSeconds<86400?Math.floor(clockSeconds):-1;
root.taycanSpeedLimitKmh=speedLimit;root.taycanClockSeconds=clockSeconds;

function inputFlag(name, fallback) {
    if(!running || testing)return 0;
    return flag(prefer(name,fallback));
}
function inputNumber(name, fallback, min, max) {
    if(!running || testing)return null;
    var value=numeric(prefer(name,fallback));
    return value!==null && value>=min && value<=max?value:null;
}
// Use explicit assist telemetry when available. In games without it,
// mirror the actual high-beam state to the AUTO indicator.
var autoHigh=inputFlag("autoHigh",get(p+"AutoHighBeamAssist",high));
var autoHold=inputFlag("autoHold",get(p+"AutoHoldActive",null));
var absFallback=packedRoad!==null && version>=972 && version<=978?
    packedRoad%2:firstAvailable(["DataCorePlugin.GameData.ABSActive",p+"ABSActive"]);
if(absFallback===null && isBeam && dashMask!==null)absFallback=(dashMask&1024)?1:0;
var absActive=inputFlag("absActive",absFallback);
var absWarning=inputFlag("absWarning",firstAvailable([p+"ABSWarning",raw+"light_Abs"]));
var frontFog=inputFlag("frontFog",get(p+"FrontFogLights",null));
var engineWarning=lamp("checkEngine","DL_ENGINE");
if(running && !testing && !override.checkEngine){
    var packedEngine=version>=975 && version<=978?numeric(get(raw+"reserved_3",null)):null;
    var engineFallback=firstAvailable([p+"CheckEngine",p+"CheckEngineLight"]);
    if(packedEngine!==null && packedEngine>=0)engineFallback=Math.floor(packedEngine/262144)%2;
    engineWarning=inputFlag("checkEngine",engineFallback===null?engineWarning:engineFallback);
}
var ambient=null;
if(isBeam && version>=973 && version<=978){
    var environment=numeric(get(raw+"reserved_1",null));
    if(environment!==null && environment>=0){
        var tempCode=Math.floor(environment/8)%4096;
        if(tempCode>0)ambient=(tempCode-1001)/10;
    }
}
if(ambient===null)ambient=firstNumber([p+"AirTemperature",p+"AmbientTemperature"]);
var outside=inputNumber("outside",ambient,-100,150);
// Keep real ambient data when supplied. This fallback is Celsius.
var socCode=version>=976 && version<=978 && packedRoad!==null?Math.floor(packedRoad/16384):0;
var socFallback=socCode>=1 && socCode<=101?socCode-1:
    firstNumber([p+"BatterySocPercent",p+"BatteryChargePercent"]);
var batterySoc=inputNumber("batterySoc",socFallback,0,100);
var rangeKm=inputNumber("rangeKm",firstNumber([p+"RemainingRangeKm",p+"FuelRangeKm"]),0,2000);
var batteryTemp=inputNumber("batteryTempC",firstNumber([p+"BatteryTemperatureC"]),-99,150);
var chargeStatus=running&&!testing?prefer("charging",get(p+"IsBatteryCharging",null)):null;
var charging=chargeStatus===null?-1:flag(chargeStatus);
var flags=autoHigh+2*autoHold+4*absActive+8*absWarning+16*frontFog;
root.taycanBatterySocPercent=batterySoc;
root.taycanBatteryTemperatureC=batteryTemp;
root.taycanBatteryCharging=charging===-1?null:charging;

return [
    running?num(get(p+"Rpms",0),0):0,
    running?num(get(p+"SpeedKmh",0),0):0,
    running?String(get(p+"Gear","N")):"N",
    num(get(p+"WaterTemperature",50),50),
    testing?2:(override.key?num(get(override.key,0),0):(running?2:0)),
    high,fog,left,right,
    handbrake,
    running?num(get(p+"Throttle",0),0):0,
    num(get(override.charge,0),0),
    outside===null?23:Math.round(outside),
    num(get(override.brightness,80),80),
    lamp("door",""),engineWarning,cruiseActive,
    low,parking,
    root.taycanHeartbeat,
    escOff,tcOff,escActive,tcActive,
    cruiseSet,fuelPct,fullRange,speedLimit,oilTemp,oilPressure,
    tpms[0],tpms[1],tpms[2],tpms[3],tyreWarning,
    laneKeep,laneChange,doors,psmSport,psmOff,
    signedPowerPermille,clockSeconds,flags,
    rangeKm===null?-1:Math.round(rangeKm),
    batterySoc===null?-1:Math.round(batterySoc),
    batteryTemp===null?-1000:Math.round(batteryTemp*10),charging
].join(";");

})();
