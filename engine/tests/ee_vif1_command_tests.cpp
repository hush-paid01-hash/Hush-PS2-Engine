#include "retroshelf/ee_vif1.hpp"
#include "retroshelf/ee_vif1_command_decoder.hpp"
#include "retroshelf/vu1_memory.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
namespace {
void append32(std::vector<std::uint8_t>& b, std::uint32_t v) { for (unsigned i=0;i<4;++i) b.push_back(static_cast<std::uint8_t>(v>>(i*8U))); }
std::uint32_t reg(const retroshelf::EeVif1Registers& r, std::uint32_t o) { std::vector<std::uint8_t>b; if(!r.read(o,4,b)) return 0xFFFFFFFFU; return static_cast<std::uint32_t>(b[0])|(static_cast<std::uint32_t>(b[1])<<8U)|(static_cast<std::uint32_t>(b[2])<<16U)|(static_cast<std::uint32_t>(b[3])<<24U); }
}
int main() {
 int failures=0; auto check=[&](bool ok,const char* m){if(!ok){std::cerr<<"FAIL: "<<m<<'\n';++failures;}else std::cout<<"PASS: "<<m<<'\n';};
 retroshelf::EeVif1Registers regs; retroshelf::Vu1Memory mem; retroshelf::EeVif1CommandDecoder decoder(regs,mem);
 std::vector<std::uint8_t> stream; append32(stream,0x01000101U); append32(stream,0x05000002U); append32(stream,0x20000000U); append32(stream,0xAABBCCDDU); append32(stream,0x30000000U); append32(stream,1);append32(stream,2);append32(stream,3);append32(stream,4); append32(stream,0x31000000U);append32(stream,5);append32(stream,6);append32(stream,7);append32(stream,8);
 check(decoder.consume(stream.data(),stream.size()),"decodes state commands and multiword payloads");
 check(reg(regs,retroshelf::EeVif1Registers::CycleOffset)==0x101U && reg(regs,retroshelf::EeVif1Registers::ModeOffset)==2U,"STCYCL and STMOD update registers");
 check(reg(regs,retroshelf::EeVif1Registers::MaskOffset)==0xAABBCCDDU,"STMASK consumes its payload");
 check(reg(regs,retroshelf::EeVif1Registers::RowBaseOffset+0x30U)==4U && reg(regs,retroshelf::EeVif1Registers::ColumnBaseOffset+0x20U)==7U,"STROW and STCOL populate all lanes");
 std::vector<std::uint8_t> top_setup; append32(top_setup,0x03000012U); append32(top_setup,0x02000007U);
 regs.write(retroshelf::EeVif1Registers::TopsOffset, std::vector<std::uint8_t>{0x2A,0,0,0});
 regs.set_status_flags(retroshelf::EeVif1Registers::StatDbf);
 check(decoder.consume(top_setup.data(),top_setup.size()),"executes BASE and OFFSET double-buffer commands");
 check(reg(regs,retroshelf::EeVif1Registers::OfstOffset)==7U && reg(regs,retroshelf::EeVif1Registers::BaseOffset)==0x2AU,"OFFSET stores OFST and copies TOPS into BASE");
 check((regs.status() & retroshelf::EeVif1Registers::StatDbf)==0U,"OFFSET clears the VIF1 double-buffer flag");
 std::vector<std::uint8_t> unpack; append32(unpack,0x60020002U); append32(unpack,0x11223344U); append32(unpack,0x55667788U);
 check(decoder.consume(unpack.data(),6U),"buffers incomplete UNPACK input");
 check(decoder.consume(unpack.data()+6U,unpack.size()-6U),"resumes split S-32 UNPACK stream");
 std::array<std::uint8_t,32> vectors{}; check(mem.read_data(32U,vectors.data(),vectors.size()),"reads decoded VU1 vectors");
 const std::array<std::uint8_t,4> a{{0x44,0x33,0x22,0x11}}, b{{0x88,0x77,0x66,0x55}}; bool replicated=true; for(std::size_t i=0;i<16;++i) replicated=replicated&&vectors[i]==a[i%4U]; for(std::size_t i=16;i<32;++i) replicated=replicated&&vectors[i]==b[i%4U];
 check(replicated,"S-32 expands each scalar across four lanes");
 std::vector<std::uint8_t> v4; append32(v4,0x6C010004U); append32(v4,0x01020304U); append32(v4,0x11121314U); append32(v4,0x21222324U); append32(v4,0x31323334U);
 check(decoder.consume(v4.data(),v4.size()),"decodes a V4-32 UNPACK vector");
 std::array<std::uint8_t,16> v4read{}; check(mem.read_data(64U,v4read.data(),v4read.size()),"reads V4-32 destination vector");
 check(v4read[0]==0x04 && v4read[4]==0x14 && v4read[8]==0x24 && v4read[12]==0x34,"V4-32 preserves all four input components");
 std::vector<std::uint8_t> mpg; append32(mpg,0x4A010001U); append32(mpg,0x44332211U); append32(mpg,0x88776655U);
 check(decoder.consume(mpg.data(),5U),"buffers incomplete MPG microcode payload");
 check(decoder.consume(mpg.data()+5U,mpg.size()-5U),"resumes split MPG microcode upload");
 std::array<std::uint8_t,16> micro{}; check(mem.read_micro(8U,micro.data(),micro.size()),"reads uploaded VU1 microcode");
 check(micro[0]==0x11 && micro[4]==0x55,"MPG writes payload bytes at IMMEDIATE times eight");
 check(reg(regs,retroshelf::EeVif1Registers::CodeOffset)==0x4A010001U,"CODE records the latest VIF1 command");
 std::vector<std::uint8_t> mpg_overflow; append32(mpg_overflow,0x4A01FFFFU);
 check(!decoder.consume(mpg_overflow.data(),mpg_overflow.size()),"rejects MPG destination overflow before payload transfer");
 decoder.reset();
 std::vector<std::uint8_t> overflow; append32(overflow,0x6002FFFFU);
 check(!decoder.consume(overflow.data(),overflow.size()),"rejects UNPACK vector ranges that exceed VU1 memory before writing");
 decoder.reset();
 const std::uint8_t invalid[4]={0,0,0,0x40}; check(!decoder.consume(invalid,4U)&&(regs.status()&retroshelf::EeVif1Registers::StatEr1)!=0U,"rejects unsupported commands and sets ER1");
 decoder.reset(); check(decoder.last_error()[0]=='\0',"reset clears decoder error state");
 return failures==0?0:1;
}
