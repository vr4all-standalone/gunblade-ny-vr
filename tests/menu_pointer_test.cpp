#include "../native/menu_pointer.h"
#include <cassert>
using namespace gunblade;
int main(){
 MenuPointer pointer;std::array<bool,2> tracked{true,true},triggers{};
 auto p=pointer.update(true,true,tracked,triggers,0);assert(p.owner==0&&!p.activate);
 triggers[1]=true;p=pointer.update(true,true,tracked,triggers,0);assert(p.owner==1&&p.changed&&!p.activate);
 p=pointer.update(true,true,tracked,triggers,0);assert(!p.changed&&!p.activate);
 triggers[1]=false;pointer.update(true,true,tracked,triggers,0);triggers[1]=true;p=pointer.update(true,true,tracked,triggers,0);assert(p.owner==1&&p.activate);
 triggers={false,false};pointer.update(true,true,tracked,triggers,0);triggers={true,true};p=pointer.update(true,true,tracked,triggers,0);assert(p.owner==1&&!p.changed&&p.activate);
 pointer.update(false,true,tracked,triggers,0);p=pointer.update(true,true,tracked,triggers,0);assert(!p.activate);
 pointer.update(true,false,tracked,triggers,0);p=pointer.update(true,true,tracked,triggers,0);assert(!p.activate);
 tracked[1]=false;p=pointer.update(true,true,tracked,triggers,0);assert(p.owner==0&&!p.activate);
 tracked={false,false};p=pointer.update(true,true,tracked,triggers,0);assert(p.owner==-1&&!p.activate);
 tracked[1]=true;p=pointer.update(true,true,tracked,triggers,0);assert(p.owner==1&&!p.activate);
 triggers[1]=false;pointer.update(true,true,tracked,triggers,0);triggers[1]=true;p=pointer.update(true,true,tracked,triggers,0);assert(p.owner==1&&p.activate);
}
