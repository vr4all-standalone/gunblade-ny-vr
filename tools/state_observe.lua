-- Read-only game-memory observation; only real cabinet inputs are driven.
local root=assert(os.getenv('GUNBLADE_RESEARCH_DIR'))
local m=manager.machine
local cpu=m.devices[':maincpu'];local memory=cpu.spaces.program
local ports=m.ioport.ports
local log=assert(io.open(root..'/observations.tsv','w'))
log:write('time\tip\tin0\tin1\tx1\ty1\tx2\ty2\n')
local function set(tag,mask,value)
    for _,field in pairs(assert(ports[tag]).fields) do if field.mask==mask then field:set_value(value);return end end
    error('Missing cabinet field '..tag..' '..mask)
end
local function press(tag,mask,value)set(tag,mask,value and 1 or 0)end
local last=-1;local second=-1
gunblade_observer=emu.add_machine_frame_notifier(function()
    local t=m.time:as_double();local tick=math.floor(t*10)
    if tick~=last then
        last=tick
        local file=assert(io.open(root..string.format('/ram-%04d.bin',tick),'wb'))
        file:write(memory:read_range(0x500000,0x5fffff,8));file:close()
        log:write(string.format('%.6f\t%08x\t%02x\t%02x\t%02x\t%02x\t%02x\t%02x\n',t,cpu.state['ip'].value,ports[':IN0']:read(),ports[':IN1']:read(),ports[':P1_X']:read(),ports[':P1_Y']:read(),ports[':P2_X']:read(),ports[':P2_Y']:read()));log:flush()
    end
    local s=math.floor(t)
    if s~=second then
        second=s
        for _,screen in pairs(m.screens)do screen:snapshot(root..string.format('/frame-%03d.png',s));break end
    end
    press(':IN0',1,t>=16 and t<16.2);press(':IN0',0x10,t>=16.5 and t<16.8)
    press(':IN0',2,t>=30 and t<30.2);press(':IN0',0x20,t>=30.5 and t<30.8)
    set(':P1_X',0xff,t>=36 and t<38 and 0x69 or 0xb1)
    set(':P1_Y',0xff,0x5f);set(':P2_X',0xff,0x50);set(':P2_Y',0xff,0x5f)
    press(':IN1',1,t>=24 and t<28 or t>=34 and t<42)
    press(':IN1',2,t>=32 and t<36 or t>=40 and t<44)
end)
