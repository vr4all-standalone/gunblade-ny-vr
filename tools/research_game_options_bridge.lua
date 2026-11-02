-- Synchronous isolated observer/real-input bridge. No native memory writes.
local root=assert(os.getenv('GUNBLADE_RESEARCH_DIR'))
local m=manager.machine
local memory=m.devices[':maincpu'].spaces.program
local fields=m.ioport.ports[':IN0'].fields
local addresses={0x075d64,0x075d68,0x0ed8f8,0x06d414,0x0f0bb0,0x0f1bcc,
    0x0efc10,0x0efca4,0x0f01bc,0x0f1ae8,0x00de30,0x00de34,
    0x532d54,0x532d48,0x532d58,0x5401d0,0x54bdf8,0x54bda4}
for address=0x54c0d0,0x54c124,4 do addresses[#addresses+1]=address end
local sequence=0
local finished=false
local function snapshot(label)
    for _,screen in pairs(m.screens) do screen:snapshot(root..'/'..label..'.png');break end
    local file=assert(io.open(root..'/'..label..'-backup.bin','wb'))
    file:write(memory:read_range(0x1d00000,0x1d03fff,8));file:close()
    file=assert(io.open(root..'/'..label..'-eeprom-buffer.bin','wb'))
    file:write(memory:read_range(0x54c0d0,0x54c127,8));file:close()
end
gunblade_game_options_bridge=emu.add_machine_frame_notifier(function()
    if finished or m.time:as_double()<16 then return end
    if sequence==0 then snapshot('before') end
    local file=assert(io.open(root..'/observe-'..sequence..'.tmp','wb'))
    for _,address in ipairs(addresses) do file:write(string.pack('<I4I4',address,memory:read_u32(address))) end
    file:close()
    assert(os.rename(root..'/observe-'..sequence..'.tmp',root..'/observe-'..sequence..'.bin'))
    local deadline=os.time()+10
    repeat file=io.open(root..'/control-'..sequence..'.txt','r') until file or os.time()>deadline
    assert(file,'Production helper bridge timed out')
    local line=file:read('*a');file:close()
    local buttons,status=line:match('(%d+) (%d+)');buttons=tonumber(buttons);status=tonumber(status)
    for _,field in pairs(fields) do
        if field.mask==4 or field.mask==8 then field:set_value((buttons&field.mask)~=0 and 1 or 0) end
    end
    if status~=0 then snapshot('after');finished=true end
    sequence=sequence+1
end)
