#include "../llama.cpp/src/llama-moe-tiled.h"
#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
static void need(bool ok,const char *why) {if(!ok){std::cerr<<why<<'\n';std::exit(2);}}
static std::string digest(const uint8_t *p,size_t n) {
 std::array<uint8_t,32> d{};CC_SHA256(p,CC_LONG(n),d.data());std::string s;
 for(auto b:d){s+="0123456789abcdef"[b>>4];s+="0123456789abcdef"[b&15];}return s;
}
static void io(int fd,uint8_t *p,size_t n,uint64_t offset,bool write) {
 size_t done=0;while(done<n){ssize_t count=write?pwrite(fd,p+done,n-done,offset+done):pread(fd,p+done,n-done,offset+done);
 need(count>0,"short/failed sidecar IO");done+=size_t(count);}
}
int main(int argc,char **argv) {
 if (argc==3 && !std::strcmp(argv[1],"--hash")) {
  int fd=open(argv[2],O_RDONLY);need(fd>=0,"hash file open");need(fcntl(fd,F_NOCACHE,1)==0,"hash nocache");
  CC_SHA256_CTX ctx;CC_SHA256_Init(&ctx);std::array<uint8_t,262144> buffer;
  ssize_t count;while((count=read(fd,buffer.data(),buffer.size()))>0)CC_SHA256_Update(&ctx,buffer.data(),CC_LONG(count));
  need(count==0,"hash read failure");close(fd);std::array<uint8_t,32> value;CC_SHA256_Final(value.data(),&ctx);
  for(auto x:value)std::cout<<"0123456789abcdef"[x>>4]<<"0123456789abcdef"[x&15];std::cout<<'\n';return 0;
 }

 need(argc==6,"usage: compile-sidecar model plan sidecar index model-sha");
 const auto start=std::chrono::steady_clock::now();
 int input=open(argv[1],O_RDONLY);need(input>=0,"model open");
 int output=open(argv[3],O_CREAT|O_EXCL|O_RDWR,0600);need(output>=0,"sidecar must not already exist");
 struct stat st{};need(!fstat(input,&st),"source stat");need(!ftruncate(output,st.st_size),"sidecar size");
 const bool input_nocache=fcntl(input,F_NOCACHE,1)==0,output_nocache=fcntl(output,F_NOCACHE,1)==0;
 std::ifstream plan(argv[2]);std::ofstream index(argv[4]);need(bool(plan)&&bool(index),"plan/index open");
 std::array<uint8_t,4096> header{};std::memcpy(header.data(),"MOETIL16",8);
 // Fixed little-endian header on this ARM64 research compiler; no model metadata.
 const uint32_t fields[4]={1,16,30720,108};std::memcpy(header.data()+8,fields,sizeof(fields));
 need(std::strlen(argv[5])==64,"model SHA size");std::memcpy(header.data()+32,argv[5],64);
 io(output,header.data(),header.size(),0,true);
 const size_t max_bytes=860160;
 std::vector<uint8_t> raw(max_bytes),packed(max_bytes),restored(max_bytes);
 uint64_t count=0,total=0,nonfinite=0;
 index<<"[\n";
 unsigned layer,expert,part,rows,blocks;uint64_t offset,bytes;
 while(plan>>layer>>expert>>part>>offset>>bytes>>rows>>blocks) {
  need(layer<40&&expert<256&&part<3&&rows==(part==2?2048U:512U)&&blocks==(part==2?2U:8U)&&
       bytes==uint64_t(rows)*blocks*(part==2?210:144)&&offset>=4096&&offset+bytes<=uint64_t(st.st_size),"invalid plan");
  io(input,raw.data(),bytes,offset,false);
  moe_tiled_pack(raw.data(),packed.data(),rows,blocks,16,part==2);
  moe_tiled_unpack(packed.data(),restored.data(),rows,blocks,16,part==2);
  need(!std::memcmp(raw.data(),restored.data(),bytes),"offline inverse mismatch");
  bool finite=true;
  for(unsigned row=0;row<rows;++row)for(unsigned b=0;b<blocks;++b) {
   const auto *p=raw.data()+(row*blocks+b)*(part==2?210:144);
   for(int h=0;h<(part==2?1:2);++h){uint16_t value;std::memcpy(&value,p+(part==2?208:0)+h*2,2);
    finite &= (value&0x7c00)!=0x7c00;}
  }
  nonfinite+=!finite;
  io(output,packed.data(),bytes,offset,true);
  index<<(count?",\n":"")<<"{\"layer\":"<<layer<<",\"expert\":"<<expert<<",\"part\":"<<part
       <<",\"offset\":"<<offset<<",\"bytes\":"<<bytes<<",\"rows\":"<<rows<<",\"blocks\":"<<blocks
       <<",\"tile\":16,\"finite\":"<<(finite?"true":"false")<<",\"source_sha256\":\""<<digest(raw.data(),bytes)
       <<"\",\"packed_sha256\":\""<<digest(packed.data(),bytes)<<"\"}";
  total+=bytes;++count;
  if(count%768==0)std::cerr<<"components "<<count<<"/30720\n";
 }
 need(plan.eof()&&count==30720&&total==20887633920ULL,"incomplete plan");
 index<<"\n]\n";index.close();need(bool(index),"index write failed");
 need(!fsync(output)&&!fchmod(output,0400),"sidecar commit");
 struct stat final_stat{};need(!fstat(output,&final_stat),"output stat");close(input);close(output);
 double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 std::cout<<"{\"status\":\"OFFLINE_TILED_SIDECAR_INVERSE_PASS\",\"components\":"<<count
          <<",\"inverse_bytes_compared\":"<<total<<",\"payload_bytes_written\":"<<total
          <<",\"logical_file_bytes\":"<<final_stat.st_size<<",\"allocated_disk_bytes\":"<<uint64_t(final_stat.st_blocks)*512
          <<",\"bounded_buffer_payload_bytes\":"<<3*max_bytes<<",\"header_bytes\":4096,\"nonfinite_components\":"<<nonfinite
          <<",\"input_nocache\":"<<(input_nocache?"true":"false")<<",\"output_nocache\":"<<(output_nocache?"true":"false")
          <<",\"compile_wall_ms\":"<<ms<<"}\n";
}
