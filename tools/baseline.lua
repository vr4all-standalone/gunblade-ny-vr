-- Native cabinet operations only. Never modify original game RAM or skip boot.
local root=assert(os.getenv('GUNBLADE_RESEARCH_DIR'))
local mode=os.getenv('GUNBLADE_DIAGNOSTIC_MODE') or 'attract'
local page=os.getenv('GUNBLADE_OPERATOR_PAGE') or 'menu'
local log=assert(io.open(root..'/events.txt','w'))
local machine=manager.machine
local ports=machine.ioport.ports
for tag,port in pairs(ports) do
    for name,field in pairs(port.fields) do log:write(string.format('PORT %s | %s | %x\n',tag,name,field.mask)) end
end
log:flush()
local function set(tag,mask,value)
    local port=assert(ports[tag],'Missing port '..tag)
    for _,field in pairs(port.fields) do if field.mask==mask then field:set_value(value);return end end
    error('Missing field '..tag..' '..mask)
end
local function press(tag,mask,enabled) set(tag,mask,enabled and 1 or 0) end
local previous=-1
gunblade_diagnostic_subscription=emu.add_machine_frame_notifier(function()
    local t=machine.time:as_double();local second=math.floor(t)
    if second~=previous then
        previous=second
        log:write(string.format('TIME %.6f IP %08x IN0 %02x IN1 %02x IN2 %02x X1 %02x Y1 %02x X2 %02x Y2 %02x\n',t,machine.devices[':maincpu'].state['ip'].value,ports[':IN0']:read(),ports[':IN1']:read(),ports[':IN2']:read(),ports[':P1_X']:read(),ports[':P1_Y']:read(),ports[':P2_X']:read(),ports[':P2_Y']:read()));log:flush()
        if second==1 or second==5 or second%5==0 then
            for _,screen in pairs(machine.screens) do screen:snapshot(root..string.format('/frame-%03d.png',second));break end
        end
        if second%5==0 and second>0 then
            local out=assert(io.open(root..string.format('/workram-%03d.bin',second),'wb'))
            out:write(machine.devices[':maincpu'].spaces.program:read_range(0x500000,0x5fffff,8));out:close()
        end
    end
    if mode=='gameplay' then
        press(':IN0',0x01,t>=16 and t<16.2)
        press(':IN0',0x10,t>=16.5 and t<16.8)
        press(':IN0',0x02,t>=30 and t<30.2)
        press(':IN0',0x20,t>=30.5 and t<30.8)
        set(':P1_X',0xff,0xb1);set(':P1_Y',0xff,0x5f)
        set(':P2_X',0xff,0x50);set(':P2_Y',0xff,0x5f)
        -- Gunblade automatic fire is investigated with sustained native pulls.
        press(':IN1',0x01,t>=24 and t<42)
        press(':IN1',0x02,t>=32 and t<48)
    elseif mode=='service' then
        -- Original TEST MENU: Service advances; Test selects. From EXIT,
        -- two advances select INPUT TEST; eight advances select AIM SET.
        local count=page=='input' and 2 or page=='aim' and 8 or 0
        local advance=t>=10 and t<10+count and t-math.floor(t)<.2
        press(':IN0',0x08,advance)
        press(':IN0',0x04,t>=6 and t<6.2 or count>0 and t>=21 and t<21.2)
        if page=='input' then
            local corner=t>=24 and t<26 and 0 or t>=26 and t<28 and 1 or .5
            set(':P1_X',0xff,math.floor(0x69+corner*(0xff-0x69)+.5))
            set(':P2_X',0xff,math.floor(corner*0x96+.5))
            set(':P1_Y',0xff,math.floor(0x11+corner*(0xae-0x11)+.5))
            set(':P2_Y',0xff,math.floor(0x11+corner*(0xae-0x11)+.5))
            press(':IN1',1,t>=24 and t<26);press(':IN1',2,t>=26 and t<28)
        end
    end
end)
