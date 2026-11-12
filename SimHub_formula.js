// Paste into the JavaScript mode of the Arduino CUSTOM PROTOCOL formula.
// These common property paths must be checked with your SimHub property picker.
// Unsupported game properties can be missing. Keep existing paths if they work.
function value(name, fallback) {
    var v = $prop('DataCorePlugin.GameData.NewData.' + name);
    return v === null || v === undefined ? fallback : v;
}
function num(name, fallback) {
    var n = Number(value(name, fallback));
    return isFinite(n) ? n : fallback;
}
function bit(name) {
    var v = value(name, 0);
    return v === true || v === 1 || v === '1' || v === 'True' || v === 'true' ? 1 : 0;
}
var rpm = Math.max(0, Math.min(20000, Math.round(num('Rpms', 0))));
var speed = Math.max(0, Math.min(500, num('SpeedKmh', 0)));
// Some games briefly clear the ignition property while reporting running RPM.
var ignition = rpm > 0 ? 1 : bit('EngineIgnitionOn');
var left = bit('TurnIndicatorLeft');
var right = bit('TurnIndicatorRight');
var gear = String(value('Gear', 'N')).replace(/[;\r\n]/g, '');
var lights = String(value('DashLights', '')).replace(/[;\r\n]/g, ',');
// OutGauge DashLights can list available lamps. Read live states from ShowLights.
var liveOnlyFlags = ['DL_ABS', 'DL_TC', 'DL_ESC', 'DL_ESC_OFF', 'DL_DSC_OFF', 'DL_DCS_OFF',
    'DL_BAS', 'DL_CHECKENGINE', 'DL_CHECK_ENGINE', 'DL_LOWOIL', 'DL_OILWARN',
    'DL_FULLBEAM', 'DL_LOWBEAM', 'DL_HEADLIGHT'];
lights = lights.split(/[^A-Za-z0-9_]+/).filter(function (token) {
    return token && liveOnlyFlags.indexOf(token.toUpperCase()) < 0;
}).join(',');
var showLights = $prop('DataCorePlugin.GameRawData.ShowLights');
if (showLights === null || showLights === undefined) showLights = value('ShowLights', '');
var showTokens = String(showLights).toUpperCase().split(/[^A-Z0-9_]+/);
var showMask = Number(showLights) || 0;
function shown(token, mask) {
    return showTokens.indexOf(token) >= 0 || (showMask & mask) !== 0;
}
if (shown('DL_ABS', 0x0400) || bit('ABSActive')) lights += ',DL_ABS';
// The standard OutGauge TC lamp can also indicate that TC is switched off.
// BeamNG's SimHub extras protocol exports intervention as light_TC.
function beamNgLamp(name) {
    var state = $prop('DataCorePlugin.GameRawData.' + name);
    return state === true || state === 'True' || state === 'true' || Number(state) === 1;
}
if (bit('TCActive') || beamNgLamp('light_TC'))
    lights += ',DL_TC';
if (shown('DL_BAS', 0)) lights += ',DL_BAS';
if (shown('DL_CHECK_ENGINE', 0x2000) || shown('DL_CHECKENGINE', 0)) lights += ',DL_CHECKENGINE';
if (shown('DL_OILWARN', 0x0100) || shown('DL_LOWOIL', 0) || beamNgLamp('light_Oil'))
    lights += ',DL_LOWOIL';
if (shown('DL_FULLBEAM', 0x0002) || beamNgLamp('light_HighBeam')) lights += ',DL_FULLBEAM';
if (shown('DL_LOWBEAM', 0) || shown('DL_HEADLIGHT', 0x1000) ||
    beamNgLamp('light_LowBeam') || beamNgLamp('light_LowHighBeam'))
    lights += ',DL_LOWBEAM';
// The installed BeamNG extras v982 packs ESC disabled into reserved_3 bit 17.
// reserved_4's low 10 bits identify the layout even when time/mode data is packed.
var extrasLayout = Number($prop('DataCorePlugin.GameRawData.reserved_4'));
var extrasStatus = Number($prop('DataCorePlugin.GameRawData.reserved_3'));
var beamNgEscOff = (extrasLayout & 0x3ff) === 982 && (extrasStatus & 0x20000) !== 0;
// Optional full property path for a game that reports ESC enabled as a boolean.
var ESC_ENABLED_PROPERTY = '';
var escEnabled = ESC_ENABLED_PROPERTY ? $prop(ESC_ENABLED_PROPERTY) : null;
if (escEnabled === false || escEnabled === 0 || escEnabled === '0' || /^false$/i.test(String(escEnabled)))
    lights += ',DL_ESC_OFF';
if (beamNgEscOff || shown('DL_ESC_OFF', 0) || shown('DL_DSC_OFF', 0) || shown('DL_DCS_OFF', 0))
    lights += ',DL_ESC_OFF';
// Optional additional game properties: paste FULL verified paths below.
// Empty entries leave the existing DashLights flags in control. Never assume
// all games expose ABS faults, SRS faults or check-engine states.
var EXTRA_LAMP_PROPERTIES = {
    DL_FULLBEAM: '', DL_LOWBEAM: '', DL_BAS: '',
    DL_SRS: '', DL_CHECKENGINE: '', DL_LOWOIL: '', DL_ABS: '', DL_LOWCOOLANT: '',
    DL_HANDBRAKE: '', DL_TC: '', DL_ESC_OFF: '' // Optional live intervention and disabled-state properties.
};
Object.keys(EXTRA_LAMP_PROPERTIES).forEach(function (token) {
    var path = EXTRA_LAMP_PROPERTIES[token];
    if (!path) return;
    var state = $prop(path);
    var numericState = Number(state);
    // Supports boolean warnings and positive pedal/handbrake percentages.
    if (state === true || state === 'True' || state === 'true' ||
        (state !== null && state !== undefined && state !== '' && isFinite(numericState) && numericState > 0))
        lights += ',' + token;
});
// Common handbrake property; change this name if your game's property differs.
// Also accepts existing DL_HANDBRAKE in DashLights or the explicit mapping above.
if (num('Handbrake', 0) > 0) lights += ',DL_HANDBRAKE';
var temp = Math.max(-40, Math.min(180, num('WaterTemperature', 80)));
// Expected scale is 0..100. If your property is 0..1, multiply by 100 here.
var throttle = Math.max(0, Math.min(100, num('Throttle', 0)));
// Paste a VERIFIED instantaneous fuel-flow property's full path here.
// Leave blank when unavailable; fuel remaining / fuel per lap are not fuel flow.
var FUEL_FLOW_PROPERTY = '';
var FUEL_FLOW_UNIT = 'L/h'; // 'L/h', 'L/s', 'mL/s', or 'L/100km'
var flow = -1;
if (FUEL_FLOW_PROPERTY) {
    var rawFlow = $prop(FUEL_FLOW_PROPERTY);
    if (rawFlow !== null && rawFlow !== undefined && rawFlow !== '') {
        var measured = Number(rawFlow);
        if (isFinite(measured) && measured >= 0) {
            if (FUEL_FLOW_UNIT === 'L/h') flow = measured;
            else if (FUEL_FLOW_UNIT === 'L/s') flow = measured * 3600;
            else if (FUEL_FLOW_UNIT === 'mL/s') flow = measured * 3.6;
            else if (FUEL_FLOW_UNIT === 'L/100km' && speed >= 0.5)
                flow = measured * speed / 100;
        }
    }
}
if (flow > 1000) flow = -1; // Reject implausible units/scales, use throttle.
// A changing field ensures the formula differs even at steady speed or idle.
var heartbeat = Math.floor(Date.now() / 250) % 1000000;
return [rpm, speed.toFixed(2), ignition, left, right, gear, lights,
        temp.toFixed(1), throttle.toFixed(1), heartbeat, flow.toFixed(4)].join(';') + '\n';
