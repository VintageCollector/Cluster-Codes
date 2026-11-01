// SimHub Arduino Custom Protocol, JavaScript enabled, Multiple USB mode.
// Do not add a newline or binary AA header. Ford Fusion F1 payload.
// Ford Fusion driving formula. Preserve reported Neutral; never infer Park from gearIndex.
// Optional: set diagnosticMode=true temporarily to inspect game, ESC and cruise telemetry.
// Diagnostic mode returns readable text instead of driving the cluster.
var diagnosticMode = false;
// Exact names from Insert property override game adapters, then common fallbacks.
var override = {key:"",parking:"",low:"",fog:"",high:"",left:"",right:"",
    checkEngine:"",handbrake:"",cruise:"",cruiseSetSpeedKmh:"",abs:"",
    tcActive:"",tcOff:"",vdcOff:"",turbo:"",gear:"",tpms:"",oilWarning:"",sport:"",rearFog:""};
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
var truck=game==="ets2" || game==="ats" || game.indexOf("eurotrucksimulator2")>=0 || game.indexOf("americantrucksimulator")>=0;
var beam=game.indexOf("beamng")>=0;
var raw="DataCorePlugin.GameRawData.";
// Installed SimHub BeamNG Extra exports a flat struct, not electrics.values.
var beamFlat={high:["light_HighBeam"],low:["light_LowBeam","light_LowHighBeam"],
    fog:["light_Fog"],left:["light_Signal_L"],right:["light_Signal_R"],
    handbrake:["input_parkingBrake","light_Parkingbrake"],abs:["light_Abs"],tcActive:["light_TC"]};
// Optional companion uses existing reserved fields, preserving the UDP layout.
var companionVersion=beam ? numeric(get(raw+"reserved_4")) : null;
var packedCompanion=companionVersion===209 || companionVersion===210;
var companion=companionVersion===203 || companionVersion===207 || packedCompanion;
// v209 packs activity + cruise + target tenths km/h into the existing reserved_3.
var packedStatus=packedCompanion ? numeric(get(raw+"reserved_3")) : null;
if(packedStatus!==null && (packedStatus<0 || packedStatus>40810 || packedStatus!==Math.floor(packedStatus))) packedStatus=null;
var companionEsc=packedStatus===null ? null : packedStatus%4;
var companionCruise=packedStatus===null ? null : Math.floor(packedStatus/4)%4;
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
var left=lamp("left","Lights.BlinkerLeftOn","signal_L",prefix+"TurnIndicatorLeft","DL_SIGNAL_L");
var right=lamp("right","Lights.BlinkerRightOn","signal_R",prefix+"TurnIndicatorRight","DL_SIGNAL_R");
var handbrake=lamp("handbrake","","parkingbrake",prefix+"Handbrake","DL_HANDBRAKE");
var abs=lamp("abs","","absActive",prefix+"ABSActive","DL_ABS");
var tc=lamp("tcActive","","tcsActive",prefix+"TCActive","DL_TC");
if(companion && !override.abs && numeric(get(raw+"reserved_2"))>=0)
    abs=running ? first([raw+"reserved_2"],booleanValue,0) : 0;
if(companion && !packedCompanion && !override.tcActive && numeric(get(raw+"reserved_3"))>=0)
    tc=running ? first([raw+"reserved_3"],booleanValue,0) : 0;
if(packedCompanion && !override.tcActive && (companionEsc===1 || companionEsc===2))
    tc=running && companionEsc===2 ? 1 : 0;
// Older GameData aliases are tried only when the standardized property is absent.
if(!beam && !override.handbrake && get(prefix+"Handbrake")===null) handbrake=lamp("handbrake","","","GameData.Handbrake","DL_HANDBRAKE");
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
    var disabledSelector=numeric(get(raw+"reserved_1"));
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
if(truck && !override.checkEngine) check=check || first([raw+"Lights.EngineOilPressureWarning"],booleanValue,0);
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
    return /^(P|N|R|D|S|0|-1|[1-9]|1[0-9]|2[0-4]|[MA][1-8])$/.test(s) ? s : null;
}
var gearNames=[override.gear];
if(beam) gearNames.push(raw+"electrics.gear");
gearNames.push(prefix+"Gear");
var gear=first(gearNames,gearToken,"N");
// BeamNG can report N for Park. Keep that N rather than invent a selector.
// Only an actual textual P or explicit gear override is allowed to request P.
// Preserve only known legacy gear tokens; prevent delimiter/newline injection.
if(!/^(P|N|R|D|S|0|-1|[1-9]|1[0-9]|2[0-4]|[MA][1-8])$/.test(gear)) gear="N";
if(!running) gear="N";
// User-confirmed TPMS warning mapping. Flat-tyre status, not guessed PSI.
var tpms=0;
if(running) {
    var explicitTpms=first([override.tpms],booleanValue,null);
    if(explicitTpms!==null) tpms=explicitTpms;
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
// Ford Fusion payload. SimHub supplies the final newline, like the Nissan sketch.
var fordFlags=[];
function flag(on,name) { if(on) fordFlags.push(name); }
flag(running && key>=2,'DL_IGNITION');
flag(parking,'DL_SIDELIGHTS'); flag(low,'DL_LOWBEAM'); flag(high,'DL_FULLBEAM');
flag(fog,'DL_FOG'); flag(left,'DL_LEFT'); flag(right,'DL_RIGHT');
flag(handbrake,'DL_HANDBRAKE'); flag(abs,'DL_ABS');
flag(tcState===1,'DL_ESC'); flag(tcState===2 || tcState===3,'DL_ESC_OFF');
flag(cruise>0,'DL_CRUISE'); flag(cruise>=2,'DL_CRUISE_ACTIVE');
// Bind override.sport to an explicit boolean property for non-Sport gear modes.
var sport=running && (first([override.sport],booleanValue,0) || gear==='S');
flag(sport,'DM_SPORT');
flag(lamp('rearFog','Lights.FogRear','fogRear','',''),'DL_REARFOG');
if(gear==='0') gear='N'; if(gear==='-1') gear='R';
if(/^[MA][1-8]$/.test(gear)) gear=gear.substring(1);
return ['F1',rpm,speed*10,clamp(first([prefix+'WaterTemperature'],numeric,20),-40,150),gear,fordFlags.join(','),'END'].join(';');
