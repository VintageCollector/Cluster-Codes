// SimHub Arduino Custom Protocol, JavaScript enabled, Multiple USB mode.
// Do not add a newline or a binary AA header. See README for the 60-field BMW-G30 contract.
// Full driving formula. Preserve reported Neutral; never infer Park from gearIndex.
// Optional: set diagnosticMode=true temporarily to inspect game, ESC and cruise telemetry.
// Diagnostic mode returns readable text instead of driving the cluster.
var diagnosticMode = false;
var simulateAutoHoldIndicator = true; // Bench display only; never applies game brakes.
// Outside temperature uses actual telemetry/override where available, otherwise 25 C.
// 1 preserves actual truck volts; 0.5 optionally represents a 24 V truck on a 12 V gauge.
var truckBatteryVoltageScale = 0.5;
// Bench policy: SCS does not expose ESC enable. Explicit extra.escOff still takes priority.
var truckLaunchAssumeEscEnabledWhenUnavailable = true;
var truckEngineDamageWarningFraction = 0.50; // User-selected 50% damage threshold.
// Extra inputs are exact property names; blank leaves the feature unavailable.
var extra = {oilTempC:"",oilPressureBar:"",seatbelt:"",airbag:"",hold:"",brakeWarning:"",
 trunk:"",hood:"",sport:"",suspension:"",menu:"",speedLimitKmh:"",altitudeM:"",tpmsCode:"",psmMode:"",escOff:"",brakeTempC:"",brakeOverheat:"",auxiliaryLamp:"",tireIssue:"",batteryVoltage:"",headingDegrees:"",outsideTempC:"",clockSeconds:"",doorFL:"",doorFR:"",doorRL:"",doorRR:"",fuelPercent:"",engineOverheat:"",driveMode:"",laneAssist:"",engineDamageFraction:"",averageConsumptionL100km:"",fuelRateLph:"",recuperationKw:"",ecoNavSymbolCode:"",nextManeuverDistanceM:"",shiftRingMaxRpm:""};
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
var packedCompanion=companionVersion===209 || companionVersion===210 || (companionVersion===971 || (companionVersion===972 || (companionVersion===973 || (companionVersion===974 || (companionVersion===975 || (companionVersion===976 || (companionVersion===977 || (companionVersion===978 || companionVersion===979))))))));
var companion=companionVersion===203 || companionVersion===207 || packedCompanion;
// v209 packs activity + cruise + target tenths km/h into the existing reserved_3.
var rawPackedStatus=packedCompanion ? numeric(get(raw+"reserved_3")) : null;
var packedStatus=(companionVersion===971 || (companionVersion===972 || (companionVersion===973 || (companionVersion===974 || (companionVersion===975 || (companionVersion===976 || (companionVersion===977 || (companionVersion===978 || companionVersion===979)))))))) && rawPackedStatus!==null ? rawPackedStatus%65536 : rawPackedStatus;
if(packedStatus!==null && (packedStatus<0 || packedStatus>40810 || packedStatus!==Math.floor(packedStatus))) packedStatus=null;
var companionEsc=packedStatus===null ? null : packedStatus%4;
var companionCruise=packedStatus===null ? null : Math.floor(packedStatus/4)%4;
// v973 packs environment data into the existing reserved_1 float as an exact integer.
var environmentPacked=(companionVersion===973 || (companionVersion===974 || (companionVersion===975 || (companionVersion===976 || (companionVersion===977 || (companionVersion===978 || companionVersion===979)))))) ? numeric(get(raw+"reserved_1")) : null;
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
var rpm=running ? clamp(first([prefix+"Rpms","TelemetryData.Rpms"],numeric,0),0,10000) : 0;
var speed=running ? clamp(first([prefix+"SpeedKmh"],numeric,0),0,340) : 0;
// Serial key:0off,1accessory,2ignition on,3starter/engine running terminal profile.
var key=running ? clamp(first([override.key],numeric,2),0,3) : 0;
var ignitionLevel=null;
var engineState=null;
if(running && !override.key && beam){
 ignitionLevel=first([raw+"electrics.ignitionLevel"],numeric,null);
 if((companionVersion===978 || companionVersion===979) && rawPackedStatus!==null){var packedKey=Math.floor(rawPackedStatus/524288)%8;if(packedKey>=1&&packedKey<=4)ignitionLevel=packedKey-1;}
 engineState=first([raw+"light_EngineRunning",raw+"electrics.engineRunning","TelemetryData.EngineRunning"],booleanValue,null);
 if(ignitionLevel!==null && ignitionLevel>=0 && ignitionLevel<=3)key=ignitionLevel;
 else {var ignitionBool=first([raw+"ignitionOn"],booleanValue,null);if(ignitionBool!==null)key=ignitionBool?2:0;}
 if(key>=2 && engineState===1)key=3;
}
if(running && !override.key && truck){
 var electricState=first([raw+"TruckValues.CurrentValues.ElectricEnabled",raw+"Drivetrain.ElectricEnabled"],booleanValue,null);
 engineState=first([raw+"TruckValues.CurrentValues.EngineEnabled",raw+"Drivetrain.EngineEnabled","TelemetryData.EngineRunning"],booleanValue,null);
 if(electricState===0)key=0;
 else if(engineState===1)key=3;
 else if(electricState===1)key=2;
}
root.bmwIgnitionLevel=ignitionLevel;root.bmwTerminalState=key;
var parking=lamp("parking","Lights.ParkingLights","parking","","DL_SIDELIGHTS");
var low=lamp("low","Lights.LowBeams","lowbeam","TelemetryData.HeadlightsOn","DL_LOWBEAM");
var high=lamp("high","Lights.HighBeams","highbeam","TelemetryData.HighBeamsOn","DL_FULLBEAM");
// User requests both headlight indicators together while high beam is on.
if(truck){
 if(!override.low)low=running ? first([raw+"TruckValues.CurrentValues.LightsValues.BeamLow"],booleanValue,low) : 0;
 if(!override.high)high=running ? first([raw+"TruckValues.CurrentValues.LightsValues.BeamHigh"],booleanValue,high) : 0;
 if(!override.parking)parking=running ? first([raw+"TruckValues.CurrentValues.LightsValues.Parking"],booleanValue,parking) : 0;
}
// User-requested DRL/parking availability while ignition is ON.
if(running && key>=2)parking=1;
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
if(truck){
 if(!override.left)left=running ? first([raw+"TruckValues.CurrentValues.LightsValues.BlinkerLeftOn"],booleanValue,left) : 0;
 if(!override.right)right=running ? first([raw+"TruckValues.CurrentValues.LightsValues.BlinkerRightOn"],booleanValue,right) : 0;
}
var handbrake=lamp("handbrake","","parkingbrake",prefix+"Handbrake","DL_HANDBRAKE");
var abs=lamp("abs","","absActive",prefix+"ABSActive","DL_ABS");
var tc=lamp("tcActive","","tcsActive",prefix+"TCActive","DL_TC");
if(companion && !override.abs && numeric(get(raw+"reserved_2"))>=0)
    abs=running ? ((companionVersion===972 || (companionVersion===973 || (companionVersion===974 || (companionVersion===975 || (companionVersion===976 || (companionVersion===977 || (companionVersion===978 || companionVersion===979))))))) ? Math.max(0,Math.floor(numeric(get(raw+"reserved_2")) || 0))%2 : first([raw+"reserved_2"],booleanValue,0)) : 0;
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
if(running && beam && !override.checkEngine && (companionVersion===971 || (companionVersion===972 || (companionVersion===973 || (companionVersion===974 || (companionVersion===975 || (companionVersion===976 || (companionVersion===977 || (companionVersion===978 || companionVersion===979)))))))) && rawPackedStatus!==null)
 check=check || (Math.floor(rawPackedStatus/262144)%2===1 ? 1 : 0);
if(running && truck && !override.checkEngine){
 var engineDamage=first([extra.engineDamageFraction,raw+"TruckValues.CurrentValues.DamageValues.Engine",raw+"Damage.WearEngine"],numeric,null);
 if(engineDamage!==null&&engineDamage>=0&&engineDamage<=1&&engineDamage>=truckEngineDamageWarningFraction)check=1;
}
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
    return /^(P|N|R|D|L|0|-1|[1-9]|[MASD][1-9])$/.test(s) ? s : null;
}
var gearNames=[override.gear];
if(beam) gearNames.push(raw+"electrics.gear");
gearNames.push(prefix+"Gear");
var gear=first(gearNames,gearToken,"N");
// BeamNG can report N for Park. Keep that N rather than invent a selector.
// Only an actual textual P or explicit gear override is allowed to request P.
// Preserve only known legacy gear tokens; prevent delimiter/newline injection.
if(!/^(P|N|R|D|L|0|-1|[1-9]|[MASD][1-9])$/.test(gear)) gear="N";
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
root.bmwHeartbeat=((Number(root.bmwHeartbeat)||0)+1)%1000000;
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
root.bmwPressureSource=pressure<0 ? "unavailable" : (pressureEstimated ? "RPM estimate" : "telemetry");
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
if(running && !extra.brakeOverheat && (companionVersion===971 || (companionVersion===972 || (companionVersion===973 || (companionVersion===974 || (companionVersion===975 || (companionVersion===976 || (companionVersion===977 || (companionVersion===978 || companionVersion===979)))))))) && rawPackedStatus!==null)
 brakeOverheat=Math.floor(rawPackedStatus/65536)%2===1 ? 1 : 0;
if(running && truck && !extra.brakeOverheat && brakeOverheatThresholdC!==null && brakeTemp!==-1000)
 brakeOverheat=brakeOverheat || (brakeTemp>=brakeOverheatThresholdC ? 1 : 0);
var psm=extraNum("psmMode",-1);
var offInput=first([extra.escOff],booleanValue,null);
var disabledForLaunch=disabledValue();
var escOff=offInput!==null ? offInput===1 : (beam && (companionVersion===971 || (companionVersion===972 || (companionVersion===973 || (companionVersion===974 || (companionVersion===975 || (companionVersion===976 || (companionVersion===977 || (companionVersion===978 || companionVersion===979)))))))) && rawPackedStatus!==null && (disabledForLaunch===2 || disabledForLaunch===3) && Math.floor(rawPackedStatus/131072)%2===1);
if(psm<0) psm=tcState===1 ? 1 : ((tcState>=2 || escOff) ? 2 : 0);
var exactSpeed=first([prefix+"SpeedKmh"],numeric,null);
var exactRpm=first([prefix+"Rpms","TelemetryData.Rpms"],numeric,null);
var launch=0;
var tpmsCode=running ? extraNum("tpmsCode",-1) : -1;
// Only the all-four-flat pattern has a fully unambiguous supplied mapping.
if(tpmsCode<0 && beam && running) {
 var allFlat=true;
 for(var c=0;c<4;c++) if(first([raw+"wheel_isDeflated_"+["fl","fr","rl","rr"][c]],booleanValue,0)!==1) allFlat=false;
 tpmsCode=allFlat ? 0x55 : 0;
}
// Speed limits are normalized to km/h. G30 sign CAN encoding remains unmapped.
var speedLimit=running ? extraNum("speedLimitKmh",-1) : -1;
if(running && !extra.speedLimitKmh && truck)
 speedLimit=first([raw+"NavigationValues.SpeedLimit.Kph",raw+"Job.SpeedLimitKmh"],numeric,-1);
if(running && !extra.speedLimitKmh && beam && (companionVersion===972 || (companionVersion===973 || (companionVersion===974 || (companionVersion===975 || (companionVersion===976 || (companionVersion===977 || (companionVersion===978 || companionVersion===979)))))))) {
 var roadPacked=numeric(get(raw+"reserved_2"));
 speedLimit=roadPacked!==null && roadPacked>=2 ? (Math.floor(roadPacked/2)-1)/10 : -1;
}
// Zero means no posted limit; do not display it as a zero-speed sign.
if(speedLimit<=0) speedLimit=-1;
var batteryVoltage=running ? first([extra.batteryVoltage,truck ? truckDash+"BatteryVoltage" : "",truck ? raw+"Drivetrain.BatteryVoltage" : ""],numeric,null) : null;
if(truck && batteryVoltage!==null)batteryVoltage=batteryVoltage*truckBatteryVoltageScale;
if(running && beam && !extra.batteryVoltage)batteryVoltage=13.1; // User-selected fixed BeamNG value.
root.bmwBatteryVoltage=batteryVoltage;
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
if(running && outside===-1000)outside=25;
root.bmwOutsideTempC=outside===-1000 ? null : outside;
var clock=running ? extraNum("clockSeconds",-1) : -1;
if(running && !extra.clockSeconds && truck){
 var gameMinutes=first([raw+"CommonValues.GameTime.Minutes",raw+"Time"],numeric,null);
 if(gameMinutes!==null && gameMinutes>=0)clock=Math.floor(gameMinutes%1440)*60;
}
if(running && !extra.clockSeconds && beam && (companionVersion===974 || (companionVersion===975 || (companionVersion===976 || (companionVersion===977 || (companionVersion===978 || companionVersion===979)))))){
 var clockMinuteCode=Math.floor(companionTag/1024)%2048;
 if(clockMinuteCode>=1 && clockMinuteCode<=1440)clock=(clockMinuteCode-1)*60;
}
root.bmwClockSeconds=clock;
function bmwMode(value){
 if(value===null||value===undefined)return null;
 var n=numeric(value);if(n!==null)return [0,1,2,3,4,5,7,8,9,10].indexOf(n)>=0?n:null;
 var name=String(value).toLowerCase().replace(/[ _-]/g,"");
 var modes={sport:1,ttsport:1,sportplus:2,"sport+":2,"ttsport+":2,sportindividual:3,comfort:4,normal:4,standard:4,"default":4,strada:4,comfortplus:5,"comfort+":5,eco:7,ecopro:7,ecoproplus:8,ecopro2:8,ecoproindividual:9,adaptive:10};
 return Object.prototype.hasOwnProperty.call(modes,name)?modes[name]:null;
}
var selectedMode=running ? first([extra.driveMode],bmwMode,0) : 0;
if(running && !extra.driveMode && beam && (companionVersion===976 || (companionVersion===977 || (companionVersion===978 || companionVersion===979)))){
 var modeCode=Math.floor(companionTag/2097152)%8;
 selectedMode=[0,4,1,2,7,5,3,10][modeCode];
}
var fuelPercent=running ? first([extra.fuelPercent,"DataCorePlugin.Computed.Fuel_Percent"],numeric,-1) : -1;
if(running && fuelPercent<0){
 var currentFuel=first([prefix+"Fuel"],numeric,null),maxFuel=first([prefix+"MaxFuel"],numeric,null);
 if(currentFuel!==null&&maxFuel!==null&&maxFuel>0)fuelPercent=currentFuel*100/maxFuel;
}
var engineOverheat=running ? first([extra.engineOverheat,truck ? truckDash+"WarningValues.WaterTemperature" : ""],booleanValue,null) : 0;
if(engineOverheat===null)engineOverheat=first([prefix+"WaterTemperature"],numeric,0)>=120?1:0;
// User preference: Comfort displays the Eco Pro theme.
if(selectedMode===4)selectedMode=7;
// Automatic numbered gears follow the selected mode; explicit manual M remains manual.
if(!override.gear){
 var automaticNumber=/^(?:[ASD])?([1-9])$/.exec(gear);
 if(automaticNumber && selectedMode!==0)gear=(selectedMode===1||selectedMode===2?"S":"D")+automaticNumber[1];
}
root.bmwSelectedDriveMode=selectedMode;
// v1.3: arm at rest above 1500 RPM with confirmed ESC enabled in Sport/Sport+.
// Latch until speed exceeds 65 mph (104.60736 km/h), or context becomes invalid.
var escEnabled=offInput!==null ? offInput===0 : (beam && packedCompanion && ((companionVersion===979 && rawPackedStatus!==null) ? Math.floor(rawPackedStatus/4194304)%2===1 : disabledForLaunch===0));
var truckEscAssumed=truck && offInput===null && truckLaunchAssumeEscEnabledWhenUnavailable;
if(truckEscAssumed)escEnabled=true;
var launchSportMode=selectedMode===1 || selectedMode===2 || (truck && selectedMode===0 && /^S[1-9]$/.test(gear));
root.bmwLaunchEscSource=truckEscAssumed?"bench assumption (SCS unavailable)":(offInput!==null?"explicit property":"BeamNG companion");
var launchContext=running && key>=2 && (beam || truck || assetto) && launchSportMode && /^S[1-9]$/.test(gear) && escEnabled && !vdcOff;
if(root.bmwLaunchVersion!==13 || root.bmwLaunchGame!==game)root.bmwLaunchLatched=false;
if(!launchContext || exactSpeed===null || exactSpeed>104.60736)root.bmwLaunchLatched=false;
else if(exactSpeed===0 && exactRpm!==null && exactRpm>1500)root.bmwLaunchLatched=true;
var launchReason=!running?"game stopped":(key<2?"ignition off/ACC":(!/^S[1-9]$/.test(gear)?"needs S gear":(!launchSportMode?"Sport mode unavailable":(!escEnabled?"ESC enabled not confirmed":(vdcOff?"ESC disabled":(exactSpeed===null?"speed unavailable":(exactSpeed>104.60736?"above cutoff":(!root.bmwLaunchLatched&&exactSpeed!==0?"needs standstill":(!root.bmwLaunchLatched&&(exactRpm===null||exactRpm<=1500)?"needs RPM above 1500":"ready/latched")))))))));
root.bmwLaunchReason=launchReason;
root.bmwLaunchVersion=13;root.bmwLaunchGame=game;
launch=root.bmwLaunchLatched ? 1 : 0;

// Public 39E reference includes calendar context; preserve actual PC day/year, game time for H:M:S.
var calendar=new Date();
var calendarDay=running ? calendar.getDate() : 0;
var calendarYear=running ? calendar.getFullYear() : 0;
var sportMode=running ? clamp(extraNum("sport",0),0,2) : 0;
var driveModeCode=running && beam && (companionVersion===975 || (companionVersion===976 || (companionVersion===977 || (companionVersion===978 || companionVersion===979)))) ? Math.floor(companionTag/2097152)%8 : 0;
if(running && beam && (companionVersion===975 || (companionVersion===976 || (companionVersion===977 || (companionVersion===978 || companionVersion===979)))) && !extra.sport) sportMode=driveModeCode===2 ? 1 : (driveModeCode===3 ? 2 : 0);
root.bmwDriveMode=driveModeCode===3 ? "Sport Plus" : (driveModeCode===2 ? "Sport" : (driveModeCode===1 ? "Comfort/Normal/Eco" : "unavailable/unmapped"));
root.bmwHeadingDegrees=heading;
root.bmwTireIssue=tpms;
root.bmwTireWarningUnmapped=!!(tpms && tpmsCode<=0);
// Acquisition/diagnostics only until the G30 consumption and Eco CAN formats are established.
var averageConsumption=running ? extraNum("averageConsumptionL100km",-1) : -1;
if(running && truck && !extra.averageConsumptionL100km){
 var litresPerKm=first([truckDash+"FuelValue.AverageConsumption"],numeric,null);
 averageConsumption=litresPerKm!==null && litresPerKm>=0 ? litresPerKm*100 : -1;
}
if(averageConsumption<0 || averageConsumption>1000)averageConsumption=-1;
root.bmwConsumptionL100km=averageConsumption;
root.bmwConsumptionMpgUS=averageConsumption>0 ? 235.214583/averageConsumption : -1;
root.bmwConsumptionKmPerL=averageConsumption>0 ? 100/averageConsumption : -1;
root.bmwFuelRateLph=running ? extraNum("fuelRateLph",-1) : -1;
root.bmwRecuperationKw=running ? extraNum("recuperationKw",-1) : -1;
root.bmwSpeedLimitKmh=running ? speedLimit : -1;
// X4M sketch gives event symbols, not ordinary left/right route-arrow encoding.
// Exact-property overrides only: do not substitute turn indicators or total route distance.
var ecoNavCode=running ? extraNum("ecoNavSymbolCode",7) : 7;
var nextManeuverMeters=running ? extraNum("nextManeuverDistanceM",-1) : -1;
if((ecoNavCode!==30&&ecoNavCode!==80&&ecoNavCode!==130)||nextManeuverMeters<0||nextManeuverMeters>2550){ecoNavCode=7;nextManeuverMeters=-1;}
// Gear-ring fill: source byte0 supports 0..8 segments. Scale from idle/zero to reported maximum RPM.
var ringMaxRpm=first([extra.shiftRingMaxRpm,prefix+"MaxRpm",prefix+"MaxRPM"],numeric,null);
if(ringMaxRpm===null || ringMaxRpm<1000 || ringMaxRpm>20000)ringMaxRpm=truck ? 2500 : 6500;
var ringLevel=running && key>=2 && rpm>0 ? Math.min(8,Math.floor(rpm*8/ringMaxRpm)) : 0;
root.bmwRingMaxRpm=ringMaxRpm;root.bmwRingLevel=ringLevel;
// Auto Hold: exact configured property wins; otherwise emulate the bench indicator.
var holdInput=first([extra.hold],booleanValue,null);
var holdBrake=beam?first([raw+"input_brake"],numeric,null):null;
var holdThrottle=beam?first([raw+"input_throttle"],numeric,null):null;
if(holdBrake===null){var commonBrake=first([prefix+"Brake"],numeric,null);holdBrake=commonBrake===null?null:commonBrake/100;}
if(holdThrottle===null){var commonThrottle=first([prefix+"Throttle"],numeric,null);holdThrottle=commonThrottle===null?null:commonThrottle/100;}
var holdGear=/^(?:[DSAM]?[1-9]|D|R|-1)$/.test(gear);
if(root.bmwHoldGame!==game){root.bmwHoldLatched=false;root.bmwHoldGame=game;}
var holdContext=running && key>=2 && (beam||truck) && holdGear && exactSpeed!==null && exactRpm!==null && exactRpm>0;
if(!holdContext || Math.abs(exactSpeed)>0.5 || holdThrottle===null || holdThrottle>0.05)root.bmwHoldLatched=false;
else if(simulateAutoHoldIndicator && holdBrake!==null && holdBrake>=0.10)root.bmwHoldLatched=true;
var autoHold=running&&key>=2?(holdInput!==null?holdInput:(simulateAutoHoldIndicator&&root.bmwHoldLatched?1:0)):0;
var fields=[rpm,speed,gear,clamp(first([prefix+"WaterTemperature"],numeric,0),-40,150),key,
    parking,low,fog,high,left,right,check,handbrake,cruise,root.bmwHeartbeat,
    3030,abs,tcState,turbo,setSpeed,tpms,oilWarning,
    clamp(oilTemp,-1000,200),pressure<0 ? -1 : clamp(pressure*1000,0,5000),
    extraFlag("seatbelt"),extraFlag("airbag"),autoHold,extraFlag("brakeWarning"),
    extraFlag("trunk"),extraFlag("hood"),sportMode,
    running ? clamp(extraNum("suspension",0),0,11) : 0,running ? clamp(extraNum("menu",0),0,4) : 0,
    running ? clamp(speedLimit,-1,500) : -1,
    running ? clamp(altitude,-32768,7160) : -32768,
    clamp(tpmsCode,-1,255),running ? clamp(psm,0,4) : 0,launch,clamp(brakeTemp,-1000,2000),brakeOverheat,heading<0 ? -1 : heading,outside===-1000 ? -10000 : clamp(outside*10,-1000,1500),batteryVoltage===null ? -1 : clamp(batteryVoltage*1000,0,40000),clock<0 ? -1 : clamp(clock,0,86399),extraFlag("doorFL"),extraFlag("doorFR"),extraFlag("doorRL"),extraFlag("doorRR"),fuelPercent<0 ? -1 : clamp(fuelPercent,0,100),engineOverheat,selectedMode,running&&escOff ? 1 : 0,running&&key>=2 ? 1 : 0,running&&key>=2 ? 1 : 0,running&&key>=2 ? 1 : 0,calendarDay,calendarYear,ecoNavCode,nextManeuverMeters<0?-1:clamp(nextManeuverMeters,0,2550),ringLevel];
if(diagnosticMode)return "profile="+fields[15]+"; fields="+fields.length+"; game="+game+"; marker="+companionVersion+"; low="+low+"; fog="+fog+"; high="+high+"; cruise="+cruise+"; targetKmh="+setSpeed+"; ESCoff="+escOff+"; launch="+launch+"; launchReason="+launchReason+"; gear="+gear+"; mode="+selectedMode+"; ESCenabled="+escEnabled+"; ESCsource="+root.bmwLaunchEscSource+"; CEL="+check+"; limitKmh="+root.bmwSpeedLimitKmh+"; avgL100km="+averageConsumption+"; mpgUS="+root.bmwConsumptionMpgUS+"; kmPerL="+root.bmwConsumptionKmPerL+"; fuelLph="+root.bmwFuelRateLph+"; recoveryKw="+root.bmwRecuperationKw+"; ecoNav="+ecoNavCode+"; nextManeuverM="+nextManeuverMeters+"; ringLevel="+ringLevel+"; ringMaxRpm="+ringMaxRpm+"; key="+key+"; ignitionLevel="+ignitionLevel;
return fields.join(";");

