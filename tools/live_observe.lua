-- Observation only. The production host's simulation drives cabinet inputs.
local root=assert(os.getenv('GUNBLADE_RESEARCH_DIR'))
local m=manager.machine;local cpu=m.devices[':maincpu'];local mem=cpu.spaces.program
local log=assert(io.open(root..'/live-native.tsv','w'));local previous=-1
log:write('native_time\tmode\tsubmode\tjoined\tlives1\tlives2\tstage_tick\tcursor\tu1\tv1\tu2\tv2\n')
local function native_point(address)
    return string.unpack('<f',string.pack('<I4',mem:read_u32(address)))
end
gunblade_live_observer=emu.add_machine_frame_notifier(function()
    local t=m.time:as_double();local sec=math.floor(t)
    if sec~=previous then
        previous=sec
        log:write(string.format('%.6f\t%d\t%d\t%d\t%d\t%d\t%d\t%d',t,mem:read_u32(0x532d54),mem:read_u32(0x532d48),mem:read_u8(0x532d58),mem:read_u32(0x540114),mem:read_u32(0x540154),mem:read_u32(0x540088),mem:read_u32(0x5329ac)))
        for player=0,1 do log:write(string.format('\t%.9f\t%.9f',(248+60*native_point(0x54011c+64*player))/495,(192-60*native_point(0x540120+64*player))/383))end
        log:write('\n');log:flush()
        if sec%5==0 then for _,screen in pairs(m.screens)do screen:snapshot(root..string.format('/native-%03d.png',sec));break end end
    end
end)
