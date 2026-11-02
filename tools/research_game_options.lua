-- Private bounded diagnostic: real cabinet controls, read-only native memory.
-- All captures and native storage use the run_diagnostic.py isolated directory.
local root=assert(os.getenv('GUNBLADE_RESEARCH_DIR'))
local m=manager.machine
local memory=m.devices[':maincpu'].spaces.program
local ports=m.ioport.ports
local log=assert(io.open(root..'/observations.tsv','w'))
local function press(mask,on)
    for _,field in pairs(ports[':IN0'].fields) do
        if field.mask==mask then field:set_value(on and 1 or 0);return end
    end
    error('Missing real cabinet input')
end
local function pulse(t,start) return t>=start and t<start+.15 end
local profile=os.getenv('GUNBLADE_OPTIONS_PROFILE') or 'initial'
local events={}
local function event(time,mask,label) events[#events+1]={time=time,mask=mask,label=label} end
local cursor=18.5
if profile=='catalog' or profile=='commit' then
    -- Full cycles in catalog return every option to its original native value.
    -- Commit changes only the four selected player-facing options.
    local steps=profile=='catalog' and {2,3,8,8,7,2,2} or {1,0,2,1,2,0,0}
    for row,count in ipairs(steps) do
        event(cursor,0x08,'next row '..row);cursor=cursor+.5
        for i=1,count do event(cursor,0x04,'value '..row..' '..i);cursor=cursor+.5 end
    end
    event(cursor,0x08,'assignment Exit');cursor=cursor+.5
    event(cursor,0x04,'leave assignments');cursor=cursor+.5
    -- Native parent keeps selection 5. Five Service advances reach Exit (10).
    for i=1,5 do event(cursor,0x08,'parent next '..i);cursor=cursor+.5 end
    event(cursor,0x04,'native EEPROM commit');cursor=cursor+3
end
local previous=-1
local tick_previous=-1
gunblade_game_options_observer=emu.add_machine_frame_notifier(function()
    local t=m.time:as_double()
    -- Original menu begins at Exit. Six Service steps reach Game Assignments.
    local service=t>=10 and t<16 and t-math.floor(t)<.15
    local test=pulse(t,6) or pulse(t,17)
    for _,e in ipairs(events) do
        if pulse(t,e.time) then if e.mask==0x08 then service=true else test=true end end
    end
    press(0x08,service);press(0x04,test)
    local tick=math.floor(t*4)
    if tick~=tick_previous then
        tick_previous=tick
        log:write(string.format('%.6f\t%08x\t%u\t%u\t%u\t%u\t%u\t',t,m.devices[':maincpu'].state['ip'].value,memory:read_u32(0x532d54),memory:read_u32(0x532d48),memory:read_u32(0x54bdf8),memory:read_u32(0x54bda4),memory:read_u32(0x54bf20)))
        for address=0x54c0d0,0x54c12b do log:write(string.format('%02x',memory:read_u8(address))) end
        log:write('\n');log:flush()
        if t>=18 then
            for _,screen in pairs(m.screens) do screen:snapshot(root..string.format('/frame-q%03d.png',tick));break end
        end
    end
    local second=math.floor(t)
    if previous~=second then
        previous=second
        if second>=18 then
            for _,screen in pairs(m.screens) do screen:snapshot(root..string.format('/frame-%03d.png',second));break end
            local out=assert(io.open(root..string.format('/ram-%03d.bin',second),'wb'))
            out:write(memory:read_range(0x500000,0x5fffff,8));out:close()
            local backup=assert(io.open(root..string.format('/backup-%03d.bin',second),'wb'))
            backup:write(memory:read_range(0x1d00000,0x1d03fff,8));backup:close()
        end
    end
end)
