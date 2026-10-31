-- Read-only original-game evidence. Only real cabinet inputs are driven.
local root=assert(os.getenv('GUNBLADE_RESEARCH_DIR'))
local mode=os.getenv('GUNBLADE_DIAGNOSTIC_MODE') or 'gameplay'
local machine=manager.machine
local mem=machine.devices[':maincpu'].spaces.program
local ports=machine.ioport.ports
local log=assert(io.open(root..'/native.tsv','w'))
log:write('time\tmode\tsubmode\tjoined\trender_mode\n')
local regions={{'workram',0x500000,0x5fffff},{'extraram',0x200000,0x23ffff},{'backup',0x1d00000,0x1d03fff}}
local function press(tag,mask,on)
 for _,field in pairs(assert(ports[tag]).fields) do
  if field.mask==mask then field:set_value(on and 1 or 0);return end
 end
 error('Missing cabinet input')
end
local previous=-1
gunblade_raster_proof_subscription=emu.add_machine_frame_notifier(function()
 local t=machine.time:as_double();local second=math.floor(t)
 log:write(string.format('%.17g\t%d\t%d\t%d\t%d\n',t,mem:read_u32(0x532d54),mem:read_u32(0x532d48),mem:read_u8(0x532d58),mem:read_u32(0x10000000)))
 if second~=previous then
  previous=second
  for _,region in ipairs(regions) do
   local out=assert(io.open(root..string.format('/%s-%03d.bin',region[1],second),'wb'))
   out:write(mem:read_range(region[2],region[3],8));out:close()
  end
  for _,screen in pairs(machine.screens) do screen:snapshot(root..string.format('/frame-%03d.png',second));break end
  log:flush()
 end
 if mode=='gameplay' then
  press(':IN0',1,t>=16 and t<16.2);press(':IN0',16,t>=16.5 and t<16.8)
  press(':IN0',2,t>=30 and t<30.2);press(':IN0',32,t>=30.5 and t<30.8)
  press(':IN1',1,t>=24 and t<42);press(':IN1',2,t>=32 and t<44)
 elseif mode=='service' then
  press(':IN0',4,t>=6 and t<6.2)
 end
end)
