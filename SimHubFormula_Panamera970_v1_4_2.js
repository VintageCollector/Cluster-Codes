// SimHub Arduino Custom Protocol, JavaScript enabled, Multiple USB mode.
// Do not add a newline or a binary AA header. See README for the 44-field Panamera-970 contract.
// Full driving formula. Preserve reported Neutral; never infer Park from gearIndex.
// Optional: set diagnosticMode=true temporarily to inspect game, ESC and cruise telemetry.
// Diagnostic mode returns readable text instead of driving the cluster.
var diagnosticMode = false;
// Outside temperature uses actual telemetry/override where available, otherwise 24 C.
// 1 preserves actual truck volts; 0.5 optionally represents a 24 V truck on a 12 V gauge.
var truckBatteryVoltageScale = 0.5;
// Extra inputs are exact property names; blank leaves the feature unavailable.
var extra = {oilTempC:"",oilPressureBar:"",seatbelt:"",airbag:"",hold:"",brakeWarning:"",
 trunk:"",hood:"",sport:"",suspension:"",menu:"",speedLimitKmh:"",altitudeM:"",tpmsCode:"",psmMode:"",escOff:"",brakeTempC:"",brakeOverheat:"",auxiliaryLamp:"",tireIssue:"",batteryVoltage:"",headingDegrees:"",outsideTempC:"",clockSeconds:""};
// Exact names from Insert property override game adapters, then common fallbacks.
var override = {key:"",parking:"",low:"",fog:"",high:"",left:"",right:"",
    checkEngine:"",handbrake:"",cruise:"",cruiseSetSpeedKmh:"",abs:"",
    tcActive:"",tcOff:"",vdcOff:"",turbo:"",gear:"",tpms:"",oilWarning:""};
function get(name) {
    if (!name) return null;
    try { var v=$prop(name); return v===undefined || v===null || v==="" ? null : v; }
    catch(e) { return null; }
}
function numeric(v) {
    if (v===null || v===undefined || v==="" || typeof v==="boolean") return null;
    var n=Number(v); return isFinite(n) ? n : null;
}
function booleanValue(v) {
    if (v===true || v===false) return v ? 1 : 0;
    if (typeof v==="string") {
        var s=v.toLowerCase().replace(/^\s+|\s+$/g,"");
        if(s==="true" || s==="on") return 1;
        if(s==="false" || s==="off") return 0;
    }
    var n=numeric(v); return n===null ? null : (n>0 ? 1 : 0);
}
function first(names,convert,fallback) {
    for(var i=0;i<names.length;i++) {
        var v=convert(get(names[i])); if(v!==null) return v;
    }
    return fallback;
}
function clamp(v,lo,hi) { return Math.max(lo,Math.min(hi,Math.round(v))); }
var prefix="DataCorePlugin.GameData.NewData.";
var running=first(["DataCorePlugin.GameRunning"],booleanValue,0);
var game=String(get("DataCorePlugin.CurrentGame") || "").toLowerCase().replace(/[^a-z0-9]/g,"");
var assetto=game==="assettocorsa" || game==="ac";
var brakeOverheatThresholdC = 180; // User-selected ETS2/ATS threshold: 180 C / 356 F.
var truck=game==="ets2" || game==="ats" || game.indexOf("eurotrucksimulator2")>=0 || game.indexOf("americantrucksimulator")>=0;
var beam=game.indexOf("beamng")>=0;
var raw="DataCorePlugin.GameRawData.";
// Installed SimHub BeamNG Extra exports a flat struct, not electrics.values.
var beamFlat={high:["light_HighBeam"],low:["light_LowBeam","light_LowHighBeam"],
    fog:["light_Fog"],left:["light_Signal_L"],right:["light_Signal_R"],
    handbrake:["input_parkingBrake","light_Parkingbrake"],abs:["light_Abs"],tcActive:["light_TC"]};
// Optional companion uses existing reserved fields, preserving the UDP layout.
var companionTag=beam ? numeric(get(raw+"reserved_4")) : null;
var companionVersion=companionTag!==null && companionTag>=1024 ? companionTag%1024 : companionTag;
var packedCompanion=companionVersion===209 || companionVersion===210 || (companionVersion===971 || (companionVersion===972 || (companionVersion===973 || (companionVersion===974 || companionVersion===975))));
var companion=companionVersion===203 || companionVersion===207 || packedCompanion;
// v209 packs activity + cruise + target tenths km/h into the existing reserved_3.
var rawPackedStatus=packedCompanion ? numeric(get(raw+"reserved_3")) : null;
var packedStatus=(companionVersion===971 || (companionVersion===972 || (companionVersion===973 || (companionVersion===974 || companionVersion===975)))) && rawPackedStatus!==null ? rawPackedStatus%65536 : rawPackedStatus;
if(packedStatus!==null && (packedStatus<0 || packedStatus>40810 || packedStatus!==Math.floor(packedStatus))) packedStatus=null;
var companionEsc=packedStatus===null ? null : packedStatus%4;
var companionCruise=packedStatus===null ? null : Math.floor(packedStatus/4)%4;
// v973 packs environment data into the existing reserved_1 float as an exact integer.
var environmentPacked=(companionVersion===973 || (companionVersion===974 || companionVersion===975)) ? numeric(get(raw+"reserved_1")) : null;
// reserved_4 packs minute-of-day above the low 10-bit marker in companion 974.
function disabledValue(){return environmentPacked!==null ? environmentPacked%8-1 : numeric(get(raw+"reserved_1"));}
var lights=String(get(prefix+"Light") || get(prefix+"DashLights") || "").split(/[^A-Za-z0-9_]+/);
function token(name) { for(var i=0;i<lights.length;i++) if(lights[i]===name) return 1; return 0; }
function lamp(name,truckPath,beamPath,common,lightToken) {
    var names=[override[name]];
    if(truck && truckPath) names.push(raw+truckPath);
    if(beam && beamFlat[name]) {
        for(var b=0;b<beamFlat[name].length;b++) names.push(raw+beamFlat[name][b]);
    }
    if(beam && beamPath) names.push(raw+"electrics."+beamPath);
    if(common) names.push(common);
    return running ? first(names,booleanValue,lightToken ? token(lightToken) : 0) : 0;
}
var rpm=running ? clamp(first([prefix+"Rpms","TelemetryData.Rpms"],numeric,0),0,8000) : 0;
var speed=running ? clamp(first([prefix+"SpeedKmh"],numeric,0),0,340) : 0;
var key=running ? clamp(first([override.key],numeric,2),0,3) : 0;
var parking=lamp("parking","Lights.ParkingLights","parking","","DL_SIDELIGHTS");
var low=lamp("low","Lights.LowBeams","lowbeam","TelemetryData.HeadlightsOn","DL_LOWBEAM");
var high=lamp("high","Lights.HighBeams","highbeam","TelemetryData.HighBeamsOn","DL_FULLBEAM");
// User requests both headlight indicators together while high beam is on.
if(high) low=1;
var fog=lamp("fog","Lights.FogFront","fog","","DL_FOG");
// Auxiliary lamps operate the fog indicator as requested. An explicit fog override wins.
if(running && !override.fog) {
 var auxiliary=first([extra.auxiliaryLamp],booleanValue,0);
 if(truck) {
  auxiliary=auxiliary || first([raw+"TruckValues.CurrentValues.LightsValues.AuxFront",raw+"Lights.FrontAux",raw+"lightsAuxFront"],booleanValue,0);
  auxiliary=auxiliary || first([raw+"TruckValues.CurrentValues.LightsValues.AuxRoof",raw+"Lights.RoofAux",raw+"lightsAuxRoof"],booleanValue,0);
 }
 fog=fog || auxiliary;
}
// User-requested display coupling: fog stays lit whenever low beam is active.
if(low)fog=1;
// Simulator convenience: light the documented parking/backlight with headlights.
var backlightWithHeadlights=true;
if(backlightWithHeadlights && (low || high || fog)) parking=1;
var left=lamp("left","Lights.BlinkerLeftOn","signal_L",prefix+"TurnIndicatorLeft","DL_SIGNAL_L");
var right=lamp("right","Lights.BlinkerRightOn","signal_R",prefix+"TurnIndicatorRight","DL_SIGNAL_R");
var handbrake=lamp("handbrake","","parkingbrake",prefix+"Handbrake","DL_HANDBRAKE");
var abs=lamp("abs","","absActive",prefix+"ABSActive","DL_ABS");
var tc=lamp("tcActive","","tcsActive",prefix+"TCActive","DL_TC");
if(companion && !override.abs && numeric(get(raw+"reserved_2"))>=0)
    abs=running ? ((companionVersion===972 || (companionVersion===973 || (companionVersion===974 || companionVersion===975))) ? Math.max(0,Math.floor(numeric(get(raw+"reserved_2")) || 0))%2 : first([raw+"reserved_2"],booleanValue,0)) : 0;
if(companion && !packedCompanion && !override.tcActive && numeric(get(raw+"reserved_3"))>=0)
    tc=running ? first([raw+"reserved_3"],booleanValue,0) : 0;
if(packedCompanion && !override.tcActive && (companionEsc===1 || companionEsc===2))
    tc=running && companionEsc===2 ? 1 : 0;
// Older GameData aliases are tried only when the standardized property is absent.
if(!beam && !override.handbrake && get(prefix+"Handbrake")===null) handbrake=lamp("handbrake","","","GameData.Handbrake","DL_HANDBRAKE");
// SCS exposes ParkingBrake in raw motor/drivetrain data. A known false value
// wins over stale aliases; missing properties retain the common fallback above.
if(truck && !override.handbrake)
    handbrake=running ? first([raw+"TruckValues.CurrentValues.MotorValues.BrakeValues.ParkingBrake",
        raw+"Drivetrain.ParkingBrake"],booleanValue,handbrake) : 0;
if(!beam && !override.abs && get(prefix+"ABSActive")===null) abs=lamp("abs","","","GameData.ABSActive","DL_ABS");
if(!beam && !override.tcActive && get(prefix+"TCActive")===null) tc=lamp("tcActive","","","GameData.TCActive","DL_TC");
if(beam && !override.tcActive && !(companion && !packedCompanion && numeric(get(raw+"reserved_3"))>=0)
    && !(packedCompanion && (companionEsc===1 || companionEsc===2))) {
    var escActivity=first([raw+"electrics.escActive"],booleanValue,null);
    var tractionActivity=first([raw+"electrics.tcsActive"],booleanValue,null);
    if(escActivity!==null || tractionActivity!==null)
        tc=running && (escActivity===1 || tractionActivity===1) ? 1 : 0;
}
// TC inactive is NOT TC disabled. Off states need explicit properties/overrides.
var tcOff=lamp("tcOff","","","","");
var vdcOff=lamp("vdcOff","","","","");
// v207 exports explicit disabled-state selector in the unused Park slot.
// -1 unknown, 0 enabled, 2 TC disabled, 3 ESC/VDC disabled.
if(running && (companionVersion===207 || packedCompanion)) {
    var disabledSelector=disabledValue();
    if(!override.tcOff) tcOff=disabledSelector===2 ? 1 : 0;
    if(!override.vdcOff) vdcOff=disabledSelector===3 ? 1 : 0;
}
// Priority for contradictory telemetry: activity, then TC disabled, then VDC off.
var tcState=tc ? 1 : (tcOff ? 2 : (vdcOff ? 3 : 0));
// Positive turbo telemetry activates the documented indicator; zero/missing does not.
// No pressure scale, gauge sweep, or turbo-equipped detection is inferred.
var turbo=running && first([override.turbo,"GameData.Turbo",prefix+"Turbo"],numeric,0)>0 ? 1 : 0;
var cruise=lamp("cruise","Control.CruiseControl","cruiseControlState","TelemetryData.CruiseControlOn","") ? 2 : 0;
// Verified installed ETS2Reader legacy and current SCS SDK object paths.
var truckDash=raw+"TruckValues.CurrentValues.DashboardValues.";
if(running && truck && !override.cruise)
    cruise=first([truckDash+"CruiseControl",raw+"Drivetrain.CruiseControl",
        raw+"Control.CruiseControl","TelemetryData.CruiseControlOn"],booleanValue,0) ? 2 : 0;
if(running && !override.cruise && packedCompanion && (companionCruise===1 || companionCruise===2))
    cruise=companionCruise===2 ? 2 : 0;
if(override.cruise) cruise=running ? clamp(first([override.cruise],numeric,0),0,3) : 0;
// User requests km/h directly for cruise target; no mph conversion.
// No assumed raw-game m/s conversions. Configure the set-speed property above.
var setSpeed=running && cruise ? clamp(first([override.cruiseSetSpeedKmh],numeric,0),0,255) : 0;
if(running && cruise && truck && !override.cruiseSetSpeedKmh)
    setSpeed=clamp(first([truckDash+"CruiseControlSpeed.Kph",raw+"Drivetrain.CruiseControlSpeedKmh"],numeric,0),0,255);
if(running && cruise && !override.cruiseSetSpeedKmh && packedStatus!==null)
    setSpeed=clamp(Math.floor(packedStatus/16)/10,0,255);
var check=lamp("checkEngine","Lights.EngineWaterTemperatureWarning","checkengine","","DL_ENGINE");
if(running && truck && !override.checkEngine) {
 check=check || first([truckDash+"WarningValues.WaterTemperature",raw+"Lights.EngineWaterTemperatureWarning"],booleanValue,0)
             || first([truckDash+"WarningValues.OilPressure",raw+"Lights.EngineOilPressureWarning"],booleanValue,0);
}
// Companion extension bit 18 carries BeamNG's actual check-engine flag.
if(running && beam && !override.checkEngine && (companionVersion===971 || (companionVersion===972 || (companionVersion===973 || (companionVersion===974 || companionVersion===975)))) && rawPackedStatus!==null)
 check=check || (Math.floor(rawPackedStatus/262144)%2===1 ? 1 : 0);
var engine=first(["TelemetryData.EngineRunning"],booleanValue,null);
// Bulb test needs electronics ON and known stopped engine, never absent RPM alone.
var rpmKnown=first([prefix+"Rpms","TelemetryData.Rpms"],numeric,null);
if(!override.checkEngine && running && key>=2 && (engine===0 || (engine===null && rpmKnown===0))) check=1;
if(override.checkEngine) check=clamp(first([override.checkEngine],numeric,booleanValue(get(override.checkEngine)) || 0),0,2);
if(!running) check=0;
function gearToken(value) {
    if(value===null || value===undefined) return null;
    var s=String(value).toUpperCase().replace(/^\s+|\s+$/g,"");
    if(s==="PARK") s="P";
    if(s==="NEUTRAL") s="N";
    if(s==="REVERSE") s="R";
    if(s==="DRIVE") s="D";
    return /^(P|N|R|D|L|0|-1|[1-8]|[MASD][1-8])$/.test(s) ? s : null;
}
var gearNames=[override.gear];
if(beam) gearNames.push(raw+"electrics.gear");
gearNames.push(prefix+"Gear");
var gear=first(gearNames,gearToken,"N");
// BeamNG can report N for Park. Keep that N rather than invent a selector.
// Only an actual textual P or explicit gear override is allowed to request P.
// Preserve only known legacy gear tokens; prevent delimiter/newline injection.
if(!/^(P|N|R|D|L|0|-1|[1-8]|[MASD][1-8])$/.test(gear)) gear="N";
if(!running) gear="N";
// User-confirmed TPMS warning mapping. Flat-tyre status, not guessed PSI.
var tpms=0;
if(running) {
    var explicitTpms=first([override.tpms],booleanValue,null);
    if(explicitTpms!==null) tpms=explicitTpms;
    else if(extra.tireIssue) tpms=first([extra.tireIssue],booleanValue,0);
    else if(beam) {
        var lowPressure=first([raw+"electrics.lowpressure"],booleanValue,null);
        var wheels=["fl","fr","rl","rr"], anyKnown=false, anyFlat=0;
        for(var w=0;w<wheels.length;w++) {
            var deflated=first([raw+"wheel_isDeflated_"+wheels[w]],booleanValue,null);
            if(deflated!==null) { anyKnown=true; if(deflated) anyFlat=1; }
        }
        tpms=anyFlat || (lowPressure===1 ? 1 : 0);
    }
}
var oilWarning=0;
if(running) {
    var explicitOil=first([override.oilWarning],booleanValue,null);
    if(explicitOil!==null) oilWarning=explicitOil;
    else if(beam) oilWarning=first([raw+"light_Oil",raw+"electrics.oil"],booleanValue,0);
    else if(truck) oilWarning=first([raw+"Lights.EngineOilPressureWarning",raw+"oilPressureWarning"],booleanValue,0);
}
if(diagnosticMode) {
    return "game="+game+"; running="+running+"; marker="+companionVersion
        +"; disabled="+get(raw+"reserved_1")+"; packed="+packedStatus
        +"; escCode="+companionEsc+"; cruiseCode="+companionCruise
        +"; tcState="+tcState+"; cruiseMode="+cruise+"; setKmh="+setSpeed;
}
root.panameraHeartbeat=((Number(root.panameraHeartbeat)||0)+1)%1000000;
function extraNum(name,fallback) { return first([extra[name]],numeric,fallback); }
function extraFlag(name) { return running ? first([extra[name]],booleanValue,0) : 0; }
var oilTemp=running ? first([extra.oilTempC,beam ? raw+"oilTemp" : "",truck ? truckDash+"OilTemperature" : "",truck ? raw+"Drivetrain.OilTemperature" : ""],numeric,-1000) : -1000;
// Only explicitly bar-valued telemetry is consumed automatically by this field.
// Truck pressure conversion can be configured using override/extra property sources.
var pressure=running ? extraNum("oilPressureBar",-1) : -1;
if(running && pressure<0 && truck) {
 var pressurePsi=first([truckDash+"OilPressure",raw+"Drivetrain.OilPressure"],numeric,null);
 if(pressurePsi!==null) pressure=pressurePsi/14.5037738;
}
// User-authorized display estimate when a real pressure source is unavailable.
// 0 RPM=0 bar, running idle through 1000=1 bar, then linear to 5 bar at 6500.
var pressureEstimated=false;
if(running && pressure<0) {
 var modelRpm=first([prefix+"Rpms","TelemetryData.Rpms"],numeric,null);
 if(modelRpm!==null) {
  pressure=modelRpm<=0 ? 0 : (modelRpm<=1000 ? 1 : Math.min(5,1+(modelRpm-1000)*4/5500));
  pressureEstimated=true;
 }
}
root.panameraPressureSource=pressure<0 ? "unavailable" : (pressureEstimated ? "RPM estimate" : "telemetry");
// Known SDK oil warning paths; BeamNG light_Oil is a temperature warning.
if(running && truck && !override.oilWarning)
 oilWarning=first([truckDash+"WarningValues.OilPressure",raw+"Lights.EngineOilPressureWarning",raw+"oilPressureWarning"],booleanValue,0);
var brakeTemp=running ? first([extra.brakeTempC,truck ? raw+"Drivetrain.BrakeTemperature" : "",truck ? raw+"brakeTemperature" : ""],numeric,-1000) : -1000;
if(running && beam && !extra.brakeTempC) {
 for(var bt=0;bt<4;bt++) brakeTemp=Math.max(brakeTemp,first([raw+"brake_surfaceTemperature_"+["fl","fr","rl","rr"][bt]],numeric,-1000));
}
if(running && assetto && !extra.brakeTempC) {
 for(var at=0;at<4;at++) brakeTemp=Math.max(brakeTemp,first([raw+"Physics.BrakeTemp"+at,raw+"Physics.BrakeTemp["+at+"]"],numeric,-1000));
}
var brakeOverheat=extraFlag("brakeOverheat");
if(running && !extra.brakeOverheat && (companionVersion===971 || (companionVersion===972 || (companionVersion===973 || (companionVersion===974 || companionVersion===975)))) && rawPackedStatus!==null)
 brakeOverheat=Math.floor(rawPackedStatus/65536)%2===1 ? 1 : 0;
if(running && truck && !extra.brakeOverheat && brakeOverheatThresholdC!==null && brakeTemp!==-1000)
 brakeOverheat=brakeOverheat || (brakeTemp>=brakeOverheatThresholdC ? 1 : 0);
var psm=extraNum("psmMode",-1);
var offInput=first([extra.escOff],booleanValue,null);
var disabledForLaunch=disabledValue();
var escOff=offInput!==null ? offInput===1 : (beam && (companionVersion===971 || (companionVersion===972 || (companionVersion===973 || (companionVersion===974 || companionVersion===975)))) && rawPackedStatus!==null && (disabledForLaunch===2 || disabledForLaunch===3) && Math.floor(rawPackedStatus/131072)%2===1);
if(psm<0) psm=tcState===1 ? 1 : ((tcState>=2 || escOff) ? 2 : 0);
var exactSpeed=first([prefix+"SpeedKmh"],numeric,null);
var exactRpm=first([prefix+"Rpms","TelemetryData.Rpms"],numeric,null);
var driveFirst=gear==="D" || gear==="S1" || gear==="D1" || gear==="A1" || gear==="1";
var launch=running && (beam || truck || assetto) && exactSpeed===0 && exactRpm!==null && exactRpm>4000 && driveFirst && escOff ? 1 : 0;
var tpmsCode=running ? extraNum("tpmsCode",-1) : -1;
// Only the all-four-flat pattern has a fully unambiguous supplied mapping.
if(tpmsCode<0 && beam && running) {
 var allFlat=true;
 for(var c=0;c<4;c++) if(first([raw+"wheel_isDeflated_"+["fl","fr","rl","rr"][c]],booleanValue,0)!==1) allFlat=false;
 tpmsCode=allFlat ? 0x55 : 0;
}
// Limits are read in km/h, then firmware applies the documented 5 mph display steps.
var speedLimit=running ? extraNum("speedLimitKmh",-1) : -1;
if(running && !extra.speedLimitKmh && truck)
 speedLimit=first([raw+"NavigationValues.SpeedLimit.Kph",raw+"Job.SpeedLimitKmh"],numeric,-1);
if(running && !extra.speedLimitKmh && beam && (companionVersion===972 || (companionVersion===973 || (companionVersion===974 || companionVersion===975)))) {
 var roadPacked=numeric(get(raw+"reserved_2"));
 speedLimit=roadPacked!==null && roadPacked>=2 ? (Math.floor(roadPacked/2)-1)/10 : -1;
}
// Zero means no posted limit; do not display it as a zero-speed sign.
if(speedLimit<=0) speedLimit=-1;
var batteryVoltage=running ? first([extra.batteryVoltage,truck ? truckDash+"BatteryVoltage" : "",truck ? raw+"Drivetrain.BatteryVoltage" : ""],numeric,null) : null;
if(truck && batteryVoltage!==null)batteryVoltage=batteryVoltage*truckBatteryVoltageScale;
if(running && beam && !extra.batteryVoltage)batteryVoltage=13.1; // User-selected fixed BeamNG value.
root.panameraBatteryVoltage=batteryVoltage;
// Calibrated voltage is carried in serial field 42.
var altitude=running ? extraNum("altitudeM",-32768) : -32768;
if(running && !extra.altitudeM) altitude=first([beam ? raw+"posZ" : "",truck ? raw+"TruckValues.Positioning.TruckPosition.Position.Y" : "",truck ? raw+"TruckValues.CurrentValues.PositionValue.Position.Y" : "",truck ? raw+"Physics.CoordinateY" : ""],numeric,-32768);
var heading=running ? extraNum("headingDegrees",-1) : -1;
if(running && !extra.headingDegrees && truck){
 var turns=first([raw+"TruckValues.Positioning.TruckPosition.Orientation.Heading",raw+"TruckValues.CurrentValues.PositionValue.Orientation.Heading",raw+"Physics.RotationX"],numeric,null);
 if(turns!==null)heading=((turns*360)%360+360)%360;
}
if(running && !extra.headingDegrees && environmentPacked!==null) heading=Math.floor(environmentPacked/32768)-1;
if(heading>=0)heading=(Math.round(heading)%360+360)%360;
var outside=running ? extraNum("outsideTempC",-1000) : -1000;
if(running && !extra.outsideTempC && environmentPacked!==null){
 var t=Math.floor(environmentPacked/8)%4096;
 if(t>0)outside=(t-1001)/10;
}
// User-selected fallback when no actual game/override temperature is available.
if(running && outside===-1000)outside=24;
root.panameraOutsideTempC=outside===-1000 ? null : outside;
var clock=running ? extraNum("clockSeconds",-1) : -1;
if(running && !extra.clockSeconds && truck){
 var gameMinutes=first([raw+"CommonValues.GameTime.Minutes",raw+"Time"],numeric,null);
 if(gameMinutes!==null && gameMinutes>=0)clock=Math.floor(gameMinutes%1440)*60;
}
if(running && !extra.clockSeconds && beam && (companionVersion===974 || companionVersion===975)){
 var clockMinuteCode=Math.floor(companionTag/1024)%2048;
 if(clockMinuteCode>=1 && clockMinuteCode<=1440)clock=(clockMinuteCode-1)*60;
}
root.panameraClockSeconds=clock;
var sportMode=running ? clamp(extraNum("sport",0),0,2) : 0;
var driveModeCode=running && beam && companionVersion===975 ? Math.floor(companionTag/2097152)%4 : 0;
if(running && beam && companionVersion===975 && !extra.sport) sportMode=driveModeCode>=2 ? driveModeCode-1 : 0;
root.panameraDriveMode=driveModeCode===3 ? "Sport Plus" : (driveModeCode===2 ? "Sport" : (driveModeCode===1 ? "Comfort/Normal/Eco" : "unavailable/unmapped"));
root.panameraHeadingDegrees=heading;
root.panameraTireIssue=tpms;
root.panameraTireWarningUnmapped=!!(tpms && tpmsCode<=0);
return [rpm,speed,gear,clamp(first([prefix+"WaterTemperature"],numeric,0),-40,150),key,
    parking,low,fog,high,left,right,check,handbrake,cruise,root.panameraHeartbeat,
    970,abs,tcState,turbo,setSpeed,tpms,oilWarning,
    clamp(oilTemp,-1000,200),pressure<0 ? -1 : clamp(pressure*1000,0,5000),
    extraFlag("seatbelt"),extraFlag("airbag"),extraFlag("hold"),extraFlag("brakeWarning"),
    extraFlag("trunk"),extraFlag("hood"),sportMode,
    running ? clamp(extraNum("suspension",0),0,11) : 0,running ? clamp(extraNum("menu",0),0,4) : 0,
    running ? clamp(speedLimit,-1,500) : -1,
    running ? clamp(altitude,-32768,7160) : -32768,
    clamp(tpmsCode,-1,255),running ? clamp(psm,0,4) : 0,launch,clamp(brakeTemp,-1000,2000),brakeOverheat,heading<0 ? -1 : heading,outside===-1000 ? -10000 : clamp(outside*10,-1000,1500),batteryVoltage===null ? -1 : clamp(batteryVoltage*1000,0,40000),clock<0 ? -1 : clamp(clock,0,86399)].join(";");

