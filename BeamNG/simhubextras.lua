-- Simhub telemetry mod, Wotever 2023
local M = {}
local function getAddress()        return settings.getValue("protocols_simhubextras_address") or "127.0.0.1" end        -- return "127.0.0.1"
local function getPort()           return settings.getValue("protocols_simhubextras_port") or 9999 end           -- return 4567
local function getMaxUpdateRate()  return settings.getValue("protocols_simhubextras_maxUpdateRate") or 100 end  -- return 60

local function init()
	log("I", "", "SimHub extras initialized (v0.1, port :" .. tostring(getPort()) .. " address :" .. getAddress() ..")")
    accelerationSmoothingX = settings.getValue("motionSimAccelerationSmoothingX") or 30
    accelerationSmoothingY = settings.getValue("motionSimAccelerationSmoothingY") or 30
    accelerationSmoothingZ = settings.getValue("motionSimAccelerationSmoothingZ") or 30
    accXSmoother = newExponentialSmoothing(accelerationSmoothingX)
    accYSmoother = newExponentialSmoothing(accelerationSmoothingY)
    accZSmoother = newExponentialSmoothing(accelerationSmoothingZ)
end


local function isPhysicsStepUsed()
  --return false-- use graphics step. performance cost is ok. the update rate could reach UP TO min(getMaxUpdateRate(), graphicsFramerate)
  return true   -- use physics step. performance cost is big. the update rate could reach UP TO min(getMaxUpdateRate(), 2000 Hz)
end

local lastSimHubFrameData = nil
local abs = math.abs
local ffi = require("ffi")
local moTimer = (HighPerfTimer or hptimer)()

local timeStamp = 0
local clockRequestNext = -1
-- BMW DOOR HELPER BEGIN
local function readBmwDoors(controller)
    local mask, known = 0, false
    local bits = {doorflcoupler=1, doorlcoupler=1, doorfrcoupler=2, doorrcoupler=2,
        doorrlcoupler=4, doorrrcoupler=8, hoodlatchcoupler=16, hoodcoupler=16,
        trunkcoupler=32, trunklatchcoupler=32, bootcoupler=32, tailgatecoupler=32}
    local openStates = {detached=true, desyncedDetached=true, broken=true, coupling=true, autoCoupling=true}
    pcall(function()
        if not controller or not controller.getControllersByType then return end
        for key, item in pairs(controller.getControllersByType("advancedCouplerControl") or {}) do
            local name = tostring(item.name or (type(key)=="string" and key) or ""):lower():gsub("[_%s%-]", "")
            local weight = bits[name]
            if weight and type(item.getGroupState)=="function" then
                local ok, state = pcall(item.getGroupState)
                if ok and (openStates[state] or state=="attached" or state=="desyncedAttached") then
                    known = true
                    if openStates[state] and math.floor(mask/weight)%2==0 then mask=mask+weight end
                end
            end
        end
    end)
    return mask, known
end
-- BMW DOOR HELPER END

-- ROAD LIMIT HELPER BEGIN: nearest drivable road metadata, sampled at 4 Hz.
local roadLimitNext = -1
local roadLimitTenths = -1
local function readRoadLimit(now)
    if now < roadLimitNext then return roadLimitTenths end
    roadLimitNext = now + 0.25
    roadLimitTenths = -1
    pcall(function()
        if not mapmgr then return end
        if not mapmgr.mapData then mapmgr.requestMap(); return end
        local a, b, distance = mapmgr.findBestRoad(obj:getPosition(), obj:getDirectionVector())
        local data = mapmgr.mapData
        local edge = a and b and data.graph[a] and data.graph[a][b]
        local radius = a and b and data.radius and math.max(data.radius[a] or 0, data.radius[b] or 0) or 0
        local limit = edge and tonumber(edge.speedLimit)
        -- Do not carry a nearby road's sign far off-road or invent missing map metadata.
        if distance and distance <= radius + 2 and limit and limit > 0 and limit < math.huge and limit * 3.6 <= 500 then
            roadLimitTenths = math.floor(limit * 36 + 0.5)
        end
    end)
    return roadLimitTenths
end
-- ROAD LIMIT HELPER END

local accX
local accY
local accZ
local accelerationSmoothingX = nil
local accelerationSmoothingY = nil
local accelerationSmoothingZ = nil
local accXSmoother = nil
local accYSmoother = nil
local accZSmoother = nil

local function ternary(cond, T, F, ...)
    if cond then
        return T(...)
    else
        return F(...)
    end
end

local function sternary(cond, T, F)
    if cond then
        return T
    else
        return F
    end
end

local function getStructDefinition()
  return [[
     
     // Just a magic string allowing to throw away invalid packets
      char magic[8];
      float timeStamp;

      char vehicleName[256];
      char vehicleModel[256];
      char vehicleConfig[256];

      float idle_rpm;
      float max_rpm;
      int gear;
      int max_gears;      
      int gearboxGrinding;      

      float speed;           // M/S
      float rpm;             // RPM
      float turbo;           // BAR
      float turboMax;           // BAR
      float engTemp;         // C
      float fuel;            // 0 to 1
      float oilTemp;         // C
      float waterTemp;
      float fuelCapacity;
      float fuelVolume;
      float engineLoad;
      int ignitionOn;

      // lights
      int light_HighBeam;
      int light_Parkingbrake;
      int light_Signal_L;
      int light_Signal_R;
      int light_Abs;
      int light_Oil;
      int light_EngineRunning;
      int light_TC;
      int light_Shift;
      int light_HazardEnabled;

      int light_LowHighBeam;
      int light_LowBeam;
      int light_Fog;

      // Inputs
      float input_throttle;     
      float input_brake;        
      float input_clutch;       
      float input_parkingBrake;    
      float input_steeringPercent;

      // Motion
      //World position of the car
      float          posX;
      float          posY;
      float          posZ;

      //Velocity of the car
      float          velX;
      float          velY;
      float          velZ;

      //Acceleration of the car, gravity not included
      float          accX;
      float          accY;
      float          accZ;

      //Vector components of a vector pointing "up" relative to the car
      float          upVecX;
      float          upVecY;
      float          upVecZ;

      //Vector components of a vector pointing "forward" relative to the car
      float          forwardVecX;
      float          forwardVecY;
      float          forwardVecZ;

      //Roll, pitch and yaw positions of the car
      float          rollPos;
      float          pitchPos;
      float          yawPos;

      //Roll, pitch and yaw "velocities" of the car
      float          rollRate;
      float          pitchRate;
      float          yawRate;

      //Roll, pitch and yaw "accelerations" of the car
      float          rollAcc;
      float          pitchAcc;
      float          yawAcc;

      // Suspensions
      float suspension_position_fl;
      float suspension_position_fr;
      float suspension_position_rl;
      float suspension_position_rr;

      float suspension_velocity_fl;
      float suspension_velocity_fr;
      float suspension_velocity_rl;
      float suspension_velocity_rr;

      float suspension_acceleration_fl;
      float suspension_acceleration_fr;
      float suspension_acceleration_rl;
      float suspension_acceleration_rr;

      float wheel_slip_fl;       
      float wheel_slip_fr;     
      float wheel_slip_rl;        
      float wheel_slip_rr;       
    
      float wheel_RPS_fl;
      float wheel_RPS_fr;
      float wheel_RPS_rl;
      float wheel_RPS_rr;

      float wheel_speed_fl;
      float wheel_speed_fr;
      float wheel_speed_rl;
      float wheel_speed_rr;

      int wheel_isRubberTire_fl;
      int wheel_isRubberTire_fr;
      int wheel_isRubberTire_rl;
      int wheel_isRubberTire_rr;

      int wheel_contactMaterial_fl;
      int wheel_contactMaterial_fr;
      int wheel_contactMaterial_rl;
      int wheel_contactMaterial_rr;
	  
      int wheel_isDeflated_fl;
      int wheel_isDeflated_fr;
      int wheel_isDeflated_rl;
      int wheel_isDeflated_rr;

      float wheel_contactDepth_fl;
      float wheel_contactDepth_fr;
      float wheel_contactDepth_rl;
      float wheel_contactDepth_rr;

      float brake_surfaceTemperature_fl;
      float brake_surfaceTemperature_fr;
      float brake_surfaceTemperature_rl;
      float brake_surfaceTemperature_rr;

      float brake_coreTemperature_fl;
      float brake_coreTemperature_fr;
      float brake_coreTemperature_rl;
      float brake_coreTemperature_rr;
	  
	  int check;

      float reserved_1;
      float reserved_2;
      float reserved_3;
      float reserved_4;
 
  ]]
end


local function reset()
    log("I", "", "SimHub extras reset")
    lastSimHubFrameData = {
        suspension_position_fl = 0,
        suspension_position_fr = 0,
        suspension_position_rl = 0,
        suspension_position_rr = 0,
        suspension_velocity_fl = 0,
        suspension_velocity_fr = 0,
        suspension_velocity_rl = 0,
        suspension_velocity_rr = 0,
        rollRate = 0,
        pitchRate = 0,
        yawRate = 0
    }
	
	if accXSmoother ~= nil then
		accXSmoother:reset()
	end
	if accXSmoother ~= nil then
		accYSmoother:reset()
	end
	if accXSmoother ~= nil then
		accZSmoother:reset()
	end
end

local function fitWheelName(wheelName)
    if wheelName == 'RL1' then
        return 'RL'
    end
    if wheelName == 'RR1' then
        return 'RR'
    end
    return wheelName;
end

local function toSuspensionPosition(invQuat, wheel)
    if wheel ~= nil then
        local wheelPos = vec3(0, 0, 0)
        wheelPos = invQuat * vec3(obj:getNodePosition(wheel.node1))
        return wheelPos.z * 10
    end
    return 0
end

local function getLastSlip(wheelRotator)
    if (wheelRotator ~= nil) then
        return wheelRotator.lastSlip
    end
    return 0
end

local function getIsDeflated(wheelRotator)
    if (wheelRotator ~= nil) then
        return wheelRotator.isTireDeflated
    end
    return 0
end

local function getWheelSpeed(wheel)
    if(wheel~= nil) then
        return abs(wheel.angularVelocity * wheel.radius)
    end
    return 0
end

local function getWheelRPS(wheel)
    if(wheel~= nil) then
        return abs(wheel.angularVelocity / 6.28318530718)
    end
    return 0
end

local function getWheelsByName()
    local wheelsByName = {}
    for _, wheel in pairs(wheels.wheels) do
        wheelsByName[fitWheelName(wheel.name)] = wheel
    end
    return wheelsByName
end

local function getWheelsRotatorByName()
    local wheelRotatorsByName = {}
    for i = 0, wheels.wheelRotatorCount - 1 do
        local wheel = wheels.wheelRotators[i]
        wheelRotatorsByName[fitWheelName(wheel.name)] = wheel
    end
    return wheelRotatorsByName
end

local function getContactMaterial(contactData)
    if (contactData ~= nil) then
        return contactData.contactMaterial
    end
    return 0
end

local function getContactDepth(contactData)
    if (contactData ~= nil) then
        return contactData.contactDepth
    end
    return -1
end

local function getIsRubberTire(contactData)
    if (contactData ~= nil) then
        if (contactData.isRubberTire) then
            return 1
        end
    end
    return 0
end

local function getWheelsContactDataByName()
    local wheelsContactByName = {}
    for wi, wd in pairs(wheels.wheels) do
        -- log("I", "",wd.name)
        wheelsContactByName[fitWheelName(wd.name)] = {}
        local mat, mat2 = wd.contactMaterialID1, wd.contactMaterialID2
        if mat == 4 then
            mat, mat2 = mat2, mat
        end
        local isRubberTire = mat2 == 4 and wd.hasTire -- if the tire is rubber
        wheelsContactByName[fitWheelName(wd.name)] = {
            isRubberTire = isRubberTire,
            contactMaterial = mat,
            contactDepth = wd.contactDepth
        }
    end
    return wheelsContactByName
end

local function isVehicleReady()
    if not electrics.values.watertemp then
        -- vehicle not completly initialized, skip sending package
        return false
    end
    return true
end

local function getThermal(wheelName, valueName)
    local wheel = electrics.values.wheelThermals[wheelName]
    if (wheel ~= nil) then
        return wheel[valueName] or -1
    end
    return -1
end

local printed = false
local function printOnce(val)
  if not printed then
    print(val)
  end
  printed = true
end


local function fillStruct(o, dtSim)

  local dt = moTimer:stopAndReset() / 1000
  timeStamp = timeStamp + dt

  accelerationSmoothingX = settings.getValue("motionSimAccelerationSmoothingX") or 30
  accelerationSmoothingY = settings.getValue("motionSimAccelerationSmoothingY") or 30
  accelerationSmoothingZ = settings.getValue("motionSimAccelerationSmoothingZ") or 30
  accXSmoother = accXSmoother or newExponentialSmoothing(accelerationSmoothingX)
  accYSmoother =accYSmoother or newExponentialSmoothing(accelerationSmoothingY)
  accZSmoother =accZSmoother or newExponentialSmoothing(accelerationSmoothingZ)
  lastSimHubFrameData = lastSimHubFrameData or {
    suspension_position_fl = 0,
    suspension_position_fr = 0,
    suspension_position_rl = 0,
    suspension_position_rr = 0,
    suspension_velocity_fl = 0,
    suspension_velocity_fr = 0,
    suspension_velocity_rl = 0,
    suspension_velocity_rr = 0,
    rollRate = 0,
    pitchRate = 0,
    yawRate = 0
}

    if not playerInfo.firstPlayerSeated then
        return
    end

    local accXRaw = -obj:getSensorX()
    local accYRaw = -obj:getSensorY()
    local accZRaw = -obj:getSensorZ()
    accX = accelerationSmoothingX > 0 and accXSmoother:get(accXRaw) or accXRaw
    accY = accelerationSmoothingY > 0 and accYSmoother:get(accYRaw) or accYRaw
    accZ = accelerationSmoothingZ > 0 and accZSmoother:get(accZRaw) or accZRaw

    if not isVehicleReady() then
        return
    end

    --printOnce(jsonEncode(v.data))

    o.magic = "magic"
    o.timeStamp = timeStamp
	  o.check = 128
    
    o.vehicleName = v.data.information.name or ""
    o.vehicleModel = v.config.model or ""
    o.vehicleConfig = v.config.partConfigFilename or ""

    -- Engine data
    local engine = powertrain.getDevice("mainEngine")
    if engine ~= nil then
        o.idle_rpm = engine.idleRPM or 0
        o.max_rpm = engine.maxRPM or 0
    end

    -- Gearbox data
    local gearbox = powertrain.getDevice("gearbox")
    if gearbox ~= nil then
        o.max_gears = gearbox and gearbox.maxGearIndex or 0
        o.gearboxGrinding =  gearbox and gearbox.isGrindingShift or 0
    end

    -- Lights data
    o.light_HighBeam = electrics.values.highbeam or 0
    o.light_Parkingbrake = electrics.values.parkingbrake or 0
    o.light_Signal_L = electrics.values.signal_L or 0
    o.light_Signal_R = electrics.values.signal_R or 0
    
    local hasABS = electrics.values.hasABS or false
    if hasABS then
        o.light_Abs = electrics.values.abs or 0
    end

    o.light_Oil = electrics.values.oil or 0
    o.light_EngineRunning = electrics.values.engineRunning or 0
    
    -- light_TC is populated below from both direct activity flags.
    o.light_Shift = sternary(electrics.values.shouldShift, 1, 0)
    o.light_HazardEnabled = electrics.values.hazard_enabled or 0

    

    o.light_LowHighBeam = electrics.values.lowhighbeam or 0;
    o.light_LowBeam = electrics.values.lowbeam or 0;
    o.light_Fog = electrics.values.fog or 0;

    -- Engine
    -- Panamera companion v971. Struct layout and UDP size stay unchanged.
    local function activeFlag(value)
        if value == nil then return -1 end
        if value == true then return 1 end
        if type(value) == "number" then return value > 0 and 1 or 0 end
        return 0
    end
    -- Current installed DSE supervisors expose getConfig().isEnabled and isActing.
    -- Resolve by type as controller instance names may be vehicle-specific.
    local function supervisorState(typeName)
        local enabled, acting = nil, nil
        local ok = pcall(function()
            local list = controller.getControllersByType(typeName) or {}
            for _, c in pairs(list) do
                local config = c.getConfig and c.getConfig()
                if type(config) == "table" and config.isEnabled ~= nil then
                    local e = activeFlag(config.isEnabled) > 0
                    if enabled == nil then enabled = e else enabled = enabled and e end
                end
                if c.isActing ~= nil then
                    acting = (acting == true) or activeFlag(c.isActing) > 0
                end
            end
        end)
        if not ok then return nil, nil end
        if enabled == false then acting = false end
        return enabled, acting
    end
    local yawEnabled, yawActing = supervisorState("drivingDynamics/supervisors/yawControl")
    local tcEnabled, tcActing = supervisorState("drivingDynamics/supervisors/tractionControl")
    -- Legacy ESC combines ESC and TC under the same configuration switch.
    if yawEnabled == nil and tcEnabled == nil then
        pcall(function()
            local legacy = controller.getController("esc")
            if legacy and legacy.getCurrentConfigData then
                local config = legacy.getCurrentConfigData()
                if config and config.escEnabled ~= nil then
                    yawEnabled = activeFlag(config.escEnabled) > 0
                    tcEnabled = yawEnabled
                end
            end
        end)
    end
    local disabled = -1
    if tcEnabled == false then disabled = 2
    elseif yawEnabled == false then disabled = 3
    elseif tcEnabled == true or yawEnabled == true then disabled = 0 end
    o.reserved_1 = disabled
    o.reserved_2 = activeFlag(electrics.values.absActive)
    local escActive = yawActing == nil and activeFlag(electrics.values.escActive) or (yawActing and 1 or 0)
    local tcsActive = tcActing == nil and activeFlag(electrics.values.tcsActive) or (tcActing and 1 or 0)
    if yawEnabled == false then escActive = 0 end
    if tcEnabled == false then tcsActive = 0 end
    o.reserved_3 = (escActive > 0 or tcsActive > 0) and 1 or
        ((escActive < 0 and tcsActive < 0) and -1 or 0)
    o.light_TC = o.reserved_3 > 0 and 1 or 0
    -- Read the existing cruise extension without enabling or changing it.
    -- Installed BeamNG cruiseControl.lua defines targetSpeed in m/s.
    local cruiseEnabled, cruiseTarget = nil, nil
    local ok, config = pcall(function()
        local cc = (extensions and extensions.cruiseControl) or cruiseControl
        if cc and cc.getConfiguration then return cc.getConfiguration() end
        return nil
    end)
    if ok and type(config) == "table" then
        cruiseEnabled = config.isEnabled
        cruiseTarget = config.targetSpeed
    end
    if cruiseEnabled == nil then cruiseEnabled = electrics.values.cruiseControlActive end
    if cruiseTarget == nil then cruiseTarget = electrics.values.cruiseControlTarget end
    local cruiseFlag = activeFlag(cruiseEnabled)
    local cruiseCode = cruiseFlag < 0 and 0 or (cruiseFlag > 0 and 2 or 1)
    local targetTenths = 0
    if type(cruiseTarget) == "number" and cruiseTarget == cruiseTarget
        and cruiseTarget >= 0 and cruiseTarget < math.huge then
        targetTenths = math.floor(math.min(cruiseTarget * 36, 2550) + 0.5)
    end
    local escCode = o.reserved_3 < 0 and 0 or (o.reserved_3 > 0 and 2 or 1)
    o.reserved_3 = targetTenths * 16 + cruiseCode * 4 + escCode
    -- Genuine BeamNG brake-overheat damage state, not an arbitrary temperature threshold.
    local anyBrakeOverheat = false
    pcall(function()
        if damageTracker and damageTracker.getDamage and wheels and wheels.wheels then
            for _, wheel in pairs(wheels.wheels) do
                local damage = damageTracker.getDamage("wheels", "brakeOverHeat" .. wheel.name)
                if activeFlag(damage) > 0 then anyBrakeOverheat = true end
            end
        end
    end)
    if anyBrakeOverheat then o.reserved_3 = o.reserved_3 + 65536 end
    -- Separate proof of ESC disabled: TC-disabled alone must not arm launch control.
    if yawEnabled == false then o.reserved_3 = o.reserved_3 + 131072 end
    -- Additive bit 18: real engine warning; existing status bits and UDP layout stay intact.
    local engineFault = activeFlag(electrics.values.checkengine) > 0
    pcall(function()
        if damageTracker and damageTracker.getDamage then
            engineFault=engineFault or activeFlag(damageTracker.getDamage("engine","engineDisabled"))>0
                or activeFlag(damageTracker.getDamage("engine","catastrophicOverrevDamage"))>0
        end
    end)
    if engineFault then o.reserved_3 = o.reserved_3 + 262144 end
    -- v972: reserved_2 low bit is ABS; higher bits are (limit tenths km/h + 1).
    -- Zero higher bits means unavailable. Integer range fits the existing float exactly.
    local roadLimit = readRoadLimit(timeStamp or 0)
    o.reserved_2 = math.max(0, o.reserved_2) + (roadLimit >= 0 and (roadLimit + 1) * 2 or 0)
    -- v973 environment packing: selector+1 (3 bits), temperature (12 bits), heading.
    local tempCode, headingCode = 0, 0
    pcall(function()
        local c = obj:getEnvTemperature() - 273.15
        if c == c and c >= -100 and c <= 150 then tempCode = math.floor(c * 10 + 1001.5) end
    end)
    pcall(function()
        local dir = obj:getDirectionVector()
        -- Same north axis as BeamNG navigationData: vecY=(0,-1,0).
        local h = math.deg(math.atan2(dir.x, -dir.y)) % 360
        headingCode = math.floor(h + 0.5) % 360 + 1
    end)
    o.reserved_1 = o.reserved_1 + 1 + tempCode * 8 + headingCode * 32768
    -- Request game time (GE environment API) without using Chrono commands or the PC clock.
    if (timeStamp or 0) >= (clockRequestNext or -1) then
        clockRequestNext = (timeStamp or 0) + 1
        pcall(function()
            obj:queueGameEngineLua(string.format([=[
                local minutes = -1
                pcall(function()
                    local tod = core_environment.getTimeOfDay()
                    local t
                    if core_environment.getTimeOfDayValueForNormalizedTime then
                        t = core_environment.getTimeOfDayValueForNormalizedTime(tod.time, tod)
                    elseif tod and type(tod.time) == "number" then
                        t = (tod.time + 0.5) %% 1
                    end
                    if type(t)=="number" and t==t and t>=0 and t<1 then minutes=math.floor(t*1440) %% 1440 end
                end)
                local vehicle=be:getObjectByID(%d)
                if vehicle then vehicle:queueLuaCommand(string.format("electrics.values.panameraClockMinutes=%%d; electrics.values.panameraClockAge=0",minutes)) end
            ]=],objectId))
        end)
    end
    electrics.values.panameraClockAge=(electrics.values.panameraClockAge or 10)+(dtSim or 0)
    local clockMinute=tonumber(electrics.values.panameraClockMinutes)
    local clockCode=0
    if clockMinute and clockMinute>=0 and clockMinute<1440 and electrics.values.panameraClockAge<5 then clockCode=math.floor(clockMinute)+1 end
    -- v975 mode codes: 0 unknown, 1 explicitly ordinary, 2 Sport, 3 Sport Plus.
    -- Read selected controller mode; ESC/gear position is never used to infer it.
    local driveModeCode = 0
    pcall(function()
        if not controller or not controller.getControllersByType then return end
        local selected = nil
        for _, driveController in pairs(controller.getControllersByType("driveModes") or {}) do
            if driveController.getCurrentDriveModeKey then
                local key = driveController.getCurrentDriveModeKey()
                if type(key)=="string" then
                    local name=key:lower():gsub("[%s_%-]", "")
                    local code=0
                    if (name=="sport" or name=="ttsport") then code=2
                    elseif name=="sportplus" or name=="sport+" or name=="ttsport+" then code=3
                    elseif name=="comfortplus" or name=="comfort+" then code=5
                    elseif name=="sportindividual" then code=6
                    elseif name=="adaptive" then code=7
                    elseif name=="eco" or name=="ecopro" then code=4
                    elseif name=="comfort" or name=="normal" or name=="standard" or name=="default" or name=="strada" then code=1 end
                    if selected and selected~=code then selected=0; break end
                    selected=code
                end
            end
        end
        driveModeCode=selected or 0
    end)
    o.reserved_4 = 980 + clockCode * 1024 + driveModeCode * 2097152
    -- Marker978: ignition level0..3 encoded as1..4 in bits19..21;0means unavailable.
    local ignitionLevel=electrics.values.ignitionLevel
    if type(ignitionLevel)=="number" and ignitionLevel>=0 and ignitionLevel<=3 and ignitionLevel==math.floor(ignitionLevel) then
        o.reserved_3=o.reserved_3+(ignitionLevel+1)*524288
    end
    -- Marker979 explicitly preserves ESC enabled independently of TC disabled.
    if yawEnabled==true then o.reserved_3=o.reserved_3+4194304 end
    -- v980: preserve ABS/road limit in low15bits; 6 panel bits plus validity above.
    local doorMask, doorKnown = readBmwDoors(controller)
    o.reserved_2 = o.reserved_2 + (doorMask + (doorKnown and 64 or 0)) * 32768
    -- CLOCK PACK END
    o.gear = (electrics.values.gearIndex or 0) + 1 -- reverse = 0 here
    o.speed = electrics.values.wheelspeed or electrics.values.airspeed or 0
    o.rpm = electrics.values.rpm or 0
    o.turbo = (electrics.values.boost or 0) / 14.504
    o.turboMax = (electrics.values.boostMax or 0) / 14.504

    o.engTemp = electrics.values.watertemp or 0
    o.fuel = electrics.values.fuel or 0
    o.fuelCapacity = electrics.values.fuelCapacity or 0
    o.fuelVolume = electrics.values.fuelVolume or 0
    o.oilTemp = electrics.values.oiltemp or 0
    o.waterTemp = electrics.values.watertemp or 0
    o.engineLoad = electrics.values.engineLoad or 0
    o.ignitionOn = electrics.values.ignition or 0

    -- Input data
    o.input_throttle = electrics.values.throttle or 0
    o.input_brake = electrics.values.brake or 0
    o.input_clutch = electrics.values.clutch or 0
    o.input_parkingBrake = electrics.values.parkingbrake or 0
    o.input_steeringPercent = input.steering or 0

    -- Motion data
    o.posX, o.posY, o.posZ = obj:getPositionXYZ()

    local velocity = obj:getVelocity()
    o.velX = velocity.x
    o.velY = velocity.y
    o.velZ = velocity.z

    o.accX = accX
    o.accY = accY
    o.accZ = accZ

    local upVector = obj:getDirectionVectorUp()
    local vectorForward = obj:getDirectionVector()

    local quat = quatFromDir(vectorForward, upVector)
    local euler = quat:toEulerYXZ()

    o.upVecX = upVector.x
    o.upVecY = upVector.y
    o.upVecZ = upVector.z

    o.forwardVecX = vectorForward.x
    o.forwardVecY = vectorForward.y
    o.forwardVecZ = vectorForward.z

    local rollRate = obj:getRollAngularVelocity()
    local pitchRate = obj:getPitchAngularVelocity()
    local yawRate = obj:getYawAngularVelocity()

    o.rollPos = -euler.z --negated angle here, seems like that is the "standard" for motion sims here
    o.pitchPos = -euler.y --negated angle here, seems like that is the "standard" for motion sims here
    o.yawPos = euler.x

    o.rollRate = rollRate
    o.pitchRate = pitchRate
    o.yawRate = yawRate

    o.rollAcc = (rollRate - lastSimHubFrameData.rollRate) / dt
    o.pitchAcc = (pitchRate - lastSimHubFrameData.pitchRate) / dt
    o.yawAcc = (yawRate - lastSimHubFrameData.yawRate) / dt

    -- Wheels data
    local upVector = obj:getDirectionVectorUp() 
    local vectorForward = obj:getDirectionVector()
    local quat = quatFromDir(vectorForward, upVector)
    local euler = quat:toEulerYXZ()
    local invQuat = quat:inversed()
    local wheelPos = vec3(0, 0, 0)

    -- Extract rotators by name
    local wheelRotatorsByName = getWheelsRotatorByName()

    -- Extract wheels by name
    local wheelsByName = getWheelsByName()

    -- Extract wheels contact infos by name
    local wheelsContactByName = getWheelsContactDataByName()

    o.suspension_position_fl = toSuspensionPosition(invQuat, wheelsByName["FL"])
    o.suspension_position_fr = toSuspensionPosition(invQuat, wheelsByName["FR"])
    o.suspension_position_rl = toSuspensionPosition(invQuat, wheelsByName["RL"])
    o.suspension_position_rr = toSuspensionPosition(invQuat, wheelsByName["RR"])

    o.suspension_velocity_fl = ((o.suspension_position_fl - lastSimHubFrameData.suspension_position_fl) / dt)
    o.suspension_velocity_fr = ((o.suspension_position_fr - lastSimHubFrameData.suspension_position_fr) / dt)
    o.suspension_velocity_rl = ((o.suspension_position_rl - lastSimHubFrameData.suspension_position_rl) / dt)
    o.suspension_velocity_rr = ((o.suspension_position_rr - lastSimHubFrameData.suspension_position_rr) / dt)

    o.suspension_acceleration_fl = (o.suspension_velocity_fl - lastSimHubFrameData.suspension_velocity_fl) / dt
    o.suspension_acceleration_fr = (o.suspension_velocity_fr - lastSimHubFrameData.suspension_velocity_fr) / dt
    o.suspension_acceleration_rl = (o.suspension_velocity_rl - lastSimHubFrameData.suspension_velocity_rl) / dt
    o.suspension_acceleration_rr = (o.suspension_velocity_rr - lastSimHubFrameData.suspension_velocity_rr) / dt

    o.wheel_speed_fl = getWheelSpeed(wheelsByName["FL"])
    o.wheel_speed_fr = getWheelSpeed(wheelsByName["FR"])
    o.wheel_speed_rl = getWheelSpeed(wheelsByName["RL"])
    o.wheel_speed_rr = getWheelSpeed(wheelsByName["RR"])

    o.wheel_RPS_fl = getWheelRPS(wheelsByName["FL"])
    o.wheel_RPS_fr = getWheelRPS(wheelsByName["FR"])
    o.wheel_RPS_rl = getWheelRPS(wheelsByName["RL"])
    o.wheel_RPS_rr = getWheelRPS(wheelsByName["RR"])

    o.wheel_slip_fl = getLastSlip(wheelRotatorsByName["FL"])
    o.wheel_slip_fr = getLastSlip(wheelRotatorsByName["FR"])
    o.wheel_slip_rl = getLastSlip(wheelRotatorsByName["RL"])
    o.wheel_slip_rr = getLastSlip(wheelRotatorsByName["RR"])

    o.wheel_contactMaterial_fl = getContactMaterial(wheelsContactByName["FL"])
    o.wheel_contactMaterial_fr = getContactMaterial(wheelsContactByName["FR"])
    o.wheel_contactMaterial_rl = getContactMaterial(wheelsContactByName["RL"])
    o.wheel_contactMaterial_rr = getContactMaterial(wheelsContactByName["RR"])

    o.wheel_contactDepth_fl = getContactDepth(wheelsContactByName["FL"])
    o.wheel_contactDepth_fr = getContactDepth(wheelsContactByName["FR"])
    o.wheel_contactDepth_rl = getContactDepth(wheelsContactByName["RL"])
    o.wheel_contactDepth_rr = getContactDepth(wheelsContactByName["RR"])

    o.wheel_isRubberTire_fl = getIsRubberTire(wheelsContactByName["FL"])
    o.wheel_isRubberTire_fr = getIsRubberTire(wheelsContactByName["FR"])
    o.wheel_isRubberTire_rl = getIsRubberTire(wheelsContactByName["RL"])
    o.wheel_isRubberTire_rr = getIsRubberTire(wheelsContactByName["RR"])

    o.wheel_isDeflated_fl = getIsDeflated(wheelRotatorsByName["FL"])
    o.wheel_isDeflated_fr = getIsDeflated(wheelRotatorsByName["FR"])
    o.wheel_isDeflated_rl = getIsDeflated(wheelRotatorsByName["RL"])
    o.wheel_isDeflated_rr = getIsDeflated(wheelRotatorsByName["RR"])

    o.brake_coreTemperature_fl = getThermal("FL", "brakeCoreTemperature")
    o.brake_coreTemperature_fr = getThermal("FR", "brakeCoreTemperature")
    o.brake_coreTemperature_rl = getThermal("RL", "brakeCoreTemperature")
    o.brake_coreTemperature_rr = getThermal("RR", "brakeCoreTemperature")

    o.brake_surfaceTemperature_fl = getThermal("FL", "brakeSurfaceTemperature")
    o.brake_surfaceTemperature_fr = getThermal("FR", "brakeSurfaceTemperature")
    o.brake_surfaceTemperature_rl = getThermal("RL", "brakeSurfaceTemperature")
    o.brake_surfaceTemperature_rr = getThermal("RR", "brakeSurfaceTemperature")

    -- Keep state for deltas
    lastSimHubFrameData.suspension_position_fl = o.suspension_position_fl
    lastSimHubFrameData.suspension_position_fr = o.suspension_position_fr
    lastSimHubFrameData.suspension_position_rl = o.suspension_position_rl
    lastSimHubFrameData.suspension_position_rr = o.suspension_position_rr
    lastSimHubFrameData.rollRate = rollRate
    lastSimHubFrameData.pitchRate = pitchRate
    lastSimHubFrameData.yawRate = yawRate

end
-- public interface
M.init = init
M.reset = reset
M.getAddress = getAddress
M.getPort = getPort
M.getMaxUpdateRate = getMaxUpdateRate
M.getStructDefinition = getStructDefinition
M.fillStruct = fillStruct
M.isPhysicsStepUsed = isPhysicsStepUsed

return M
